#ifndef BADGEACCESSCHECKER_H
#define BADGEACCESSCHECKER_H

#include <QString>
#include <QTime>
#include <QList>

/*
 * ============================================================================
 *  BadgeAccessChecker  (copie locale au module de test des droits d'acces)
 * ============================================================================
 *
 *  Repond a la question : "Le badge X a-t-il le droit d'entrer maintenant ?"
 *
 *  Verification PUREMENT LOCALE (aucune dependance reseau) :
 *    1. Le badge fait-il partie de la liste blanche ? (UID enregistre)
 *    2. L'heure est-elle dans la plage horaire autorisee ?
 *
 *  Acces accorde uniquement si les DEUX conditions sont vraies.
 *
 *  NOTE (module de test) : une surcharge check(uid, now) permet d'evaluer
 *  l'acces a une HEURE SIMULEE, pour demontrer le cas "hors plage" sans avoir
 *  a changer l'horloge du PC.
 *
 * ============================================================================
 */

struct AuthorizedBadge
{
    QString uid;    // hex minuscule, sans prefixe
    QString name;   // nom du porteur
};

struct AccessResult
{
    QString uid;
    bool    granted        = false;
    QString userName;
    QString reason;         // raison du refus (vide si accorde)
    bool    badgeKnown     = false;
    bool    inScheduleSlot = false;
};

class BadgeAccessChecker
{
public:
    BadgeAccessChecker();

    // ===== Gestion de la liste blanche =====
    void addBadge(const QString &uid, const QString &name);
    void removeBadge(const QString &uid);
    void clearBadges();
    QList<AuthorizedBadge> badges() const { return m_badges; }

    // ===== Plage horaire autorisee =====
    // Gere aussi les plages "de nuit" ou start > end (ex: 22h -> 06h).
    void  setSchedule(const QTime &start, const QTime &end);
    QTime scheduleStart() const { return m_start; }
    QTime scheduleEnd()   const { return m_end;   }

    // ===== Verification =====
    AccessResult check(const QString &uid) const;                    // heure actuelle
    AccessResult check(const QString &uid, const QTime &now) const;  // heure donnee (test)

    // Normalise un UID : trim, minuscule, retire un eventuel prefixe "0x".
    static QString normalizeUid(const QString &uid);

private:
    QList<AuthorizedBadge> m_badges;
    QTime m_start;
    QTime m_end;

    bool isWithinSchedule(const QTime &now) const;
};

#endif // BADGEACCESSCHECKER_H
