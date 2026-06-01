#include "RfidHttpServer.h"

#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

// ============================================================================
//  Constructeur / destructeur
// ============================================================================

RfidHttpServer::RfidHttpServer(QObject *parent)
    : QObject(parent),
      m_tcpServer(nullptr)
{
}

RfidHttpServer::~RfidHttpServer()
{
    stop();
}

// ============================================================================
//  start / stop
// ============================================================================

bool RfidHttpServer::start(quint16 port)
{
    if (m_tcpServer) stop();

    m_tcpServer = new QTcpServer(this);
    connect(m_tcpServer, &QTcpServer::newConnection,
            this, &RfidHttpServer::onNewConnection);

    if (!m_tcpServer->listen(QHostAddress::Any, port)) {
        emit logMessage(QString("Serveur RFID : impossible d'ecouter sur le port %1 : %2")
                            .arg(port)
                            .arg(m_tcpServer->errorString()));
        if (port < 1024)
            emit logMessage("Astuce : le port < 1024 demande souvent des droits administrateur sous Windows.");
        m_tcpServer->deleteLater();
        m_tcpServer = nullptr;
        return false;
    }

    emit logMessage(QString("Serveur RFID demarre, en ecoute sur le port %1.").arg(port));
    emit logMessage("En attente des POST /rfid/scan de l'Arduino...");
    return true;
}

void RfidHttpServer::stop()
{
    if (m_tcpServer) {
        m_tcpServer->close();
        m_tcpServer->deleteLater();
        m_tcpServer = nullptr;
        emit logMessage("Serveur RFID arrete.");
    }
}

bool RfidHttpServer::isListening() const
{
    return m_tcpServer && m_tcpServer->isListening();
}

// ============================================================================
//  Connexions entrantes
// ============================================================================

void RfidHttpServer::onNewConnection()
{
    while (m_tcpServer && m_tcpServer->hasPendingConnections()) {
        QTcpSocket *client = m_tcpServer->nextPendingConnection();
        connect(client, &QTcpSocket::readyRead,
                this, &RfidHttpServer::onClientReadyRead);
        connect(client, &QTcpSocket::disconnected,
                this, &RfidHttpServer::onClientDisconnected);
    }
}

void RfidHttpServer::onClientReadyRead()
{
    QTcpSocket *client = qobject_cast<QTcpSocket*>(sender());
    if (!client) return;

    // Accumulation des octets recus (la requete peut arriver en plusieurs paquets).
    QByteArray buffer = client->property("buffer").toByteArray();
    buffer += client->readAll();
    client->setProperty("buffer", buffer);

    // On attend la fin des en-tetes.
    int headerEnd = buffer.indexOf("\r\n\r\n");
    if (headerEnd == -1) return;

    // On attend le body complet (selon Content-Length).
    int contentLength = 0;
    QByteArray headers = buffer.left(headerEnd);
    QRegularExpression reCL("Content-Length:\\s*(\\d+)", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch m = reCL.match(QString::fromLatin1(headers));
    if (m.hasMatch())
        contentLength = m.captured(1).toInt();

    int bodyReceived = buffer.size() - (headerEnd + 4);
    if (bodyReceived < contentLength) return;

    handleRequest(client, buffer);
}

void RfidHttpServer::onClientDisconnected()
{
    QTcpSocket *client = qobject_cast<QTcpSocket*>(sender());
    if (client)
        client->deleteLater();
}

// ============================================================================
//  Traitement de la requete
// ============================================================================

void RfidHttpServer::handleRequest(QTcpSocket *client, const QByteArray &request)
{
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

    const QString method = QString::fromLatin1(parts[0]).toUpper();
    const QString path   = QString::fromLatin1(parts[1]);

    if (method == "POST" && path == "/rfid/scan") {
        // Extraction du body JSON (apres la ligne vide).
        int headerEnd = request.indexOf("\r\n\r\n");
        QByteArray body = (headerEnd != -1) ? request.mid(headerEnd + 4) : QByteArray();

        QJsonParseError perr;
        QJsonDocument doc = QJsonDocument::fromJson(body, &perr);
        if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
            emit logMessage(QString("Requete /rfid/scan : JSON invalide (%1)").arg(perr.errorString()));
            client->write(buildResponse(400, "{\"ok\":false,\"error\":\"invalid json\"}"));
            client->flush();
            client->disconnectFromHost();
            return;
        }

        QJsonObject obj = doc.object();
        const QString uid     = obj.value("card_id").toString();
        const QString rawHex  = obj.value("raw_hex").toString();
        const int     wType   = obj.value("wiegand_type").toInt();

        if (uid.isEmpty()) {
            emit logMessage("Requete /rfid/scan : champ card_id manquant.");
            client->write(buildResponse(400, "{\"ok\":false,\"error\":\"missing card_id\"}"));
        } else {
            emit logMessage(QString("Badge recu : card_id=%1 raw_hex=%2 wiegand=W%3")
                                .arg(uid, rawHex).arg(wType));
            emit cardScanned(uid, rawHex, wType);
            client->write(buildResponse(200, "{\"ok\":true}"));
        }
    }
    else {
        emit logMessage(QString("Requete ignoree : %1 %2").arg(method, path));
        client->write(buildResponse(404, "{\"ok\":false,\"error\":\"endpoint inconnu\"}"));
    }

    client->flush();
    client->disconnectFromHost();
}

// ============================================================================
//  Construction d'une reponse HTTP
// ============================================================================

QByteArray RfidHttpServer::buildResponse(int statusCode, const QString &jsonBody)
{
    QString statusText;
    switch (statusCode) {
    case 200: statusText = "OK";          break;
    case 400: statusText = "Bad Request"; break;
    case 404: statusText = "Not Found";   break;
    default:  statusText = "Unknown";     break;
    }

    QByteArray body = jsonBody.toUtf8();
    QByteArray response;
    response += "HTTP/1.1 " + QByteArray::number(statusCode) + " " + statusText.toUtf8() + "\r\n";
    response += "Content-Type: application/json\r\n";
    response += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    response += "Connection: close\r\n";
    response += "\r\n";
    response += body;
    return response;
}
