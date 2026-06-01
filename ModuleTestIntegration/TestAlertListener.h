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
 *  Contexte (sujet BTS E6, diagramme de classe page 9) :
 *
 *    LaboAlertEventListener   <-- interface abstraite (definie par E2)
 *          |
 *          +--  TestAlertListener   <-- stub de test (E2, ce fichier)
 *          |
 *          +--  AlertNotifier       <-- implementation reelle (E3)
 *                   |
 *                   +-- MailAlertNotifierStrategy  (envoie un mail)
 *                   +-- SMSAlertNotifierStrategy   (envoie un SMS)
 *
 *  Role :
 *    Implémentation de substitution (test stub) pour le cas ou les classes
 *    de l'Etudiant 3 ne sont pas pretes. A la place d'envoyer un vrai mail
 *    ou un vrai SMS, elle emet un signal Qt qui sera affiche dans l'IHM.
 *
 *  Usage dans SurveillanceController :
 *    controller->addAlertEventListener(new TestAlertListener(this));
 *
 *  Quand l'Etudiant 3 est pret, on remplace (ou on ajoute) simplement :
 *    auto *notifier = new AlertNotifier(this);
 *    notifier->addNotificationStrategy(new MailAlertNotifierStrategy());
 *    notifier->addNotificationStrategy(new SMSAlertNotifierStrategy());
 *    controller->addAlertEventListener(notifier);
 *
 * ============================================================================
 */

class TestAlertListener : public LaboAlertEventListener
{
    Q_OBJECT

public:
    explicit TestAlertListener(QObject *parent = nullptr);

    // Implantation de l'interface Observer : appelee par SurveillanceController
    // lors d'une intrusion ou d'une alerte. Au lieu d'envoyer mail/SMS,
    // on emet un signal pour afficher l'alerte dans l'IHM.
    void onAlert(QString description) override;

signals:
    // Signal emis vers le widget (connexion via connect())
    void alertReceived(const QString &description);
};

#endif // TESTALERTLISTENER_H
