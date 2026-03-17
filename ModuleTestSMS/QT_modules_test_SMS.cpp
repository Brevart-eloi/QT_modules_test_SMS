#include "QT_modules_test_SMS.h"

QT_modules_test_SMS::QT_modules_test_SMS(QWidget *parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);
	smsAlertNotifierStrategy.sendAlert("Test SMS Alert");
}

QT_modules_test_SMS::~QT_modules_test_SMS()
{}

