#include "IntegrationVideoWidget.h"

#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QListWidget>
#include <QListWidgetItem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QDateTime>
#include <QFont>

// ============================================================================
//  Constructeur
// ============================================================================

IntegrationVideoWidget::IntegrationVideoWidget(QWidget *parent)
    : QWidget(parent),
      m_controller  (new SurveillanceController(this)),
      m_httpApi     (new SurveillanceHttpApi(m_controller, this)),
      m_testListener(new TestAlertListener(this)),
      m_videoClient (new VideoRecorderClient(this))
{
    setWindowTitle("Test d'integration - Surveillance + Alarme + Enregistrement video (E2)");
    resize(920, 860);

    QVBoxLayout *main = new QVBoxLayout(this);

    // ======================================================================
    //  Configuration Modbus + API HTTP
    // ======================================================================
    QGroupBox   *grpConf  = new QGroupBox("Configuration des modules Modbus et de l'API HTTP");
    QFormLayout *formConf = new QFormLayout(grpConf);

    editOutputIp = new QLineEdit("172.29.240.1");   // PET-7067 (sorties)
    editInputIp  = new QLineEdit("172.29.240.2");   // PET-7050 (entrees)
    spinHttpPort = new QSpinBox();
    spinHttpPort->setRange(1024, 65535);
    spinHttpPort->setValue(8080);
    spinAlarmDuration = new QSpinBox();
    spinAlarmDuration->setRange(1, 600);
    spinAlarmDuration->setValue(5);
    spinAlarmDuration->setSuffix(" s");

    spinSirenCoil   = new QSpinBox(); spinSirenCoil->setRange(0, 15);   spinSirenCoil->setValue(2);
    spinFlashCoil   = new QSpinBox(); spinFlashCoil->setRange(0, 15);   spinFlashCoil->setValue(1);
    spinDoorInput   = new QSpinBox(); spinDoorInput->setRange(0, 17);   spinDoorInput->setValue(0);
    spinWindowInput = new QSpinBox(); spinWindowInput->setRange(0, 17); spinWindowInput->setValue(1);

    chkInvertInputs = new QCheckBox("Capteurs NC (Normalement Fermes — contact ferme = porte/fenetre fermee)");
    chkInvertInputs->setChecked(true);

    formConf->addRow("IP PET-7067 (sorties : sirene, flash, gache) :", editOutputIp);
    formConf->addRow("IP PET-7050 (entrees : capteurs porte/fenetre) :", editInputIp);
    formConf->addRow("Port API HTTP (arm/disarm depuis IHM E1) :", spinHttpPort);
    formConf->addRow("Duree alarme :", spinAlarmDuration);
    formConf->addRow("Adresse DO sirene :", spinSirenCoil);
    formConf->addRow("Adresse DO flash :", spinFlashCoil);
    formConf->addRow("Adresse DI porte :", spinDoorInput);
    formConf->addRow("Adresse DI fenetre :", spinWindowInput);
    formConf->addRow("", chkInvertInputs);

    main->addWidget(grpConf);

    // ======================================================================
    //  NOUVEAU : configuration du service d'enregistrement video (E1)
    // ======================================================================
    QGroupBox   *grpVideo  = new QGroupBox("Service d'enregistrement video (E1) — protocole non encore defini");
    QFormLayout *formVideo = new QFormLayout(grpVideo);

    editVideoIp  = new QLineEdit("127.0.0.1");      // service video de E1 (a definir)
    spinVideoPort = new QSpinBox();
    spinVideoPort->setRange(1, 65535);
    spinVideoPort->setValue(9000);

    QLabel *labelVideoInfo = new QLabel(
        "Messages arbitraires envoyes en TCP (START_RECORDING / STOP_RECORDING),\n"
        "a remplacer quand E1 aura publie son protocole de pilotage.\n"
        "Pour tester sans le service de E1 : lancer un ecouteur (ex. ncat -lk 9000)."
    );
    labelVideoInfo->setStyleSheet("color: #555; font-style: italic; font-size: 10px;");

    formVideo->addRow("IP service video :", editVideoIp);
    formVideo->addRow("Port service video :", spinVideoPort);
    formVideo->addRow("", labelVideoInfo);

    main->addWidget(grpVideo);

    // ======================================================================
    //  Boutons start / stop service
    // ======================================================================
    QHBoxLayout *srvLayout = new QHBoxLayout();
    btnStart = new QPushButton("Demarrer le service");
    btnStop  = new QPushButton("Arreter");
    btnStop->setEnabled(false);
    btnStart->setMinimumHeight(36);
    btnStop->setMinimumHeight(36);
    srvLayout->addWidget(btnStart);
    srvLayout->addWidget(btnStop);
    srvLayout->addStretch();
    main->addLayout(srvLayout);

    // ======================================================================
    //  Mode surveillance (arm / disarm)
    // ======================================================================
    QGroupBox   *grpArm   = new QGroupBox("Mode surveillance");
    QHBoxLayout *armLayout = new QHBoxLayout(grpArm);
    btnArm    = new QPushButton("ARMER");
    btnDisarm = new QPushButton("DESARMER");
    btnArm->setEnabled(false);
    btnDisarm->setEnabled(false);
    btnArm->setMinimumHeight(40);
    btnDisarm->setMinimumHeight(40);
    btnArm->setStyleSheet("font-weight: bold;");
    btnDisarm->setStyleSheet("font-weight: bold;");
    armLayout->addWidget(btnArm);
    armLayout->addWidget(btnDisarm);
    main->addWidget(grpArm);

    // ======================================================================
    //  Etat du systeme
    // ======================================================================
    QGroupBox   *grpState = new QGroupBox("Etat du systeme");
    QFormLayout *formSt   = new QFormLayout(grpState);

    labelArmed  = new QLabel("DESARME");
    labelArmed->setStyleSheet("font-weight: bold; color: gray;");
    labelDoor   = new QLabel("---");
    labelWindow = new QLabel("---");
    labelAlarm  = new QLabel("---");
    labelRecording = new QLabel("inactif");          // NOUVEAU
    labelRecording->setStyleSheet("color: gray;");

    formSt->addRow("Surveillance :", labelArmed);
    formSt->addRow("Porte :",        labelDoor);
    formSt->addRow("Fenetre :",      labelWindow);
    formSt->addRow("Alarme :",       labelAlarm);
    formSt->addRow("Enregistrement video :", labelRecording);   // NOUVEAU

    main->addWidget(grpState);

    // ======================================================================
    //  NOUVEAU : test direct du client video (sans declencher l'alarme)
    // ======================================================================
    QGroupBox   *grpVideoTest  = new QGroupBox("Test direct du client video (envoi manuel des messages TCP)");
    QHBoxLayout *videoTestLayout = new QHBoxLayout(grpVideoTest);
    btnTestVideoStart = new QPushButton("Tester START enregistrement");
    btnTestVideoStop  = new QPushButton("Tester STOP enregistrement");
    videoTestLayout->addWidget(btnTestVideoStart);
    videoTestLayout->addWidget(btnTestVideoStop);
    main->addWidget(grpVideoTest);

    // ======================================================================
    //  Zone alertes (pattern Observer)
    // ======================================================================
    QGroupBox   *grpAlerts  = new QGroupBox("Alertes recues — Pattern Observer (TestAlertListener -> onAlert())");
    QVBoxLayout *alertsLayout = new QVBoxLayout(grpAlerts);

    listAlerts = new QListWidget();
    listAlerts->setMaximumHeight(110);
    listAlerts->setStyleSheet(
        "QListWidget { background: #fff8e1; border: 1px solid #f0c040; }"
        "QListWidget::item { padding: 3px; }"
    );
    alertsLayout->addWidget(listAlerts);

    btnClearAlerts = new QPushButton("Effacer les alertes");
    alertsLayout->addWidget(btnClearAlerts);

    main->addWidget(grpAlerts);

    // ======================================================================
    //  Journal general
    // ======================================================================
    main->addWidget(new QLabel("Journal :"));
    textLog = new QTextEdit();
    textLog->setReadOnly(true);
    textLog->setStyleSheet("font-family: Consolas, monospace; font-size: 11px;");
    main->addWidget(textLog);

    // ======================================================================
    //  Connexions signaux / slots
    // ======================================================================

    // Boutons
    connect(btnStart,       &QPushButton::clicked, this, &IntegrationVideoWidget::onBtnStartClicked);
    connect(btnStop,        &QPushButton::clicked, this, &IntegrationVideoWidget::onBtnStopClicked);
    connect(btnArm,         &QPushButton::clicked, this, &IntegrationVideoWidget::onBtnArmClicked);
    connect(btnDisarm,      &QPushButton::clicked, this, &IntegrationVideoWidget::onBtnDisarmClicked);
    connect(btnClearAlerts, &QPushButton::clicked, this, &IntegrationVideoWidget::onBtnClearAlertsClicked);
    connect(btnTestVideoStart, &QPushButton::clicked, this, &IntegrationVideoWidget::onBtnTestVideoStartClicked);
    connect(btnTestVideoStop,  &QPushButton::clicked, this, &IntegrationVideoWidget::onBtnTestVideoStopClicked);

    // SurveillanceController -> IHM
    connect(m_controller, &SurveillanceController::logMessage,
            this, &IntegrationVideoWidget::onLog);
    connect(m_controller, &SurveillanceController::armedChanged,
            this, &IntegrationVideoWidget::onArmedChanged);
    connect(m_controller, &SurveillanceController::doorStateChanged,
            this, &IntegrationVideoWidget::onDoorStateChanged);
    connect(m_controller, &SurveillanceController::windowStateChanged,
            this, &IntegrationVideoWidget::onWindowStateChanged);
    connect(m_controller, &SurveillanceController::alarmStarted,
            this, &IntegrationVideoWidget::onAlarmStarted);
    connect(m_controller, &SurveillanceController::alarmStopped,
            this, &IntegrationVideoWidget::onAlarmStopped);
    connect(m_controller, &SurveillanceController::intrusionDetected,
            this, &IntegrationVideoWidget::onIntrusionDetected);

    // SurveillanceHttpApi -> IHM
    connect(m_httpApi, &SurveillanceHttpApi::logMessage,
            this, &IntegrationVideoWidget::onLog);

    // Pattern Observer
    connect(m_testListener, &TestAlertListener::alertReceived,
            this, &IntegrationVideoWidget::onAlertReceived);
    m_controller->addAlertEventListener(m_testListener);

    // NOUVEAU : VideoRecorderClient -> IHM
    connect(m_videoClient, &VideoRecorderClient::logMessage,
            this, &IntegrationVideoWidget::onVideoLog);
    connect(m_videoClient, &VideoRecorderClient::recordingStarted,
            this, &IntegrationVideoWidget::onRecordingStarted);
    connect(m_videoClient, &VideoRecorderClient::recordingStopped,
            this, &IntegrationVideoWidget::onRecordingStopped);

    // ──────────────────────────────────────────────────────────────────────

    log("=== Test d'integration - Surveillance / Alarme / Observer / VIDEO ===");
    log("Configurez les IP des modules PET et du service video, puis 'Demarrer le service'.");
    log("Regle d'integration : quelqu'un entre (intrusion) -> START video ; fin d'alarme -> STOP video.");
}

IntegrationVideoWidget::~IntegrationVideoWidget()
{
    m_controller->removeAlertEventListener(m_testListener);
}

// ============================================================================
//  Start / Stop service
// ============================================================================

void IntegrationVideoWidget::onBtnStartClicked()
{
    const QString outIp = editOutputIp->text().trimmed();
    const QString inIp  = editInputIp->text().trimmed();

    if (!outIp.isEmpty()) m_controller->setOutputModuleAddress(outIp);
    if (!inIp.isEmpty())  m_controller->setInputModuleAddress(inIp);

    m_controller->setSirenCoil  (static_cast<quint16>(spinSirenCoil->value()));
    m_controller->setFlashCoil  (static_cast<quint16>(spinFlashCoil->value()));
    m_controller->setDoorInputAddress  (static_cast<quint16>(spinDoorInput->value()));
    m_controller->setWindowInputAddress(static_cast<quint16>(spinWindowInput->value()));
    m_controller->setAlarmDuration(spinAlarmDuration->value() * 1000);
    m_controller->setInputsInverted(chkInvertInputs->isChecked());

    // NOUVEAU : configuration de la cible video
    m_videoClient->setServerAddress(editVideoIp->text().trimmed(),
                                    static_cast<quint16>(spinVideoPort->value()));

    if (m_controller->start()) {
        m_httpApi->start(static_cast<quint16>(spinHttpPort->value()));
        updateButtons();
    }
}

void IntegrationVideoWidget::onBtnStopClicked()
{
    m_httpApi->stop();
    m_controller->stop();
    updateButtons();
}

// ============================================================================
//  Arm / Disarm
// ============================================================================

void IntegrationVideoWidget::onBtnArmClicked()
{
    m_controller->arm();
}

void IntegrationVideoWidget::onBtnDisarmClicked()
{
    m_controller->disarm();
}

// ============================================================================
//  Alertes (pattern Observer)
// ============================================================================

void IntegrationVideoWidget::onAlertReceived(const QString &description)
{
    const QString ts   = QDateTime::currentDateTime().toString("hh:mm:ss");
    const QString text = QString("[%1] /!\\ %2").arg(ts, description);

    QListWidgetItem *item = new QListWidgetItem(text, listAlerts);
    item->setForeground(QColor("#b71c1c"));
    QFont f = item->font();
    f.setBold(true);
    item->setFont(f);
    listAlerts->scrollToBottom();

    log(QString("*** ALERTE via Observer (TestAlertListener::onAlert) : %1").arg(description));
}

void IntegrationVideoWidget::onBtnClearAlertsClicked()
{
    listAlerts->clear();
}

// ============================================================================
//  Mise a jour de l'etat du systeme
// ============================================================================

void IntegrationVideoWidget::onArmedChanged(bool armed)
{
    if (armed) {
        labelArmed->setText("ARME — surveillance active");
        labelArmed->setStyleSheet("font-weight: bold; color: #b71c1c;");
    } else {
        labelArmed->setText("DESARME");
        labelArmed->setStyleSheet("font-weight: bold; color: #2e7d32;");
    }
    updateButtons();
}

void IntegrationVideoWidget::onDoorStateChanged(bool open)
{
    labelDoor->setText(open ? "OUVERTE" : "fermee");
    labelDoor->setStyleSheet(open ? "color: #b71c1c; font-weight: bold;" : "color: #2e7d32;");
}

void IntegrationVideoWidget::onWindowStateChanged(bool open)
{
    labelWindow->setText(open ? "OUVERTE" : "fermee");
    labelWindow->setStyleSheet(open ? "color: #b71c1c; font-weight: bold;" : "color: #2e7d32;");
}

void IntegrationVideoWidget::onAlarmStarted()
{
    labelAlarm->setText("EN COURS — sirene + flash actifs");
    labelAlarm->setStyleSheet("font-weight: bold; color: #b71c1c;");
    log("!!! ALARME DECLENCHEE !!!");
}

void IntegrationVideoWidget::onAlarmStopped()
{
    labelAlarm->setText("inactive");
    labelAlarm->setStyleSheet("color: gray;");

    // ── INTEGRATION VIDEO : fin de l'episode d'alarme -> arret enregistrement ──
    if (m_videoClient->isRecording()) {
        log(">>> Fin de l'alarme -> arret de l'enregistrement video.");
        m_videoClient->stopRecording();
    }
}

void IntegrationVideoWidget::onIntrusionDetected(const QString &source)
{
    log(QString(">>> INTRUSION DETECTEE : %1").arg(source));

    // ── INTEGRATION VIDEO : quelqu'un entre dans le labo -> demarrage video ──
    if (!m_videoClient->isRecording()) {
        log(">>> Quelqu'un est entre -> demarrage de l'enregistrement video.");
        m_videoClient->startRecording();
    }
}

// ============================================================================
//  NOUVEAU : enregistrement video (signaux du VideoRecorderClient)
// ============================================================================

void IntegrationVideoWidget::onVideoLog(const QString &msg)
{
    log("[VIDEO] " + msg);
}

void IntegrationVideoWidget::onRecordingStarted()
{
    labelRecording->setText("EN COURS — enregistrement video actif");
    labelRecording->setStyleSheet("font-weight: bold; color: #b71c1c;");
}

void IntegrationVideoWidget::onRecordingStopped()
{
    labelRecording->setText("inactif");
    labelRecording->setStyleSheet("color: gray;");
}

void IntegrationVideoWidget::onBtnTestVideoStartClicked()
{
    // Test direct : on configure l'adresse puis on envoie le message de debut
    m_videoClient->setServerAddress(editVideoIp->text().trimmed(),
                                    static_cast<quint16>(spinVideoPort->value()));
    m_videoClient->startRecording();
}

void IntegrationVideoWidget::onBtnTestVideoStopClicked()
{
    m_videoClient->setServerAddress(editVideoIp->text().trimmed(),
                                    static_cast<quint16>(spinVideoPort->value()));
    m_videoClient->stopRecording();
}

// ============================================================================
//  Utilitaires
// ============================================================================

void IntegrationVideoWidget::onLog(const QString &msg)
{
    log(msg);
}

void IntegrationVideoWidget::log(const QString &msg)
{
    const QString ts = QDateTime::currentDateTime().toString("hh:mm:ss");
    textLog->append(QString("[%1] %2").arg(ts, msg));
}

void IntegrationVideoWidget::updateButtons()
{
    const bool running = m_controller->isRunning();
    btnStart->setEnabled(!running);
    btnStop->setEnabled(running);
    btnArm->setEnabled(running);
    btnDisarm->setEnabled(running);

    editOutputIp->setEnabled(!running);
    editInputIp->setEnabled(!running);
    spinHttpPort->setEnabled(!running);
    spinAlarmDuration->setEnabled(!running);
    spinSirenCoil->setEnabled(!running);
    spinFlashCoil->setEnabled(!running);
    spinDoorInput->setEnabled(!running);
    spinWindowInput->setEnabled(!running);
    chkInvertInputs->setEnabled(!running);

    // Le service video peut etre reconfigure tant que le service n'est pas demarre
    editVideoIp->setEnabled(!running);
    spinVideoPort->setEnabled(!running);
}
