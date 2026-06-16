#ifndef INTEGRATIONTESTWIDGET_H
#define INTEGRATIONTESTWIDGET_H

#include <QWidget>
#include "SurveillanceController.h"
#include "SurveillanceHttpApi.h"
#include "TestAlertListener.h"

class QPushButton;
class QLabel;
class QTextEdit;
class QLineEdit;
class QSpinBox;
class QCheckBox;
class QListWidget;

/*
 * ============================================================================
 *  IntegrationTestWidget
 * ============================================================================
 *
 *  Test d'integration (Revue 2, sujet E6 - Etudiant 2) :
 *    "Surveillance des salles, declenchement de l'alarme et envoi des alertes
 *     en cas de probleme (utiliser une implementation de test de
 *     LaboAlertEventListener si les classes de E3 ne sont pas pretes)"
 *
 *  Ce widget met en place et teste la chaine complete :
 *
 *    1. SurveillanceController
 *         - Connexion Modbus TCP aux PET-7050 (capteurs) et PET-7067 (actionneurs)
 *         - Polling des entrees DI (portes / fenetres)
 *         - Algorithme d'alarme : si arme ET capteur declenche → ALARME
 *         - Activation sirene + flash via sorties DO
 *
 *    2. Pattern Observer (diagramme de classe sujet, page 9)
 *         - TestAlertListener enregistre sur le controller
 *         - Quand notifyAlert() est appele, TestAlertListener::onAlert()
 *           est appele → alerte affichee dans l'IHM
 *         - Quand E3 sera pret : remplacer/ajouter AlertNotifier avec
 *           MailAlertNotifierStrategy + SMSAlertNotifierStrategy
 *
 *    3. SurveillanceHttpApi (port configurable)
 *         - Permet d'armer/desarmer depuis l'IHM web de E1
 *         - ou depuis un StreamDeck / autre outil de test
 *
 *  Objectif de demonstration :
 *    → Armer le systeme, declencher un capteur (ouvrir une porte/fenetre),
 *      constater le declenchement de l'alarme, et voir l'alerte arriver
 *      dans la section "Alertes reçues" via le pattern Observer.
 *
 * ============================================================================
 */

class IntegrationTestWidget : public QWidget
{
    Q_OBJECT

public:
    explicit IntegrationTestWidget(QWidget *parent = nullptr);
    ~IntegrationTestWidget();

private slots:
    void onBtnStartClicked();
    void onBtnStopClicked();
    void onBtnArmClicked();
    void onBtnDisarmClicked();
    void onBtnClearAlertsClicked();

    // Slots relies au SurveillanceController
    void onLog(const QString &msg);
    void onArmedChanged(bool armed);
    void onDoorStateChanged(bool open);
    void onWindowStateChanged(bool open);
    void onAlarmStarted();
    void onAlarmStopped();
    void onIntrusionDetected(const QString &source);

    // Slot du TestAlertListener (pattern Observer)
    void onAlertReceived(const QString &description);

private:
    // ── Classes metier (E2) ──
    SurveillanceController *m_controller;
    SurveillanceHttpApi    *m_httpApi;

    // ── Listener de test (stub Observer) ──
    TestAlertListener      *m_testListener;

    // ── Widgets de configuration ──
    QLineEdit *editOutputIp;
    QLineEdit *editInputIp;
    QSpinBox  *spinHttpPort;
    QSpinBox  *spinAlarmDuration;
    QSpinBox  *spinSirenCoil;
    QSpinBox  *spinFlashCoil;
    QSpinBox  *spinDoorInput;
    QSpinBox  *spinWindowInput;
    QCheckBox *chkInvertInputs;  // NC (Normalement Fermé) = logique inversée

    // ── Boutons de controle ──
    QPushButton *btnStart;
    QPushButton *btnStop;
    QPushButton *btnArm;
    QPushButton *btnDisarm;

    // ── Indicateurs d'etat ──
    QLabel *labelArmed;
    QLabel *labelDoor;
    QLabel *labelWindow;
    QLabel *labelAlarm;

    // ── Zone d'alertes (pattern Observer) ──
    QListWidget *listAlerts;
    QPushButton *btnClearAlerts;

    // ── Journal general ──
    QTextEdit *textLog;

    void log(const QString &msg);
    void updateButtons();
};

#endif // INTEGRATIONTESTWIDGET_H
