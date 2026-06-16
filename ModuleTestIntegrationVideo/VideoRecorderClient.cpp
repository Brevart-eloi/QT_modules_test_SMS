#include "VideoRecorderClient.h"

#include <QTcpSocket>

// ============================================================================
//  Constructeur / Destructeur
// ============================================================================

VideoRecorderClient::VideoRecorderClient(QObject *parent)
    : QObject(parent)
{
}

VideoRecorderClient::~VideoRecorderClient()
{
}

// ============================================================================
//  Configuration
// ============================================================================

void VideoRecorderClient::setServerAddress(const QString &host, quint16 port)
{
    m_host = host;
    m_port = port;
}

// ============================================================================
//  Commandes publiques : debut / fin d'enregistrement
// ============================================================================

void VideoRecorderClient::startRecording()
{
    emit logMessage(QString("Enregistrement video : envoi du message de DEBUT vers %1:%2 ...")
                        .arg(m_host).arg(m_port));

    if (sendCommand(m_startMessage)) {
        m_recording = true;
        emit recordingStarted();
        emit logMessage("-> Message de DEBUT d'enregistrement transmis au service video.");
    }
}

void VideoRecorderClient::stopRecording()
{
    emit logMessage(QString("Enregistrement video : envoi du message de FIN vers %1:%2 ...")
                        .arg(m_host).arg(m_port));

    if (sendCommand(m_stopMessage)) {
        m_recording = false;
        emit recordingStopped();
        emit logMessage("-> Message de FIN d'enregistrement transmis au service video.");
    }
}

// ============================================================================
//  sendCommand — TOUT le "protocole" est concentre ici.
//
//  Tant que E1 n'a pas defini son protocole de pilotage, on se contente
//  d'ouvrir une connexion TCP et d'envoyer le message texte arbitraire.
//  Quand E1 aura termine, c'est CETTE methode (et eventuellement les chaines
//  m_startMessage / m_stopMessage) qu'il faudra adapter — rien d'autre.
//
//  Modele : connexion fraiche par commande (connect -> write -> close).
//  Synchrone avec timeout court : suffisant pour un module de test ; si le
//  service de E1 n'est pas lance, l'erreur est simplement remontee.
// ============================================================================

bool VideoRecorderClient::sendCommand(const QString &message)
{
    QTcpSocket socket;
    socket.connectToHost(m_host, m_port);

    // 1. Connexion
    if (!socket.waitForConnected(m_timeoutMs)) {
        const QString err = QString("connexion impossible au service video (%1:%2) : %3")
                                .arg(m_host).arg(m_port).arg(socket.errorString());
        emit logMessage("ERREUR : " + err);
        emit connectionError(err);
        return false;
    }

    // 2. Envoi du message
    const QByteArray payload = message.toUtf8();
    socket.write(payload);
    if (!socket.waitForBytesWritten(m_timeoutMs)) {
        const QString err = QString("echec d'envoi du message : %1").arg(socket.errorString());
        emit logMessage("ERREUR : " + err);
        emit connectionError(err);
        socket.abort();
        return false;
    }

    // 3. Fermeture propre
    socket.disconnectFromHost();
    if (socket.state() != QAbstractSocket::UnconnectedState)
        socket.waitForDisconnected(m_timeoutMs);

    return true;
}
