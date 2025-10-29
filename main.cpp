#include "cpu_server.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    CpuServer server;
    server.show();

    return app.exec();
}
