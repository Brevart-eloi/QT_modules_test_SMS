#ifndef TESTRFIDLECTEUR_H
#define TESTRFIDLECTEUR_H

#include <QWidget>
#include <QTimer>
#include <QVector>
#include "qmodbustcpclient.h"
#include "ui_TestRfidLecteur.h"


// Forward declarations des widgets (créés dans le .ui)
class QPushButton;
class QLineEdit;
class QLabel;
class QTextEdit;
class QSpinBox;

class TestRfidLecteur : public QWidget
{
    Q_OBJECT

public:
    explicit TestRfidLecteur(QWidget *parent = nullptr);
    ~TestRfidLecteur();

private slots:
    // --- Boutons UI ---
    void onBtnConnecterClicked();
    void onBtnPollingClicked();
    void onBtnResetFlagClicked();

    // --- Timer de polling ---
    void onPollTimerTimeout();

    // --- Réponses Modbus (signaux de QModbusTcpClient) ---
    void onHoldingRegistersReceived(quint16 startAddress, QVector<quint16> values);
    void onWriteSingleWordDone(bool writeSuccess, quint16 wordAddress, quint16 wordValue);

    // --- Etat de la connexion TCP ---
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError socketError);

private:
    // ======================================================
    // WIDGETS UI (noms = objectName dans le .ui)
    // ======================================================
    QLineEdit   *lineEditIp;          // Adresse IP du lecteur
    QSpinBox    *spinBoxPort;         // Port Modbus TCP (502)
    QSpinBox    *spinBoxUnitId;       // Unit ID Modbus (1)
    QPushButton *btnConnecter;        // Bouton Connecter / Déconnecter
    QPushButton *btnPolling;          // Bouton Démarrer / Arrêter le polling
    QPushButton *btnResetFlag;        // Bouton Reset manuel du flag de lecture

    QLabel      *labelStatutConnexion;  // Affiche "Connecté" / "Déconnecté"
    QLabel      *labelCardId;           // Affiche l'UID de la dernière carte lue
    QLabel      *labelTagType;          // Affiche le type de tag (inconnu/user/master)
    QLabel      *labelIdLen;            // Affiche la longueur de l'ID

    QTextEdit   *textEditLog;           // Zone de log

    // ======================================================
    // LOGIQUE METIER
    // ======================================================
    QModbusTcpClient *modbusClient;
    QTimer           *pollTimer;

    bool isConnected;
    bool isPolling;

    // --- Adresses Modbus Holding Registers (doc Inveo, 1-based) ---
    // IMPORTANT : si la lecture ne fonctionne pas, essayer de soustraire 1
    //             à toutes ces adresses (convention 0-based du protocole Modbus).
    static const quint16 HR_NEW_ID_FLAG   = 1;   // Flag nouvelle lecture (1 = nouveau tag lu)
    static const quint16 HR_ID_LEN        = 2;   // Longueur de l'ID en octets
    static const quint16 HR_ID_START      = 3;   // Début des octets de l'ID (3 à 12)
    static const quint16 HR_TAG_TYPE      = 17;  // Type de tag lu (0=aucun, 1=inconnu, 2=user, 3=master)

    // Nombre de registres à lire en un seul bloc (de l'adresse 1 à 17)
    static const quint16 NB_REGISTERS_TO_READ = 17;

    // --- Méthodes internes ---
    void connecterAuLecteur();
    void deconnecterDuLecteur();
    void demarrerPolling();
    void arreterPolling();
    void lireRegistres();
    void resetReadFlag();
    void traiterDonneesCartes(const QVector<quint16> &values);
    QString formaterCardId(const QVector<quint16> &values, int startIndex, int idLen);
    QString tagTypeToString(quint16 tagType);
    void updateEtatBoutons();
    void log(const QString &message);
};

#endif // TESTRFIDLECTEUR_H