#ifndef LABOMONITORINGTESTWIDGET_H
#define LABOMONITORINGTESTWIDGET_H

#include <QWidget>
#include "LaboMonitoring.h"
#include "LaboAlertEventListener.h"

/*
 * ============================================================================
 *  LaboMonitoringTestWidget
 * ============================================================================
 *
 *  Petit widget de test pour valider toute la chaîne :
 *    - L'Arduino POST un badge -> on le voit dans le journal
 *    - La vérification d'accès est faite -> "AUTORISE" ou "REFUSE"
 *    - Bouton pour activer le mode surveillance
 *    - Les alertes (pattern Observer) s'affichent dans le journal
 *
 *  NOTE : depuis que LaboAlertEventListener hérite de QObject, on ne peut
 *  plus faire hériter le widget à la fois de QWidget ET de
 *  LaboAlertEventListener (double héritage de QObject interdit en Qt).
 *  On utilise donc une petite classe listener séparée (DemoAlertListener).
 *
 * ============================================================================
 */

class QPushButton;
class QLabel;
class QTextEdit;
class QLineEdit;
class QCheckBox;

// ----------------------------------------------------------------------------
//  Petit listener de démonstration : reçoit les alertes et les relaie
//  au widget via un signal.
// ----------------------------------------------------------------------------
class DemoAlertListener : public LaboAlertEventListener
{
    Q_OBJECT
public:
    explicit DemoAlertListener(QObject* parent = nullptr)
        : LaboAlertEventListener(parent) {
    }

    void onAlert(QString description) override {
        emit alertReceived(description);
    }

signals:
    void alertReceived(const QString& description);
};



//  Le widget de test
 
class LaboMonitoringTestWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LaboMonitoringTestWidget(QWidget* parent = nullptr);
    ~LaboMonitoringTestWidget();

private slots:
    void onBtnStartClicked();
    void onBtnStopClicked();
    void onCheckSurveillanceToggled(bool checked);

    void onLog(const QString& msg);
    void onCardBadged(const QString& uid);
    void onAccessGranted(const QString& uid, const QString& userName);
    void onAccessDenied(const QString& uid, const QString& reason);
    void onAlertReceived(const QString& description);

private:
    LaboMonitoring* m_monitoring;
    DemoAlertListener* m_demoListener;

    QLineEdit* editApiUrl;
    QLineEdit* editOutputIp;
    QLineEdit* editInputIp;
    QPushButton* btnStart;
    QPushButton* btnStop;
    QCheckBox* checkSurveillance;

    QLabel* labelLastCard;
    QLabel* labelLastUser;
    QLabel* labelLastResult;

    QTextEdit* textLog;

    void log(const QString& msg);
};

#endif // LABOMONITORINGTESTWIDGET_H