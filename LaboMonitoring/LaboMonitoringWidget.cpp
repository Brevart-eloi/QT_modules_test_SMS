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

// ─── Adresses reseau (cablage labo — ne pas modifier) ────────────────────────
static const QString MODBUS_OUT_IP   = "172.29.240.1"; // PET-7067 sorties
static const QString MODBUS_IN_IP    = "172.29.240.2"; // PET-7050 entrees
static const QString ADJUDICATOR_URL = "http://172.29.19.193";
static const quint16 RFID_PORT       = 8080;
static const quint16 ALARM_API_PORT  = 80;

// IPs des lecteurs RFID Arduino — identifies pour le journal
static const QString READER_IP_1 = "172.29.19.200"; // Lecteur 1 (entree principale)
static const QString READER_IP_2 = "172.29.19.201"; // Lecteur 2 (entree secondaire)

// ─── Capteurs DI / PET-7050 ───────────────────────────────────────────────────
//  Tous NC (Normalement Fermes) : contact ferme = DI HIGH = 1
//  isOpen = !value (inversion pour avoir vrai = ouvert)
//
//  Index : DI4=0 DI5=1 DI6=2 DI7=3 DI8=4 DI9=5
static const QString SENSOR_NAMES[6] = {
    "Mvt CIEL1",        // DI4 — detecteur mouvement CIEL1
    "Porte CIEL1",      // DI5 — porte CIEL1 (NC)
    "Mvt CIEL2",        // DI6 — detecteur mouvement CIEL2
    "Porte CIEL2",      // DI7 — porte CIEL2 (NC)
    "Mvt Physique",     // DI8 — detecteur mouvement salle physique
    "Portes Physique",  // DI9 — portes salle physique (NC)
};

// ─── Sorties DO / PET-7067 (canal, base 0) ────────────────────────────────────
static const quint16 DO_GACHE          = 0;
static const quint16 DO_FLASH_CIEL1    = 1;
static const quint16 DO_SIREN_CIEL1    = 2;
static const quint16 DO_FLASH_CIEL2    = 3;
static const quint16 DO_SIREN_CIEL2    = 4;
// DO5 non cablé
static const quint16 DO_SIREN_PHYSIQUE = 6;
static const quint16 DO_FLASH_PHYSIQUE = 7;

// ─── Durées (ne pas modifier sans decision du groupe) ─────────────────────────
static const int GACHE_DURATION_MS  = 2000;    //  2 secondes (pulse gache)
static const int SIREN_DURATION_MS  = 180000;  //  3 minutes  (sirenes)
static const int SENSOR_POLL_MS     = 1000;    //  1 seconde  (polling capteurs)
static const int SCHEDULE_POLL_MS   = 60000;   // 60 secondes (planning adjudicator)


// ─────────────────────────────────────────────────────────────────────────────
//  Constructeur / Destructeur
// ─────────────────────────────────────────────────────────────────────────────

LaboMonitoringWidget::LaboMonitoringWidget(QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle("Labo Monitoring — Surveillance & Controle d'acces (E2)");
    resize(900, 740);

    // ── 1. SurveillanceController ─────────────────────────────────────────────
    //  Configure SANS adresse IP Modbus :
    //    -> ne cree pas de connexion reseau elle-meme
    //    -> sert uniquement de machine a etats arm/disarm pour SurveillanceHttpApi
    //  Le widget gere directement les capteurs et les sorties.
    m_surveillance = new SurveillanceController(this);
    // Pas de setOutputModuleAddress / setInputModuleAddress => IPs vides

    // ── 2. API HTTP pour l'Etudiant 1 (arm/disarm depuis le site web) ─────────
    m_httpApi = new SurveillanceHttpApi(m_surveillance, this);

    // ── 3. Pattern Observer : AlertNotifier + SMS + Mail ─────────────────────
    m_alertNotifier = new AlertNotifier(this);
    m_alertNotifier->addNotificationStrategy(new SMSAlertNotifierStrategy (m_alertNotifier));
    m_alertNotifier->addNotificationStrategy(new MailAlertNotifierStrategy(m_alertNotifier));
    // Note : l'alerte est declenchee directement (onAlert) depuis activateAlarm()
    // -> pas besoin d'enregistrer comme listener de m_surveillance
    //    (dont les sorties ne sont pas utilisees ici)

    // ── 4. Modbus PET-7050 (lecture capteurs DI4..DI9) ────────────────────────
    m_inputModbus = new QModbusTcpClient(MODBUS_IN_IP, 502, 1, this);
    m_pollTimer   = new QTimer(this);
    m_pollTimer->setInterval(SENSOR_POLL_MS);

    // ── 5. Modbus PET-7067 (gache + flash + sirenes) ─────────────────────────
    m_outputModbus = new QModbusTcpClient(MODBUS_OUT_IP, 502, 1, this);
    m_gacheTimer   = new QTimer(this);
    m_gacheTimer->setSingleShot(true);

    m_sirenTimer = new QTimer(this);
    m_sirenTimer->setSingleShot(true);
    m_sirenTimer->setInterval(SIREN_DURATION_MS);

    // ── 6. Serveur RFID + reseau ──────────────────────────────────────────────
    m_rfidServer    = new QTcpServer(this);
    m_nam           = new QNetworkAccessManager(this);
    m_scheduleTimer = new QTimer(this);
    m_scheduleTimer->setInterval(SCHEDULE_POLL_MS);

    // ── Connexions signaux/slots ──────────────────────────────────────────────

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

    // ── IHM ──────────────────────────────────────────────────────────────────
    buildUi();
    updateButtons();

    log("=== Labo Monitoring — Etudiant 2 (BTS Cybersecu / IR) ===");
    log(QString("PET-7067 sorties : %1  |  PET-7050 entrees : %2")
            .arg(MODBUS_OUT_IP, MODBUS_IN_IP));
    log(QString("Adjudicator (E1) : %1  |  RFID : port %2  |  API E1 : port %3")
            .arg(ADJUDICATOR_URL).arg(RFID_PORT).arg(ALARM_API_PORT));
    log("Capteurs : DI4 Mvt1 | DI5 Porte1 | DI6 Mvt2 | DI7 Porte2 | DI8 MvtPhy | DI9 PortesPhy");
    log(QString("Alarme   : sirenes %1 s puis extinction — flash restent ON jusqu'au reset")
            .arg(SIREN_DURATION_MS / 1000));
    log("Appuyez sur 'Demarrer le service' pour activer la surveillance.");
}

LaboMonitoringWidget::~LaboMonitoringWidget()
{
    // Securite : tout eteindre avant destruction
    if (m_outputModbus &&
        m_outputModbus->state() == QAbstractSocket::ConnectedState) {
        stopAllOutputs();
    }
}


// ─────────────────────────────────────────────────────────────────────────────
//  Construction de l'IHM
// ─────────────────────────────────────────────────────────────────────────────

void LaboMonitoringWidget::buildUi()
{
    QVBoxLayout *main = new QVBoxLayout(this);
    main->setSpacing(5);

    // ── Demarrage / Arret ────────────────────────────────────────────────────
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

    // ── Connexions ────────────────────────────────────────────────────────────
    {
        QGroupBox   *grp  = new QGroupBox("Connexions");
        QGridLayout *grid = new QGridLayout(grp);
        grid->setColumnStretch(1, 1);
        grid->setColumnStretch(3, 1);

        m_lblModbusIn    = new QLabel("non connecte");
        m_lblModbusOut   = new QLabel("non connecte");
        m_lblSrvRfid     = new QLabel("arrete");
        m_lblSrvAlarmApi = new QLabel("arretee");

        grid->addWidget(new QLabel("PET-7050 (capteurs) :"),   0, 0);
        grid->addWidget(m_lblModbusIn,                          0, 1);
        grid->addWidget(new QLabel("PET-7067 (sorties) :"),     0, 2);
        grid->addWidget(m_lblModbusOut,                         0, 3);
        grid->addWidget(new QLabel("Serveur RFID (port 80) :"), 1, 0);
        grid->addWidget(m_lblSrvRfid,                           1, 1);
        grid->addWidget(new QLabel("API E1 (port 8080) :"),     1, 2);
        grid->addWidget(m_lblSrvAlarmApi,                       1, 3);
        main->addWidget(grp);
    }

    // ── Capteurs (toutes salles) ──────────────────────────────────────────────
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

    // ── Surveillance (alarme) ─────────────────────────────────────────────────
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
            "Armement automatique selon le planning (recupere depuis adjudicator)");
        m_chkAutoSchedule->setChecked(true);
        vbox->addWidget(m_chkAutoSchedule);

        m_lblSchedule = new QLabel("Planning : non disponible (service arrete)");
        m_lblSchedule->setStyleSheet("color:gray; font-style:italic;");
        vbox->addWidget(m_lblSchedule);

        main->addWidget(grp);
    }

    // ── Controle d'acces (RFID + gache) ──────────────────────────────────────
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

    // ── Journal ───────────────────────────────────────────────────────────────
    main->addWidget(new QLabel("Journal :"));
    m_textLog = new QTextEdit();
    m_textLog->setReadOnly(true);
    m_textLog->setStyleSheet("font-family: Consolas, monospace; font-size: 11px;");
    main->addWidget(m_textLog, 1);
}


// ─────────────────────────────────────────────────────────────────────────────
//  Demarrage / Arret
// ─────────────────────────────────────────────────────────────────────────────

void LaboMonitoringWidget::onBtnStartClicked()
{
    if (m_running) return;

    // 1. SurveillanceController (machine a etats pour API E1, pas de Modbus)
    m_surveillance->start();

    // 2. API HTTP E1 (arm/disarm depuis le site web)
    if (m_httpApi->start(ALARM_API_PORT)) {
        setStatus(m_lblSrvAlarmApi, QString("active (port %1)").arg(ALARM_API_PORT), "#2e7d32");
        log(QString("API HTTP E1 active — port %1.").arg(ALARM_API_PORT));
    } else {
        setStatus(m_lblSrvAlarmApi, "erreur (port occupe ?)", "#b71c1c");
        log(QString("Avertissement : API HTTP E1 indisponible (port %1).").arg(ALARM_API_PORT));
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
        log(QString("Serveur RFID actif — port %1.").arg(RFID_PORT));
    } else {
        setStatus(m_lblSrvRfid, "erreur : port occupe", "#b71c1c");
        log(QString("ERREUR serveur RFID : port %1 — %2")
                .arg(RFID_PORT).arg(m_rfidServer->errorString()));
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


// ─────────────────────────────────────────────────────────────────────────────
//  Arm / Disarm (manuel ou via planning — ou via HTTP API E1)
// ─────────────────────────────────────────────────────────────────────────────

void LaboMonitoringWidget::onBtnArmClicked()
{
    // Refuser l'armement si un capteur est ouvert
    for (int i = 0; i < NUM_SENSORS; ++i) {
        if (m_sensorOpen[i]) {
            log(QString("ARMEMENT REFUSE : %1 est ouvert — ferme toutes les ouvertures d'abord !")
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
        m_outputModbus->forceSingleCoilFC5(DO_FLASH_CIEL1,    false);
        m_outputModbus->forceSingleCoilFC5(DO_FLASH_CIEL2,    false);
        m_outputModbus->forceSingleCoilFC5(DO_FLASH_PHYSIQUE, false);
    }
    m_flashActive = false;
    updateButtons(); // desactive le bouton Reset Flash
    m_lblAlarmStatus->setText("OK — flash eteints");
    m_lblAlarmStatus->setStyleSheet("color:#2e7d32;");
    log("Flash eteints (reset manuel).");
}


// ─────────────────────────────────────────────────────────────────────────────
//  Polling capteurs (PET-7050, DI4..DI9)
// ─────────────────────────────────────────────────────────────────────────────

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

            log(QString("%1 (DI%2) : %3")
                    .arg(SENSOR_NAMES[i])
                    .arg(DI_START + i)
                    .arg(isOpen ? "OUVERT" : "ferme"));

            // Declenchement alarme si : sensor ouvert + systeme arme + pas deja actif
            if (isOpen && m_surveillance->isArmed() && !m_alarmActive) {
                activateAlarm(SENSOR_NAMES[i]);
            }
        }
    }

    // Reset anti-spam quand tout est ferme et alarme inactive
    if (!m_alarmActive && m_intrusionAlerted) {
        bool allClosed = true;
        for (int i = 0; i < NUM_SENSORS; ++i)
            if (m_sensorOpen[i]) { allClosed = false; break; }
        if (allClosed) {
            m_intrusionAlerted = false;
            log("Tous les capteurs fermes — systeme pret pour prochaine intrusion.");
        }
    }
}

void LaboMonitoringWidget::onInputModbusConnected()
{
    setStatus(m_lblModbusIn, QString("connecte (%1)").arg(MODBUS_IN_IP), "#2e7d32");
    m_pollTimer->start();
    log(QString("PET-7050 connecte (%1) — polling capteurs toutes les %2 s.")
            .arg(MODBUS_IN_IP).arg(SENSOR_POLL_MS / 1000));
}

void LaboMonitoringWidget::onInputModbusDisconnected()
{
    setStatus(m_lblModbusIn, "deconnecte", "#b71c1c");
    m_pollTimer->stop();
    log("PET-7050 deconnecte — polling capteurs arrete.");
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
    log(QString("PET-7067 connecte (%1) — toutes les sorties mises a OFF (securite).")
            .arg(MODBUS_OUT_IP));
}

void LaboMonitoringWidget::onOutputModbusDisconnected()
{
    setStatus(m_lblModbusOut, "deconnecte", "#b71c1c");
    m_gacheOpen    = false;
    m_alarmActive  = false;
    log("PET-7067 deconnecte — sorties hors tension (relais repos).");
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
        // Allumer TOUTES les sirenes
        m_outputModbus->forceSingleCoilFC5(DO_SIREN_CIEL1,    true);
        m_outputModbus->forceSingleCoilFC5(DO_SIREN_CIEL2,    true);
        m_outputModbus->forceSingleCoilFC5(DO_SIREN_PHYSIQUE, true);
        // Allumer TOUS les flash
        m_outputModbus->forceSingleCoilFC5(DO_FLASH_CIEL1,    true);
        m_outputModbus->forceSingleCoilFC5(DO_FLASH_CIEL2,    true);
        m_outputModbus->forceSingleCoilFC5(DO_FLASH_PHYSIQUE, true);
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

    log(QString("!!! ALARME DECLENCHEE — %1 — sirenes %2 min + flash permanents !!!")
            .arg(source).arg(SIREN_DURATION_MS / 60000));

    // SMS + mail (anti-spam : une seule alerte par episode)
    if (!m_intrusionAlerted) {
        m_intrusionAlerted = true;
        const QString msg = QString("INTRUSION detectee au labo — source : %1 — %2")
                .arg(source, QDateTime::currentDateTime().toString("dd/MM hh:mm:ss"));
        m_alertNotifier->onAlert(msg);
        log("SMS + mail envoyes (AlertNotifier).");
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
        m_outputModbus->forceSingleCoilFC5(DO_SIREN_CIEL1,    false);
        m_outputModbus->forceSingleCoilFC5(DO_SIREN_CIEL2,    false);
        m_outputModbus->forceSingleCoilFC5(DO_SIREN_PHYSIQUE, false);
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

    log("Sirenes arretees (3 min ecoules) — flash restent allumes.");
    log("Appuyez sur 'Reset Flash' pour eteindre les flash.");
}

void LaboMonitoringWidget::stopAllOutputs()
{
    m_sirenTimer->stop();
    m_gacheTimer->stop();

    m_outputModbus->forceSingleCoilFC5(DO_GACHE,          false);
    m_outputModbus->forceSingleCoilFC5(DO_FLASH_CIEL1,    false);
    m_outputModbus->forceSingleCoilFC5(DO_SIREN_CIEL1,    false);
    m_outputModbus->forceSingleCoilFC5(DO_FLASH_CIEL2,    false);
    m_outputModbus->forceSingleCoilFC5(DO_SIREN_CIEL2,    false);
    m_outputModbus->forceSingleCoilFC5(DO_SIREN_PHYSIQUE, false);
    m_outputModbus->forceSingleCoilFC5(DO_FLASH_PHYSIQUE, false);

    m_alarmActive = false;
    m_flashActive = false;
    m_gacheOpen   = false;
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
        log("Surveillance ARMEE.");
    } else {
        // Desarmement : couper sirenes si actives (inline, sans appeler stopSirens
        // pour eviter la boucle stopSirens -> onArmedChanged)
        if (m_alarmActive) {
            m_sirenTimer->stop();
            if (m_outputModbus->state() == QAbstractSocket::ConnectedState) {
                m_outputModbus->forceSingleCoilFC5(DO_SIREN_CIEL1,    false);
                m_outputModbus->forceSingleCoilFC5(DO_SIREN_CIEL2,    false);
                m_outputModbus->forceSingleCoilFC5(DO_SIREN_PHYSIQUE, false);
            }
            m_alarmActive = false;
        }
        // Flash s'eteignent aussi au desarmement
        if (m_flashActive) {
            if (m_outputModbus->state() == QAbstractSocket::ConnectedState) {
                m_outputModbus->forceSingleCoilFC5(DO_FLASH_CIEL1,    false);
                m_outputModbus->forceSingleCoilFC5(DO_FLASH_CIEL2,    false);
                m_outputModbus->forceSingleCoilFC5(DO_FLASH_PHYSIQUE, false);
            }
            m_flashActive = false;
            updateButtons();
        }
        m_lblArmed->setText("DESARMEE");
        m_lblArmed->setStyleSheet("font-weight:bold; color:gray; font-size:13px;");
        m_lblAlarmStatus->setText("---");
        m_lblAlarmStatus->setStyleSheet("color:gray;");
        log("Surveillance DESARMEE.");
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
    log(QString("Gache ouverte — DO%1 ON — fermeture dans %2 s.")
            .arg(DO_GACHE).arg(GACHE_DURATION_MS / 1000));
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
    log("Gache fermee.");
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

    if (body.isEmpty()) { log(QString("POST RFID recu de %1 — body vide.").arg(readerName)); return; }

    const QJsonObject json = QJsonDocument::fromJson(body).object();
    if (json.isEmpty()) {
        log(QString("POST RFID (%1) — JSON invalide : %2")
                .arg(readerName, QString::fromUtf8(body.left(80))));
        return;
    }

    QString uid = json["card_id"].toString();
    if (uid.isEmpty()) uid = json["uid"].toString();
    uid = uid.trimmed().toUpper();

    if (!uid.isEmpty()) handleRfidScan(uid, readerName);
    else log(QString("POST RFID (%1) — card_id absent.").arg(readerName));
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
    log(QString("Badge : %1 — %2 — verification adjudicator...").arg(uid, readerName));
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
        log(QString("[%1] (%2) ERREUR adjudicator : %3 -> acces refuse.")
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
        log(QString("[%1] (%2) ACCES AUTORISE — %3").arg(uid, readerName, owner));
        m_lblLastBadge->setText(QString("%1  —  %2  —  AUTORISE  (%3)").arg(uid, owner, readerName));
        m_lblLastBadge->setStyleSheet("color:#2e7d32; font-weight:bold;");
        openGache();
    } else {
        log(QString("[%1] (%2) ACCES REFUSE — %3 — %4")
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

void LaboMonitoringWidget::onSurvLog(const QString &msg)
{
    // Filtrer les messages verbeux du controleur (timers, polls internes)
    // On garde uniquement les messages arm/disarm
    if (msg.contains("arme") || msg.contains("desarme") || msg.contains("REFUSE"))
        log(QString("[E1-API] %1").arg(msg));
}

void LaboMonitoringWidget::onApiLog(const QString &msg)
{
    log(QString("[API E1] %1").arg(msg));
}


// ─────────────────────────────────────────────────────────────────────────────
//  Utilitaires
// ─────────────────────────────────────────────────────────────────────────────

void LaboMonitoringWidget::log(const QString &msg)
{
    m_textLog->append(QString("[%1] %2")
                          .arg(QDateTime::currentDateTime().toString("hh:mm:ss"), msg));
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
