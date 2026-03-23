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
//  CONSTRUCTEUR
// ============================================================================

TestRfidLecteur::TestRfidLecteur(QWidget* parent)
    : QWidget(parent),
    modbusClient(nullptr),
    pollTimer(nullptr),
    isConnected(false),
    isSurveillance(false),
    dernierCardId("")
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // ---- Ligne 1 : Connexion ----
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

    // ---- Ligne 2 : Statut + bouton surveillance ----
    QHBoxLayout* statutLayout = new QHBoxLayout();

    labelStatut = new QLabel("Déconnecté");
    labelStatut->setObjectName("labelStatut");
    labelStatut->setStyleSheet("color: red; font-weight: bold; font-size: 13px;");
    statutLayout->addWidget(labelStatut);

    statutLayout->addStretch();

    btnSurveiller = new QPushButton("Surveiller les badges");
    btnSurveiller->setObjectName("btnSurveiller");
    btnSurveiller->setEnabled(false);
    btnSurveiller->setStyleSheet("padding: 8px 20px; font-weight: bold;");
    statutLayout->addWidget(btnSurveiller);

    mainLayout->addLayout(statutLayout);

    // ---- Zone résultat carte ----
    QGroupBox* groupCarte = new QGroupBox("Dernière carte détectée");
    QFormLayout* formCarte = new QFormLayout(groupCarte);

    labelCardId = new QLabel("En attente...");
    labelCardId->setObjectName("labelCardId");
    labelCardId->setStyleSheet("font-size: 22px; font-weight: bold; color: #1a5fb4;");
    labelCardId->setTextInteractionFlags(Qt::TextSelectableByMouse);
    formCarte->addRow("UID :", labelCardId);

    labelTagType = new QLabel("---");
    labelTagType->setObjectName("labelTagType");
    formCarte->addRow("Type :", labelTagType);

    mainLayout->addWidget(groupCarte);

    // ---- Zone log ----
    textEditLog = new QTextEdit();
    textEditLog->setObjectName("textEditLog");
    textEditLog->setReadOnly(true);
    textEditLog->setMaximumHeight(200);
    textEditLog->setStyleSheet("font-family: Consolas, monospace; font-size: 10px;");
    mainLayout->addWidget(textEditLog);

    // ---- Connexions signaux ----
    connect(btnConnecter, &QPushButton::clicked, this, &TestRfidLecteur::onBtnConnecterClicked);
    connect(btnSurveiller, &QPushButton::clicked, this, &TestRfidLecteur::onBtnSurveillerClicked);

    // Timer de surveillance (800ms entre chaque lecture)
    pollTimer = new QTimer(this);
    pollTimer->setInterval(800);
    connect(pollTimer, &QTimer::timeout, this, &TestRfidLecteur::onPollTimerTimeout);

    log("Prêt. Cliquez sur Connecter puis Surveiller les badges.");
}

TestRfidLecteur::~TestRfidLecteur()
{
    if (pollTimer && pollTimer->isActive())
        pollTimer->stop();
    if (modbusClient) {
        modbusClient->close();
        delete modbusClient;
    }
}

// ============================================================================
//  BOUTONS
// ============================================================================

void TestRfidLecteur::onBtnConnecterClicked()
{
    if (!isConnected)
        connecter();
    else
        deconnecter();
}

void TestRfidLecteur::onBtnSurveillerClicked()
{
    if (!isSurveillance)
        demarrerSurveillance();
    else
        arreterSurveillance();
}

// ============================================================================
//  CONNEXION
// ============================================================================

void TestRfidLecteur::connecter()
{
    QString ip = lineEditIp->text().trimmed();
    quint16 port = static_cast<quint16>(spinBoxPort->value());
    quint8  uid = static_cast<quint8>(spinBoxUnitId->value());

    if (ip.isEmpty()) {
        QMessageBox::warning(this, "Erreur", "Entrez une adresse IP.");
        return;
    }

    log(QString("Connexion à %1:%2 ...").arg(ip).arg(port));

    if (modbusClient) {
        modbusClient->close();
        modbusClient->deleteLater();
        modbusClient = nullptr;
    }

    modbusClient = new QModbusTcpClient(ip, port, uid, this);

    // Signaux socket
    connect(modbusClient, &QTcpSocket::connected,
        this, &TestRfidLecteur::onSocketConnected);
    connect(modbusClient, &QTcpSocket::disconnected,
        this, &TestRfidLecteur::onSocketDisconnected);
    connect(modbusClient, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
        this, &TestRfidLecteur::onSocketError);

    // Signal Modbus FC03 (lecture Holding Registers)
    connect(modbusClient, &QModbusTcpClient::onReadMultipleHoldingRegistersSentence,
        this, &TestRfidLecteur::onHoldingRegistersReceived);

    modbusClient->connectToHost();
}

void TestRfidLecteur::deconnecter()
{
    arreterSurveillance();
    if (modbusClient) {
        modbusClient->close();
        modbusClient->deleteLater();
        modbusClient = nullptr;
    }
    isConnected = false;
    updateBoutons();
    labelStatut->setText("Déconnecté");
    labelStatut->setStyleSheet("color: red; font-weight: bold; font-size: 13px;");
    log("Déconnecté.");
}

// ============================================================================
//  SURVEILLANCE (= le polling)
// ============================================================================
//
//  Le "polling" (= surveillance) c'est tout simplement :
//    → Toutes les 800ms, on envoie une requête Modbus FC03
//      pour lire les registres du lecteur RFID.
//    → Quand on reçoit la réponse, on regarde si l'ID de la carte
//      a changé par rapport à la dernière lecture.
//    → Si oui → nouvelle carte détectée !
//
//  Pourquoi on fait ça en boucle ?
//    Parce que le lecteur RFID ne nous "prévient" pas tout seul
//    quand quelqu'un badge. C'est nous qui devons lui demander
//    régulièrement "hey, quelqu'un a badgé ?".
//    C'est ça le polling : demander en boucle.
//
// ============================================================================

void TestRfidLecteur::demarrerSurveillance()
{
    if (!isConnected || !modbusClient) return;

    dernierCardId = "";  // On oublie l'ancienne carte
    isSurveillance = true;
    pollTimer->start();
    updateBoutons();
    log("Surveillance démarrée. Présentez un badge sur le lecteur...");
}

void TestRfidLecteur::arreterSurveillance()
{
    isSurveillance = false;
    if (pollTimer && pollTimer->isActive())
        pollTimer->stop();
    updateBoutons();
    if (isConnected)
        log("Surveillance arrêtée.");
}

void TestRfidLecteur::onPollTimerTimeout()
{
    // À chaque tick du timer, on lit les registres
    lireRegistres();
}

// ============================================================================
//  LECTURE MODBUS
// ============================================================================

void TestRfidLecteur::lireRegistres()
{
    if (!modbusClient || !isConnected) return;

    // On lit 17 registres à partir de l'adresse 0 (0-based).
    //
    // D'après ton dump, l'adressage est 0-based :
    //   Registre  0 = flag newId (HR 1 dans la doc Inveo)
    //   Registre  1 = ID_LEN     (HR 2 dans la doc)
    //   Registre  2 = ID[0]      (HR 3 dans la doc)
    //   ...
    //   Registre 11 = ID[9]      (HR 12 dans la doc)
    //   Registre 12 = Tag type   (HR 13 dans la doc)
    //   Registre 13 = ID_MODEL   (HR 14 dans la doc) → 0x5408 chez toi
    //   Registre 14 = ID_SW      (HR 15 dans la doc) → 0x0078
    //   Registre 15 = ID_HW      (HR 16 dans la doc) → 0x0200
    //   Registre 16 = Type TAG   (HR 17 dans la doc) → 0x0002 = User
    //
    // ATTENTION : la doc Inveo est en 1-based mais le lecteur
    // utilise l'adressage PDU Modbus standard (0-based).

    modbusClient->readMultipleHoldingRegistersFC3(0, 17);
}

// ============================================================================
//  RÉPONSE MODBUS - C'est ici que tout se passe !
// ============================================================================

void TestRfidLecteur::onHoldingRegistersReceived(quint16 startAddress, QVector<quint16> values)
{
    if (startAddress != 0 || values.size() < 17)
        return;

    // ------------------------------------------------------------------
    // Extraction de l'ID de la carte depuis les registres
    // ------------------------------------------------------------------
    //
    // D'après ton dump avec une carte :
    //   [5]=0x00d8  [6]=0x0035  [7]=0x002a  [8]=0x00dc
    //
    // Cela donne l'UID : D8 35 2A DC (4 octets = Mifare Classic)
    //
    // Le registre [1] (ID_LEN) vaut 0 chez toi, ce qui est bizarre.
    // La doc dit qu'il devrait contenir le nombre d'octets de l'ID.
    // Peut-être que ton lecteur ne remplit pas ce champ en mode Autonomic.
    //
    // SOLUTION : on ne se fie PAS à ID_LEN. On regarde directement
    // les registres 2 à 11 (indices dans le vecteur) et on prend
    // tous les octets non-nuls consécutifs comme étant l'ID.
    // ------------------------------------------------------------------

    // Extraire l'ID (registres index 2 à 11 = HR 3 à HR 12)
    QString cardId = extraireCardId(values);

    // Le type de tag est dans le registre index 16 (HR 17 doc)
    quint16 tagType = values[16];

    // ------------------------------------------------------------------
    // Détection d'une nouvelle carte
    // ------------------------------------------------------------------
    // On compare l'ID actuel avec le dernier ID connu.
    // Si c'est différent ET non-vide → nouvelle carte !
    // ------------------------------------------------------------------

    if (!cardId.isEmpty() && cardId != dernierCardId)
    {
        dernierCardId = cardId;
        QString typeStr = tagTypeToString(tagType);

        // Mise à jour de l'affichage
        labelCardId->setText(cardId);
        labelTagType->setText(typeStr);

        log(QString("CARTE DETECTEE : %1  (type: %2)").arg(cardId, typeStr));
    }
}

// ============================================================================
//  EXTRACTION DE L'ID DEPUIS LES REGISTRES
// ============================================================================

QString TestRfidLecteur::extraireCardId(const QVector<quint16>& values)
{
    // Les octets de l'ID sont dans les registres d'index 2 à 11
    // (correspondant à HR 3 - HR 12 dans la doc Inveo).
    // Chaque registre contient 1 octet dans ses bits de poids faible (0x00XX).
    //
    // On prend TOUS les octets de l'index 2 à 11 qui ne sont pas zéro,
    // pour construire l'UID en hexadécimal.
    //
    // Exemple : si values[2..11] = {0, 0, 0, 0xD8, 0x35, 0x2A, 0xDC, 0, 0, 0}
    //           → on prend les index 5,6,7,8 relatifs au vecteur complet
    //           → mais ici c'est l'index 2+3=5 etc.
    //
    // MAIS d'après ton dump, les octets de l'ID commencent à l'index 5
    // du vecteur (pas à l'index 2). Cela signifie que dans TON lecteur,
    // l'ID commence peut-être au registre HR 6 (index 5 en 0-based)
    // au lieu de HR 3 (index 2).
    //
    // Pour être robuste, on scanne les index 2 à 11 et on prend
    // la première séquence d'octets non-nuls consécutifs.

    int debut = -1;
    int fin = -1;

    // Trouver le premier octet non-nul entre index 2 et 11
    for (int i = 2; i <= 11 && i < values.size(); i++)
    {
        quint8 octet = static_cast<quint8>(values[i] & 0xFF);
        if (octet != 0) {
            if (debut == -1) debut = i;
            fin = i;
        }
        else {
            // Si on avait déjà commencé, on s'arrête
            if (debut != -1) break;
        }
    }

    // Si rien trouvé, pas de carte
    if (debut == -1)
        return QString();

    // Construire l'UID hex
    QString uid;
    for (int i = debut; i <= fin; i++) {
        quint8 octet = static_cast<quint8>(values[i] & 0xFF);
        uid += QString("%1").arg(octet, 2, 16, QChar('0')).toUpper();
    }

    return uid;
}

// ============================================================================
//  SOCKET TCP
// ============================================================================

void TestRfidLecteur::onSocketConnected()
{
    isConnected = true;
    updateBoutons();
    labelStatut->setText("Connecté");
    labelStatut->setStyleSheet("color: green; font-weight: bold; font-size: 13px;");
    log(QString("Connecté à %1").arg(lineEditIp->text()));
    log("Cliquez sur 'Surveiller les badges' puis présentez une carte.");
}

void TestRfidLecteur::onSocketDisconnected()
{
    arreterSurveillance();
    isConnected = false;
    updateBoutons();
    labelStatut->setText("Déconnecté");
    labelStatut->setStyleSheet("color: red; font-weight: bold; font-size: 13px;");
    log("Connexion perdue !");
}

void TestRfidLecteur::onSocketError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);
    QString err = modbusClient ? modbusClient->errorString() : "?";
    log(QString("Erreur : %1").arg(err));

    if (!isConnected && modbusClient) {
        modbusClient->deleteLater();
        modbusClient = nullptr;
    }
    updateBoutons();
}

// ============================================================================
//  UTILITAIRES
// ============================================================================

QString TestRfidLecteur::tagTypeToString(quint16 tagType)
{
    switch (tagType) {
    case 0:  return "Aucun";
    case 1:  return "Inconnu (non enregistré)";
    case 2:  return "Utilisateur";
    case 3:  return "Master";
    default: return QString("Code %1").arg(tagType);
    }
}

void TestRfidLecteur::updateBoutons()
{
    btnConnecter->setText(isConnected ? "Déconnecter" : "Connecter");
    lineEditIp->setEnabled(!isConnected);
    spinBoxPort->setEnabled(!isConnected);
    spinBoxUnitId->setEnabled(!isConnected);

    btnSurveiller->setEnabled(isConnected);
    btnSurveiller->setText(isSurveillance ? "Arrêter la surveillance" : "Surveiller les badges");
}

void TestRfidLecteur::log(const QString& message)
{
    if (!textEditLog) return;
    QString ts = QDateTime::currentDateTime().toString("hh:mm:ss");
    textEditLog->append(QString("[%1] %2").arg(ts, message));
}