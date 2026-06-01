#ifndef SURVEILLANCEHTTPAPI_H
#define SURVEILLANCEHTTPAPI_H

#include "AlarmCore.h"
#include <QObject>
#include <QTcpServer>

#include "SurveillanceController.h"

/*
 * ============================================================================
 *  SurveillanceHttpApi
 * ============================================================================
 *
 *  Petite API HTTP REST pour piloter le SurveillanceController depuis
 *  l'extérieur (typiquement l'IHM web de l'Étudiant 1, ou un Stream Deck).
 *
 *  ENDPOINTS exposés :
 *
 *    POST /surveillance/arm
 *      -> Active le mode surveillance.
 *      -> Réponse : { "ok": true, "armed": true }
 *
 *    POST /surveillance/disarm
 *      -> Désactive le mode surveillance.
 *      -> Réponse : { "ok": true, "armed": false }
 *
 *    GET /surveillance/status
 *      -> Retourne l'état complet du système.
 *      -> Réponse : { "armed": bool, "alarm_active": bool,
 *                     "door_open": bool, "window_open": bool }
 *
 *  Pas d'authentification ici (consommée en réseau local par l'IHM web E1).
 *
 * ============================================================================
 */

class ALARMCORE_EXPORT SurveillanceHttpApi : public QObject
{
    Q_OBJECT

public:
    explicit SurveillanceHttpApi(SurveillanceController *controller, QObject *parent = nullptr);
    ~SurveillanceHttpApi();

    bool start(quint16 port = 8080);
    void stop();
    bool isListening() const;

signals:
    void logMessage(const QString &message);

private slots:
    void onNewConnection();
    void onClientReadyRead();
    void onClientDisconnected();

private:
    SurveillanceController *m_controller;
    QTcpServer             *m_tcpServer;

    void handleRequest(QTcpSocket *client, const QByteArray &request);
    QByteArray buildResponse(int statusCode, const QString &jsonBody);
    QString    buildStatusJson() const;
};

#endif // SURVEILLANCEHTTPAPI_H
