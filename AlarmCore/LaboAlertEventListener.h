#ifndef LABOALERTEVENTLISTENER_H
#define LABOALERTEVENTLISTENER_H

#include "alarmcore_global.h"
#include <QObject>
#include <QString>

/*
 * ============================================================================
 *  LaboAlertEventListener (interface)
 * ============================================================================
 *
 *  Interface imposée par le diagramme de classes du projet (page 9 du sujet).
 *
 *  Toute classe qui veut être notifiée des alertes du LaboMonitoring doit
 *  hériter de cette interface et implémenter onAlert(description).
 *
 *  L'Étudiant 3 implémente AlertNotifier qui hérite de cette interface et
 *  qui, dans onAlert(), appelle ses AlarmNotifierStrategy (Mail + SMS).
 *
 *  Note d'implémentation :
 *    On fait hériter de QObject pour rester cohérent avec l'usage de
 *    l'Étudiant 3 (constructeur prenant un parent, macro Q_OBJECT dans
 *    ses classes dérivées). La méthode onAlert reste virtuelle pure.
 *
 * ============================================================================
 */

class ALARMCORE_EXPORT LaboAlertEventListener : public QObject
{
    Q_OBJECT

public:
    explicit LaboAlertEventListener(QObject* parent = nullptr)
        : QObject(parent) {
    }

    virtual ~LaboAlertEventListener() = default;

    // Appelé quand une alerte est levée (intrusion, badge refusé, etc.)
    virtual void onAlert(QString description) = 0;
};

#endif // LABOALERTEVENTLISTENER_H