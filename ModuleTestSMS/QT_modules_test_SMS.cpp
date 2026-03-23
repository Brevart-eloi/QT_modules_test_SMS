#include "QT_modules_test_SMS.h"

QT_modules_test_SMS::QT_modules_test_SMS(QWidget *parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);
    smsAlertNotifierStrategy.sendAlert("Alerte : une intrusion a ete detectee, le systeme d'alarme s'est active.");
    mailAlertNotifierStrategy.sendAlert("Alerte : une intrusion a ete detectee, le systeme d'alarme s'est active.");
}

QT_modules_test_SMS::~QT_modules_test_SMS()
{}

