#ifndef DROITSACCESTESTWIDGET_H
#define DROITSACCESTESTWIDGET_H

#include <QWidget>
#include "BadgeAccessChecker.h"

class QLineEdit;
class QTimeEdit;
class QCheckBox;
class QTableWidget;
class QLabel;
class QTextEdit;

/*
 * ============================================================================
 *  DroitsAccesTestWidget
 * ============================================================================
 *
 *  Module de test 3 (Etudiant 2) :
 *    Verifier les DROITS D'ACCES au laboratoire en fonction de :
 *      1. le BADGE est-il enregistre (liste blanche) ?
 *      2. l'heure est-elle dans la PLAGE HORAIRE autorisee (ex. 7h - 21h) ?
 *
 *    Acces accorde uniquement si les DEUX conditions sont vraies.
 *
 *  S'appuie sur BadgeAccessChecker (logique purement locale, deterministe).
 *  Une "heure simulee" permet de tester le cas "hors plage" sans changer
 *  l'horloge du PC -> ideal pour une demonstration en revue.
 *
 * ============================================================================
 */

class DroitsAccesTestWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DroitsAccesTestWidget(QWidget *parent = nullptr);

private slots:
    void onAddBadge();
    void onRemoveBadge();
    void onCheckAccess();
    void onScheduleChanged();
    void onUseCurrentTimeToggled(bool checked);

private:
    BadgeAccessChecker m_checker;

    // Plage horaire autorisee
    QTimeEdit *m_editStart    = nullptr;
    QTimeEdit *m_editEnd      = nullptr;

    // Liste blanche (badges enregistres)
    QTableWidget *m_table     = nullptr;
    QLineEdit    *m_editUid   = nullptr;
    QLineEdit    *m_editName  = nullptr;

    // Test d'un acces
    QLineEdit *m_editTestUid  = nullptr;
    QCheckBox *m_chkNow       = nullptr;
    QTimeEdit *m_editTestTime = nullptr;
    QLabel    *m_lblResult    = nullptr;
    QLabel    *m_lblDetails   = nullptr;

    // Journal
    QTextEdit *m_log          = nullptr;

    void buildUi();
    void refreshTable();
    void syncScheduleFromUi();
    void log(const QString &msg);
};

#endif // DROITSACCESTESTWIDGET_H
