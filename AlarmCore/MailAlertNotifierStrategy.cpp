#include "MailAlertNotifierStrategy.h"
#include <iostream>
#include <sstream>

// Structure pour passer le contenu du mail à libcurl
struct UploadStatus {
    const std::string* data;
    size_t bytesRead;
};

MailAlertNotifierStrategy::MailAlertNotifierStrategy()
{
    curl_global_init(CURL_GLOBAL_ALL);
}

MailAlertNotifierStrategy::~MailAlertNotifierStrategy()
{
    curl_global_cleanup();
}

size_t MailAlertNotifierStrategy::payloadSource(void* ptr, size_t size, size_t nmemb, void* userp)
{
    UploadStatus* upload = (UploadStatus*)userp;
    size_t room = size * nmemb;

    if (upload->bytesRead >= upload->data->size())
        return 0;

    size_t toSend = upload->data->size() - upload->bytesRead;
    if (toSend > room) toSend = room;

    memcpy(ptr, upload->data->c_str() + upload->bytesRead, toSend);
    upload->bytesRead += toSend;
    return toSend;
}

void MailAlertNotifierStrategy::sendAlert(const std::string& message)
{
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Erreur : impossible d'initialiser curl" << std::endl;
        return;
    }

    // Construction du corps du mail (format RFC 2822)
    std::ostringstream mailContent;
    mailContent << "To: " << m_to << "\r\n";
    mailContent << "From: " << m_from << "\r\n";
    mailContent << "Subject: " << m_subject << "\r\n";
    mailContent << "\r\n";
    mailContent << message << "\r\n";

    std::string mailStr = mailContent.str();
    UploadStatus uploadCtx = { &mailStr, 0 };

    // Liste des destinataires
    struct curl_slist* recipients = nullptr;
    recipients = curl_slist_append(recipients, m_to.c_str());

    // Configuration curl SMTP
    curl_easy_setopt(curl, CURLOPT_URL, m_smtpServer.c_str());
    curl_easy_setopt(curl, CURLOPT_USERNAME, m_username.c_str());
    curl_easy_setopt(curl, CURLOPT_PASSWORD, m_password.c_str());
    curl_easy_setopt(curl, CURLOPT_MAIL_FROM, m_from.c_str());
    curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);
    curl_easy_setopt(curl, CURLOPT_USE_SSL, (long)CURLUSESSL_ALL);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);  
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, payloadSource);
    curl_easy_setopt(curl, CURLOPT_READDATA, &uploadCtx);
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);  

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK)
        std::cerr << "[MAIL] Erreur envoi : " << curl_easy_strerror(res) << std::endl;
    else
        std::cout << "[MAIL] Email envoyé avec succès !" << std::endl;

    curl_slist_free_all(recipients);
    curl_easy_cleanup(curl);
}