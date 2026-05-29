#include "LaboMonitoring.h"

// ============================================================================
//  Constructeur
// ============================================================================

LaboMonitoring::LaboMonitoring(QObject *parent)
    : QObject(parent),
      m_httpServer(new RfidHttpServer(this)),
      m_accessChecker(new BadgeAccessChecker(this)),
      m_outputModbus(nullptr),
      m_inputModbus(nullptr),
      m_doorUnlockTimer(new QTimer(this)),
      m_pollInputsTimer(new QTimer(this)),
      m_alarmTickTimer(new QTimer(this))
{
    // --- HTTP server (badges de l'Arduino) ---
    connect(m_httpServer, &RfidHttpServer::cardScanned,
            this, &LaboMonitoring::onCardScanned);
    connect(m_httpServer, &RfidHttpServer::logMessage,
            this, &LaboMonitoring::onSubLog);

    // --- Access checker ---
    connect(m_accessChecker, &BadgeAccessChecker::accessChecked,
            this, &LaboMonitoring::onAccessChecked);
    connect(m_accessChecker, &BadgeAccessChecker::logMessage,
            this, &LaboMonitoring::onSubLog);

    // --- Timer gâche (one-shot) ---
    m_doorUnlockTimer->setSingleShot(true);
    connect(m_doorUnlockTimer, &QTimer::timeout,
            this, &LaboMonitoring::onDoorUnlockTimeout);

    // --- Timer polling capteurs ---
    m_pollInputsTimer->setInterval(1000);
    connect(m_pollInputsTimer, &QTimer::timeout,
            this, &LaboMonitoring::onPollInputsTimeout);

    // --- Timer tick de l'algorithme d'alarme ---
    // Tourne en permanence pour vérifier si l'alarme doit s'arrêter.
    m_alarmTickTimer->setInterval(500);
    connect(m_alarmTickTimer, &QTimer::timeout,
            this, &LaboMonitoring::onAlarmTick);
}

LaboMonitoring::~LaboMonitoring()
{
    stop();
}

// ============================================================================
//  Configuration
// ============================================================================

void LaboMonitoring::setApiBaseUrl(const QString &baseUrl)
{
    m_accessChecker->setApiBaseUrl(baseUrl);
}

void LaboMonitoring::setOutputModuleAddress(const QString &ip, quint16 port, quint8 unitId)
{
    m_outputIp = ip; m_outputPort = port; m_outputUnitId = unitId;
}

void LaboMonitoring::setInputModuleAddress(const QString &ip, quint16 port, quint8 unitId)
{
    m_inputIp = ip; m_inputPort = port; m_inputUnitId = unitId;
}

// ============================================================================
//  start / stop
// ============================================================================

bool LaboMonitoring::start(quint16 httpPort)
{
    if (!m_httpServer->start(httpPort)) {
        emit logMessage("Impossible de demarrer le serveur HTTP RFID.");
        return false;
    }

    if (!m_outputIp.isEmpty()) {
        m_outputModbus = new QModbusTcpClient(m_outputIp, m_outputPort, m_outputUnitId, this);
        connect(m_outputModbus, &QTcpSocket::connected,
                this, &LaboMonitoring::onOutputModuleConnected);
        connect(m_outputModbus, &QTcpSocket::disconnected,
                this, &LaboMonitoring::onOutputModuleDisconnected);
        m_outputModbus->connectToHost();
    }

    if (!m_inputIp.isEmpty()) {
        m_inputModbus = new QModbusTcpClient(m_inputIp, m_inputPort, m_inputUnitId, this);
        connect(m_inputModbus, &QTcpSocket::connected,
                this, &LaboMonitoring::onInputModuleConnected);
        connect(m_inputModbus, &QTcpSocket::disconnected,
                this, &LaboMonitoring::onInputModuleDisconnected);
        connect(m_inputModbus, &QModbusTcpClient::onReadMultipleInputsStatusSentence,
                this, &LaboMonitoring::onInputsReceived);
        m_inputModbus->connectToHost();
    }

    m_pollInputsTimer->start();
    m_alarmTickTimer->start();

    emit logMessage("LaboMonitoring demarre.");
    return true;
}

void LaboMonitoring::stop()
{
    m_pollInputsTimer->stop();
    m_doorUnlockTimer->stop();
    m_alarmTickTimer->stop();

    // On coupe l'alarme proprement si elle était active
    if (m_alarmActive) {
        stopAlarm();
    }

    m_httpServer->stop();

    if (m_outputModbus) {
        m_outputModbus->close();
        m_outputModbus->deleteLater();
        m_outputModbus = nullptr;
    }
    if (m_inputModbus) {
        m_inputModbus->close();
        m_inputModbus->deleteLater();
        m_inputModbus = nullptr;
    }

    emit logMessage("LaboMonitoring arrete.");
}

// ============================================================================
//  Pattern Observer
// ============================================================================

void LaboMonitoring::addAlertEventListener(LaboAlertEventListener *listener)
{
    if (listener && !m_listeners.contains(listener)) {
        m_listeners.push_back(listener);
    }
}

void LaboMonitoring::removeAlertEventListener(LaboAlertEventListener *listener)
{
    m_listeners.removeAll(listener);
}

void LaboMonitoring::notifyAlert(const QString &description)
{
    emit logMessage(QString("ALERTE : %1").arg(description));
    for (auto *listener : m_listeners) {
        if (listener) {
            listener->onAlert(description);
        }
    }
}

// ============================================================================
//  Mode surveillance
// ============================================================================

void LaboMonitoring::setSurveillanceMode(bool enabled)
{
    m_surveillanceMode = enabled;

    if (enabled) {
        emit logMessage("Mode SURVEILLANCE active. Toute ouverture = intrusion.");
    } else {
        emit logMessage("Mode surveillance desactive.");
        // En sortant de surveillance, on coupe tout et on remet l'algo à zéro
        if (m_alarmActive) {
            stopAlarm();
        }
        m_intrusionAlerted = false;
    }
}

// ============================================================================
//  Réception d'un badge
// ============================================================================

void LaboMonitoring::onCardScanned(const QString &uid)
{
    emit cardBadged(uid);
    emit logMessage(QString("Badge detecte : %1").arg(uid));
    m_accessChecker->checkAccess(uid);
}

// ============================================================================
//  Résultat de la vérification d'accès
// ============================================================================

void LaboMonitoring::onAccessChecked(const AccessResult &result)
{
    if (result.granted)
    {
        emit accessGranted(result.uid, result.userName);
        emit logMessage(QString("ACCES AUTORISE pour %1 (%2)")
                            .arg(result.uid)
                            .arg(result.userName.isEmpty() ? "?" : result.userName));

        // Ouverture de la gâche
        unlockDoor();

        // Un badge valide DÉSARME le système : on quitte la surveillance
        // et on coupe l'alarme si elle sonnait.
        if (m_surveillanceMode) {
            setSurveillanceMode(false);
        }

        // TODO : signaler à l'Étudiant 1 le démarrage de l'enregistrement vidéo
    }
    else
    {
        emit accessDenied(result.uid, result.reason);
        emit logMessage(QString("ACCES REFUSE pour %1 : %2")
                            .arg(result.uid)
                            .arg(result.reason));

        // Un badge refusé en mode surveillance est suspect : on alerte
        // (mais une seule fois par épisode grâce au flag m_intrusionAlerted)
        if (m_surveillanceMode && !m_intrusionAlerted) {
            m_intrusionAlerted = true;
            notifyAlert(QString("Tentative d'acces refusee en surveillance : %1 (%2)")
                            .arg(result.uid)
                            .arg(result.reason));
        }
    }
}

// ============================================================================
//  Pilotage de la gâche
// ============================================================================

void LaboMonitoring::unlockDoor()
{
    if (!m_outputModbus) {
        emit logMessage("Impossible d'ouvrir la gache : pas de connexion Modbus.");
        return;
    }
    emit logMessage("Ouverture de la gache.");
    m_outputModbus->forceSingleCoilFC5(m_doorLockCoil, true);
    m_doorUnlockTimer->start(m_doorUnlockDurationMs);
}

void LaboMonitoring::lockDoor()
{
    if (!m_outputModbus) return;
    m_outputModbus->forceSingleCoilFC5(m_doorLockCoil, false);
    emit logMessage("Gache refermee.");
}

void LaboMonitoring::onDoorUnlockTimeout()
{
    lockDoor();
}

// ============================================================================
//  ALGORITHME D'ALARME
// ============================================================================
//
//  startAlarm() :
//    - active sirène + flash via Modbus
//    - calcule l'instant de fin (maintenant + durée)
//    - marque l'alarme comme active
//
//  onAlarmTick() (appelé toutes les 500ms) :
//    - si l'alarme est active ET que l'instant de fin est dépassé
//      -> on coupe l'alarme
//
//  stopAlarm() :
//    - coupe sirène + flash
//
//  L'utilisation d'un timestamp de fin (m_alarmActiveUntil) plutôt qu'un
//  simple QTimer::singleShot permet de "prolonger" l'alarme si une nouvelle
//  intrusion survient pendant qu'elle sonne déjà (on repousse juste la fin).
//
// ============================================================================

void LaboMonitoring::startAlarm(const QString &source)
{
    // On (re)calcule toujours l'instant de fin, même si déjà active :
    // ça prolonge la sirène si une nouvelle ouverture arrive.
    m_alarmActiveUntil = QDateTime::currentDateTime().addMSecs(m_alarmDurationMs);

    if (!m_alarmActive) {
        m_alarmActive = true;
        emit logMessage(QString("=== ALARME DECLENCHEE (source: %1) ===").arg(source));

        // Activation physique sirène + flash
        if (m_outputModbus) {
            m_outputModbus->forceSingleCoilFC5(m_sirenCoil, true);
            m_outputModbus->forceSingleCoilFC5(m_flashCoil, true);
        }

        emit alarmStarted();
    }
}

void LaboMonitoring::stopAlarm()
{
    if (!m_alarmActive) return;

    m_alarmActive = false;

    if (m_outputModbus) {
        m_outputModbus->forceSingleCoilFC5(m_sirenCoil, false);
        m_outputModbus->forceSingleCoilFC5(m_flashCoil, false);
    }

    emit logMessage("=== ALARME ARRETEE ===");
    emit alarmStopped();
}

void LaboMonitoring::onAlarmTick()
{
    // Si l'alarme est active et que le temps est écoulé, on l'arrête.
    if (m_alarmActive && QDateTime::currentDateTime() >= m_alarmActiveUntil) {
        stopAlarm();
    }

    // Réinitialisation du flag anti-spam :
    // si plus aucune ouverture ET alarme finie, on est "revenu au calme",
    // donc une future intrusion pourra à nouveau déclencher une alerte.
    if (!m_alarmActive && !m_doorOpen && !m_windowOpen) {
        m_intrusionAlerted = false;
    }
}

// ============================================================================
//  Évaluation d'une intrusion sur un capteur
// ============================================================================
//
//  Appelée quand l'état d'un capteur change.
//  Si on est en surveillance ET que le capteur passe à "ouvert" :
//    -> intrusion immédiate : alarme + alerte (une seule fois)
//
// ============================================================================

void LaboMonitoring::evaluateIntrusion(const QString &source, bool sensorOpen)
{
    if (!sensorOpen) {
        return; // fermeture : rien à faire ici
    }

    if (!m_surveillanceMode) {
        return; // ouverture autorisée (pas en surveillance)
    }

    // --- INTRUSION détectée ---
    emit intrusionDetected(source);

    // Alarme immédiate (sirène + flash). Si déjà active, ça la prolonge.
    startAlarm(source);

    // Alerte mail/SMS : UNE SEULE par épisode d'intrusion
    if (!m_intrusionAlerted) {
        m_intrusionAlerted = true;
        notifyAlert(QString("INTRUSION detectee : ouverture %1 en mode surveillance !")
                        .arg(source));
    }
}

// ============================================================================
//  Polling des capteurs (PET-7050)
// ============================================================================

void LaboMonitoring::onPollInputsTimeout()
{
    if (m_inputModbus && m_inputModbus->state() == QAbstractSocket::ConnectedState) {
        // Lit 2 entrées : porte (index 0) + fenêtre (index 1)
        // À adapter selon le câblage réel.
        m_inputModbus->readMultipleInputsStatusFC2(0, 2);
    }
}

void LaboMonitoring::onInputsReceived(quint16 startAddress, QVector<bool> values)
{
    Q_UNUSED(startAddress);
    if (values.size() < 2) return;

    bool doorOpen   = values[0];
    bool windowOpen = values[1];

    // --- Changement d'état porte ---
    if (doorOpen != m_doorOpen) {
        m_doorOpen = doorOpen;
        emit logMessage(QString("Porte : %1").arg(doorOpen ? "OUVERTE" : "fermee"));
        evaluateIntrusion("porte", doorOpen);
    }

    // --- Changement d'état fenêtre ---
    if (windowOpen != m_windowOpen) {
        m_windowOpen = windowOpen;
        emit logMessage(QString("Fenetre : %1").arg(windowOpen ? "OUVERTE" : "fermee"));
        evaluateIntrusion("fenetre", windowOpen);
    }
}

// ============================================================================
//  Évènements Modbus connexion
// ============================================================================

void LaboMonitoring::onOutputModuleConnected()
{
    emit logMessage(QString("Module sorties (PET-7067) connecte sur %1.").arg(m_outputIp));
}

void LaboMonitoring::onOutputModuleDisconnected()
{
    emit logMessage("Module sorties (PET-7067) deconnecte.");
}

void LaboMonitoring::onInputModuleConnected()
{
    emit logMessage(QString("Module entrees (PET-7050) connecte sur %1.").arg(m_inputIp));
}

void LaboMonitoring::onInputModuleDisconnected()
{
    emit logMessage("Module entrees (PET-7050) deconnecte.");
}

// ============================================================================
//  Relais de logs
// ============================================================================

void LaboMonitoring::onSubLog(const QString &message)
{
    emit logMessage(message);
}
