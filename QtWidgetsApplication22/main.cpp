#include "QtWidgetsApplication22.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QtWidgetsApplication22 window;
    window.show();
    return app.exec();
}
