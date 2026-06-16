#include <QApplication>
#include "IntegrationVideoWidget.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    IntegrationVideoWidget w;
    w.show();

    return app.exec();
}
