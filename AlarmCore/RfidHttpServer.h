#ifndef RFIDHTTPSERVER_H
#define RFIDHTTPSERVER_H

#include "alarmcore_global.h"
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>

/*
 * ============================================================================
 *  RfidHttpServer
 * ============================================================================
 *
 *  Petit serveur HTTP qui écoute sur un port (par défaut 80) et attend
 *  les POST /rfid/scan envoyés par l'Arduino.
 *
 *  Quand un badge est lu, l'Arduino envoie un JSON de la forme :
 *      { "card_id": "abcd1234", "raw_hex": "...", "wiegand_type": 26 }
 *
 *  Cette classe :
 *    - parse le JSON,
 *    - extrait le champ "card_id",
 *    - émet le signal cardScanned(uid) que LaboMonitoring écoute,
 *    - répond 200 OK à l'Arduino.
 *
 *  On reste volontairement minimaliste : pas d'auth, pas de TLS,
 *  juste un parsing basique des requêtes HTTP.
 *
 * ============================================================================
 */

class ALARMCORE_EXPORT RfidHttpServer : public QObject
{
    Q_OBJECT

public:
    explicit RfidHttpServer(QObject *parent = nullptr);
    ~RfidHttpServer();

    // Démarre le serveur sur le port donné (80 par défaut).
    // Retourne true si OK, false si le port est déjà pris par exemple.
    bool start(quint16 port = 80);

    void stop();
    bool isListening() const;

signals:
    // Émis dès qu'un badge a été lu (UID en minuscules hex sans préfixe).
    void cardScanned(const QString &uid);

    // Émis pour le debug : chaque requête reçue, OK ou non.
    void logMessage(const QString &message);

private slots:
    void onNewConnection();
    void onClientReadyRead();
    void onClientDisconnected();

private:
    QTcpServer *tcpServer;

    // Parse une requête HTTP brute, extrait le body JSON, retourne le card_id
    // (chaîne vide si requête mal formée ou pas de card_id).
    QString parseCardIdFromRequest(const QByteArray &rawRequest);

    // Construit une réponse HTTP 200 OK avec un petit JSON
    QByteArray buildResponse(const QString &body);
};

#endif // RFIDHTTPSERVER_H
