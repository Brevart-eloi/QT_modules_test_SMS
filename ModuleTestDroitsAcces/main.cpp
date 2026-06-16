#include <QApplication>
#include "DroitsAccesTestWidget.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    DroitsAccesTestWidget w;
    w.show();

    return app.exec();
}
