#ifndef SURVEILLANCECONTROLLER_H
#define SURVEILLANCECONTROLLER_H

#include "AlarmCore.h"
#include <QObject>
#include <QTimer>
#include <QDateTime>
#include <QList>

#include "LaboAlertEventListener.h"
#include "qmodbustcpclient.h"

/*
 * ============================================================================
 *  SurveillanceController
 * ============================================================================
 *
 *  Coeur de l'algorithme d'alarme du PC Labo (Étudiant 2).
 *
 *  RESPONSABILITÉS :
 *    1. Se connecte aux modules Modbus PET-7050 (entrées) et PET-7067 (sorties)
 *    2. Surveille en permanence les capteurs porte/fenêtre via FC02
 *    3. Quand le mode surveillance est armé ET qu'un capteur s'ouvre :
 *       -> ALARME IMMÉDIATE (sirène + flash) via FC05
 *       -> Notification des observers (mail/SMS via classes de E3)
 *    4. La sirène/flash s'arrêtent automatiquement après X secondes (timestamp)
 *    5. Une seule alerte par épisode d'intrusion (anti-spam)
 *
 *  RESPECT DU SUJET :
 *    - Algorithme d'alarme : OK (lignes 4 du contrat E2)
 *    - Pattern Observer : OK (listeners notifiés via notifyAlert)
 *    - Lecture Modbus PET-7050 + pilotage PET-7067 : OK
 *
 *  HORS SCOPE :
 *    - Vérification des badges : faite par le backend Node.js (E1)
 *    - Mode normal (badge -> gâche) : géré par E1
 *    - On ne fait QUE l'alarme et la surveillance
 *
 * ============================================================================
 */

class ALARMCORE_EXPORT SurveillanceController : public QObject
{
    Q_OBJECT

public:
    explicit SurveillanceController(QObject *parent = nullptr);
    ~SurveillanceController();

    // ============== CONFIGURATION ==============
    void setOutputModuleAddress(const QString &ip, quint16 port = 502, quint8 unitId = 1);
    void setInputModuleAddress (const QString &ip, quint16 port = 502, quint8 unitId = 1);

    // Adresses Modbus des sorties relais sur le PET-7067
    void setSirenCoil(quint16 coilAddress) { m_sirenCoil = coilAddress; }
    void setFlashCoil(quint16 coilAddress) { m_flashCoil = coilAddress; }

    // Adresses Modbus des entrées sur le PET-7050
    void setDoorInputAddress  (quint16 addr) { m_doorInputAddress   = addr; }
    void setWindowInputAddress(quint16 addr) { m_windowInputAddress = addr; }

    // Durée pendant laquelle la sirène/flash restent actifs (ms)
    void setAlarmDuration(int ms) { m_alarmDurationMs = ms; }

    // Logique des entrées capteurs :
    //   true  (défaut) = capteurs NC (Normalement Fermés) : contact fermé quand porte/fenêtre FERMÉE
    //                    → DI HIGH quand fermé, LOW quand ouvert → il faut inverser
    //   false           = capteurs NO (Normalement Ouverts) : DI HIGH quand ouvert → logique directe
    void setInputsInverted(bool inverted) { m_invertInputs = inverted; }
    bool inputsInverted() const { return m_invertInputs; }

    // Période de polling des capteurs (ms)
    void setPollInterval(int ms) { m_pollInputsTimer->setInterval(ms); }

    // ============== DÉMARRAGE / ARRÊT ==============
    bool start();
    void stop();
    bool isRunning() const { return m_running; }

    // ============== MODE SURVEILLANCE ==============
    void arm();                         // active le mode surveillance
    void disarm();                      // désactive (coupe l'alarme si en cours)
    bool isArmed() const { return m_armed; }

    // ============== PATTERN OBSERVER ==============
    void addAlertEventListener   (LaboAlertEventListener *listener);
    void removeAlertEventListener(LaboAlertEventListener *listener);

    // ============== ÉTAT (pour l'IHM ou l'API) ==============
    bool isDoorOpen()   const { return m_doorOpen;   }
    bool isWindowOpen() const { return m_windowOpen; }
    bool isAlarmActive() const { return m_alarmActive; }

signals:
    void logMessage(const QString &message);
    void armedChanged(bool armed);
    void doorStateChanged(bool open);
    void windowStateChanged(bool open);
    void intrusionDetected(const QString &source);
    void alarmStarted();
    void alarmStopped();

protected:
    // Notifie tous les listeners (pattern Observer)
    void notifyAlert(const QString &description);

private slots:
    // Modbus connexion
    void onOutputModuleConnected();
    void onOutputModuleDisconnected();
    void onInputModuleConnected();
    void onInputModuleDisconnected();

    // Modbus lecture des entrées
    void onInputsReceived(quint16 startAddress, QVector<bool> values);

    // Timers
    void onPollInputsTimeout();
    void onAlarmTick();

private:
    // === Modbus ===
    QModbusTcpClient *m_outputModbus;   // PET-7067 (sirène, flash)
    QModbusTcpClient *m_inputModbus;    // PET-7050 (capteurs)

    QString m_outputIp; quint16 m_outputPort = 502; quint8 m_outputUnitId = 1;
    QString m_inputIp;  quint16 m_inputPort  = 502; quint8 m_inputUnitId  = 1;

    // Logique d'inversion des entrées (NC par défaut)
    bool m_invertInputs = true;

    // Adresses des sorties (par défaut, à adapter au câblage)
    quint16 m_sirenCoil = 1;
    quint16 m_flashCoil = 2;

    // Adresses des entrées (par défaut, à adapter au câblage)
    quint16 m_doorInputAddress   = 0;
    quint16 m_windowInputAddress = 1;

    // === État global ===
    bool m_running = false;
    bool m_armed   = false;

    // === État des capteurs ===
    bool m_doorOpen   = false;
    bool m_windowOpen = false;

    // === État de l'alarme ===
    bool      m_alarmActive       = false;
    QDateTime m_alarmActiveUntil;
    bool      m_intrusionAlerted  = false;
    int       m_alarmDurationMs   = 30000;  // 30s par défaut

    // === Observers (pattern Observer) ===
    QList<LaboAlertEventListener*> m_listeners;

    // === Timers ===
    QTimer *m_pollInputsTimer;   // poll des capteurs (1s par défaut)
    QTimer *m_alarmTickTimer;    // tick algo d'alarme (500ms)

    // === Méthodes internes ===
    void startAlarm(const QString &source);
    void stopAlarm();
    void evaluateIntrusion(const QString &source, bool sensorOpen);
};

#endif // SURVEILLANCECONTROLLER_H
