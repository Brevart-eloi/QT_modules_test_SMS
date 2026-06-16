#include "LaboMonitoringWidget.h"

#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QUrl>
#include <QHostAddress>
#include <QTextDocument>
#include <QPalette>
#include <QTextCursor>

// ─── Adresses reseau (cablage labo — ne pas modifier) 
static const QString MODBUS_OUT_IP   = "172.29.240.1"; // PET-7067 sorties
static const QString MODBUS_IN_IP    = "172.29.240.2"; // PET-7050 entrees
static const QString ADJUDICATOR_URL = "http://172.29.19.193";
static const quint16 RFID_PORT       = 8080;
static const quint16 ALARM_API_PORT  = 80;

// IPs des lecteurs RFID Arduino
static const QString READER_IP_1 = "172.29.19.200"; // Lecteur 1 
static const QString READER_IP_2 = "172.29.19.201"; // Lecteur 2 

static const QString SENSOR_NAMES[6] = {
    "Mvt CIEL1",        // DI4 
    "Porte CIEL1",      // DI5 
    "Mvt CIEL2",        // DI6 
    "Porte CIEL2",      // DI7 
    "Mvt Physique",     // DI8 
    "Portes Physique",  // DI9 
};

static const QString SENSOR_ZONES[6] = {
    "ciel1",    // DI4
    "ciel1",    // DI5
    "ciel2",    // DI6
    "ciel2",    // DI7
    "physique", // DI8
    "physique", // DI9
};
static const QString SENSOR_LABELS_BACK[6] = {
    QString::fromUtf8("D\xc3\xa9tecteur mouvement CIEL 1"),              // DI4
    QString::fromUtf8("Porte transition CIEL 1-2 / fen\xc3\xaatre"),     // DI5
    QString::fromUtf8("D\xc3\xa9tecteur mouvement CIEL 2"),              // DI6
    QString::fromUtf8("Porte CIEL 2 + fen\xc3\xaatre"),                  // DI7
    QString::fromUtf8("D\xc3\xa9tecteur mouvement Physique"),            // DI8
    QString::fromUtf8("Portes Physique + fen\xc3\xaatre + bureau"),      // DI9
};

// Capteurs bloquant l'armement; portes seulement 
static const bool SENSOR_BLOCKS_ARM[6] = {
    false,  
    true,   
    false,  
    true,   
    false,  
    true,   
};

// Sorties DO / PET-7067 (canal, base 0) 
static const quint16 DO_GACHE          = 0;
static const quint16 DO_FLASH_CIEL1    = 1;
static const quint16 DO_SIREN_CIEL1    = 2;
static const quint16 DO_FLASH_CIEL2    = 3;
static const quint16 DO_SIREN_CIEL2    = 4;
// DO5 non cablé
static const quint16 DO_SIREN_PHYSIQUE = 6; // DO6 — sirene Labo Physique
static const quint16 DO_FLASH_PHYSIQUE = 7; // DO7 — flash  Labo Physique

//  Durées (ne pas modifier sans decision du groupe) 
static const int GACHE_DURATION_MS  = 2000;    //  2 secondes (pulse gache)
static const int SIREN_DURATION_MS  = 2000;  //  3 minutes  (sirenes)
static const int SENSOR_POLL_MS     = 1000;    //  1 seconde  (polling capteurs)
static const int SCHEDULE_POLL_MS   = 60000;   // 60 secondes (planning adjudicator)


//  Constructeur / Destructeur

LaboMonitoringWidget::LaboMonitoringWidget(QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle("Labo Monitoring — Surveillance & Controle d'acces");
    resize(900, 740);

    
    m_surveillance = new SurveillanceController(this);

    // ── 2. API HTTP pour l'Etudiant 1 (arm/disarm depuis le site web) 
    m_httpApi = new SurveillanceHttpApi(m_surveillance, this);

    // ── 3. Pattern Observer : AlertNotifier + SMS + Mail
    m_alertNotifier = new AlertNotifier(this);
    m_alertNotifier->addNotificationStrategy(new SMSAlertNotifierStrategy (m_alertNotifier));
    m_alertNotifier->addNotificationStrategy(new MailAlertNotifierStrategy(m_alertNotifier));

    // ── 4. Modbus PET-7050 (lecture capteurs DI4..DI9)
    m_inputModbus = new QModbusTcpClient(MODBUS_IN_IP, 502, 1, this);
    m_pollTimer   = new QTimer(this);
    m_pollTimer->setInterval(SENSOR_POLL_MS);

    // ── 5. Modbus PET-7067 (gache + flash + sirenes)
    m_outputModbus = new QModbusTcpClient(MODBUS_OUT_IP, 502, 1, this);
    m_gacheTimer   = new QTimer(this);
    m_gacheTimer->setSingleShot(true);

    m_sirenTimer = new QTimer(this);
    m_sirenTimer->setSingleShot(true);
    m_sirenTimer->setInterval(SIREN_DURATION_MS);

    // ── 6. Serveur RFID + reseau 
    m_rfidServer    = new QTcpServer(this);
    m_nam           = new QNetworkAccessManager(this);
    m_scheduleTimer = new QTimer(this);
    m_scheduleTimer->setInterval(SCHEDULE_POLL_MS);

    // Connexions signaux/slots 

    // SurveillanceController (arm/disarm depuis HTTP API E1)
    connect(m_surveillance, &SurveillanceController::armedChanged,
            this, &LaboMonitoringWidget::onArmedChanged);
    connect(m_surveillance, &SurveillanceController::logMessage,
            this, &LaboMonitoringWidget::onSurvLog);
    connect(m_httpApi, &SurveillanceHttpApi::logMessage,
            this, &LaboMonitoringWidget::onApiLog);

    // Modbus PET-7050 (capteurs)
    connect(m_inputModbus, &QTcpSocket::connected,
            this, &LaboMonitoringWidget::onInputModbusConnected);
    connect(m_inputModbus, &QTcpSocket::disconnected,
            this, &LaboMonitoringWidget::onInputModbusDisconnected);
    connect(m_inputModbus, &QModbusTcpClient::onReadMultipleInputsStatusSentence,
            this, &LaboMonitoringWidget::onSensorsReceived);
    connect(m_pollTimer, &QTimer::timeout,
            this, &LaboMonitoringWidget::onPollTimer);

    // Modbus PET-7067 (sorties)
    connect(m_outputModbus, &QTcpSocket::connected,
            this, &LaboMonitoringWidget::onOutputModbusConnected);
    connect(m_outputModbus, &QTcpSocket::disconnected,
            this, &LaboMonitoringWidget::onOutputModbusDisconnected);

    // Timers
    connect(m_gacheTimer, &QTimer::timeout, this, &LaboMonitoringWidget::onGacheTimer);
    connect(m_sirenTimer, &QTimer::timeout, this, &LaboMonitoringWidget::onSirenTimer);

    // Serveur RFID
    connect(m_rfidServer, &QTcpServer::newConnection,
            this, &LaboMonitoringWidget::onRfidNewConnection);

    // Planning
    connect(m_scheduleTimer, &QTimer::timeout,
            this, &LaboMonitoringWidget::onScheduleTimer);

    // ── IHM 
    buildUi();
    updateButtons();

    log("=== Labo Monitoring — Etudiant 2 (BTS Cybersecu / IR) ===");
}

LaboMonitoringWidget::~LaboMonitoringWidget()
{
    // Securitée; tous eteindre avant destruction
    if (m_outputModbus &&
        m_outputModbus->state() == QAbstractSocket::ConnectedState) {
        stopAllOutputs();
    }
}


//  Construction de l'IHM

void LaboMonitoringWidget::buildUi()
{
    QVBoxLayout *main = new QVBoxLayout(this);
    main->setSpacing(5);

    // ── Demarrage / Arret
    {
        QHBoxLayout *row = new QHBoxLayout();
        m_btnStart = new QPushButton("Demarrer le service");
        m_btnStop  = new QPushButton("Arreter");
        m_btnStart->setMinimumHeight(36);
        m_btnStop ->setMinimumHeight(36);
        m_btnStart->setStyleSheet("font-weight:bold; font-size:13px;");
        row->addWidget(m_btnStart);
        row->addWidget(m_btnStop);
        row->addStretch();
        main->addLayout(row);
        connect(m_btnStart, &QPushButton::clicked, this, &LaboMonitoringWidget::onBtnStartClicked);
        connect(m_btnStop,  &QPushButton::clicked, this, &LaboMonitoringWidget::onBtnStopClicked);
    }

  
    m_lblModbusIn    = new QLabel("---", this);
    m_lblModbusOut   = new QLabel("---", this);
    m_lblSrvRfid     = new QLabel("---", this);
    m_lblSrvAlarmApi = new QLabel("---", this);
    m_lblModbusIn->hide();
    m_lblModbusOut->hide();
    m_lblSrvRfid->hide();
    m_lblSrvAlarmApi->hide();

    // Capteurs (toutes salles) 
    {
        QGroupBox   *grp  = new QGroupBox("Capteurs — toutes salles (NC : vert=ferme, rouge=ouvert)");
        QGridLayout *grid = new QGridLayout(grp);

        for (int i = 0; i < NUM_SENSORS; ++i) {
            m_lblSensors[i] = new QLabel("---");
            m_lblSensors[i]->setStyleSheet("color:gray;");
            const QString addr = QString("DI%1 %2 :").arg(DI_START + i).arg(SENSOR_NAMES[i]);
            grid->addWidget(new QLabel(addr),    i / 3, (i % 3) * 2);
            grid->addWidget(m_lblSensors[i],     i / 3, (i % 3) * 2 + 1);
        }
        main->addWidget(grp);
    }

    // ── Surveillance (alarme) 
    {
        QGroupBox   *grp  = new QGroupBox("Surveillance");
        QVBoxLayout *vbox = new QVBoxLayout(grp);

        // Etat armement + alarme
        QHBoxLayout *stRow = new QHBoxLayout();
        m_lblArmed       = new QLabel("DESARMEE");
        m_lblAlarmStatus = new QLabel("---");
        m_lblArmed->setStyleSheet("font-weight:bold; color:gray; font-size:13px;");
        m_lblAlarmStatus->setStyleSheet("color:gray;");

        stRow->addWidget(new QLabel("Etat :"));
        stRow->addWidget(m_lblArmed);
        stRow->addSpacing(20);
        stRow->addWidget(new QLabel("Alarme :"));
        stRow->addWidget(m_lblAlarmStatus);
        stRow->addStretch();
        vbox->addLayout(stRow);

        // Boutons arm/disarm + reset flash
        QHBoxLayout *armRow = new QHBoxLayout();
        m_btnArm        = new QPushButton("ARMER");
        m_btnDisarm     = new QPushButton("DESARMER");
        m_btnResetFlash = new QPushButton("Reset Flash");
        m_btnArm->setMinimumHeight(32);
        m_btnDisarm->setMinimumHeight(32);
        m_btnResetFlash->setMinimumHeight(32);
        m_btnArm->setStyleSheet("font-weight:bold;");
        m_btnResetFlash->setStyleSheet("background:#e65100; color:white; font-weight:bold;");
        armRow->addWidget(m_btnArm);
        armRow->addWidget(m_btnDisarm);
        armRow->addSpacing(20);
        armRow->addWidget(m_btnResetFlash);
        armRow->addStretch();
        vbox->addLayout(armRow);
        connect(m_btnArm,        &QPushButton::clicked, this, &LaboMonitoringWidget::onBtnArmClicked);
        connect(m_btnDisarm,     &QPushButton::clicked, this, &LaboMonitoringWidget::onBtnDisarmClicked);
        connect(m_btnResetFlash, &QPushButton::clicked, this, &LaboMonitoringWidget::onBtnResetFlashClicked);

        // Planning
        m_chkAutoSchedule = new QCheckBox(
            "Armement automatique selon le planning définit sur le site internet");
        m_chkAutoSchedule->setChecked(true);
        vbox->addWidget(m_chkAutoSchedule);

        m_lblSchedule = new QLabel("Planning : non disponible (service arrete)");
        m_lblSchedule->setStyleSheet("color:gray; font-style:italic;");
        vbox->addWidget(m_lblSchedule);

        main->addWidget(grp);
    }

    // ── Controle d'acces (RFID + gache) 
    {
        QGroupBox   *grp  = new QGroupBox("Controle d'acces (badge -> gache DO0 pulse 2 s)");
        QFormLayout *form = new QFormLayout(grp);

        m_lblGache     = new QLabel("VERROUILLEE");
        m_lblLastBadge = new QLabel("---");
        m_lblGache->setStyleSheet("font-weight:bold; color:#2e7d32;");

        QHBoxLayout *gRow = new QHBoxLayout();
        gRow->addWidget(m_lblGache);
        gRow->addSpacing(10);
        m_btnCloseGache = new QPushButton("Forcer fermeture");
        m_btnCloseGache->setEnabled(false);
        gRow->addWidget(m_btnCloseGache);
        gRow->addStretch();

        form->addRow("Gache :",         gRow);
        form->addRow("Dernier badge :", m_lblLastBadge);

        main->addWidget(grp);
        connect(m_btnCloseGache, &QPushButton::clicked,
                this, [this]() { closeGache(); });
    }

    // ── Journal 
    main->addWidget(new QLabel("Journal :"));
    m_textLog = new QTextEdit();
    m_textLog->setReadOnly(true);
    m_textLog->setStyleSheet("font-family: Consolas, monospace; font-size: 11px;");
    main->addWidget(m_textLog, 1);
}


//  Demarrage / Arret

void LaboMonitoringWidget::onBtnStartClicked()
{
    if (m_running) return;

    // 1. SurveillanceController
    m_surveillance->start();

    // 2. API HTTP E1 (arm/disarm depuis le site web)
    if (m_httpApi->start(ALARM_API_PORT)) {
        setStatus(m_lblSrvAlarmApi, QString("active (port %1)").arg(ALARM_API_PORT), "#2e7d32");
    } else {
        setStatus(m_lblSrvAlarmApi, "erreur (port occupe ?)", "#b71c1c");
    }

    // 3. Modbus PET-7050 (lecture capteurs)
    setStatus(m_lblModbusIn, "connexion...", "#e65100");
    m_inputModbus->connectToHost();

    // 4. Modbus PET-7067 (gache + flash + sirenes)
    setStatus(m_lblModbusOut, "connexion...", "#e65100");
    m_outputModbus->connectToHost();

    // 5. Serveur RFID (Arduino POST /rfid/scan)
    if (m_rfidServer->listen(QHostAddress::Any, RFID_PORT)) {
        setStatus(m_lblSrvRfid, QString("actif (port %1)").arg(RFID_PORT), "#2e7d32");
    } else {
        setStatus(m_lblSrvRfid, "erreur : port occupe", "#b71c1c");
    }

    // 6. Planning adjudicator
    m_scheduleTimer->start();
    fetchAndApplySchedule();

    m_running = true;
    updateButtons();
    log("--- Service Labo Monitoring demarre ---");
}

void LaboMonitoringWidget::onBtnStopClicked()
{
    if (!m_running) return;

    // Arret propre des timers
    m_sirenTimer->stop();
    m_gacheTimer->stop();
    m_pollTimer->stop();
    m_scheduleTimer->stop();

    // Extinction de toutes les sorties
    if (m_outputModbus->state() == QAbstractSocket::ConnectedState)
        stopAllOutputs();

    // Deconnexions reseau
    m_surveillance->stop();
    m_httpApi->stop();
    m_rfidServer->close();

    if (m_inputModbus->state()  != QAbstractSocket::UnconnectedState)
        m_inputModbus->close();
    if (m_outputModbus->state() != QAbstractSocket::UnconnectedState)
        m_outputModbus->close();

    for (QTcpSocket *sock : m_rfidBuffers.keys())
        sock->disconnectFromHost();
    m_rfidBuffers.clear();

    // Reset etats
    m_alarmActive      = false;
    m_flashActive      = false;
    m_intrusionAlerted = false;
    m_gacheOpen        = false;
    for (int i = 0; i < NUM_SENSORS; ++i) m_sensorOpen[i] = false;

    // Remise a zero UI
    setStatus(m_lblModbusIn,    "non connecte", "gray");
    setStatus(m_lblModbusOut,   "non connecte", "gray");
    setStatus(m_lblSrvRfid,     "arrete",       "gray");
    setStatus(m_lblSrvAlarmApi, "arretee",      "gray");
    for (int i = 0; i < NUM_SENSORS; ++i) {
        m_lblSensors[i]->setText("---");
        m_lblSensors[i]->setStyleSheet("color:gray;");
    }
    m_lblAlarmStatus->setText("---");
    m_lblAlarmStatus->setStyleSheet("color:gray;");
    m_lblSchedule->setText("Planning : non disponible (service arrete)");
    m_lblSchedule->setStyleSheet("color:gray; font-style:italic;");
    m_scheduleValid = false;

    m_running = false;
    updateButtons();
    log("--- Service arrete ---");
}


//  Arm / Disarm

void LaboMonitoringWidget::onBtnArmClicked()
{
    // Refuser l'armement uniquement si une PORTE est ouverte
    // (les detecteurs de mouvement sont ignores pour l'armement)
    for (int i = 0; i < NUM_SENSORS; ++i) {
        if (SENSOR_BLOCKS_ARM[i] && m_sensorOpen[i]) {
            log(QString("ARMEMENT REFUSE : %1 est ouverte — ferme la porte d'abord !")
                    .arg(SENSOR_NAMES[i]));
            return;
        }
    }
    // Arme via SurveillanceController (met a jour l'etat retourne par l'API E1 aussi)
    m_surveillance->arm();
    // -> declenche onArmedChanged(true) qui remet m_intrusionAlerted = false
}

void LaboMonitoringWidget::onBtnDisarmClicked()
{
    m_surveillance->disarm();
    // -> declenche onArmedChanged(false) qui stoppe sirenes si actives
}

void LaboMonitoringWidget::onBtnResetFlashClicked()
{
    if (!m_flashActive) return;

    if (m_outputModbus->state() == QAbstractSocket::ConnectedState) {
        m_outputModbus->forceMultipleCoilsFC15(DO_FLASH_CIEL1,    {false, false, false, false});
        m_outputModbus->forceMultipleCoilsFC15(DO_SIREN_PHYSIQUE, {false, false});
    }
    m_flashActive = false;
    updateButtons(); // desactive le bouton Reset Flash
    m_lblAlarmStatus->setText("OK — flash eteints");
    m_lblAlarmStatus->setStyleSheet("color:#2e7d32;");
}


//  Polling capteurs (PET-7050, DI4..DI9)

void LaboMonitoringWidget::onPollTimer()
{
    if (m_inputModbus->state() == QAbstractSocket::ConnectedState)
        m_inputModbus->readMultipleInputsStatusFC2(DI_START, NUM_SENSORS);
}

void LaboMonitoringWidget::onSensorsReceived(quint16 /*startAddress*/, QVector<bool> values)
{
    if (values.size() < NUM_SENSORS) return;

    for (int i = 0; i < NUM_SENSORS; ++i) {
        // Tous les capteurs sont NC -> inversion : true en physique = ferme -> on inverse
        const bool isOpen = !values[i];

        if (isOpen != m_sensorOpen[i]) {
            m_sensorOpen[i] = isOpen;

            // Mise a jour label
            setStatus(m_lblSensors[i],
                      isOpen ? "OUVERT" : "ferme",
                      isOpen ? "#b71c1c" : "#2e7d32");

            // Log capteurs uniquement quand une alarme est en cours (declenchee et pas encore desarmee)
            // m_intrusionAlerted = vrai depuis le declenchement, isArmed() = faux apres desarme
            if (m_intrusionAlerted && m_surveillance->isArmed()) {
                log(QString("%1 (DI%2) : %3")
                        .arg(SENSOR_NAMES[i])
                        .arg(DI_START + i)
                        .arg(isOpen ? "OUVERT" : "ferme"));
            }

            // Declenchement alarme si : sensor ouvert + systeme arme + alarme non active
            // ET pas deja signale (anti-boucle : empêche le re-déclenchement apres fin sirènes)
            const bool wasAlarmActive = m_alarmActive;
            if (isOpen && m_surveillance->isArmed() && !m_alarmActive && !m_intrusionAlerted) {
                activateAlarm(SENSOR_NAMES[i]);
            }

            // Enregistrer en BDD (backend E1) uniquement si le systeme est ARME
            if (m_surveillance->isArmed()) {
                // triggered = vrai si ce changement a provoque le declenchement de l'alarme
                const bool justTriggered = !wasAlarmActive && m_alarmActive;
                sendDiEventToBackend(i, isOpen, justTriggered);
            }
        }
    }

    // m_intrusionAlerted ne se remet a false QUE via onArmedChanged(true)
    // (desarmement + rearmement delibere) — pas de reset automatique quand les
    // capteurs se ferment, pour eviter le re-declenchement dans le meme episode.
}

void LaboMonitoringWidget::onInputModbusConnected()
{
    setStatus(m_lblModbusIn, QString("connecte (%1)").arg(MODBUS_IN_IP), "#2e7d32");
    m_pollTimer->start();
}

void LaboMonitoringWidget::onInputModbusDisconnected()
{
    setStatus(m_lblModbusIn, "deconnecte", "#b71c1c");
    m_pollTimer->stop();
}

void LaboMonitoringWidget::onOutputModbusConnected()
{
    setStatus(m_lblModbusOut, QString("connecte (%1)").arg(MODBUS_OUT_IP), "#2e7d32");
    // Securite au demarrage : tout eteindre sauf la gache (deja verrouille)
    m_outputModbus->forceSingleCoilFC5(DO_GACHE,          false);
    m_outputModbus->forceSingleCoilFC5(DO_FLASH_CIEL1,    false);
    m_outputModbus->forceSingleCoilFC5(DO_SIREN_CIEL1,    false);
    m_outputModbus->forceSingleCoilFC5(DO_FLASH_CIEL2,    false);
    m_outputModbus->forceSingleCoilFC5(DO_SIREN_CIEL2,    false);
    m_outputModbus->forceSingleCoilFC5(DO_SIREN_PHYSIQUE, false);
    m_outputModbus->forceSingleCoilFC5(DO_FLASH_PHYSIQUE, false);
}

void LaboMonitoringWidget::onOutputModbusDisconnected()
{
    setStatus(m_lblModbusOut, "deconnecte", "#b71c1c");
    m_gacheOpen    = false;
    m_alarmActive  = false;
}


// ─────────────────────────────────────────────────────────────────────────────
//  Logique d'alarme
//  - Intrusion : sirenes ON + flash ON + SMS/mail (anti-spam)
//  - Apres 3 min : sirenes OFF — flash restent ON
//  - Reset flash : bouton manuel uniquement
// ─────────────────────────────────────────────────────────────────────────────

void LaboMonitoringWidget::activateAlarm(const QString &source)
{
    if (m_outputModbus->state() != QAbstractSocket::ConnectedState) {
        log("ATTENTION : PET-7067 non connecte — impossible d'activer l'alarme physique !");
    } else {
        // FC15 : active TOUS les relais sirenes + flash en deux commandes atomiques
        // DO1=flash CIEL1, DO2=sirene CIEL1, DO3=flash CIEL2, DO4=sirene CIEL2
        m_outputModbus->forceMultipleCoilsFC15(DO_FLASH_CIEL1,    {true, true, true, true});
        // DO6=sirene Physique, DO7=flash Physique
        m_outputModbus->forceMultipleCoilsFC15(DO_SIREN_PHYSIQUE, {true, true});
    }

    m_alarmActive = true;
    m_flashActive = true;
    m_sirenTimer->start(); // 3 min -> onSirenTimer
    updateButtons(); // active le bouton Reset Flash

    // Mise a jour IHM
    m_lblArmed->setText("!!! ALARME !!!");
    m_lblArmed->setStyleSheet(
        "font-weight:bold; color:white; font-size:13px; "
        "background:#b71c1c; padding:2px 6px;");
    m_lblAlarmStatus->setText(QString("Sirenes actives (%1 min) — source : %2")
                                  .arg(SIREN_DURATION_MS / 60000).arg(source));
    m_lblAlarmStatus->setStyleSheet("font-weight:bold; color:#b71c1c;");

    // Log intrusion en ROUGE pour le distinguer dans le journal
    appendLog(QString("!!! ALARME DECLENCHEE — %1 — sirenes %2 min + flash permanents !!!")
                  .arg(source).arg(SIREN_DURATION_MS / 60000),
              QColor("#b71c1c"));

    // SMS + mail (anti-spam : une seule alerte par episode)
    if (!m_intrusionAlerted) {
        m_intrusionAlerted = true;
        const QString msg = QString("INTRUSION detectee au labo — source : %1 — %2")
                .arg(source, QDateTime::currentDateTime().toString("dd/MM hh:mm:ss"));
        m_alertNotifier->onAlert(msg);
        appendLog("SMS + mail envoyes (AlertNotifier).", QColor("#b71c1c"));
    }
}

void LaboMonitoringWidget::onSirenTimer()
{
    // 3 minutes ecoulees : sirenes OFF, flash restent ON
    stopSirens();
}

void LaboMonitoringWidget::stopSirens()
{
    m_sirenTimer->stop();
    if (!m_alarmActive) return;

    if (m_outputModbus->state() == QAbstractSocket::ConnectedState) {
        // DO1=flash(keep ON), DO2=sirene(OFF), DO3=flash(keep ON), DO4=sirene(OFF)
        m_outputModbus->forceMultipleCoilsFC15(DO_FLASH_CIEL1,    {true, false, true, false});
        // DO6=sirene(OFF), DO7=flash(keep ON)
        m_outputModbus->forceMultipleCoilsFC15(DO_SIREN_PHYSIQUE, {false, true});
    }
    m_alarmActive = false;
    // m_flashActive reste true — flash permanents jusqu'au reset manuel

    // Retour label alarme -> ARMEE (sans declencher onArmedChanged)
    if (m_surveillance->isArmed()) {
        m_lblArmed->setText("ARMEE");
        m_lblArmed->setStyleSheet("font-weight:bold; color:#b71c1c; font-size:13px;");
    }
    m_lblAlarmStatus->setText("Sirenes arretees — FLASH ACTIFS — appuyer sur Reset Flash");
    m_lblAlarmStatus->setStyleSheet("font-weight:bold; color:#e65100;");
}

void LaboMonitoringWidget::stopAllOutputs()
{
    m_sirenTimer->stop();
    m_gacheTimer->stop();

    m_outputModbus->forceSingleCoilFC5(DO_GACHE, false);           // gache seule (DO0)
    m_outputModbus->forceMultipleCoilsFC15(DO_FLASH_CIEL1,    {false, false, false, false}); // DO1-DO4
    m_outputModbus->forceMultipleCoilsFC15(DO_SIREN_PHYSIQUE, {false, false});               // DO6-DO7

    m_alarmActive = false;
    m_flashActive = false;
    m_gacheOpen   = false;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Envoi d'un evenement capteur au backend E1 (POST /api/di/event)
//  Appele uniquement quand le systeme est ARME — la BDD ne garde que les
//  evenements qui ont de la valeur pour l'historique de surveillance.
// ─────────────────────────────────────────────────────────────────────────────

void LaboMonitoringWidget::sendDiEventToBackend(int idx, bool isOpen, bool triggered)
{
    QJsonObject body;
    body["channel"]   = static_cast<int>(DI_START + idx);
    body["zone"]      = SENSOR_ZONES[idx];
    body["label"]     = SENSOR_LABELS_BACK[idx];
    body["value"]     = isOpen ? 1 : 0;
    body["triggered"] = triggered ? 1 : 0;

    QNetworkRequest req(QUrl(ADJUDICATOR_URL + "/api/di/event"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QNetworkReply *reply = m_nam->post(req,
        QJsonDocument(body).toJson(QJsonDocument::Compact));
    // fire-and-forget : on supprime la reply quand elle arrive, sans bloquer
    connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
}

void LaboMonitoringWidget::onArmedChanged(bool armed)
{
    if (armed) {
        m_intrusionAlerted = false; // reset anti-spam a chaque nouvel armement
        if (!m_alarmActive && !m_flashActive) {
            m_lblArmed->setText("ARMEE");
            m_lblArmed->setStyleSheet("font-weight:bold; color:#b71c1c; font-size:13px;");
            m_lblAlarmStatus->setText("OK — en surveillance");
            m_lblAlarmStatus->setStyleSheet("color:#2e7d32;");
        }
    } else {
        // Desarmement : couper sirenes + flash si actifs (inline, sans appeler stopSirens
        // pour eviter la boucle stopSirens -> onArmedChanged)
        if (m_alarmActive || m_flashActive) {
            m_sirenTimer->stop();
            if (m_outputModbus->state() == QAbstractSocket::ConnectedState) {
                // Tout eteindre en deux commandes FC15 atomiques
                m_outputModbus->forceMultipleCoilsFC15(DO_FLASH_CIEL1,    {false, false, false, false});
                m_outputModbus->forceMultipleCoilsFC15(DO_SIREN_PHYSIQUE, {false, false});
            }
            m_alarmActive = false;
            m_flashActive = false;
            updateButtons();
        }
        m_lblArmed->setText("DESARMEE");
        m_lblArmed->setStyleSheet("font-weight:bold; color:gray; font-size:13px;");
        m_lblAlarmStatus->setText("---");
        m_lblAlarmStatus->setStyleSheet("color:gray;");
    }
}


// ─────────────────────────────────────────────────────────────────────────────
//  Gache — pulse DO0 (2 s)
// ─────────────────────────────────────────────────────────────────────────────

void LaboMonitoringWidget::openGache()
{
    if (m_outputModbus->state() != QAbstractSocket::ConnectedState) {
        log("ATTENTION : PET-7067 non connecte — gache non activee !");
        return;
    }
    m_outputModbus->forceSingleCoilFC5(DO_GACHE, true);
    m_gacheOpen = true;
    m_gacheTimer->start(GACHE_DURATION_MS);

    m_lblGache->setText(QString("DEVERROUILLEE (%1 s)").arg(GACHE_DURATION_MS / 1000));
    m_lblGache->setStyleSheet("font-weight:bold; color:#b71c1c;");
    m_btnCloseGache->setEnabled(true);
}

void LaboMonitoringWidget::closeGache()
{
    m_gacheTimer->stop();
    if (!m_gacheOpen) return;

    if (m_outputModbus->state() == QAbstractSocket::ConnectedState)
        m_outputModbus->forceSingleCoilFC5(DO_GACHE, false);

    m_gacheOpen = false;
    m_lblGache->setText("VERROUILLEE");
    m_lblGache->setStyleSheet("font-weight:bold; color:#2e7d32;");
    m_btnCloseGache->setEnabled(false);
}

void LaboMonitoringWidget::onGacheTimer() { closeGache(); }


// ─────────────────────────────────────────────────────────────────────────────
//  Serveur RFID
// ─────────────────────────────────────────────────────────────────────────────

void LaboMonitoringWidget::onRfidNewConnection()
{
    while (m_rfidServer && m_rfidServer->hasPendingConnections()) {
        QTcpSocket *socket = m_rfidServer->nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead,
                this, &LaboMonitoringWidget::onRfidClientReadyRead);
        connect(socket, &QTcpSocket::disconnected,
                this, &LaboMonitoringWidget::onRfidClientDisconnected);
    }
}

void LaboMonitoringWidget::onRfidClientReadyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket) return;

    m_rfidBuffers[socket] += socket->readAll();
    QByteArray &buf = m_rfidBuffers[socket];

    int sep = buf.indexOf("\r\n\r\n");
    if (sep < 0) sep = buf.indexOf("\n\n");
    if (sep < 0) return;

    const int        bodyOffset = sep + (buf.mid(sep, 4) == "\r\n\r\n" ? 4 : 2);
    const QByteArray headers    = buf.left(sep);

    int contentLength = 0;
    for (const QByteArray &line : headers.split('\n')) {
        const QByteArray lower = line.trimmed().toLower();
        if (lower.startsWith("content-length:")) {
            contentLength = lower.mid(15).trimmed().toInt();
            break;
        }
    }
    if (contentLength > 0 && buf.size() < bodyOffset + contentLength) return;

    const QByteArray body = (contentLength > 0)
        ? buf.mid(bodyOffset, contentLength)
        : buf.mid(bodyOffset);
    // Capturer l'IP du lecteur AVANT disconnectFromHost (socket detruit apres)
    const QString readerIp = socket->peerAddress().toString();
    const QString readerName =
        readerIp.contains(READER_IP_1) ? QString("Lecteur 1 (%1)").arg(READER_IP_1) :
        readerIp.contains(READER_IP_2) ? QString("Lecteur 2 (%1)").arg(READER_IP_2) :
        QString("Lecteur inconnu (%1)").arg(readerIp);

    m_rfidBuffers.remove(socket);

    sendHttpResponse(socket, 200, R"({"ok":true})");
    socket->disconnectFromHost();

    if (body.isEmpty()) return;

    const QJsonObject json = QJsonDocument::fromJson(body).object();
    if (json.isEmpty()) return;

    QString uid = json["card_id"].toString();
    if (uid.isEmpty()) uid = json["uid"].toString();
    uid = uid.trimmed().toUpper();

    if (!uid.isEmpty()) handleRfidScan(uid, readerName);
}

void LaboMonitoringWidget::onRfidClientDisconnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (socket) { m_rfidBuffers.remove(socket); socket->deleteLater(); }
}

void LaboMonitoringWidget::sendHttpResponse(QTcpSocket *socket, int code,
                                            const QByteArray &body)
{
    const QString status = (code == 200) ? "200 OK" : "400 Bad Request";
    socket->write(
        QString("HTTP/1.1 %1\r\nContent-Type: application/json\r\n"
                "Content-Length: %2\r\nConnection: close\r\n\r\n")
            .arg(status).arg(body.size()).toUtf8()
        + body);
    socket->flush();
}

void LaboMonitoringWidget::handleRfidScan(const QString &uid, const QString &readerName)
{
    m_lblLastBadge->setText(uid + "  (verification...)");
    m_lblLastBadge->setStyleSheet("color:#555;");

    QNetworkReply *reply = m_nam->get(
        QNetworkRequest(QUrl(ADJUDICATOR_URL + "/api/rfid/check/" + uid)));
    reply->setProperty("uid", uid);
    reply->setProperty("readerName", readerName);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply]() { onAdjudicatorReply(reply); });
}

void LaboMonitoringWidget::onAdjudicatorReply(QNetworkReply *reply)
{
    reply->deleteLater();
    const QString uid        = reply->property("uid").toString();
    const QString readerName = reply->property("readerName").toString();

    if (reply->error() != QNetworkReply::NoError) {
        logBadge(QString("[%1] (%2) ERREUR adjudicator : %3 -> acces refuse.")
                .arg(uid, readerName, reply->errorString()));
        m_lblLastBadge->setText(uid + "  — ERREUR RESEAU");
        m_lblLastBadge->setStyleSheet("color:#b71c1c; font-weight:bold;");
        return;
    }

    const QJsonObject obj        = QJsonDocument::fromJson(reply->readAll()).object();
    const bool        authorized = obj["authorized"].toBool(false);
    const QString     owner      = obj["owner"].toString("inconnu");
    const QString     message    = obj.value("message").toString();

    if (authorized) {
        logBadge(QString("[%1] (%2) ACCES AUTORISE — %3").arg(uid, readerName, owner));
        m_lblLastBadge->setText(QString("%1  —  %2  —  AUTORISE  (%3)").arg(uid, owner, readerName));
        m_lblLastBadge->setStyleSheet("color:#2e7d32; font-weight:bold;");
        openGache();

        // ── Timer de presence ───────────────────────────────────────────────
        // Lecteur EXTERIEUR (.200) : la personne ENTRE → on enregistre l'heure
        // Lecteur INTERIEUR (.201) : la personne SORT  → on calcule la duree
        if (readerName.contains(READER_IP_1)) {
            m_entryTimes[uid] = QDateTime::currentDateTime();
        } else if (readerName.contains(READER_IP_2)) {
            if (m_entryTimes.contains(uid)) {
                const QDateTime entree = m_entryTimes.take(uid); // retire de la map
                const qint64 secsTotal = entree.secsTo(QDateTime::currentDateTime());
                const qint64 h    = secsTotal / 3600;
                const qint64 mins = (secsTotal % 3600) / 60;
                const qint64 secs = secsTotal % 60;
                const QString duree = (h > 0)
                    ? QString("%1 h %2 min %3 s").arg(h).arg(mins).arg(secs)
                    : QString("%1 min %2 s").arg(mins).arg(secs);
                logBadge(QString("[%1] (%2) SORTIE — duree de presence : %3")
                             .arg(uid, owner, duree));
                m_lblLastBadge->setText(
                    QString("%1  —  %2  —  SORTIE  (%3)").arg(uid, owner, duree));
            }
        }
        // ────────────────────────────────────────────────────────────────────
    } else {
        logBadge(QString("[%1] (%2) ACCES REFUSE — %3 — %4")
                .arg(uid, readerName, owner, message.isEmpty() ? "non autorise" : message));
        m_lblLastBadge->setText(QString("%1  —  %2  —  REFUSE  (%3)").arg(uid, owner, readerName));
        m_lblLastBadge->setStyleSheet("color:#b71c1c; font-weight:bold;");
    }
}


// ─────────────────────────────────────────────────────────────────────────────
//  Planning (armement automatique)
// ─────────────────────────────────────────────────────────────────────────────

void LaboMonitoringWidget::onScheduleTimer() { fetchAndApplySchedule(); }

void LaboMonitoringWidget::fetchAndApplySchedule()
{
    QNetworkReply *reply = m_nam->get(
        QNetworkRequest(QUrl(ADJUDICATOR_URL + "/api/schedule")));
    connect(reply, &QNetworkReply::finished, this,
            [this, reply]() { onScheduleReply(reply); });
}

void LaboMonitoringWidget::onScheduleReply(QNetworkReply *reply)
{
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        if (!m_scheduleValid)
            m_lblSchedule->setText("Planning : adjudicator inaccessible");
        return;
    }

    const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
    QString startStr = obj["start"].toString();
    QString endStr   = obj["end"].toString();

    if ((startStr.isEmpty() || endStr.isEmpty()) && obj.contains("plages")) {
        const QJsonArray plages = obj["plages"].toArray();
        if (!plages.isEmpty()) {
            const QJsonObject first = plages[0].toObject();
            startStr = first["debut"].toString();
            endStr   = first["fin"].toString();
        }
    }

    if (startStr.isEmpty() || endStr.isEmpty()) {
        m_lblSchedule->setText("Planning : format non reconnu");
        m_scheduleValid = false;
        return;
    }

    m_scheduleStart = QTime::fromString(startStr, "HH:mm");
    m_scheduleEnd   = QTime::fromString(endStr,   "HH:mm");
    m_scheduleValid = m_scheduleStart.isValid() && m_scheduleEnd.isValid();

    if (m_scheduleValid) {
        m_lblSchedule->setText(
            QString("Planning : %1 -> %2  (auto %3)")
                .arg(m_scheduleStart.toString("HH:mm"),
                     m_scheduleEnd.toString("HH:mm"),
                     m_chkAutoSchedule->isChecked() ? "ON" : "OFF"));
        m_lblSchedule->setStyleSheet("color:#2e7d32;");
        applyScheduleArming();
    }
}

void LaboMonitoringWidget::applyScheduleArming()
{
    if (!m_chkAutoSchedule->isChecked() || !m_scheduleValid) return;
    const bool inSchedule = (QTime::currentTime() >= m_scheduleStart &&
                             QTime::currentTime() <= m_scheduleEnd);
    if (inSchedule && !m_surveillance->isArmed())
        onBtnArmClicked(); // passe par la verification des capteurs
    else if (!inSchedule && m_surveillance->isArmed())
        m_surveillance->disarm();
}


// ─────────────────────────────────────────────────────────────────────────────
//  Logs (SurveillanceController / API E1)
// ─────────────────────────────────────────────────────────────────────────────

void LaboMonitoringWidget::onSurvLog(const QString &) { /* logs internes E1 supprimes */ }

void LaboMonitoringWidget::onApiLog(const QString &) { /* logs API E1 supprimes */ }


// ─────────────────────────────────────────────────────────────────────────────
//  Utilitaires
// ─────────────────────────────────────────────────────────────────────────────

void LaboMonitoringWidget::appendLog(const QString &msg, const QColor &color)
{
    // Filtrer le polling de statut E1 (trop frequent, ne rien afficher)
    if (msg.contains("GET /surveillance/status")) return;

    m_textLog->setTextColor(color);   // couleur de la ligne a ajouter
    m_textLog->append(QString("[%1] %2")
                          .arg(QDateTime::currentDateTime().toString("hh:mm:ss"), msg));

    // Limiter a 100 lignes : supprimer la plus ancienne si depassement
    QTextDocument *doc = m_textLog->document();
    if (doc->blockCount() > 100) {
        QTextCursor cur(doc);          // curseur au debut du document
        cur.movePosition(QTextCursor::Start);
        cur.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        cur.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
        cur.removeSelectedText();
    }
}

void LaboMonitoringWidget::log(const QString &msg)
{
    // Couleur par defaut = couleur de texte du theme
    appendLog(msg, m_textLog->palette().color(QPalette::Text));
}

void LaboMonitoringWidget::logBadge(const QString &msg)
{
    // Evenements de badge : couleur distincte (bleu) pour ressortir dans le journal
    appendLog(msg, QColor("#1565c0"));
}

void LaboMonitoringWidget::updateButtons()
{
    m_btnStart->setEnabled(!m_running);
    m_btnStop ->setEnabled(m_running);
    m_btnArm  ->setEnabled(m_running);
    m_btnDisarm->setEnabled(m_running);
    m_btnResetFlash->setEnabled(m_running && m_flashActive);
}

void LaboMonitoringWidget::setStatus(QLabel *lbl, const QString &text,
                                     const QString &color)
{
    lbl->setText(text);
    lbl->setStyleSheet(QString("color:%1;").arg(color));
}
