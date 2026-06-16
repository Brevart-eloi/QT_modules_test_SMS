#ifndef INTEGRATIONVIDEOWIDGET_H
#define INTEGRATIONVIDEOWIDGET_H

#include <QWidget>
#include "SurveillanceController.h"
#include "SurveillanceHttpApi.h"
#include "TestAlertListener.h"
#include "VideoRecorderClient.h"

class QPushButton;
class QLabel;
class QTextEdit;
class QLineEdit;
class QSpinBox;
class QCheckBox;
class QListWidget;

/*
 * ============================================================================
 *  IntegrationVideoWidget
 * ============================================================================
 *
 *  Test d'integration (Revue 3, sujet E6 - Etudiant 2) :
 *    Reprend le test d'integration precedent (surveillance + alarme + Observer)
 *    et y AJOUTE le pilotage de l'enregistrement video :
 *
 *      >> Lancement de l'enregistrement video (envoi des messages de
 *         demarrage / arret) LORSQUE QUELQU'UN ENTRE DANS LE LABORATOIRE. <<
 *
 *  CHAINE TESTEE :
 *    1. SurveillanceController (Modbus PET-7050 / PET-7067, algo d'alarme)
 *    2. Pattern Observer (TestAlertListener -> alerte affichee)
 *    3. SurveillanceHttpApi (arm/disarm depuis l'IHM web de E1)
 *    4. NOUVEAU : VideoRecorderClient (TCP/IP)
 *         - intrusionDetected (quelqu'un entre)  -> startRecording()
 *         - alarmStopped      (fin de l'episode) -> stopRecording()
 *
 *  Note : le protocole video de E1 n'etant pas defini, VideoRecorderClient
 *  envoie des messages arbitraires (cf. VideoRecorderClient.h).
 *
 * ============================================================================
 */

class IntegrationVideoWidget : public QWidget
{
    Q_OBJECT

public:
    explicit IntegrationVideoWidget(QWidget *parent = nullptr);
    ~IntegrationVideoWidget();

private slots:
    void onBtnStartClicked();
    void onBtnStopClicked();
    void onBtnArmClicked();
    void onBtnDisarmClicked();
    void onBtnClearAlertsClicked();

    // Slots relies au SurveillanceController
    void onLog(const QString &msg);
    void onArmedChanged(bool armed);
    void onDoorStateChanged(bool open);
    void onWindowStateChanged(bool open);
    void onAlarmStarted();
    void onAlarmStopped();
    void onIntrusionDetected(const QString &source);

    // Slot du TestAlertListener (pattern Observer)
    void onAlertReceived(const QString &description);

    // ── NOUVEAU : enregistrement video ──
    void onVideoLog(const QString &msg);
    void onRecordingStarted();
    void onRecordingStopped();
    void onBtnTestVideoStartClicked();
    void onBtnTestVideoStopClicked();

private:
    // ── Classes metier (E2) ──
    SurveillanceController *m_controller;
    SurveillanceHttpApi    *m_httpApi;
    TestAlertListener      *m_testListener;

    // ── NOUVEAU : client TCP d'enregistrement video ──
    VideoRecorderClient    *m_videoClient;

    // ── Widgets de configuration ──
    QLineEdit *editOutputIp;
    QLineEdit *editInputIp;
    QSpinBox  *spinHttpPort;
    QSpinBox  *spinAlarmDuration;
    QSpinBox  *spinSirenCoil;
    QSpinBox  *spinFlashCoil;
    QSpinBox  *spinDoorInput;
    QSpinBox  *spinWindowInput;
    QCheckBox *chkInvertInputs;

    // ── NOUVEAU : configuration du service video ──
    QLineEdit *editVideoIp;
    QSpinBox  *spinVideoPort;

    // ── Boutons de controle ──
    QPushButton *btnStart;
    QPushButton *btnStop;
    QPushButton *btnArm;
    QPushButton *btnDisarm;

    // ── Indicateurs d'etat ──
    QLabel *labelArmed;
    QLabel *labelDoor;
    QLabel *labelWindow;
    QLabel *labelAlarm;
    QLabel *labelRecording;          // NOUVEAU : etat enregistrement video

    // ── NOUVEAU : boutons de test direct du client video ──
    QPushButton *btnTestVideoStart;
    QPushButton *btnTestVideoStop;

    // ── Zone d'alertes (pattern Observer) ──
    QListWidget *listAlerts;
    QPushButton *btnClearAlerts;

    // ── Journal general ──
    QTextEdit *textLog;

    void log(const QString &msg);
    void updateButtons();
};

#endif // INTEGRATIONVIDEOWIDGET_H
