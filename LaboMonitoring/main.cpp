#include <QApplication>
#include "LaboMonitoringWidget.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    LaboMonitoringWidget w;
    w.show();

    return app.exec();
}
