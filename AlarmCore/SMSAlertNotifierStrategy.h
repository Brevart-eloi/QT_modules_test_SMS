#pragma once
#include "alarmcore_global.h"
#include "AlarmNotifierStrategy.h"
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>

class ALARMCORE_EXPORT SMSAlertNotifierStrategy :
    public AlarmNotifierStrategy
{
    Q_OBJECT
public:
    SMSAlertNotifierStrategy(QObject* parent = nullptr);
    virtual bool sendAlert(QString description);

private slots:
    void onResult(QNetworkReply* reply);

private:
    QNetworkAccessManager* manager;
    QString getSignature(QString AS, QString CK, QString method, QString query, QString body, QString tstamp);
};

