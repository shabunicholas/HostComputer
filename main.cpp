#include "mainwindow.h"

#include <QApplication>
#include"config.h"
#include"log.h"
#include"serialport.h"
#include<QDebug>
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    Config config=Config::load();
    Serialport port(nullptr);
    port.open(config);
    return a.exec();
}
