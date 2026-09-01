#include "config.h"
#include<QDir>
#include<QCoreApplication>
#include<QSettings>
#include<QDebug>
#include<QFile>
//Config::Config()
//{

//}

Config Config::load(const QString &path)
{
    Config config;
    QString configPath=path;

    if(path.isEmpty()){
        configPath=QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("config.ini"));
    }

    QSettings settings(configPath,QSettings::IniFormat);
    config.serialPort=settings.value(QStringLiteral("Serial/Port"),
                                     config.serialPort).toString();
    config.baudRate=settings.value(QStringLiteral("Serial/BaudRate"),
                                     config.baudRate).toInt();
    config.reconnectIntervalMs=settings.value(QStringLiteral("Serial/ReconnectIntervalMs"),
                                     config.reconnectIntervalMs).toInt();
    config.pressureThreshold=settings.value(QStringLiteral("Alarm/PressureThreshold"),
                                     config.pressureThreshold).toInt();
    config.pollingIntervalMs=settings.value(QStringLiteral("Polling/IntervalMs"),
                                     config.pollingIntervalMs).toInt();

    return config;
}
