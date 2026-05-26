#include "vrification_droit_accs.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    vrification_droit_accs window;
    window.show();
    return app.exec();
}
