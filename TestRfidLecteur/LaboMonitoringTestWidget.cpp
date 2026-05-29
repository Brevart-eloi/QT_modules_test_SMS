#include "LaboMonitoringTestWidget.h"

#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include <QLineEdit>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QDateTime>

LaboMonitoringTestWidget::LaboMonitoringTestWidget(QWidget* parent)
    : QWidget(parent),
    m_monitoring(new LaboMonitoring(this)),
    m_demoListener(new DemoAlertListener(this))
{
    setWindowTitle("Test LaboMonitoring");
    resize(800, 600);

    QVBoxLayout* main = new QVBoxLayout(this);

    //  Configuration 
    QGroupBox* grpConfig = new QGroupBox("Configuration");
    QFormLayout* formConf = new QFormLayout(grpConfig);

    editApiUrl = new QLineEdit("http://172.29.19.193");
    editOutputIp = new QLineEdit("172.29.240.1");    // PET-7067 
    editInputIp = new QLineEdit("172.29.240.2");    // PET-7050 

    formConf->addRow("URL API (E3) :", editApiUrl);
    formConf->addRow("IP PET-7067 (sorties) :", editOutputIp);
    formConf->addRow("IP PET-7050 (entrees) :", editInputIp);

    main->addWidget(grpConfig);

    //  Boutons 
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnStart = new QPushButton("Demarrer le service");
    btnStop = new QPushButton("Arreter");
    btnStop->setEnabled(false);
    checkSurveillance = new QCheckBox("Mode surveillance");
    checkSurveillance->setEnabled(false);

    btnLayout->addWidget(btnStart);
    btnLayout->addWidget(btnStop);
    btnLayout->addStretch();
    btnLayout->addWidget(checkSurveillance);
    main->addLayout(btnLayout);

    //  Résultat dernier badge 
    QGroupBox* grpResult = new QGroupBox("Dernier badge");
    QFormLayout* formRes = new QFormLayout(grpResult);

    labelLastCard = new QLabel("---");
    labelLastCard->setStyleSheet("font-size: 18px; font-weight: bold; color: #1a5fb4;");
    labelLastCard->setTextInteractionFlags(Qt::TextSelectableByMouse);

    labelLastUser = new QLabel("---");
    labelLastResult = new QLabel("---");
    labelLastResult->setStyleSheet("font-weight: bold;");

    formRes->addRow("UID :", labelLastCard);
    formRes->addRow("Utilisateur :", labelLastUser);
    formRes->addRow("Decision :", labelLastResult);

    main->addWidget(grpResult);

    //  Journal 
    main->addWidget(new QLabel("Journal :"));
    textLog = new QTextEdit();
    textLog->setReadOnly(true);
    textLog->setStyleSheet("font-family: Consolas, monospace; font-size: 11px;");
    main->addWidget(textLog);

    //  Connexions boutons 
    connect(btnStart, &QPushButton::clicked, this, &LaboMonitoringTestWidget::onBtnStartClicked);
    connect(btnStop, &QPushButton::clicked, this, &LaboMonitoringTestWidget::onBtnStopClicked);
    connect(checkSurveillance, &QCheckBox::toggled,
        this, &LaboMonitoringTestWidget::onCheckSurveillanceToggled);

    //  Connexions monitoring 
    connect(m_monitoring, &LaboMonitoring::logMessage,
        this, &LaboMonitoringTestWidget::onLog);
    connect(m_monitoring, &LaboMonitoring::cardBadged,
        this, &LaboMonitoringTestWidget::onCardBadged);
    connect(m_monitoring, &LaboMonitoring::accessGranted,
        this, &LaboMonitoringTestWidget::onAccessGranted);
    connect(m_monitoring, &LaboMonitoring::accessDenied,
        this, &LaboMonitoringTestWidget::onAccessDenied);

    //  Pattern Observer 
    connect(m_demoListener, &DemoAlertListener::alertReceived,
        this, &LaboMonitoringTestWidget::onAlertReceived);
    m_monitoring->addAlertEventListener(m_demoListener);

    log("Pret. Configurez puis cliquez 'Demarrer le service'.");
}

LaboMonitoringTestWidget::~LaboMonitoringTestWidget()
{
    m_monitoring->removeAlertEventListener(m_demoListener);
}

//  Boutons

void LaboMonitoringTestWidget::onBtnStartClicked()
{
    m_monitoring->setApiBaseUrl(editApiUrl->text().trimmed());

    QString outIp = editOutputIp->text().trimmed();
    QString inIp = editInputIp->text().trimmed();

    if (!outIp.isEmpty()) m_monitoring->setOutputModuleAddress(outIp);
    if (!inIp.isEmpty())  m_monitoring->setInputModuleAddress(inIp);

    if (m_monitoring->start(80)) {
        btnStart->setEnabled(false);
        btnStop->setEnabled(true);
        checkSurveillance->setEnabled(true);
        editApiUrl->setEnabled(false);
        editOutputIp->setEnabled(false);
        editInputIp->setEnabled(false);
    }
}

void LaboMonitoringTestWidget::onBtnStopClicked()
{
    m_monitoring->stop();
    btnStart->setEnabled(true);
    btnStop->setEnabled(false);
    checkSurveillance->setEnabled(false);
    checkSurveillance->setChecked(false);
    editApiUrl->setEnabled(true);
    editOutputIp->setEnabled(true);
    editInputIp->setEnabled(true);
}

void LaboMonitoringTestWidget::onCheckSurveillanceToggled(bool checked)
{
    m_monitoring->setSurveillanceMode(checked);
}

//  Slots de mise à jour de l'IHM

void LaboMonitoringTestWidget::onLog(const QString& msg)
{
    log(msg);
}

void LaboMonitoringTestWidget::onCardBadged(const QString& uid)
{
    labelLastCard->setText(uid);
    labelLastUser->setText("...");
    labelLastResult->setText("(verification en cours)");
    labelLastResult->setStyleSheet("font-weight: bold; color: orange;");
}

void LaboMonitoringTestWidget::onAccessGranted(const QString& uid, const QString& userName)
{
    Q_UNUSED(uid);
    labelLastUser->setText(userName.isEmpty() ? "?" : userName);
    labelLastResult->setText("AUTORISE");
    labelLastResult->setStyleSheet("font-weight: bold; color: green;");
}

void LaboMonitoringTestWidget::onAccessDenied(const QString& uid, const QString& reason)
{
    Q_UNUSED(uid);
    labelLastResult->setText(QString("REFUSE - %1").arg(reason));
    labelLastResult->setStyleSheet("font-weight: bold; color: red;");
}

void LaboMonitoringTestWidget::onAlertReceived(const QString& description)
{
    log(QString("*** ALERTE RECUE (via Observer) : %1").arg(description));
}

//  Log

void LaboMonitoringTestWidget::log(const QString& msg)
{
    QString ts = QDateTime::currentDateTime().toString("hh:mm:ss");
    textLog->append(QString("[%1] %2").arg(ts, msg));
}