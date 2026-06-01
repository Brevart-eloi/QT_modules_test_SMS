#include <QApplication>
#include "BadgeTestWidget.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    BadgeTestWidget w;
    w.show();

    return app.exec();
}
