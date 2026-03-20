#include "TestRfidLecteur.h"

#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QTextEdit>
#include <QSpinBox>
#include <QDateTime>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>

// ============================================================================
// Si tu utilises le .ui avec Qt Designer, remplace le contenu du constructeur
// par :   ui->setupUi(this);
// et récupère les widgets via :   lineEditIp = ui->lineEditIp;   etc.
//
// Sinon, ce constructeur récupère les widgets par leur objectName défini
// dans le .ui grâce à findChild<>().
// ============================================================================

TestRfidLecteur::TestRfidLecteur(QWidget* parent)
    : QWidget(parent),
    modbusClient(nullptr),
    pollTimer(nullptr),
    isConnected(false),
    isPolling(false)
{
    // ------------------------------------------------------------------
    // Récupération des widgets depuis le .ui (par objectName)
    // ------------------------------------------------------------------
    // IMPORTANT : les noms ci-dessous doivent correspondre EXACTEMENT
    //             aux objectName que tu définis dans Qt Designer.
    // ------------------------------------------------------------------

    // Comme le .ui n'est pas encore créé, on ne peut pas faire setupUi().
    // À la place, le code suivant sera utilisé APRES que tu aies
    // fait setupUi(this) dans le constructeur.
    //
    // Pour le moment, je laisse des commentaires pour guider l'intégration.
    // Quand ton .ui est prêt, décommente setupUi(this) et commente les findChild.

    // === OPTION A : si tu utilises un .ui avec la classe Ui:: ===
    // #include "ui_TestRfidLecteur.h"
    // Ui::TestRfidLecteurClass ui;
    // ui.setupUi(this);
    // lineEditIp             = ui.lineEditIp;
    // spinBoxPort            = ui.spinBoxPort;
    // spinBoxUnitId          = ui.spinBoxUnitId;
    // btnConnecter           = ui.btnConnecter;
    // btnPolling             = ui.btnPolling;
    // btnResetFlag           = ui.btnResetFlag;
    // labelStatutConnexion   = ui.labelStatutConnexion;
    // labelCardId            = ui.labelCardId;
    // labelTagType           = ui.labelTagType;
    // labelIdLen             = ui.labelIdLen;
    // textEditLog            = ui.textEditLog;

    // === OPTION B : récupération par objectName (après setupUi) ===
    // Décommente ces lignes après avoir fait setupUi(this) :
    /*
    lineEditIp             = findChild<QLineEdit*>("lineEditIp");
    spinBoxPort            = findChild<QSpinBox*>("spinBoxPort");
    spinBoxUnitId          = findChild<QSpinBox*>("spinBoxUnitId");
    btnConnecter           = findChild<QPushButton*>("btnConnecter");
    btnPolling             = findChild<QPushButton*>("btnPolling");
    btnResetFlag           = findChild<QPushButton*>("btnResetFlag");
    labelStatutConnexion   = findChild<QLabel*>("labelStatutConnexion");
    labelCardId            = findChild<QLabel*>("labelCardId");
    labelTagType           = findChild<QLabel*>("labelTagType");
    labelIdLen             = findChild<QLabel*>("labelIdLen");
    textEditLog            = findChild<QTextEdit*>("textEditLog");
    */

    // === OPTION C : création en code (pour tester sans .ui) ===
    // Tu peux supprimer tout ce bloc quand ton .ui est prêt.
    {
        QVBoxLayout* mainLayout = new QVBoxLayout(this);

        // --- Zone connexion ---
        QHBoxLayout* connLayout = new QHBoxLayout();
        connLayout->addWidget(new QLabel("IP :"));
        lineEditIp = new QLineEdit("172.29.18.200");
        lineEditIp->setObjectName("lineEditIp");
        connLayout->addWidget(lineEditIp);

        connLayout->addWidget(new QLabel("Port :"));
        spinBoxPort = new QSpinBox();
        spinBoxPort->setObjectName("spinBoxPort");
        spinBoxPort->setRange(1, 65535);
        spinBoxPort->setValue(502);
        connLayout->addWidget(spinBoxPort);

        connLayout->addWidget(new QLabel("Unit ID :"));
        spinBoxUnitId = new QSpinBox();
        spinBoxUnitId->setObjectName("spinBoxUnitId");
        spinBoxUnitId->setRange(0, 255);
        spinBoxUnitId->setValue(1);
        connLayout->addWidget(spinBoxUnitId);

        btnConnecter = new QPushButton("Connecter");
        btnConnecter->setObjectName("btnConnecter");
        connLayout->addWidget(btnConnecter);

        mainLayout->addLayout(connLayout);

        // --- Zone statut ---
        QHBoxLayout* statutLayout = new QHBoxLayout();
        statutLayout->addWidget(new QLabel("Statut :"));
        labelStatutConnexion = new QLabel("Déconnecté");
        labelStatutConnexion->setObjectName("labelStatutConnexion");
        labelStatutConnexion->setStyleSheet("color: red; font-weight: bold;");
        statutLayout->addWidget(labelStatutConnexion);
        statutLayout->addStretch();
        mainLayout->addLayout(statutLayout);

        // --- Zone polling ---
        QHBoxLayout* pollLayout = new QHBoxLayout();
        btnPolling = new QPushButton("Démarrer le polling");
        btnPolling->setObjectName("btnPolling");
        btnPolling->setEnabled(false);
        pollLayout->addWidget(btnPolling);

        btnResetFlag = new QPushButton("Reset Flag Lecture");
        btnResetFlag->setObjectName("btnResetFlag");
        btnResetFlag->setEnabled(false);
        pollLayout->addWidget(btnResetFlag);
        pollLayout->addStretch();
        mainLayout->addLayout(pollLayout);

        // --- Zone résultat carte ---
        QGroupBox* groupResultat = new QGroupBox("Dernière carte lue");
        QFormLayout* formLayout = new QFormLayout(groupResultat);

        labelCardId = new QLabel("---");
        labelCardId->setObjectName("labelCardId");
        labelCardId->setStyleSheet("font-size: 18px; font-weight: bold; color: #2060c0;");
        formLayout->addRow("UID Carte :", labelCardId);

        labelIdLen = new QLabel("---");
        labelIdLen->setObjectName("labelIdLen");
        formLayout->addRow("Longueur ID :", labelIdLen);

        labelTagType = new QLabel("---");
        labelTagType->setObjectName("labelTagType");
        formLayout->addRow("Type de tag :", labelTagType);

        mainLayout->addWidget(groupResultat);

        // --- Zone log ---
        mainLayout->addWidget(new QLabel("Journal :"));
        textEditLog = new QTextEdit();
        textEditLog->setObjectName("textEditLog");
        textEditLog->setReadOnly(true);
        mainLayout->addWidget(textEditLog);
    }

    // ------------------------------------------------------------------
    // Connexion des signaux des boutons
    // ------------------------------------------------------------------
    connect(btnConnecter, &QPushButton::clicked, this, &TestRfidLecteur::onBtnConnecterClicked);
    connect(btnPolling, &QPushButton::clicked, this, &TestRfidLecteur::onBtnPollingClicked);
    connect(btnResetFlag, &QPushButton::clicked, this, &TestRfidLecteur::onBtnResetFlagClicked);

    // ------------------------------------------------------------------
    // Timer de polling (non démarré)
    // ------------------------------------------------------------------
    pollTimer = new QTimer(this);
    pollTimer->setInterval(500); // Polling toutes les 500ms
    connect(pollTimer, &QTimer::timeout, this, &TestRfidLecteur::onPollTimerTimeout);

    log("Application prête. Connectez-vous au lecteur RFID.");
}

TestRfidLecteur::~TestRfidLecteur()
{
    if (pollTimer && pollTimer->isActive()) {
        pollTimer->stop();
    }
    if (modbusClient) {
        modbusClient->close();
        delete modbusClient;
        modbusClient = nullptr;
    }
}

// ============================================================================
//  SLOTS BOUTONS
// ============================================================================

void TestRfidLecteur::onBtnConnecterClicked()
{
    if (!isConnected) {
        connecterAuLecteur();
    }
    else {
        deconnecterDuLecteur();
    }
}

void TestRfidLecteur::onBtnPollingClicked()
{
    if (!isPolling) {
        demarrerPolling();
    }
    else {
        arreterPolling();
    }
}

void TestRfidLecteur::onBtnResetFlagClicked()
{
    if (modbusClient && isConnected) {
        resetReadFlag();
    }
}

// ============================================================================
//  CONNEXION / DÉCONNEXION
// ============================================================================

void TestRfidLecteur::connecterAuLecteur()
{
    QString ip = lineEditIp->text().trimmed();
    quint16 port = static_cast<quint16>(spinBoxPort->value());
    quint8  unitId = static_cast<quint8>(spinBoxUnitId->value());

    if (ip.isEmpty()) {
        QMessageBox::warning(this, "Erreur", "Veuillez entrer une adresse IP.");
        return;
    }

    log(QString("Connexion à %1:%2 (Unit ID: %3)...").arg(ip).arg(port).arg(unitId));

    // Nettoyage de l'ancien client si existant
    if (modbusClient) {
        modbusClient->close();
        modbusClient->deleteLater();
        modbusClient = nullptr;
    }

    // Création du client Modbus TCP (classe du projet AlarmCore)
    modbusClient = new QModbusTcpClient(ip, port, unitId, this);

    // Connexion des signaux de QTcpSocket (classe parente de QModbusTcpClient)
    connect(modbusClient, &QTcpSocket::connected,
        this, &TestRfidLecteur::onSocketConnected);
    connect(modbusClient, &QTcpSocket::disconnected,
        this, &TestRfidLecteur::onSocketDisconnected);
    connect(modbusClient, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
        this, &TestRfidLecteur::onSocketError);

    // Connexion des signaux Modbus
    // FC03 : lecture de Holding Registers
    connect(modbusClient, &QModbusTcpClient::onReadMultipleHoldingRegistersSentence,
        this, &TestRfidLecteur::onHoldingRegistersReceived);

    // FC06 : écriture d'un mot (pour reset du flag)
    connect(modbusClient, &QModbusTcpClient::onWriteSingleWordSentence,
        this, &TestRfidLecteur::onWriteSingleWordDone);

    // Lancement de la connexion TCP
    modbusClient->connectToHost();
}

void TestRfidLecteur::deconnecterDuLecteur()
{
    arreterPolling();

    if (modbusClient) {
        modbusClient->close();
        modbusClient->deleteLater();
        modbusClient = nullptr;
    }

    isConnected = false;
    updateEtatBoutons();
    labelStatutConnexion->setText("Déconnecté");
    labelStatutConnexion->setStyleSheet("color: red; font-weight: bold;");
    log("Déconnecté du lecteur.");
}

// ============================================================================
//  POLLING
// ============================================================================

void TestRfidLecteur::demarrerPolling()
{
    if (!isConnected || !modbusClient) return;

    isPolling = true;
    pollTimer->start();
    updateEtatBoutons();
    log("Polling démarré (intervalle : 500ms). Présentez un badge...");
}

void TestRfidLecteur::arreterPolling()
{
    isPolling = false;
    if (pollTimer->isActive()) {
        pollTimer->stop();
    }
    updateEtatBoutons();
    log("Polling arrêté.");
}

void TestRfidLecteur::onPollTimerTimeout()
{
    lireRegistres();
}

// ============================================================================
//  LECTURE / ÉCRITURE MODBUS
// ============================================================================

void TestRfidLecteur::lireRegistres()
{
    if (!modbusClient || !isConnected) return;

    // Lecture des Holding Registers de l'adresse HR_NEW_ID_FLAG (1) sur 17 registres.
    // Cela couvre : flag(1), idLen(2), id[0..9](3-12), ..., tagType(17).
    //
    // NOTE IMPORTANTE SUR L'ADRESSAGE :
    // La doc Inveo utilise des adresses 1-based (registre 1, 2, 3...).
    // Le protocole Modbus standard utilise des adresses 0-based dans la trame PDU.
    // Si la classe QModbusTcpClient utilise l'adressage 0-based, il faudra
    // remplacer HR_NEW_ID_FLAG par (HR_NEW_ID_FLAG - 1), soit 0.
    // Essayez d'abord tel quel ; si ça ne fonctionne pas, soustrayez 1.

    modbusClient->readMultipleHoldingRegistersFC3(HR_NEW_ID_FLAG, NB_REGISTERS_TO_READ);
}

void TestRfidLecteur::resetReadFlag()
{
    if (!modbusClient || !isConnected) return;

    log("Reset du flag de lecture (HR 1 = 0)...");

    // Écriture de 0 dans le Holding Register 1 (flag de nouvelle lecture)
    // Cela permet au lecteur de détecter la prochaine carte.
    modbusClient->writeSingleWordFC6(HR_NEW_ID_FLAG, 0);
}

// ============================================================================
//  RÉPONSES MODBUS
// ============================================================================

void TestRfidLecteur::onHoldingRegistersReceived(quint16 startAddress, QVector<quint16> values)
{
    // On vérifie qu'on a bien reçu les registres attendus
    if (values.size() < NB_REGISTERS_TO_READ) {
        // Réponse trop courte, on ignore silencieusement
        return;
    }

    // ----------------------------------------------------------------
    // Mapping des indices dans le vecteur :
    //   values[0]  = HR 1  = newId flag
    //   values[1]  = HR 2  = ID_LEN (longueur de l'ID en octets)
    //   values[2]  = HR 3  = ID[0]
    //   values[3]  = HR 4  = ID[1]
    //   values[4]  = HR 5  = ID[2]
    //   values[5]  = HR 6  = ID[3]
    //   values[6]  = HR 7  = ID[4]
    //   values[7]  = HR 8  = ID[5]
    //   values[8]  = HR 9  = ID[6]
    //   values[9]  = HR 10 = ID[7]
    //   values[10] = HR 11 = ID[8]
    //   values[11] = HR 12 = ID[9]
    //   ...
    //   values[16] = HR 17 = Type de tag lu
    // ----------------------------------------------------------------

    quint16 newIdFlag = values[0];  // 1 = nouveau tag détecté
    quint16 idLen = values[1];  // nombre d'octets de l'ID
    quint16 tagType = values[16]; // 0=aucun, 1=inconnu, 2=user, 3=master

    if (newIdFlag == 1) {
        // ============================================================
        // NOUVELLE CARTE DÉTECTÉE !
        // ============================================================

        // Sécurité sur la longueur
        if (idLen == 0 || idLen > 10) {
            log(QString("Carte détectée mais longueur ID invalide : %1").arg(idLen));
            resetReadFlag();
            return;
        }

        // Construction du UID en hexadécimal
        // Les octets de l'ID sont dans values[2] à values[2 + idLen - 1]
        QString cardId = formaterCardId(values, 2, idLen);
        QString typeStr = tagTypeToString(tagType);

        // Mise à jour de l'affichage
        labelCardId->setText(cardId);
        labelIdLen->setText(QString("%1 octets").arg(idLen));
        labelTagType->setText(typeStr);

        log(QString("CARTE LUE : UID = %1 | Longueur = %2 | Type = %3")
            .arg(cardId)
            .arg(idLen)
            .arg(typeStr));

        // Reset du flag pour permettre la lecture suivante
        resetReadFlag();
    }
    // Si newIdFlag == 0, pas de nouvelle carte → on ne fait rien, le timer relancera la lecture.
}

void TestRfidLecteur::onWriteSingleWordDone(bool writeSuccess, quint16 wordAddress, quint16 wordValue)
{
    if (wordAddress == HR_NEW_ID_FLAG) {
        if (writeSuccess) {
            log("Flag de lecture remis à zéro. Prêt pour la prochaine carte.");
        }
        else {
            log("ERREUR : échec du reset du flag de lecture !");
        }
    }
}

// ============================================================================
//  ÉVÉNEMENTS SOCKET TCP
// ============================================================================

void TestRfidLecteur::onSocketConnected()
{
    isConnected = true;
    updateEtatBoutons();
    labelStatutConnexion->setText("Connecté");
    labelStatutConnexion->setStyleSheet("color: green; font-weight: bold;");
    log(QString("Connecté au lecteur RFID (%1:%2).")
        .arg(lineEditIp->text())
        .arg(spinBoxPort->value()));
}

void TestRfidLecteur::onSocketDisconnected()
{
    arreterPolling();
    isConnected = false;
    updateEtatBoutons();
    labelStatutConnexion->setText("Déconnecté");
    labelStatutConnexion->setStyleSheet("color: red; font-weight: bold;");
    log("Connexion TCP perdue.");
}

void TestRfidLecteur::onSocketError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);

    QString errMsg = modbusClient ? modbusClient->errorString() : "Erreur inconnue";
    log(QString("ERREUR SOCKET : %1").arg(errMsg));

    // Si on n'était pas encore connecté, on nettoie
    if (!isConnected && modbusClient) {
        modbusClient->deleteLater();
        modbusClient = nullptr;
    }
    updateEtatBoutons();
}

// ============================================================================
//  UTILITAIRES
// ============================================================================

QString TestRfidLecteur::formaterCardId(const QVector<quint16>& values, int startIndex, int idLen)
{
    // Chaque registre contient 1 octet de l'ID (dans les 8 bits de poids faible).
    // On les formate en hexadécimal, 2 caractères par octet.
    QString result;
    for (int i = 0; i < idLen && (startIndex + i) < values.size(); i++) {
        quint8 byte = static_cast<quint8>(values[startIndex + i] & 0xFF);
        result += QString("%1").arg(byte, 2, 16, QChar('0')).toUpper();
    }
    return result;
}

QString TestRfidLecteur::tagTypeToString(quint16 tagType)
{
    switch (tagType) {
    case 0:  return "Aucun";
    case 1:  return "Inconnu (non enregistré)";
    case 2:  return "Utilisateur";
    case 3:  return "Master";
    default: return QString("Inconnu (%1)").arg(tagType);
    }
}

void TestRfidLecteur::updateEtatBoutons()
{
    // Bouton connexion
    btnConnecter->setText(isConnected ? "Déconnecter" : "Connecter");

    // Champs de connexion : désactivés quand connecté
    lineEditIp->setEnabled(!isConnected);
    spinBoxPort->setEnabled(!isConnected);
    spinBoxUnitId->setEnabled(!isConnected);

    // Boutons polling et reset : actifs seulement si connecté
    btnPolling->setEnabled(isConnected);
    btnPolling->setText(isPolling ? "Arrêter le polling" : "Démarrer le polling");
    btnResetFlag->setEnabled(isConnected);
}

void TestRfidLecteur::log(const QString& message)
{
    if (!textEditLog) return;

    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    textEditLog->append(QString("[%1] %2").arg(timestamp, message));
}