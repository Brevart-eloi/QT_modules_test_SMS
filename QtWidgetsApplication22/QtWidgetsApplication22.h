#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_QtWidgetsApplication22.h"

class QtWidgetsApplication22 : public QMainWindow
{
    Q_OBJECT

public:
    QtWidgetsApplication22(QWidget *parent = nullptr);
    ~QtWidgetsApplication22();

private:
    Ui::QtWidgetsApplication22Class ui;
};

