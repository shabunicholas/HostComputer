#include "mainwindow.h"

#include <QApplication>
#include"config.h"
#include"log.h"
#include"serialport.h"
#include<QDebug>
int main(int argc, char *argv[])
{
    qRegisterMetaType<uint8_t>("uint8_t");
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}
