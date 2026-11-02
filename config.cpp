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
    qDebug()<<"configPaht"<<configPath;
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

    // 协议选择与 Modbus 参数
    config.protocol=settings.value(QStringLiteral("Protocol/Name"),
                                   config.protocol).toString();
    config.modbusSlaveAddress=settings.value(QStringLiteral("Protocol/ModbusSlaveAddress"),
                                             config.modbusSlaveAddress).toInt();
    config.modbusResponseTimeoutMs=settings.value(QStringLiteral("Protocol/ModbusResponseTimeoutMs"),
                                                  config.modbusResponseTimeoutMs).toInt();
    config.modbusRetries=settings.value(QStringLiteral("Protocol/ModbusRetries"),
                                        config.modbusRetries).toInt();
    config.modbusTempRegister=settings.value(QStringLiteral("Protocol/ModbusTempRegister"),
                                             config.modbusTempRegister).toInt();
    config.modbusPressureRegister=settings.value(QStringLiteral("Protocol/ModbusPressureRegister"),
                                                 config.modbusPressureRegister).toInt();

    return config;
}
