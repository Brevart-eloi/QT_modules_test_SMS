#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_test_module_bus.h"
#include <qmodbustcpclient.h>
#include <QDateTime>
#include <QLabel>

class test_module_bus : public QMainWindow
{
    Q_OBJECT

public:
    test_module_bus(QWidget *parent = nullptr);
    ~test_module_bus();

private:
    Ui::test_module_busClass ui;

    QModbusTcpClient* client;

    QLabel* m_lblArmed = nullptr;   // indicateur d'etat arme / desarme
    bool    m_armed    = false;     // etat courant du systeme
    void setArmed(bool armed);      // met a jour l'etat + l'affichage + le journal

private slots:
    void onConnected();
    void onButtonClicked();
};

