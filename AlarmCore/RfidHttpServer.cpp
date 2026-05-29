#include "RfidHttpServer.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

// ============================================================================
//  Constructeur / Destructeur
// ============================================================================

RfidHttpServer::RfidHttpServer(QObject *parent)
    : QObject(parent),
      tcpServer(nullptr)
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
    if (tcpServer) {
        stop();
    }

    tcpServer = new QTcpServer(this);
    connect(tcpServer, &QTcpServer::newConnection,
            this, &RfidHttpServer::onNewConnection);

    // Écoute sur toutes les interfaces (Any), pour que l'Arduino puisse
    // se connecter depuis n'importe quelle IP.
    if (!tcpServer->listen(QHostAddress::Any, port)) {
        emit logMessage(QString("Impossible d'ecouter sur le port %1 : %2")
                            .arg(port)
                            .arg(tcpServer->errorString()));
        tcpServer->deleteLater();
        tcpServer = nullptr;
        return false;
    }

    emit logMessage(QString("Serveur HTTP RFID demarre sur le port %1").arg(port));
    return true;
}

void RfidHttpServer::stop()
{
    if (tcpServer) {
        tcpServer->close();
        tcpServer->deleteLater();
        tcpServer = nullptr;
        emit logMessage("Serveur HTTP RFID arrete.");
    }
}

bool RfidHttpServer::isListening() const
{
    return tcpServer && tcpServer->isListening();
}

// ============================================================================
//  Nouvelle connexion entrante
// ============================================================================

void RfidHttpServer::onNewConnection()
{
    while (tcpServer && tcpServer->hasPendingConnections()) {
        QTcpSocket *client = tcpServer->nextPendingConnection();

        connect(client, &QTcpSocket::readyRead,
                this, &RfidHttpServer::onClientReadyRead);
        connect(client, &QTcpSocket::disconnected,
                this, &RfidHttpServer::onClientDisconnected);
    }
}

// ============================================================================
//  Le client (Arduino) a envoyé des données
// ============================================================================

void RfidHttpServer::onClientReadyRead()
{
    QTcpSocket *client = qobject_cast<QTcpSocket*>(sender());
    if (!client) return;

    // ----------------------------------------------------------------
    // Lecture de tout ce qui est disponible.
    // On accumule dans une propriété dynamique du socket pour gérer
    // le cas où la requête arrive en plusieurs paquets TCP.
    // ----------------------------------------------------------------
    QByteArray buffer = client->property("buffer").toByteArray();
    buffer += client->readAll();
    client->setProperty("buffer", buffer);

    // On considère la requête complète quand on a vu \r\n\r\n (fin headers)
    // ET qu'on a reçu le nombre d'octets indiqué par Content-Length.
    int headerEnd = buffer.indexOf("\r\n\r\n");
    if (headerEnd == -1) {
        return; // pas encore tous les headers
    }

    // Extraire Content-Length
    int contentLength = 0;
    QByteArray headers = buffer.left(headerEnd);
    QRegularExpression reCL("Content-Length:\\s*(\\d+)", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch m = reCL.match(headers);
    if (m.hasMatch()) {
        contentLength = m.captured(1).toInt();
    }

    // Vérifier qu'on a bien le body complet
    int bodyStart = headerEnd + 4;
    int bodyReceived = buffer.size() - bodyStart;
    if (bodyReceived < contentLength) {
        return; // attendre la suite
    }

    // ----------------------------------------------------------------
    // Requête complète : on parse et on émet le signal
    // ----------------------------------------------------------------
    QString uid = parseCardIdFromRequest(buffer);

    if (!uid.isEmpty()) {
        emit logMessage(QString("Badge recu de l'Arduino : %1").arg(uid));
        emit cardScanned(uid);
    } else {
        emit logMessage("Requete recue mais card_id introuvable.");
    }

    // ----------------------------------------------------------------
    // Réponse HTTP à l'Arduino
    // ----------------------------------------------------------------
    QJsonObject response;
    response["status"] = uid.isEmpty() ? "error" : "ok";
    response["card_id"] = uid;
    QByteArray jsonResp = QJsonDocument(response).toJson(QJsonDocument::Compact);

    client->write(buildResponse(QString::fromUtf8(jsonResp)));
    client->flush();
    client->disconnectFromHost();
}

// ============================================================================
//  Le client a fini : on nettoie
// ============================================================================

void RfidHttpServer::onClientDisconnected()
{
    QTcpSocket *client = qobject_cast<QTcpSocket*>(sender());
    if (client) {
        client->deleteLater();
    }
}

// ============================================================================
//  Parsing : extraire card_id du body JSON
// ============================================================================

QString RfidHttpServer::parseCardIdFromRequest(const QByteArray &rawRequest)
{
    // Cherche le début du body (après \r\n\r\n)
    int bodyStart = rawRequest.indexOf("\r\n\r\n");
    if (bodyStart == -1) return QString();

    QByteArray body = rawRequest.mid(bodyStart + 4);

    // Parsing JSON
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(body, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return QString();
    }

    QJsonObject obj = doc.object();
    if (!obj.contains("card_id")) {
        return QString();
    }

    // On normalise l'UID en MAJUSCULES sans préfixe 0x
    QString uid = obj.value("card_id").toString().trimmed();
    if (uid.startsWith("0x", Qt::CaseInsensitive)) {
        uid = uid.mid(2);
    }
    return uid.toUpper();
}

// ============================================================================
//  Construction de la réponse HTTP 200
// ============================================================================

QByteArray RfidHttpServer::buildResponse(const QString &body)
{
    QByteArray bodyBytes = body.toUtf8();
    QByteArray response;
    response += "HTTP/1.1 200 OK\r\n";
    response += "Content-Type: application/json\r\n";
    response += "Content-Length: " + QByteArray::number(bodyBytes.size()) + "\r\n";
    response += "Connection: close\r\n";
    response += "\r\n";
    response += bodyBytes;
    return response;
}
