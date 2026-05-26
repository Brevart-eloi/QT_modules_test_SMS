#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_vrification_droit_accs.h"

class vrification_droit_accs : public QMainWindow
{
    Q_OBJECT

public:
    vrification_droit_accs(QWidget *parent = nullptr);
    ~vrification_droit_accs();

private:
    Ui::vrification_droit_accsClass ui;
};

