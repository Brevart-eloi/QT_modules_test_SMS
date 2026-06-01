#ifndef SURVEILLANCETESTWIDGET_H
#define SURVEILLANCETESTWIDGET_H

#include <QWidget>
#include "SurveillanceController.h"
#include "SurveillanceHttpApi.h"
#include "LaboAlertEventListener.h"

class QPushButton;
class QLabel;
class QTextEdit;
class QLineEdit;
class QSpinBox;

//  Listener démo pour relayer les alertes vers le widget

class DemoAlertListener : public LaboAlertEventListener
{
    Q_OBJECT
public:
    explicit DemoAlertListener(QObject *parent = nullptr)
        : LaboAlertEventListener(parent) {}

    void onAlert(QString description) override {
        emit alertReceived(description);
    }

signals:
    void alertReceived(const QString &description);
};


class SurveillanceTestWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SurveillanceTestWidget(QWidget *parent = nullptr);
    ~SurveillanceTestWidget();

private slots:
    void onBtnStartClicked();
    void onBtnStopClicked();
    void onBtnArmClicked();
    void onBtnDisarmClicked();

    void onLog(const QString &msg);
    void onArmedChanged(bool armed);
    void onDoorStateChanged(bool open);
    void onWindowStateChanged(bool open);
    void onAlarmStarted();
    void onAlarmStopped();
    void onAlertReceived(const QString &description);

private:
    SurveillanceController *m_controller;
    SurveillanceHttpApi    *m_httpApi;
    DemoAlertListener      *m_demoListener;

    QLineEdit *editOutputIp;
    QLineEdit *editInputIp;
    QSpinBox  *spinHttpPort;
    QSpinBox  *spinAlarmDuration;

    QPushButton *btnStart;
    QPushButton *btnStop;
    QPushButton *btnArm;
    QPushButton *btnDisarm;

    QLabel *labelArmed;
    QLabel *labelDoor;
    QLabel *labelWindow;
    QLabel *labelAlarm;

    QTextEdit *textLog;

    void log(const QString &msg);
    void updateUiButtons();
};

#endif // SURVEILLANCETESTWIDGET_H
