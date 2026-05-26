#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_test_module_bus.h"
#include <qmodbustcpclient.h>
#include <QDateTime>

class test_module_bus : public QMainWindow
{
    Q_OBJECT

public:
    test_module_bus(QWidget *parent = nullptr);
    ~test_module_bus();

private:
    Ui::test_module_busClass ui;

    QModbusTcpClient* client;

private slots:
    void onConnected();
    void onButtonClicked();
};

