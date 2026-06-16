#pragma once

#include <QWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QTime>
#include <QMap>
#include <QDateTime>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QCheckBox>
#include <QColor>

#include "SurveillanceController.h"
#include "SurveillanceHttpApi.h"
#include "AlertNotifier.h"
#include "SMSAlertNotifierStrategy.h"
#include "MailAlertNotifierStrategy.h"
#include "qmodbustcpclient.h"

/*
 * ============================================================================
 *  LaboMonitoringWidget — application unique E2
 * ============================================================================
 *
 *  FONCTIONS :
 *    1. Controle d'acces RFID
 *       Arduino POST /rfid/scan -> adjudicator -> gache DO0 (pulse 3 s)
 *
 *    2. Surveillance multi-salles (CIEL1 + CIEL2 + Physique)
 *       Polling PET-7050 DI4..DI9 toutes les secondes.
 *       Intrusion -> sirenes 3 min + flash permanent + SMS + mail.
 *       Apres 3 min : sirenes OFF, flash restent ON jusqu'au reset manuel.
 *
 *    3. Pattern Observer
 *       AlertNotifier (SMS + Mail) appele directement sur intrusion.
 *
 *    4. API HTTP pour l'Etudiant 1 (port 8080)
 *       SurveillanceHttpApi : arm/disarm/status
 *
 *  Capteurs (PET-7050, tous NC = Normalement Fermes, base 0) :
 *    DI4 = detecteur mouvement CIEL1
 *    DI5 = porte CIEL1       (NC)
 *    DI6 = detecteur mouvement CIEL2
 *    DI7 = porte CIEL2       (NC)
 *    DI8 = detecteur mouvement physique
 *    DI9 = portes physique   (NC)
 *
 *  Sorties (PET-7067, base 0) :
 *    DO0 = gache electrique
 *    DO1 = flash   CIEL1
 *    DO2 = sirene  CIEL1
 *    DO3 = flash   CIEL2
 *    DO4 = sirene  CIEL2
 *    DO6 = sirene  physique
 *    DO7 = flash   physique
 *
 * ============================================================================
 */

class LaboMonitoringWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LaboMonitoringWidget(QWidget *parent = nullptr);
    ~LaboMonitoringWidget();

private slots:
    // Service
    void onBtnStartClicked();
    void onBtnStopClicked();

    // Alarme (manuel) 
    void onBtnArmClicked();
    void onBtnDisarmClicked();
    void onBtnResetFlashClicked();

    // Serveur RFID (POST /rfid/scan depuis Arduino) 
    void onRfidNewConnection();
    void onRfidClientReadyRead();
    void onRfidClientDisconnected();

    // Adjudicator 
    void onAdjudicatorReply(QNetworkReply *reply);

    // Gache
    void onGacheTimer();

    // Alarme : sirenes s'arretent apres 3 min, flash restent 
    void onSirenTimer();

    // ── Modbus PET-7050 (capteurs) 
    void onPollTimer();
    void onInputModbusConnected();
    void onInputModbusDisconnected();
    void onSensorsReceived(quint16 startAddress, QVector<bool> values);

    // Modbus PET-7067 (sorties) 
    void onOutputModbusConnected();
    void onOutputModbusDisconnected();

    // Planning 
    void onScheduleTimer();
    void onScheduleReply(QNetworkReply *reply);

    // ── SurveillanceController (arm/disarm via API HTTP E1)
    void onArmedChanged(bool armed);
    void onSurvLog(const QString &msg);
    void onApiLog(const QString &msg);

private:
    // ── Pattern Observer 
    // SurveillanceController est configure SANS IP Modbus :
    //   - ne gere pas les capteurs ni les sorties physiques
    //   - sert uniquement de machine a etats arm/disarm pour SurveillanceHttpApi
    SurveillanceController *m_surveillance  = nullptr;
    SurveillanceHttpApi    *m_httpApi       = nullptr;
    AlertNotifier          *m_alertNotifier = nullptr;

    // Modbus 
    QModbusTcpClient *m_inputModbus  = nullptr; // PET-7050 — lecture capteurs
    QModbusTcpClient *m_outputModbus = nullptr; // PET-7067 — gache + flash + sirenes
    QTimer           *m_pollTimer    = nullptr; // polling capteurs 1 s

    // ── Serveur RFID 
    QTcpServer *m_rfidServer = nullptr;
    QMap<QTcpSocket *, QByteArray> m_rfidBuffers;

    // ── Gache ─────────────────────────────────────────────────────────────────
    QTimer *m_gacheTimer = nullptr;
    bool    m_gacheOpen  = false;

    // ── Alarme ────────────────────────────────────────────────────────────────
    QTimer *m_sirenTimer       = nullptr; // 3 min -> extinction sirenes
    bool    m_alarmActive      = false;   // sirenes en cours
    bool    m_flashActive      = false;   // flash allumes (restes apres sirenes)
    bool    m_intrusionAlerted = false;   // anti-spam SMS/mail (reset a l'armement)

    // ── Capteurs (DI4..DI9, tous traites en NC) 
    static const int     NUM_SENSORS = 6;
    static const quint16 DI_START    = 4; // premier canal (DI4)
    bool m_sensorOpen[NUM_SENSORS]   = {};

    // ── Timer de presence RFID 
    // Cle : UID badge  |  Valeur : heure d'entree (lecteur exterieur .200)
    // Efface a la sortie (lecteur interieur .201) pour afficher la duree de presence
    QMap<QString, QDateTime> m_entryTimes;

    // ── Reseau 
    QNetworkAccessManager *m_nam           = nullptr;
    QTimer                *m_scheduleTimer = nullptr;
    QTime                  m_scheduleStart;
    QTime                  m_scheduleEnd;
    bool                   m_scheduleValid = false;

    // ── Etat interne 
    bool m_running = false;

    // ── UI 
    QPushButton *m_btnStart        = nullptr;
    QPushButton *m_btnStop         = nullptr;
    QPushButton *m_btnArm          = nullptr;
    QPushButton *m_btnDisarm       = nullptr;
    QPushButton *m_btnResetFlash   = nullptr;
    QPushButton *m_btnCloseGache   = nullptr;
    QCheckBox   *m_chkAutoSchedule = nullptr;
    QLabel      *m_lblArmed        = nullptr;
    QLabel      *m_lblAlarmStatus  = nullptr;
    QLabel      *m_lblGache        = nullptr;
    QLabel      *m_lblLastBadge    = nullptr;
    QLabel      *m_lblSchedule     = nullptr;
    QLabel      *m_lblModbusIn     = nullptr;
    QLabel      *m_lblModbusOut    = nullptr;
    QLabel      *m_lblSrvRfid      = nullptr;
    QLabel      *m_lblSrvAlarmApi  = nullptr;
    QLabel      *m_lblSensors[NUM_SENSORS] = {};
    QTextEdit   *m_textLog         = nullptr;

    // Methodes 
    void buildUi();
    void log(const QString &msg);
    void logBadge(const QString &msg);
    void sendDiEventToBackend(int idx, bool isOpen, bool triggered);                        
    void appendLog(const QString &msg, const QColor &color);  
    void handleRfidScan(const QString &uid, const QString &readerName);
    void openGache();
    void closeGache();
    void activateAlarm(const QString &source);
    void stopSirens();
    void stopAllOutputs();
    void sendHttpResponse(QTcpSocket *socket, int code, const QByteArray &body);
    void updateButtons();
    void fetchAndApplySchedule();
    void applyScheduleArming();
    void setStatus(QLabel *lbl, const QString &text, const QString &color);
};
