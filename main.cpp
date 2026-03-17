#include "QT_modules_test_SMS.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QT_modules_test_SMS window;
    window.show();
    return app.exec();
}
