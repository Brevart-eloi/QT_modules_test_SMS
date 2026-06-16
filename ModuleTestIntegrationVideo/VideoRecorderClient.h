#ifndef VIDEORECORDERCLIENT_H
#define VIDEORECORDERCLIENT_H

#include <QObject>
#include <QString>

class QTcpSocket;

/*
 * ============================================================================
 *  VideoRecorderClient  —  Client TCP/IP d'enregistrement video (Etudiant 2)
 * ============================================================================
 *
 *  CONTEXTE (sujet E6, Etudiant 2) :
 *    << Envoi des signaux de debut et de fin d'enregistrement au service
 *       surveillance (selon le protocole de pilotage defini par E1). Si E1
 *       n'a pas termine la specification de son protocole, preparer une classe
 *       permettant de se connecter en TCP/IP et d'envoyer des messages de
 *       demarrage et d'arret arbitraires que vous remplacerez quand E1 aura
 *       termine son travail. >>
 *
 *  /!\ PROTOCOLE NON ENCORE DEFINI PAR E1 :
 *    Cette classe envoie pour l'instant des messages texte ARBITRAIRES
 *    ("START_RECORDING" / "STOP_RECORDING"). Quand E1 aura publie son
 *    protocole de pilotage, il suffira de :
 *      1. remplacer les messages via setStartMessage() / setStopMessage()
 *         OU
 *      2. reecrire la methode privee sendCommand()  (un seul endroit a changer)
 *    Le reste du code (connexion TCP, signaux, IHM) reste inchange.
 *
 *  MODELE DE CONNEXION :
 *    Une connexion TCP fraiche est ouverte pour CHAQUE commande
 *    (connect -> envoi -> close). Simple, sans etat, robuste : si le service
 *    de E1 n'est pas encore lance, l'erreur est remontee via connectionError().
 *
 *  TEST SANS LE SERVICE DE E1 :
 *    Lancer un serveur d'ecoute quelconque pour voir arriver les messages, ex :
 *      ncat -lk 9000          (ou)   python -m ... / un netcat
 *    puis pointer le client sur 127.0.0.1:9000.
 *
 * ============================================================================
 */

class VideoRecorderClient : public QObject
{
    Q_OBJECT

public:
    explicit VideoRecorderClient(QObject *parent = nullptr);
    ~VideoRecorderClient();

    // ── Configuration de la cible (service video de E1) ──
    void setServerAddress(const QString &host, quint16 port);
    QString host() const { return m_host; }
    quint16 port() const { return m_port; }

    // ── Messages arbitraires (A REMPLACER quand E1 aura defini le protocole) ──
    void setStartMessage(const QString &msg) { m_startMessage = msg; }
    void setStopMessage (const QString &msg) { m_stopMessage  = msg; }
    QString startMessage() const { return m_startMessage; }
    QString stopMessage()  const { return m_stopMessage;  }

    // Delai max d'attente de connexion / d'envoi (ms)
    void setTimeout(int ms) { m_timeoutMs = ms; }

    bool isRecording() const { return m_recording; }

public slots:
    // Demarre l'enregistrement video (envoie le message de DEBUT a E1)
    void startRecording();
    // Arrete l'enregistrement video (envoie le message de FIN a E1)
    void stopRecording();

signals:
    void logMessage(const QString &message);
    void recordingStarted();                 // message de debut envoye avec succes
    void recordingStopped();                 // message de fin envoye avec succes
    void connectionError(const QString &error);

private:
    // Ouvre une connexion TCP, envoie le message, ferme. Retourne true si OK.
    // /!\ C'est ICI que tout le "protocole" est concentre : seul point a
    //     reecrire quand E1 aura termine sa specification.
    bool sendCommand(const QString &message);

    QString m_host          = "127.0.0.1";
    quint16 m_port          = 9000;
    QString m_startMessage  = "START_RECORDING\n";   // arbitraire — a remplacer
    QString m_stopMessage   = "STOP_RECORDING\n";    // arbitraire — a remplacer
    int     m_timeoutMs     = 2000;
    bool    m_recording     = false;
};

#endif // VIDEORECORDERCLIENT_H
