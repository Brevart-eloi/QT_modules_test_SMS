#include "SMSAlertNotifierStrategy.h"
#include <qdebug.h>
#include <qurl.h>
#include <qbytearray.h>
#include <qnetworkrequest.h>
#include <QtCore/QCryptographicHash>
#include <QtCore/QDateTime> 

SMSAlertNotifierStrategy::SMSAlertNotifierStrategy(QObject* parent)
	: AlarmNotifierStrategy(parent)
{
	manager = new QNetworkAccessManager(this);
	connect(manager, &QNetworkAccessManager::finished, this, &SMSAlertNotifierStrategy::onResult);
}



QString SMSAlertNotifierStrategy::getSignature(QString AS, QString CK, QString method, QString query, QString body, QString tstamp) {
    QString toHash = AS + "+" + CK + "+" + method + "+" + query + "+" + body + "+" + tstamp;
    QByteArray hashResult = QCryptographicHash::hash(toHash.toUtf8(), QCryptographicHash::Sha1);
    QString hashStr = QString(hashResult.toHex()).toLower();
	std::string stdHashStr = hashStr.toStdString();
    hashStr = hashStr.toLower();
    return "$1$" + hashStr;
}

bool SMSAlertNotifierStrategy::sendAlert(QString description)
{
    QString AK = "e18db9c343895221";
    QString AS = "82fdd73235d02eb89debae5014f6b2f0";
    QString CK = "d8ae1ba17d1b917f5edfab7480861d96";
    QString service = "sms-gh175099-1";

    QString URL = "https://eu.api.ovh.com/1.0/sms/" + service + "/jobs";

    QString method = "POST"; 
    QString fullUrl = URL; 
    QString body = "{\"message\":\"" + description + "\",\"noStopClause\":true,\"receivers\":[\"+33781850278\"],\"senderForResponse\":true}"; 
    QString TSTAMP = QString::number(QDateTime::currentSecsSinceEpoch()); 

    QString signature = getSignature(AS, CK, method, fullUrl, body, TSTAMP);

	qDebug() << "SMSAlertNotifierStrategy::sendAlert: " << description;

    QNetworkRequest request = QNetworkRequest(QUrl(URL));
	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	request.setRawHeader("X-Ovh-Application", AK.toUtf8());
	request.setRawHeader("X-Ovh-Consumer", CK.toUtf8());
	request.setRawHeader("X-Ovh-Timestamp", QString::number(QDateTime::currentSecsSinceEpoch()).toUtf8());
	request.setRawHeader("X-Ovh-Signature", (QString("$1$") + QString(QCryptographicHash::hash(
        (AS + "+" + CK + "+POST+" + URL + "+" +
        "{\"message\":\"" + description + "\",\"noStopClause\":true,\"receivers\":[\"+33781850278\"],\"senderForResponse\":true}" +
        "+" + QString::number(QDateTime::currentSecsSinceEpoch())
    ).toUtf8(), QCryptographicHash::Sha1).toHex()).toLower()).toUtf8());
    QString contentStr = "{\"message\":\"" + description + "\",\"noStopClause\":true,\"receivers\":[\"+33781850278\"],\"senderForResponse\":true}";
    QByteArray content(contentStr.toUtf8());

	//manager->post(request, content);  // Pour éviter de dépenser des crédits :D
    return false;
}

void SMSAlertNotifierStrategy::onResult(QNetworkReply* reply)
{
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        qDebug() << "Réponse reçue :" << data;
    }
    else {
        qDebug() << "Erreur HTTP :" << reply->errorString();
    }
    reply->deleteLater(); 
}
