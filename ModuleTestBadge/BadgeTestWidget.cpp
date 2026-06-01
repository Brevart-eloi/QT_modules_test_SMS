#include "BadgeTestWidget.h"

#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QDateTime>

BadgeTestWidget::BadgeTestWidget(QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle("Verification droits d'acces badges RFID");
    resize(700, 560);

    QVBoxLayout *main = new QVBoxLayout(this);

    //  Configuration 
    QGroupBox   *grpConfig  = new QGroupBox("Configuration");
    QFormLayout *formConf   = new QFormLayout(grpConfig);

    spinPort = new QSpinBox();
    spinPort->setRange(1, 65535);
    spinPort->setValue(8080);
    spinPort->setToolTip("Port sur lequel ce module ecoute les POST /rfid/scan de l'Arduino.\n"
                         "L'Arduino est cable en dur sur le port 80.\n"
                         "Ecouter < 1024 requiert d'etre Administrateur sous Windows.");

    editApiUrl = new QLineEdit("http://172.29.19.193");
    editApiUrl->setToolTip("URL de base du backend.\n"
                           "L'endpoint appele sera : {URL}/api/rfid/check/{uid}");

    formConf->addRow("Port d'ecoute HTTP arduino :", spinPort);
    formConf->addRow("URL Back :", editApiUrl);

    main->addWidget(grpConfig);

    //  Serveur start / stop 
    QHBoxLayout *srvLayout = new QHBoxLayout();
    btnStart = new QPushButton("Demarrer le serveur");
    btnStop  = new QPushButton("Arreter");
    btnStop->setEnabled(false);
    srvLayout->addWidget(btnStart);
    srvLayout->addWidget(btnStop);
    srvLayout->addStretch();
    main->addLayout(srvLayout);

    //  Simulation manuelle 
    QGroupBox   *grpSim   = new QGroupBox("Simulation d'un badge");
    QHBoxLayout *simLayout = new QHBoxLayout(grpSim);
    editSimUid  = new QLineEdit();
    editSimUid->setPlaceholderText("UID hex");
    btnSimulate = new QPushButton("Verifier ce badge");
    simLayout->addWidget(editSimUid);
    simLayout->addWidget(btnSimulate);
    main->addWidget(grpSim);

    //  Resultat du dernier badge 
    QGroupBox   *grpResult = new QGroupBox("Dernier badge lu");
    QFormLayout *formRes   = new QFormLayout(grpResult);

    labelDecision = new QLabel("---");
    labelDecision->setStyleSheet("font-weight: bold; font-size: 18px; color: gray;");
    labelName   = new QLabel("---");
    labelReason = new QLabel("---");

    formRes->addRow("Decision :", labelDecision);
    formRes->addRow("Porteur :",  labelName);
    formRes->addRow("Statut :",   labelReason);
    main->addWidget(grpResult);

    //  Journal 
    main->addWidget(new QLabel("Journal :"));
    textLog = new QTextEdit();
    textLog->setReadOnly(true);
    textLog->setStyleSheet("font-family: Consolas, monospace; font-size: 11px;");
    main->addWidget(textLog);

    //  Connexions signaux / slots 
    connect(btnStart,    &QPushButton::clicked, this, &BadgeTestWidget::onBtnStartClicked);
    connect(btnStop,     &QPushButton::clicked, this, &BadgeTestWidget::onBtnStopClicked);
    connect(btnSimulate, &QPushButton::clicked, this, &BadgeTestWidget::onBtnSimulateClicked);

    // Serveur HTTP : reception des scans Arduino
    connect(&m_server, &RfidHttpServer::logMessage,  this, &BadgeTestWidget::onLog);
    connect(&m_server, &RfidHttpServer::cardScanned, this, &BadgeTestWidget::onCardScanned);

    // Client API adjudicator : reception des resultats
    connect(&m_client, &AdjudicatorClient::logMessage,   this, &BadgeTestWidget::onLog);
    connect(&m_client, &AdjudicatorClient::badgeChecked, this, &BadgeTestWidget::onBadgeChecked);
}


//  Serveur start / stop


void BadgeTestWidget::onBtnStartClicked()
{
    // Met a jour l'URL de l'adjudicator avant de demarrer
    m_client.setApiBaseUrl(editApiUrl->text().trimmed());

    if (m_server.start(static_cast<quint16>(spinPort->value()))) {
        setControlsRunning(true);
    }
}

void BadgeTestWidget::onBtnStopClicked()
{
    m_server.stop();
    setControlsRunning(false);
}

//  Simulation manuelle 

void BadgeTestWidget::onBtnSimulateClicked()
{
    const QString uid = editSimUid->text().trimmed();
    if (uid.isEmpty()) {
        log("Simulation : UID vide.");
        return;
    }

    m_client.setApiBaseUrl(editApiUrl->text().trimmed());

    log(QString("Simulation : interrogation de l'adjudicator pour UID = %1").arg(uid));
    m_client.checkBadge(uid);
}

//  Reception d'un badge depuis l'Arduino (via RfidHttpServer)

void BadgeTestWidget::onCardScanned(const QString &uid, const QString &rawHex, int wiegandType)
{
    Q_UNUSED(rawHex);
    Q_UNUSED(wiegandType);

    log(QString("Badge scanne par l'Arduino : %1 → interrogation adjudicator...").arg(uid));
    m_client.checkBadge(uid);
}

//  Resultat de la verification (depuis AdjudicatorClient)

void BadgeTestWidget::onBadgeChecked(const AccessResult &result)
{
    if (result.granted) {
        labelDecision->setText("ACCES ACCORDE");
        labelDecision->setStyleSheet("font-weight: bold; font-size: 18px; color: green;");
    } else {
        labelDecision->setText("ACCES REFUSE");
        labelDecision->setStyleSheet("font-weight: bold; font-size: 18px; color: red;");
    }

    labelName->setText(result.userName.isEmpty() ? "(aucun)" : result.userName);
    labelReason->setText(result.reason.isEmpty() ? "---" : result.reason);

    const QString statusLine = result.granted
        ? QString("ACCORDE — %1 (%2)").arg(result.userName, result.uid)
        : QString("REFUSE  — %1 [%2]").arg(result.uid, result.reason);

    log(QString(">>> Resultat : %1").arg(statusLine));
}

//  Utilitaires

void BadgeTestWidget::onLog(const QString &msg)
{
    log(msg);
}

void BadgeTestWidget::log(const QString &msg)
{
    const QString ts = QDateTime::currentDateTime().toString("hh:mm:ss");
    textLog->append(QString("[%1] %2").arg(ts, msg));
}

void BadgeTestWidget::setControlsRunning(bool running)
{
    btnStart->setEnabled(!running);
    btnStop->setEnabled(running);
    spinPort->setEnabled(!running);
    editApiUrl->setEnabled(!running);
}
