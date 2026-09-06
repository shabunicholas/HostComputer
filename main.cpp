#include "mainwindow.h"

#include <QApplication>
#include"config.h"
#include"log.h"
#include<QDebug>
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    Log::updataLog(Level::NORMAL,"narmal");
    Log::updataLog(Level::WARN,"警告");
    Log::updataLog(Level::ALL,"总结信息");
    Log::updataLog(Level::NO,"未处理");
    return a.exec();
}
