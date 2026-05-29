#ifndef BADGEACCESSCHECKER_H
#define BADGEACCESSCHECKER_H

#include "alarmcore_global.h"
#include <QObject>
#include <QString>
#include <QTime>
#include <QList>
#include <QNetworkAccessManager>

/*
 * ============================================================================
 *  BadgeAccessChecker
 * ============================================================================
 *
 *  Cette classe répond à la question : "Le badge X a-t-il le droit d'entrer
 *  dans le labo maintenant ?"
 *
 *  Elle vérifie deux choses :
 *    1. Le badge est-il enregistré ? (interroge l'API de l'Étudiant 3)
 *    2. L'heure actuelle est-elle dans une plage autorisée ? (interroge
 *       l'API de l'Étudiant 1 / la base via E3)
 *
 *  L'appel est ASYNCHRONE : on émet checkAccess(uid), et on récupère
 *  la réponse via le signal accessChecked(uid, granted, userName, reason).
 *
 *  Configuration : on indique l'URL de base de l'API (ex: http://172.29.19.193)
 *  et la classe se débrouille pour faire les bons appels.
 *
 *  Endpoints attendus côté API (à voir avec l'Étudiant 3) :
 *    - GET /api/badges/{uid}          -> renvoie {found, name} ou 404
 *    - GET /api/schedule/now          -> renvoie {open, start, end}
 *      (où "open" = bool indiquant si on est en plage d'ouverture)
 *
 *  Si l'API n'est pas joignable, on refuse l'accès par défaut (mode "fail-safe").
 *
 * ============================================================================
 */

struct AccessResult
{
    QString uid;
    bool    granted     = false;
    QString userName;       // nom du porteur si trouvé
    QString reason;         // raison du refus si granted=false

    // Détails techniques (pour les logs)
    bool    badgeKnown    = false;
    bool    inScheduleSlot = false;
};

class ALARMCORE_EXPORT BadgeAccessChecker : public QObject
{
    Q_OBJECT

public:
    explicit BadgeAccessChecker(QObject *parent = nullptr);

    // Configuration de l'URL de base de l'API (ex: "http://172.29.19.193")
    void setApiBaseUrl(const QString &baseUrl);
    QString apiBaseUrl() const;

    // Lance une vérification d'accès. Le résultat arrive via accessChecked().
    void checkAccess(const QString &uid);

signals:
    // Résultat de la vérification (UID, autorisé ou non, nom, raison)
    void accessChecked(const AccessResult &result);

    // Pour les logs
    void logMessage(const QString &message);

private:
    QNetworkAccessManager *netManager;
    QString m_apiBaseUrl;

    // États intermédiaires pendant un check : on attend 2 réponses HTTP
    // donc on stocke ce qu'on a déjà reçu en mémoire le temps que tout arrive.
    struct PendingCheck {
        QString uid;
        bool    badgeReceived = false;
        bool    scheduleReceived = false;
        bool    badgeKnown    = false;
        QString userName;
        bool    inScheduleSlot = false;
    };
    QMap<QString, PendingCheck> pendingChecks;  // clé = uid

    void queryBadge(const QString &uid);
    void querySchedule(const QString &uid);
    void tryFinalize(const QString &uid);
};

#endif // BADGEACCESSCHECKER_H
