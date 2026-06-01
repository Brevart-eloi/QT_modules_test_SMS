#include "SurveillanceHttpApi.h"

#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

// ============================================================================
//  Constructeur
// ============================================================================

SurveillanceHttpApi::SurveillanceHttpApi(SurveillanceController *controller, QObject *parent)
    : QObject(parent),
      m_controller(controller),
      m_tcpServer(nullptr)
{
}

SurveillanceHttpApi::~SurveillanceHttpApi()
{
    stop();
}

// ============================================================================
//  start / stop
// ============================================================================

bool SurveillanceHttpApi::start(quint16 port)
{
    if (m_tcpServer) stop();

    m_tcpServer = new QTcpServer(this);
    connect(m_tcpServer, &QTcpServer::newConnection,
            this, &SurveillanceHttpApi::onNewConnection);

    if (!m_tcpServer->listen(QHostAddress::Any, port)) {
        emit logMessage(QString("API HTTP : impossible d'ecouter sur le port %1 : %2")
                            .arg(port)
                            .arg(m_tcpServer->errorString()));
        m_tcpServer->deleteLater();
        m_tcpServer = nullptr;
        return false;
    }

    emit logMessage(QString("API HTTP demarree sur le port %1").arg(port));
    emit logMessage("Endpoints : POST /surveillance/arm, POST /surveillance/disarm, GET /surveillance/status");
    return true;
}

void SurveillanceHttpApi::stop()
{
    if (m_tcpServer) {
        m_tcpServer->close();
        m_tcpServer->deleteLater();
        m_tcpServer = nullptr;
        emit logMessage("API HTTP arretee.");
    }
}

bool SurveillanceHttpApi::isListening() const
{
    return m_tcpServer && m_tcpServer->isListening();
}

// ============================================================================
//  Nouvelle connexion
// ============================================================================

void SurveillanceHttpApi::onNewConnection()
{
    while (m_tcpServer && m_tcpServer->hasPendingConnections()) {
        QTcpSocket *client = m_tcpServer->nextPendingConnection();
        connect(client, &QTcpSocket::readyRead,
                this, &SurveillanceHttpApi::onClientReadyRead);
        connect(client, &QTcpSocket::disconnected,
                this, &SurveillanceHttpApi::onClientDisconnected);
    }
}

void SurveillanceHttpApi::onClientReadyRead()
{
    QTcpSocket *client = qobject_cast<QTcpSocket*>(sender());
    if (!client) return;

    // Accumulation des octets reçus
    QByteArray buffer = client->property("buffer").toByteArray();
    buffer += client->readAll();
    client->setProperty("buffer", buffer);

    // Attente de la fin des headers
    int headerEnd = buffer.indexOf("\r\n\r\n");
    if (headerEnd == -1) return;

    // Attente du body complet
    int contentLength = 0;
    QByteArray headers = buffer.left(headerEnd);
    QRegularExpression reCL("Content-Length:\\s*(\\d+)", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch m = reCL.match(headers);
    if (m.hasMatch()) {
        contentLength = m.captured(1).toInt();
    }

    int bodyReceived = buffer.size() - (headerEnd + 4);
    if (bodyReceived < contentLength) return;

    handleRequest(client, buffer);
}

void SurveillanceHttpApi::onClientDisconnected()
{
    QTcpSocket *client = qobject_cast<QTcpSocket*>(sender());
    if (client) {
        client->deleteLater();
    }
}

// ============================================================================
//  Traitement de la requête
// ============================================================================

void SurveillanceHttpApi::handleRequest(QTcpSocket *client, const QByteArray &request)
{
    // Première ligne : METHOD PATH HTTP/1.1
    int firstLineEnd = request.indexOf("\r\n");
    if (firstLineEnd == -1) {
        client->write(buildResponse(400, "{\"ok\":false,\"error\":\"bad request\"}"));
        client->flush();
        client->disconnectFromHost();
        return;
    }

    QByteArray firstLine = request.left(firstLineEnd);
    QList<QByteArray> parts = firstLine.split(' ');
    if (parts.size() < 2) {
        client->write(buildResponse(400, "{\"ok\":false,\"error\":\"bad request\"}"));
        client->flush();
        client->disconnectFromHost();
        return;
    }

    QString method = QString::fromLatin1(parts[0]).toUpper();
    QString path   = QString::fromLatin1(parts[1]);

    emit logMessage(QString("API HTTP : %1 %2").arg(method, path));

    // -------- Routage --------
    if (method == "POST" && path == "/surveillance/arm") {
        m_controller->arm();
        client->write(buildResponse(200, buildStatusJson()));
    }
    else if (method == "POST" && path == "/surveillance/disarm") {
        m_controller->disarm();
        client->write(buildResponse(200, buildStatusJson()));
    }
    else if (method == "GET" && path == "/surveillance/status") {
        client->write(buildResponse(200, buildStatusJson()));
    }
    else {
        client->write(buildResponse(404, "{\"ok\":false,\"error\":\"endpoint inconnu\"}"));
    }

    client->flush();
    client->disconnectFromHost();
}

// ============================================================================
//  Construction du JSON de statut
// ============================================================================

QString SurveillanceHttpApi::buildStatusJson() const
{
    QJsonObject obj;
    obj["ok"]          = true;
    obj["armed"]       = m_controller->isArmed();
    obj["alarm_active"] = m_controller->isAlarmActive();
    obj["door_open"]   = m_controller->isDoorOpen();
    obj["window_open"] = m_controller->isWindowOpen();
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

// ============================================================================
//  Construction d'une réponse HTTP
// ============================================================================

QByteArray SurveillanceHttpApi::buildResponse(int statusCode, const QString &jsonBody)
{
    QString statusText;
    switch (statusCode) {
    case 200: statusText = "OK"; break;
    case 400: statusText = "Bad Request"; break;
    case 404: statusText = "Not Found"; break;
    default:  statusText = "Unknown"; break;
    }

    QByteArray body = jsonBody.toUtf8();
    QByteArray response;
    response += "HTTP/1.1 " + QByteArray::number(statusCode) + " " + statusText.toUtf8() + "\r\n";
    response += "Content-Type: application/json\r\n";
    response += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    response += "Access-Control-Allow-Origin: *\r\n";  // pour que l'IHM web E1 puisse appeler
    response += "Connection: close\r\n";
    response += "\r\n";
    response += body;
    return response;
}
