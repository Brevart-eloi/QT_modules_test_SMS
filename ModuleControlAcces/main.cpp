#include <QApplication>
#include "ControlAccesWidget.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    ControlAccesWidget w;
    w.show();

    return app.exec();
}
