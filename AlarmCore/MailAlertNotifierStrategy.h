#pragma once
#include "alarmcore_global.h"
#include "AlarmNotifierStrategy.h" 
#include <string>
#include <curl/curl.h>

class ALARMCORE_EXPORT MailAlertNotifierStrategy : public AlarmNotifierStrategy
{
    Q_OBJECT 

public:
    MailAlertNotifierStrategy(QObject* parent = nullptr); 
    ~MailAlertNotifierStrategy();

    bool sendAlert(QString description) override; 

private:
    const std::string m_smtpServer = "smtps://smtp.gmail.com:465";
    const std::string m_username = "alertes.labo@gmail.com";
    const std::string m_password = "efqc nlex ilsq eljh";
    const std::string m_from = "alertes.labo@gmail.com";
    const std::string m_to = "eloi.brevart@gmail.com";
    const std::string m_subject = "Alerte système";
    static size_t payloadSource(void* ptr, size_t size, size_t nmemb, void* userp);
};