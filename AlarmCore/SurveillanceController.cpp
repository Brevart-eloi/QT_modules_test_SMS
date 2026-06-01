#include "SurveillanceController.h"

// ============================================================================
//  Constructeur
// ============================================================================

SurveillanceController::SurveillanceController(QObject *parent)
    : QObject(parent),
      m_outputModbus(nullptr),
      m_inputModbus(nullptr),
      m_pollInputsTimer(new QTimer(this)),
      m_alarmTickTimer(new QTimer(this))
{
    // Timer de polling des capteurs
    m_pollInputsTimer->setInterval(1000);  // 1 seconde par défaut
    connect(m_pollInputsTimer, &QTimer::timeout,
            this, &SurveillanceController::onPollInputsTimeout);

    // Timer de tick de l'algorithme d'alarme (vérifie si on doit couper l'alarme)
    m_alarmTickTimer->setInterval(500);
    connect(m_alarmTickTimer, &QTimer::timeout,
            this, &SurveillanceController::onAlarmTick);
}

SurveillanceController::~SurveillanceController()
{
    stop();
}

// ============================================================================
//  Configuration
// ============================================================================

void SurveillanceController::setOutputModuleAddress(const QString &ip, quint16 port, quint8 unitId)
{
    m_outputIp = ip; m_outputPort = port; m_outputUnitId = unitId;
}

void SurveillanceController::setInputModuleAddress(const QString &ip, quint16 port, quint8 unitId)
{
    m_inputIp = ip; m_inputPort = port; m_inputUnitId = unitId;
}

// ============================================================================
//  start / stop
// ============================================================================

bool SurveillanceController::start()
{
    if (m_running) {
        emit logMessage("Le controleur de surveillance est deja demarre.");
        return true;
    }

    // Connexion au PET-7067 (sorties : sirène, flash)
    if (!m_outputIp.isEmpty()) {
        m_outputModbus = new QModbusTcpClient(m_outputIp, m_outputPort, m_outputUnitId, this);
        connect(m_outputModbus, &QTcpSocket::connected,
                this, &SurveillanceController::onOutputModuleConnected);
        connect(m_outputModbus, &QTcpSocket::disconnected,
                this, &SurveillanceController::onOutputModuleDisconnected);
        m_outputModbus->connectToHost();
    } else {
        emit logMessage("ATTENTION : pas d'IP configuree pour le PET-7067 (sorties).");
    }

    // Connexion au PET-7050 (entrées : capteurs)
    if (!m_inputIp.isEmpty()) {
        m_inputModbus = new QModbusTcpClient(m_inputIp, m_inputPort, m_inputUnitId, this);
        connect(m_inputModbus, &QTcpSocket::connected,
                this, &SurveillanceController::onInputModuleConnected);
        connect(m_inputModbus, &QTcpSocket::disconnected,
                this, &SurveillanceController::onInputModuleDisconnected);
        connect(m_inputModbus, &QModbusTcpClient::onReadMultipleInputsStatusSentence,
                this, &SurveillanceController::onInputsReceived);
        m_inputModbus->connectToHost();
    } else {
        emit logMessage("ATTENTION : pas d'IP configuree pour le PET-7050 (entrees).");
    }

    // Démarrage des timers
    m_pollInputsTimer->start();
    m_alarmTickTimer->start();

    m_running = true;
    emit logMessage("Controleur de surveillance demarre.");
    return true;
}

void SurveillanceController::stop()
{
    m_pollInputsTimer->stop();
    m_alarmTickTimer->stop();

    // On coupe proprement l'alarme si elle était active
    if (m_alarmActive) {
        stopAlarm();
    }

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

    m_running = false;
    m_armed   = false;
    emit logMessage("Controleur de surveillance arrete.");
}

// ============================================================================
//  Armer / Désarmer
// ============================================================================

void SurveillanceController::arm()
{
    if (m_armed) return;

    m_armed = true;
    m_intrusionAlerted = false;
    emit armedChanged(true);
    emit logMessage("Mode SURVEILLANCE arme. Toute ouverture = intrusion.");
}

void SurveillanceController::disarm()
{
    if (!m_armed) return;

    m_armed = false;

    // En désarmant, on coupe l'alarme et on reset le flag anti-spam
    if (m_alarmActive) {
        stopAlarm();
    }
    m_intrusionAlerted = false;

    emit armedChanged(false);
    emit logMessage("Mode surveillance desarme.");
}

// ============================================================================
//  Pattern Observer
// ============================================================================

void SurveillanceController::addAlertEventListener(LaboAlertEventListener *listener)
{
    if (listener && !m_listeners.contains(listener)) {
        m_listeners.push_back(listener);
    }
}

void SurveillanceController::removeAlertEventListener(LaboAlertEventListener *listener)
{
    m_listeners.removeAll(listener);
}

void SurveillanceController::notifyAlert(const QString &description)
{
    emit logMessage(QString("ALERTE : %1").arg(description));
    for (auto *listener : m_listeners) {
        if (listener) {
            listener->onAlert(description);
        }
    }
}

// ============================================================================
//  ALGORITHME D'ALARME
// ============================================================================

void SurveillanceController::startAlarm(const QString &source)
{
    // Calcule (ou prolonge) l'instant de fin
    m_alarmActiveUntil = QDateTime::currentDateTime().addMSecs(m_alarmDurationMs);

    if (!m_alarmActive) {
        m_alarmActive = true;
        emit logMessage(QString("=== ALARME DECLENCHEE (source: %1) ===").arg(source));

        // Activation physique sirène + flash via Modbus
        if (m_outputModbus) {
            m_outputModbus->forceSingleCoilFC5(m_sirenCoil, true);
            m_outputModbus->forceSingleCoilFC5(m_flashCoil, true);
        }

        emit alarmStarted();
    }
}

void SurveillanceController::stopAlarm()
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

void SurveillanceController::onAlarmTick()
{
    // Si l'alarme est active et que le temps est écoulé, on l'arrête
    if (m_alarmActive && QDateTime::currentDateTime() >= m_alarmActiveUntil) {
        stopAlarm();
    }

    // Réarmement du flag anti-spam quand tout est calme
    if (!m_alarmActive && !m_doorOpen && !m_windowOpen) {
        m_intrusionAlerted = false;
    }
}

void SurveillanceController::evaluateIntrusion(const QString &source, bool sensorOpen)
{
    if (!sensorOpen) return;      // fermeture : on ignore
    if (!m_armed)    return;      // pas en surveillance : on ignore

    // INTRUSION
    emit intrusionDetected(source);
    startAlarm(source);

    // Notification mail/SMS : UNE SEULE par épisode
    if (!m_intrusionAlerted) {
        m_intrusionAlerted = true;
        notifyAlert(QString("INTRUSION : ouverture %1 detectee en mode surveillance !")
                        .arg(source));
    }
}

// ============================================================================
//  Polling des capteurs
// ============================================================================

void SurveillanceController::onPollInputsTimeout()
{
    if (m_inputModbus && m_inputModbus->state() == QAbstractSocket::ConnectedState) {
        // Lit 2 entrées consécutives à partir de l'adresse de la porte.
        // On suppose porte (offset 0) + fenêtre (offset 1) côte à côte.
        // Si ce n'est pas le cas chez toi, fais deux lectures séparées.
        m_inputModbus->readMultipleInputsStatusFC2(m_doorInputAddress, 2);
    }
}

void SurveillanceController::onInputsReceived(quint16 startAddress, QVector<bool> values)
{
    Q_UNUSED(startAddress);
    if (values.size() < 2) return;

    bool doorOpen   = values[0];
    bool windowOpen = values[1];

    // Changement d'état porte
    if (doorOpen != m_doorOpen) {
        m_doorOpen = doorOpen;
        emit doorStateChanged(doorOpen);
        emit logMessage(QString("Porte : %1").arg(doorOpen ? "OUVERTE" : "fermee"));
        evaluateIntrusion("porte", doorOpen);
    }

    // Changement d'état fenêtre
    if (windowOpen != m_windowOpen) {
        m_windowOpen = windowOpen;
        emit windowStateChanged(windowOpen);
        emit logMessage(QString("Fenetre : %1").arg(windowOpen ? "OUVERTE" : "fermee"));
        evaluateIntrusion("fenetre", windowOpen);
    }
}

// ============================================================================
//  Évènements Modbus connexion
// ============================================================================

void SurveillanceController::onOutputModuleConnected()
{
    emit logMessage(QString("Module sorties (PET-7067) connecte sur %1.").arg(m_outputIp));
}

void SurveillanceController::onOutputModuleDisconnected()
{
    emit logMessage("Module sorties (PET-7067) deconnecte.");
}

void SurveillanceController::onInputModuleConnected()
{
    emit logMessage(QString("Module entrees (PET-7050) connecte sur %1.").arg(m_inputIp));
}

void SurveillanceController::onInputModuleDisconnected()
{
    emit logMessage("Module entrees (PET-7050) deconnecte.");
}
