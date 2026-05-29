#include "BadgeAccessChecker.h"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

// ============================================================================
//  Constructeur
// ============================================================================

BadgeAccessChecker::BadgeAccessChecker(QObject *parent)
    : QObject(parent),
      netManager(new QNetworkAccessManager(this)),
      m_apiBaseUrl("http://172.29.19.193")   // valeur par défaut (VM E3)
{
}

void BadgeAccessChecker::setApiBaseUrl(const QString &baseUrl)
{
    // On retire le / final s'il y en a un, pour ne pas avoir d'URL en double slash
    m_apiBaseUrl = baseUrl;
    while (m_apiBaseUrl.endsWith('/')) {
        m_apiBaseUrl.chop(1);
    }
}

QString BadgeAccessChecker::apiBaseUrl() const
{
    return m_apiBaseUrl;
}

// ============================================================================
//  checkAccess : lance les deux requêtes en parallèle
// ============================================================================

void BadgeAccessChecker::checkAccess(const QString &uid)
{
    QString uidUpper = uid.toUpper();

    // Initialise une nouvelle entrée dans la map des vérifs en cours
    PendingCheck pc;
    pc.uid = uidUpper;
    pendingChecks[uidUpper] = pc;

    emit logMessage(QString("Verification d'acces pour UID = %1").arg(uidUpper));

    // Lance les deux requêtes en parallèle
    queryBadge(uidUpper);
    querySchedule(uidUpper);
}

// ============================================================================
//  queryBadge : appelle GET /api/badges/{uid}
// ============================================================================

void BadgeAccessChecker::queryBadge(const QString &uid)
{
    QString url = m_apiBaseUrl + "/api/badges/" + uid;
    QNetworkRequest request{QUrl(url)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *reply = netManager->get(request);

    // On stocke l'UID dans la propriété du reply pour le retrouver dans le slot
    reply->setProperty("uid", uid);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {

        QString uid = reply->property("uid").toString();

        if (!pendingChecks.contains(uid)) {
            reply->deleteLater();
            return;
        }

        PendingCheck &pc = pendingChecks[uid];

        int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (reply->error() == QNetworkReply::NoError && httpStatus == 200) {
            // Badge trouvé dans la base
            QByteArray data = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                pc.badgeKnown = obj.value("found").toBool(true);  // par défaut, si 200 = trouvé
                pc.userName   = obj.value("name").toString();
            } else {
                pc.badgeKnown = true;     // statut 200 = trouvé même sans JSON exploitable
            }
        }
        else if (httpStatus == 404) {
            // Badge inconnu : c'est une réponse normale
            pc.badgeKnown = false;
        }
        else {
            // Erreur réseau : on considère l'accès refusé (fail-safe)
            pc.badgeKnown = false;
            emit logMessage(QString("Erreur API badges (%1) : %2")
                                .arg(httpStatus)
                                .arg(reply->errorString()));
        }

        pc.badgeReceived = true;
        reply->deleteLater();
        tryFinalize(uid);
    });
}

// ============================================================================
//  querySchedule : appelle GET /api/schedule/now
// ============================================================================

void BadgeAccessChecker::querySchedule(const QString &uid)
{
    QString url = m_apiBaseUrl + "/api/schedule/now";
    QNetworkRequest request{QUrl(url)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *reply = netManager->get(request);
    reply->setProperty("uid", uid);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {

        QString uid = reply->property("uid").toString();

        if (!pendingChecks.contains(uid)) {
            reply->deleteLater();
            return;
        }

        PendingCheck &pc = pendingChecks[uid];

        int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (reply->error() == QNetworkReply::NoError && httpStatus == 200) {
            QByteArray data = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                pc.inScheduleSlot = obj.value("open").toBool(false);
            }
        }
        else {
            // En cas d'erreur API, on considère qu'on est HORS plage
            pc.inScheduleSlot = false;
            emit logMessage(QString("Erreur API schedule (%1) : %2")
                                .arg(httpStatus)
                                .arg(reply->errorString()));
        }

        pc.scheduleReceived = true;
        reply->deleteLater();
        tryFinalize(uid);
    });
}

// ============================================================================
//  tryFinalize : si les 2 réponses sont arrivées, on calcule le résultat
// ============================================================================

void BadgeAccessChecker::tryFinalize(const QString &uid)
{
    if (!pendingChecks.contains(uid)) return;

    PendingCheck &pc = pendingChecks[uid];

    if (!pc.badgeReceived || !pc.scheduleReceived) {
        return; // on attend l'autre réponse
    }

    // ----------------------------------------------------------------
    // Construction du résultat final
    // ----------------------------------------------------------------
    AccessResult result;
    result.uid             = pc.uid;
    result.userName        = pc.userName;
    result.badgeKnown      = pc.badgeKnown;
    result.inScheduleSlot  = pc.inScheduleSlot;

    if (!pc.badgeKnown) {
        result.granted = false;
        result.reason  = "Badge inconnu";
    }
    else if (!pc.inScheduleSlot) {
        result.granted = false;
        result.reason  = "Hors plage horaire autorisee";
    }
    else {
        result.granted = true;
        result.reason  = "Acces autorise";
    }

    emit logMessage(QString("Resultat : %1 (%2) - %3")
                        .arg(result.uid)
                        .arg(result.userName.isEmpty() ? "?" : result.userName)
                        .arg(result.reason));

    // Nettoyage de la map
    pendingChecks.remove(uid);

    // Émission du résultat
    emit accessChecked(result);
}
