#include "BadgeAccessChecker.h"

BadgeAccessChecker::BadgeAccessChecker()
    : m_start(7, 0),    // 07:00 par defaut (labos accessibles de 7h a 21h)
      m_end(21, 0)      // 21:00 par defaut
{
}

QString BadgeAccessChecker::normalizeUid(const QString &uid)
{
    QString s = uid.trimmed().toLower();
    if (s.startsWith("0x"))
        s = s.mid(2);
    return s;
}

void BadgeAccessChecker::addBadge(const QString &uid, const QString &name)
{
    const QString norm = normalizeUid(uid);
    if (norm.isEmpty())
        return;

    // Si l'UID existe deja, on met juste a jour le nom.
    for (AuthorizedBadge &b : m_badges) {
        if (b.uid == norm) {
            b.name = name;
            return;
        }
    }
    m_badges.append({ norm, name });
}

void BadgeAccessChecker::removeBadge(const QString &uid)
{
    const QString norm = normalizeUid(uid);
    for (int i = 0; i < m_badges.size(); ++i) {
        if (m_badges[i].uid == norm) {
            m_badges.removeAt(i);
            return;
        }
    }
}

void BadgeAccessChecker::clearBadges()
{
    m_badges.clear();
}

void BadgeAccessChecker::setSchedule(const QTime &start, const QTime &end)
{
    m_start = start;
    m_end   = end;
}

bool BadgeAccessChecker::isWithinSchedule(const QTime &now) const
{
    if (m_start == m_end)
        return true;                      // plage vide => ouvert 24h/24

    if (m_start < m_end)
        return now >= m_start && now <= m_end;   // plage classique (jour)

    // Plage qui passe minuit (ex: 22:00 -> 06:00)
    return now >= m_start || now <= m_end;
}

AccessResult BadgeAccessChecker::check(const QString &uid) const
{
    return check(uid, QTime::currentTime());
}

AccessResult BadgeAccessChecker::check(const QString &uid, const QTime &now) const
{
    AccessResult r;
    r.uid = normalizeUid(uid);

    // 1. Badge connu (enregistre) ?
    for (const AuthorizedBadge &b : m_badges) {
        if (b.uid == r.uid) {
            r.badgeKnown = true;
            r.userName   = b.name;
            break;
        }
    }

    // 2. Dans la plage horaire ?
    r.inScheduleSlot = isWithinSchedule(now);

    // 3. Decision : les DEUX conditions doivent etre vraies
    r.granted = r.badgeKnown && r.inScheduleSlot;

    if (!r.badgeKnown) {
        r.reason = "Badge non enregistre (absent de la liste blanche)";
    } else if (!r.inScheduleSlot) {
        r.reason = QString("Hors plage horaire autorisee (%1 - %2)")
                       .arg(m_start.toString("HH:mm"), m_end.toString("HH:mm"));
    }

    return r;
}
