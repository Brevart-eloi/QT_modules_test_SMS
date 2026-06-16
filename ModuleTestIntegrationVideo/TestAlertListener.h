#ifndef TESTALERTLISTENER_H
#define TESTALERTLISTENER_H

#include "LaboAlertEventListener.h"
// TestAlertListener est une classe locale au projet de test :
// pas de ALARMCORE_EXPORT (qui serait Q_DECL_IMPORT depuis ce projet).

/*
 * ============================================================================
 *  TestAlertListener  —  Implementation de test du pattern Observer
 * ============================================================================
 *
 *  Stub de substitution pour le pattern Observer quand les classes de
 *  l'Etudiant 3 (AlertNotifier + strategies mail/SMS) ne sont pas pretes.
 *  Au lieu d'envoyer un vrai mail / SMS, on emet un signal Qt affiche dans
 *  l'IHM.
 *
 *  Usage :
 *    controller->addAlertEventListener(new TestAlertListener(this));
 *
 * ============================================================================
 */

class TestAlertListener : public LaboAlertEventListener
{
    Q_OBJECT

public:
    explicit TestAlertListener(QObject *parent = nullptr);

    // Implantation de l'interface Observer : appelee par SurveillanceController
    void onAlert(QString description) override;

signals:
    void alertReceived(const QString &description);
};

#endif // TESTALERTLISTENER_H
