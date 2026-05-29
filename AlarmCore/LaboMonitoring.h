#ifndef LABOMONITORING_H
#define LABOMONITORING_H

#include "alarmcore_global.h"
#include <QObject>
#include <QList>
#include <QTimer>
#include <QDateTime>

#include "RfidHttpServer.h"
#include "BadgeAccessChecker.h"
#include "LaboAlertEventListener.h"
#include "qmodbustcpclient.h"

/*
 * ============================================================================
 *  LaboMonitoring
 * ============================================================================
 *
 *  Classe principale du service "PC Labo".
 *
 *  Rôles :
 *    1. Écoute les badges (via RfidHttpServer qui reçoit les POST de l'Arduino)
 *    2. Vérifie les droits d'accès (via BadgeAccessChecker -> API E3)
 *    3. Pilote la gâche / sirène / flash (via Modbus PET-7067)
 *    4. Surveille les capteurs porte/fenêtre (via Modbus PET-7050)
 *    5. ALGORITHME D'ALARME + alertes (pattern Observer)
 *
 *  ----------------------------------------------------------------------------
 *  ALGORITHME D'ALARME (contrat de tâches E2) :
 *  ----------------------------------------------------------------------------
 *    - En mode surveillance, TOUTE ouverture (porte ou fenêtre) déclenche
 *      IMMÉDIATEMENT l'alarme (sirène + flash). Pas de délai de grâce.
 *    - La sirène + le flash s'arrêtent automatiquement après une durée
 *      paramétrable (m_alarmDurationMs). On utilise un TIMESTAMP de fin
 *      (m_alarmActiveUntil) vérifié à chaque tick, plutôt qu'un setTimeout
 *      unique : c'est plus robuste si plusieurs intrusions s'enchaînent.
 *    - UNE SEULE alerte mail/SMS est envoyée par "épisode" d'intrusion
 *      (anti-spam). On repart à zéro quand l'intrusion est résolue
 *      (capteurs refermés ET alarme terminée).
 *
 *  Pattern Observer (page 9 du sujet) :
 *    - addAlertEventListener(listener)
 *    - removeAlertEventListener(listener)
 *    - notifyAlert(description) [protected]
 *
 * ============================================================================
 */

class ALARMCORE_EXPORT LaboMonitoring : public QObject
{
    Q_OBJECT

public:
    explicit LaboMonitoring(QObject *parent = nullptr);
    ~LaboMonitoring();

    // ============== CONFIGURATION ==============
    void setApiBaseUrl(const QString &baseUrl);
    void setOutputModuleAddress(const QString &ip, quint16 port = 502, quint8 unitId = 1);
    void setInputModuleAddress (const QString &ip, quint16 port = 502, quint8 unitId = 1);

    void setDoorLockCoil(quint16 coilAddress) { m_doorLockCoil = coilAddress; }
    void setSirenCoil   (quint16 coilAddress) { m_sirenCoil    = coilAddress; }
    void setFlashCoil   (quint16 coilAddress) { m_flashCoil    = coilAddress; }

    // Durée d'ouverture de la gâche (ms)
    void setDoorUnlockDuration(int ms) { m_doorUnlockDurationMs = ms; }

    // Durée pendant laquelle la sirène/flash restent actifs (ms)
    void setAlarmDuration(int ms) { m_alarmDurationMs = ms; }

    // ============== DÉMARRAGE / ARRÊT ==============
    bool start(quint16 httpPort = 80);
    void stop();

    // ============== PATTERN OBSERVER ==============
    void addAlertEventListener   (LaboAlertEventListener *listener);
    void removeAlertEventListener(LaboAlertEventListener *listener);

    // ============== MODE SURVEILLANCE ==============
    void setSurveillanceMode(bool enabled);
    bool surveillanceMode() const { return m_surveillanceMode; }

signals:
    void logMessage(const QString &message);

    void cardBadged(const QString &uid);
    void accessGranted(const QString &uid, const QString &userName);
    void accessDenied (const QString &uid, const QString &reason);

    void intrusionDetected(const QString &source);
    void alarmStarted();
    void alarmStopped();

protected:
    // Notifie tous les listeners (pattern Observer)
    void notifyAlert(const QString &description);

private slots:
    void onCardScanned(const QString &uid);
    void onAccessChecked(const AccessResult &result);
    void onSubLog(const QString &message);

    void onOutputModuleConnected();
    void onOutputModuleDisconnected();
    void onInputModuleConnected();
    void onInputModuleDisconnected();
    void onInputsReceived(quint16 startAddress, QVector<bool> values);

    void onDoorUnlockTimeout();
    void onPollInputsTimeout();

    // Tick périodique de l'algorithme d'alarme
    void onAlarmTick();

private:
    // === Composants ===
    RfidHttpServer     *m_httpServer;
    BadgeAccessChecker *m_accessChecker;
    QModbusTcpClient   *m_outputModbus;   // PET-7067 : gâche, sirène, flash
    QModbusTcpClient   *m_inputModbus;    // PET-7050 : capteurs

    // === Adresses Modbus ===
    QString m_outputIp; quint16 m_outputPort = 502; quint8 m_outputUnitId = 1;
    QString m_inputIp;  quint16 m_inputPort  = 502; quint8 m_inputUnitId  = 1;

    quint16 m_doorLockCoil = 0;
    quint16 m_sirenCoil    = 1;
    quint16 m_flashCoil    = 2;

    int m_doorUnlockDurationMs = 5000;    // gâche ouverte 5s
    int m_alarmDurationMs      = 30000;   // sirène 30s

    // === État surveillance / capteurs ===
    bool m_surveillanceMode = false;
    bool m_doorOpen         = false;
    bool m_windowOpen       = false;

    // === État de l'algorithme d'alarme ===
    bool      m_alarmActive       = false;  // sirène/flash en cours ?
    QDateTime m_alarmActiveUntil;           // instant de fin de l'alarme
    bool      m_intrusionAlerted  = false;  // a-t-on déjà notifié pour CET épisode ?

    // === Observers ===
    QList<LaboAlertEventListener*> m_listeners;

    // === Timers ===
    QTimer *m_doorUnlockTimer;   // ferme la gâche
    QTimer *m_pollInputsTimer;   // poll des capteurs
    QTimer *m_alarmTickTimer;    // tick de l'algorithme d'alarme

    // === Méthodes internes ===
    void unlockDoor();
    void lockDoor();
    void startAlarm(const QString &source);   // déclenche sirène + flash
    void stopAlarm();                          // coupe sirène + flash
    void evaluateIntrusion(const QString &source, bool sensorOpen);
};

#endif // LABOMONITORING_H
