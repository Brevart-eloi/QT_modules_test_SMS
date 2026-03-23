#ifndef TESTRFIDLECTEUR_H
#define TESTRFIDLECTEUR_H

#include <QWidget>
#include <QTimer>
#include <QVector>
#include "qmodbustcpclient.h"

class QPushButton;
class QLineEdit;
class QLabel;
class QTextEdit;
class QSpinBox;

class TestRfidLecteur : public QWidget
{
    Q_OBJECT

public:
    explicit TestRfidLecteur(QWidget* parent = nullptr);
    ~TestRfidLecteur();

private slots:
    // Boutons
    void onBtnConnecterClicked();
    void onBtnSurveillerClicked();

    // Timer
    void onPollTimerTimeout();

    // Réponses Modbus
    void onHoldingRegistersReceived(quint16 startAddress, QVector<quint16> values);

    // Socket TCP
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError socketError);

private:
    // === WIDGETS ===
    QLineEdit* lineEditIp;
    QSpinBox* spinBoxPort;
    QSpinBox* spinBoxUnitId;
    QPushButton* btnConnecter;
    QPushButton* btnSurveiller;

    QLabel* labelStatut;
    QLabel* labelCardId;
    QLabel* labelTagType;

    QTextEdit* textEditLog;

    // === LOGIQUE ===
    QModbusTcpClient* modbusClient;
    QTimer* pollTimer;

    bool isConnected;
    bool isSurveillance;

    // Dernière carte connue (pour détecter une NOUVELLE carte)
    QString dernierCardId;

    // === MÉTHODES ===
    void connecter();
    void deconnecter();
    void demarrerSurveillance();
    void arreterSurveillance();
    void lireRegistres();
    QString extraireCardId(const QVector<quint16>& values);
    QString tagTypeToString(quint16 tagType);
    void updateBoutons();
    void log(const QString& message);
};

#endif // TESTRFIDLECTEUR_H