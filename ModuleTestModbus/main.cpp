#include "test_module_bus.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    test_module_bus window;
    window.show();
    return app.exec();
}
