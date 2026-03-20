#include "QT_modules_test_SMS.h"

QT_modules_test_SMS::QT_modules_test_SMS(QWidget *parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);
	smsAlertNotifierStrategy.sendAlert("Alerte : une intrusion a été détectée, le système d’alarme s’est activé.");
    mailAlertNotifierStrategy.sendAlert("Alerte : intrusion détectée !");
}

QT_modules_test_SMS::~QT_modules_test_SMS()
{}

