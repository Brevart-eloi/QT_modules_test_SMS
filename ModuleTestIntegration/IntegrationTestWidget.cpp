#include "IntegrationTestWidget.h"

#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include <QLineEdit>
#include <QSpinBox>
#include <QListWidget>
#include <QListWidgetItem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QSplitter>
#include <QDateTime>
#include <QFont>

// ============================================================================
//  Constructeur
// ============================================================================

IntegrationTestWidget::IntegrationTestWidget(QWidget *parent)
    : QWidget(parent),
      m_controller  (new SurveillanceController(this)),
      m_httpApi     (new SurveillanceHttpApi(m_controller, this)),
      m_testListener(new TestAlertListener(this))
{
    setWindowTitle("Test d'integration - Surveillance + Alarme + Observer (E2)");
    resize(900, 780);

    QVBoxLayout *main = new QVBoxLayout(this);

    // ======================================================================
    //  Configuration
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
    spinAlarmDuration->setValue(1);
    spinAlarmDuration->setSuffix(" s");

    // Adresses Modbus (par defaut, a adapter au cablage reel)
    spinSirenCoil   = new QSpinBox(); spinSirenCoil->setRange(0, 15);   spinSirenCoil->setValue(2);
    spinFlashCoil   = new QSpinBox(); spinFlashCoil->setRange(0, 15);   spinFlashCoil->setValue(1);
    spinDoorInput   = new QSpinBox(); spinDoorInput->setRange(0, 17);   spinDoorInput->setValue(0);
    spinWindowInput = new QSpinBox(); spinWindowInput->setRange(0, 17); spinWindowInput->setValue(1);

    formConf->addRow("IP PET-7067 (sorties : sirene, flash, gache) :", editOutputIp);
    formConf->addRow("IP PET-7050 (entrees : capteurs porte/fenetre) :", editInputIp);
    formConf->addRow("Port API HTTP (arm/disarm depuis IHM E1) :", spinHttpPort);
    formConf->addRow("Duree alarme :", spinAlarmDuration);
    formConf->addRow("Adresse DO sirene :", spinSirenCoil);
    formConf->addRow("Adresse DO flash :", spinFlashCoil);
    formConf->addRow("Adresse DI porte :", spinDoorInput);
    formConf->addRow("Adresse DI fenetre :", spinWindowInput);

    main->addWidget(grpConf);

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

    formSt->addRow("Surveillance :", labelArmed);
    formSt->addRow("Porte :",        labelDoor);
    formSt->addRow("Fenetre :",      labelWindow);
    formSt->addRow("Alarme :",       labelAlarm);

    main->addWidget(grpState);

    //  Zone alertes (pattern Observer)
    QGroupBox   *grpAlerts  = new QGroupBox("Alertes recues — Pattern Observer (TestAlertListener → onAlert())");
    QVBoxLayout *alertsLayout = new QVBoxLayout(grpAlerts);

    QLabel *labelObserverInfo = new QLabel(
        "Chaque ligne ci-dessous correspond a un appel de TestAlertListener::onAlert()\n"
        "declenche par SurveillanceController::notifyAlert() via le pattern Observer.\n"
        "En production, remplacer TestAlertListener par AlertNotifier (E3) pour les vrais mails/SMS."
    );
    labelObserverInfo->setStyleSheet("color: #555; font-style: italic; font-size: 10px;");
    alertsLayout->addWidget(labelObserverInfo);

    listAlerts = new QListWidget();
    listAlerts->setMaximumHeight(120);
    listAlerts->setStyleSheet(
        "QListWidget { background: #fff8e1; border: 1px solid #f0c040; }"
        "QListWidget::item { padding: 3px; }"
    );
    alertsLayout->addWidget(listAlerts);

    btnClearAlerts = new QPushButton("Effacer les alertes");
    alertsLayout->addWidget(btnClearAlerts);

    main->addWidget(grpAlerts);

    //  Journal general
    main->addWidget(new QLabel("Journal :"));
    textLog = new QTextEdit();
    textLog->setReadOnly(true);
    textLog->setStyleSheet("font-family: Consolas, monospace; font-size: 11px;");
    main->addWidget(textLog);

    //  Connexions signaux / slots

    // Boutons
    connect(btnStart,       &QPushButton::clicked, this, &IntegrationTestWidget::onBtnStartClicked);
    connect(btnStop,        &QPushButton::clicked, this, &IntegrationTestWidget::onBtnStopClicked);
    connect(btnArm,         &QPushButton::clicked, this, &IntegrationTestWidget::onBtnArmClicked);
    connect(btnDisarm,      &QPushButton::clicked, this, &IntegrationTestWidget::onBtnDisarmClicked);
    connect(btnClearAlerts, &QPushButton::clicked, this, &IntegrationTestWidget::onBtnClearAlertsClicked);

    // SurveillanceController → IHM
    connect(m_controller, &SurveillanceController::logMessage,
            this, &IntegrationTestWidget::onLog);
    connect(m_controller, &SurveillanceController::armedChanged,
            this, &IntegrationTestWidget::onArmedChanged);
    connect(m_controller, &SurveillanceController::doorStateChanged,
            this, &IntegrationTestWidget::onDoorStateChanged);
    connect(m_controller, &SurveillanceController::windowStateChanged,
            this, &IntegrationTestWidget::onWindowStateChanged);
    connect(m_controller, &SurveillanceController::alarmStarted,
            this, &IntegrationTestWidget::onAlarmStarted);
    connect(m_controller, &SurveillanceController::alarmStopped,
            this, &IntegrationTestWidget::onAlarmStopped);
    connect(m_controller, &SurveillanceController::intrusionDetected,
            this, &IntegrationTestWidget::onIntrusionDetected);

    // SurveillanceHttpApi → IHM
    connect(m_httpApi, &SurveillanceHttpApi::logMessage,
            this, &IntegrationTestWidget::onLog);

    connect(m_testListener, &TestAlertListener::alertReceived,
            this, &IntegrationTestWidget::onAlertReceived);

    m_controller->addAlertEventListener(m_testListener);

    // ──────────────────────────────────────────────────────────────────────

    log("=== Test d'integration - Surveillance / Alarme / Observer ===");
    log("Configurez les IP des modules PET, puis cliquez sur 'Demarrer le service'.");
    log("TestAlertListener enregistre sur SurveillanceController.");
}

IntegrationTestWidget::~IntegrationTestWidget()
{
    m_controller->removeAlertEventListener(m_testListener);
}

//  Start / Stop

void IntegrationTestWidget::onBtnStartClicked()
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

    if (m_controller->start()) {
        m_httpApi->start(static_cast<quint16>(spinHttpPort->value()));
        updateButtons();
    }
}

void IntegrationTestWidget::onBtnStopClicked()
{
    m_httpApi->stop();
    m_controller->stop();
    updateButtons();
}

//  Arm / Disarm

void IntegrationTestWidget::onBtnArmClicked()
{
    m_controller->arm();
}

void IntegrationTestWidget::onBtnDisarmClicked()
{
    m_controller->disarm();
}

//  Alertes (pattern Observer)

void IntegrationTestWidget::onAlertReceived(const QString &description)
{
    const QString ts   = QDateTime::currentDateTime().toString("hh:mm:ss");
    const QString text = QString("[%1] ⚠ %2").arg(ts, description);

    QListWidgetItem *item = new QListWidgetItem(text, listAlerts);
    item->setForeground(QColor("#b71c1c"));   
    QFont f = item->font();
    f.setBold(true);
    item->setFont(f);
    listAlerts->scrollToBottom();

    log(QString("*** ALERTE via Observer (TestAlertListener::onAlert) : %1").arg(description));
}

void IntegrationTestWidget::onBtnClearAlertsClicked()
{
    listAlerts->clear();
}

//  Mise a jour de l'etat du systeme

void IntegrationTestWidget::onArmedChanged(bool armed)
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

void IntegrationTestWidget::onDoorStateChanged(bool open)
{
    labelDoor->setText(open ? "OUVERTE" : "fermee");
    labelDoor->setStyleSheet(open ? "color: #b71c1c; font-weight: bold;" : "color: #2e7d32;");
}

void IntegrationTestWidget::onWindowStateChanged(bool open)
{
    labelWindow->setText(open ? "OUVERTE" : "fermee");
    labelWindow->setStyleSheet(open ? "color: #b71c1c; font-weight: bold;" : "color: #2e7d32;");
}

void IntegrationTestWidget::onAlarmStarted()
{
    labelAlarm->setText("EN COURS — sirene + flash actifs");
    labelAlarm->setStyleSheet("font-weight: bold; color: #b71c1c;");
    log("!!! ALARME DECLENCHEE !!!");
}

void IntegrationTestWidget::onAlarmStopped()
{
    labelAlarm->setText("inactive");
    labelAlarm->setStyleSheet("color: gray;");
}

void IntegrationTestWidget::onIntrusionDetected(const QString &source)
{
    log(QString(">>> INTRUSION DETECTEE : %1").arg(source));
}

//  Utilitaires

void IntegrationTestWidget::onLog(const QString &msg)
{
    log(msg);
}

void IntegrationTestWidget::log(const QString &msg)
{
    const QString ts = QDateTime::currentDateTime().toString("hh:mm:ss");
    textLog->append(QString("[%1] %2").arg(ts, msg));
}

void IntegrationTestWidget::updateButtons()
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
}
