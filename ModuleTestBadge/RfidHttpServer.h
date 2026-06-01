#ifndef RFIDHTTPSERVER_H
#define RFIDHTTPSERVER_H

#include <QObject>
#include <QTcpServer>

class QTcpSocket;


class RfidHttpServer : public QObject
{
    Q_OBJECT

public:
    explicit RfidHttpServer(QObject *parent = nullptr);
    ~RfidHttpServer();

    bool start(quint16 port = 8080);
    void stop();
    bool isListening() const;

signals:
    // Emis a chaque badge lu.
    void cardScanned(const QString &uid, const QString &rawHex, int wiegandType);

    // Logs (chaque requete recue, erreurs, etc.).
    void logMessage(const QString &message);

private slots:
    void onNewConnection();
    void onClientReadyRead();
    void onClientDisconnected();

private:
    QTcpServer *m_tcpServer;

    void       handleRequest(QTcpSocket *client, const QByteArray &request);
    QByteArray buildResponse(int statusCode, const QString &jsonBody);
};

#endif // RFIDHTTPSERVER_H
