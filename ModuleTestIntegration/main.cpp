#include <QApplication>
#include "IntegrationTestWidget.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    IntegrationTestWidget w;
    w.show();

    return app.exec();
}
