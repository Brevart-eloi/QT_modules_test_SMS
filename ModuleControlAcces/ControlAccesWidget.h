#ifndef CONTROLEACCESWIDGET_H
#define CONTROLEACCESWIDGET_H

#include <QWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QMap>
#include "qmodbustcpclient.h"   // AlarmCore

class QPushButton;
class QLabel;
class QTextEdit;
class QLineEdit;
class QSpinBox;

/*
 * ============================================================================
 *  ControlAccesWidget  —  Serveur de controle d'acces par badge RFID (E2)
 * ============================================================================
 *
 *  Flux :
 *    1. Arduino (lecteur exterieur) envoie POST /rfid/scan {"card_id":"AABB…"}
 *    2. Ce serveur interroge adjudicator : GET /api/rfid/check/{uid}
 *    3. Si authorized=true  → ouvre la gache (Modbus PET-7067 DO0 = ON)
 *       Si authorized=false → refuse silencieusement (log seulement)
 *    4. Timer : la gache se referme automatiquement apres N secondes
 *
 *  Note port HTTP :
 *    Defaut 80 pour s'adapter a l'Arduino (souvent hardcode sur :80).
 *    Si adjudicator.js tourne deja sur :80 sur la meme machine,
 *    utiliser un port different (ex. 8080) ET reconfigurer l'Arduino.
 *
 * ============================================================================
 */

class ControlAccesWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ControlAccesWidget(QWidget *parent = nullptr);
    ~ControlAccesWidget();

private slots:
    void onBtnStartClicked();
    void onBtnStopClicked();
    void onBtnCloseGacheClicked();   // fermeture manuelle d'urgence

    // Serveur HTTP (reception scans Arduino)
    void onNewConnection();
    void onClientReadyRead();
    void onClientDisconnected();

    // Verification badge via adjudicator
    void onAdjudicatorReply(QNetworkReply *reply);

    // Gache
    void onGacheTimer();

    // Modbus PET-7067
    void onModbusConnected();
    void onModbusDisconnected();

private:
    // ── Composants metier ──
    QTcpServer            *m_server     = nullptr;
    QNetworkAccessManager *m_nam        = nullptr;
    QModbusTcpClient      *m_modbus     = nullptr;
    QTimer                *m_gacheTimer = nullptr;

    // Buffer TCP par socket (accumule les donnees jusqu'au body complet)
    QMap<QTcpSocket*, QByteArray> m_buffers;

    bool m_gacheOpen = false;

    // ── Widgets de configuration ──
    QLineEdit *editModbusIp;
    QSpinBox  *spinHttpPort;
    QLineEdit *editAdjudicatorUrl;
    QSpinBox  *spinGacheCoil;
    QSpinBox  *spinGacheDuration;

    // ── Boutons ──
    QPushButton *btnStart;
    QPushButton *btnStop;
    QPushButton *btnCloseGache;

    // ── Indicateurs ──
    QLabel *labelServerStatus;
    QLabel *labelModbusStatus;
    QLabel *labelLastBadge;
    QLabel *labelGacheStatus;

    // ── Journal ──
    QTextEdit *textLog;

    // ── Méthodes internes ──
    void log(const QString &msg);
    void handleRfidScan(const QString &uid);
    void openGache();
    void closeGache();
    void sendHttpResponse(QTcpSocket *socket, int code, const QByteArray &body);
    void updateButtons(bool running);
};

#endif // CONTROLEACCESWIDGET_H
