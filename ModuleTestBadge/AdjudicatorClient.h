#ifndef ADJUDICATORCLIENT_H
#define ADJUDICATORCLIENT_H

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

struct AccessResult
{
    QString uid;
    bool    granted    = false;
    QString userName;
    QString reason;
    bool    badgeKnown = false;  
};

class AdjudicatorClient : public QObject
{
    Q_OBJECT

public:
    explicit AdjudicatorClient(QObject *parent = nullptr);

    void    setApiBaseUrl(const QString &url);
    QString apiBaseUrl() const { return m_apiBaseUrl; }

 
    void checkBadge(const QString &uid);

    static QString normalizeUid(const QString &uid);

signals:
    void badgeChecked(const AccessResult &result);
    void logMessage(const QString &message);

private slots:
    void onReplyFinished(QNetworkReply *reply);

private:
    QNetworkAccessManager *m_nam;
    QString                m_apiBaseUrl;
};

#endif // ADJUDICATORCLIENT_H
