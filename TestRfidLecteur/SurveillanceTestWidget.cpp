#include "SurveillanceTestWidget.h"

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

SurveillanceTestWidget::SurveillanceTestWidget(QWidget *parent)
    : QWidget(parent),
      m_controller(new SurveillanceController(this)),
      m_httpApi(new SurveillanceHttpApi(m_controller, this)),
      m_demoListener(new DemoAlertListener(this))
{
    setWindowTitle("Test SurveillanceController (PC Labo - E2)");
    resize(800, 650);

    QVBoxLayout *main = new QVBoxLayout(this);

    //  Configuration 
    QGroupBox *grpConfig = new QGroupBox("Configuration");
    QFormLayout *formConf = new QFormLayout(grpConfig);

    editOutputIp = new QLineEdit("172.29.240.1");   
    editInputIp  = new QLineEdit("172.29.240.2");  
    spinHttpPort = new QSpinBox();
    spinHttpPort->setRange(1024, 65535);
    spinHttpPort->setValue(8080);
    spinAlarmDuration = new QSpinBox();
    spinAlarmDuration->setRange(1, 600);
    spinAlarmDuration->setValue(30);
    spinAlarmDuration->setSuffix(" s");

    formConf->addRow("IP PET-7067 (sorties) :", editOutputIp);
    formConf->addRow("IP PET-7050 (entrees) :", editInputIp);
    formConf->addRow("Port API HTTP :",         spinHttpPort);
    formConf->addRow("Duree alarme :",          spinAlarmDuration);

    main->addWidget(grpConfig);

    //  Boutons start/stop 
    QHBoxLayout *startLayout = new QHBoxLayout();
    btnStart = new QPushButton("Demarrer le service");
    btnStop  = new QPushButton("Arreter");
    btnStop->setEnabled(false);
    startLayout->addWidget(btnStart);
    startLayout->addWidget(btnStop);
    startLayout->addStretch();
    main->addLayout(startLayout);

    //  Armer/désarmer 
    QGroupBox *grpArm = new QGroupBox("Mode surveillance");
    QHBoxLayout *armLayout = new QHBoxLayout(grpArm);
    btnArm    = new QPushButton("ARMER");
    btnDisarm = new QPushButton("DESARMER");
    btnArm->setEnabled(false);
    btnDisarm->setEnabled(false);
    btnArm->setStyleSheet("padding: 10px; font-weight: bold;");
    btnDisarm->setStyleSheet("padding: 10px; font-weight: bold;");
    armLayout->addWidget(btnArm);
    armLayout->addWidget(btnDisarm);
    main->addWidget(grpArm);

    //  État 
    QGroupBox *grpState = new QGroupBox("Etat actuel");
    QFormLayout *formState = new QFormLayout(grpState);

    labelArmed = new QLabel("DESARME");
    labelArmed->setStyleSheet("font-weight: bold; color: gray;");
    labelDoor   = new QLabel("---");
    labelWindow = new QLabel("---");
    labelAlarm  = new QLabel("---");

    formState->addRow("Surveillance :", labelArmed);
    formState->addRow("Porte :",        labelDoor);
    formState->addRow("Fenetre :",      labelWindow);
    formState->addRow("Alarme :",       labelAlarm);

    main->addWidget(grpState);

    //  Journal 
    main->addWidget(new QLabel("Journal :"));
    textLog = new QTextEdit();
    textLog->setReadOnly(true);
    textLog->setStyleSheet("font-family: Consolas, monospace; font-size: 11px;");
    main->addWidget(textLog);

    //  Connexions 
    connect(btnStart,  &QPushButton::clicked, this, &SurveillanceTestWidget::onBtnStartClicked);
    connect(btnStop,   &QPushButton::clicked, this, &SurveillanceTestWidget::onBtnStopClicked);
    connect(btnArm,    &QPushButton::clicked, this, &SurveillanceTestWidget::onBtnArmClicked);
    connect(btnDisarm, &QPushButton::clicked, this, &SurveillanceTestWidget::onBtnDisarmClicked);

    connect(m_controller, &SurveillanceController::logMessage,        this, &SurveillanceTestWidget::onLog);
    connect(m_controller, &SurveillanceController::armedChanged,      this, &SurveillanceTestWidget::onArmedChanged);
    connect(m_controller, &SurveillanceController::doorStateChanged,  this, &SurveillanceTestWidget::onDoorStateChanged);
    connect(m_controller, &SurveillanceController::windowStateChanged,this, &SurveillanceTestWidget::onWindowStateChanged);
    connect(m_controller, &SurveillanceController::alarmStarted,      this, &SurveillanceTestWidget::onAlarmStarted);
    connect(m_controller, &SurveillanceController::alarmStopped,      this, &SurveillanceTestWidget::onAlarmStopped);

    connect(m_httpApi, &SurveillanceHttpApi::logMessage, this, &SurveillanceTestWidget::onLog);

    connect(m_demoListener, &DemoAlertListener::alertReceived,
            this, &SurveillanceTestWidget::onAlertReceived);
    m_controller->addAlertEventListener(m_demoListener);

    log("Pret. Configurez les IP des PET puis cliquez sur 'Demarrer le service'.");
}

SurveillanceTestWidget::~SurveillanceTestWidget()
{
    m_controller->removeAlertEventListener(m_demoListener);
}

//  Boutons start/stop

void SurveillanceTestWidget::onBtnStartClicked()
{
    QString outIp = editOutputIp->text().trimmed();
    QString inIp  = editInputIp->text().trimmed();

    if (!outIp.isEmpty()) m_controller->setOutputModuleAddress(outIp);
    if (!inIp.isEmpty())  m_controller->setInputModuleAddress(inIp);

    m_controller->setAlarmDuration(spinAlarmDuration->value() * 1000);

    if (m_controller->start()) {
        m_httpApi->start(static_cast<quint16>(spinHttpPort->value()));

        btnStart->setEnabled(false);
        btnStop->setEnabled(true);
        btnArm->setEnabled(true);
        btnDisarm->setEnabled(true);
        editOutputIp->setEnabled(false);
        editInputIp->setEnabled(false);
        spinHttpPort->setEnabled(false);
        spinAlarmDuration->setEnabled(false);
    }
}

void SurveillanceTestWidget::onBtnStopClicked()
{
    m_httpApi->stop();
    m_controller->stop();

    btnStart->setEnabled(true);
    btnStop->setEnabled(false);
    btnArm->setEnabled(false);
    btnDisarm->setEnabled(false);
    editOutputIp->setEnabled(true);
    editInputIp->setEnabled(true);
    spinHttpPort->setEnabled(true);
    spinAlarmDuration->setEnabled(true);
}

void SurveillanceTestWidget::onBtnArmClicked()
{
    m_controller->arm();
}

void SurveillanceTestWidget::onBtnDisarmClicked()
{
    m_controller->disarm();
}

//  Slots de mise à jour

void SurveillanceTestWidget::onLog(const QString &msg)
{
    log(msg);
}

void SurveillanceTestWidget::onArmedChanged(bool armed)
{
    if (armed) {
        labelArmed->setText("ARME");
        labelArmed->setStyleSheet("font-weight: bold; color: red;");
    } else {
        labelArmed->setText("DESARME");
        labelArmed->setStyleSheet("font-weight: bold; color: green;");
    }
}

void SurveillanceTestWidget::onDoorStateChanged(bool open)
{
    labelDoor->setText(open ? "OUVERTE" : "fermee");
    labelDoor->setStyleSheet(open ? "color: red;" : "color: green;");
}

void SurveillanceTestWidget::onWindowStateChanged(bool open)
{
    labelWindow->setText(open ? "OUVERTE" : "fermee");
    labelWindow->setStyleSheet(open ? "color: red;" : "color: green;");
}

void SurveillanceTestWidget::onAlarmStarted()
{
    labelAlarm->setText("EN COURS");
    labelAlarm->setStyleSheet("font-weight: bold; color: red;");
}

void SurveillanceTestWidget::onAlarmStopped()
{
    labelAlarm->setText("inactive");
    labelAlarm->setStyleSheet("color: gray;");
}

void SurveillanceTestWidget::onAlertReceived(const QString &description)
{
    log(QString("*** ALERTE RECUE (via Observer) : %1").arg(description));
}

//  Log

void SurveillanceTestWidget::log(const QString &msg)
{
    QString ts = QDateTime::currentDateTime().toString("hh:mm:ss");
    textLog->append(QString("[%1] %2").arg(ts, msg));
}
