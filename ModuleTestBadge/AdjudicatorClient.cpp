#include "AdjudicatorClient.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

// ============================================================================
//  Constructeur
// ============================================================================

AdjudicatorClient::AdjudicatorClient(QObject *parent)
    : QObject(parent),
      m_nam(new QNetworkAccessManager(this)),
      m_apiBaseUrl("http://172.29.19.193")
{
    connect(m_nam, &QNetworkAccessManager::finished,
            this,  &AdjudicatorClient::onReplyFinished);
}

void AdjudicatorClient::setApiBaseUrl(const QString &url)
{
    m_apiBaseUrl = url.trimmed().trimmed();
    // Retire le slash de fin pour eviter les doubles slashes dans l'URL
    while (m_apiBaseUrl.endsWith('/'))
        m_apiBaseUrl.chop(1);
}

// ============================================================================
//  normalizeUid
//  Meme logique que l'adjudicator.js : trim + uppercase.
//  (La BDD stocke "UPPER(uid)", comparaison insensible a la casse.)
// ============================================================================

QString AdjudicatorClient::normalizeUid(const QString &uid)
{
    return uid.trimmed().toUpper();
}

// ============================================================================
//  checkBadge
// ============================================================================

void AdjudicatorClient::checkBadge(const QString &uid)
{
    const QString norm = normalizeUid(uid);
    if (norm.isEmpty()) {
        AccessResult r;
        r.uid     = uid;
        r.granted = false;
        r.reason  = "UID vide";
        emit badgeChecked(r);
        return;
    }

    // Construit l'URL : GET /api/rfid/check/{uid}
    const QString urlStr = m_apiBaseUrl + "/api/rfid/check/" + norm;
    emit logMessage(QString("API >> GET %1").arg(urlStr));

    QNetworkRequest req{ QUrl(urlStr) };
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    // On stocke l'UID original dans la propriete de la requete pour le
    // retrouver facilement dans le slot (QNetworkReply ne donne que l'URL).
    QNetworkReply *reply = m_nam->get(req);
    reply->setProperty("uid", uid);
}

// ============================================================================
//  onReplyFinished
// ============================================================================

void AdjudicatorClient::onReplyFinished(QNetworkReply *reply)
{
    reply->deleteLater();

    const QString uid = reply->property("uid").toString();

    AccessResult r;
    r.uid = normalizeUid(uid);

    // ── Erreur reseau ──
    if (reply->error() != QNetworkReply::NoError) {
        r.granted = false;
        r.reason  = QString("Erreur reseau : %1").arg(reply->errorString());
        emit logMessage(QString("API << ERREUR: %1").arg(r.reason));
        emit badgeChecked(r);
        return;
    }

    // ── Code HTTP ──
    const int httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray body = reply->readAll();
    emit logMessage(QString("API << HTTP %1 : %2").arg(httpCode).arg(QString::fromUtf8(body)));

    // ── Parse JSON ──
    QJsonParseError perr;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &perr);
    if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
        r.granted = false;
        r.reason  = QString("Reponse JSON invalide (%1)").arg(perr.errorString());
        emit badgeChecked(r);
        return;
    }

    const QJsonObject obj = doc.object();

    // ── Construction du resultat depuis la reponse adjudicator ──
    //
    // Champs attendus :
    //   { "ok": true, "authorized": bool, "uid": "...",
    //     "owner": "...", "message": "..." }
    //
    // "message" peut valoir :
    //   "Badge autorise"  → authorized=true
    //   "Badge desactive" → authorized=false, badge connu
    //   "Badge inconnu"   → authorized=false, badge absent
    //

    r.granted   = obj.value("authorized").toBool(false);
    r.userName  = obj.value("owner").toString();
    r.reason    = obj.value("message").toString();

    // Deduit badgeKnown : si le message ne dit pas "inconnu", le badge est en BDD
    const QString msgLower = r.reason.toLower();
    r.badgeKnown = !msgLower.contains("inconnu");

    // Si l'API dit ok=false (erreur interne adjudicator)
    if (!obj.value("ok").toBool(true)) {
        r.granted    = false;
        r.badgeKnown = false;
        r.reason     = obj.value("error").toString("Erreur inconnue cote serveur");
    }

    emit badgeChecked(r);
}
