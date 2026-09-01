#include "mainwindow.h"

#include <QApplication>
#include"config.h"
#include<QDebug>
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    Config f=Config::load("../config.ini");
    qDebug()<<f.serialPort;
    qDebug()<<f.baudRate;
    qDebug()<<f.reconnectIntervalMs;
    qDebug()<<f.pressureThreshold;
    qDebug()<<f.pollingIntervalMs;
    return a.exec();
}
