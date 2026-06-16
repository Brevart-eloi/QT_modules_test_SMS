#include "TestAlertListener.h"

TestAlertListener::TestAlertListener(QObject *parent)
    : LaboAlertEventListener(parent)
{
}

void TestAlertListener::onAlert(QString description)
{
    emit alertReceived(description);
}
