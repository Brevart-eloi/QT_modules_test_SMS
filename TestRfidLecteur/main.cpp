/*
#include <QApplication>
#include "TestRfidLecteur.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    TestRfidLecteur window;
    window.setWindowTitle("Test Lecteur RFID - Modbus TCP");
    window.resize(650, 550);
    window.show();

    return app.exec();
}
*/

#include <QApplication>
#include "LaboMonitoringTestWidget.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    LaboMonitoringTestWidget w;
    w.show();

    return app.exec();
}