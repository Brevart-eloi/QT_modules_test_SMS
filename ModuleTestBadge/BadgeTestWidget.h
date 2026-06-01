#ifndef BADGETESTWIDGET_H
#define BADGETESTWIDGET_H

#include <QWidget>

#include "RfidHttpServer.h"
#include "AdjudicatorClient.h"

class QPushButton;
class QLabel;
class QTextEdit;
class QLineEdit;
class QSpinBox;


class BadgeTestWidget : public QWidget
{
    Q_OBJECT

public:
    explicit BadgeTestWidget(QWidget *parent = nullptr);

private slots:
    void onBtnStartClicked();
    void onBtnStopClicked();
    void onBtnSimulateClicked();

    void onLog(const QString &msg);
    void onCardScanned(const QString &uid, const QString &rawHex, int wiegandType);
    void onBadgeChecked(const AccessResult &result);

private:
    RfidHttpServer    m_server;
    AdjudicatorClient m_client;

    // Configuration
    QSpinBox  *spinPort;
    QLineEdit *editApiUrl;

    // Serveur
    QPushButton *btnStart;
    QPushButton *btnStop;

    // Simulation manuelle
    QLineEdit   *editSimUid;
    QPushButton *btnSimulate;

    // Resultat du dernier badge
    QLabel *labelDecision;
    QLabel *labelName;
    QLabel *labelReason;

    // Journal
    QTextEdit *textLog;

    void log(const QString &msg);
    void setControlsRunning(bool running);
};

#endif // BADGETESTWIDGET_H
