#ifndef CONFIG_H
#define CONFIG_H

#include<QString>
class Config
{
public:
    // Config();
     static Config load(const QString &path="");

     QString serialPort=QStringLiteral("COM4");
     int baudRate = 19200;// 波特率
     int pressureThreshold = 1150;//压力阈值(Pa)
     int pollingIntervalMs = 2000;//轮询间隔(ms)
     int reconnectIntervalMs = 3000;//串口重连间隔


};

#endif // CONFIG_H
