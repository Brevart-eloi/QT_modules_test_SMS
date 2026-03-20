#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_QT_modules_test_SMS.h"
#include <SMSAlertNotifierStrategy.h>
#include "MailAlertNotifierStrategy.h"


class QT_modules_test_SMS : public QMainWindow
{
    Q_OBJECT

public:
    QT_modules_test_SMS(QWidget* parent = nullptr);
    ~QT_modules_test_SMS();

private:
    Ui::QT_modules_test_SMSClass ui;

    SMSAlertNotifierStrategy  smsAlertNotifierStrategy;
    MailAlertNotifierStrategy mailAlertNotifierStrategy;
};