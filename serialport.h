#ifndef SERIALPORT_H
#define SERIALPORT_H

#include <QObject>
#include<QSerialPort>
#include<QTimer>
#include"config.h"
#include"log.h"
class Serialport : public QObject
{
    Q_OBJECT
public:
    Serialport(QObject *parent=nullptr);
    bool open(Config &config);

    void close();

signals:
    void writeSig();
    void readSig();
//private slots:暂时不适应信号与曹 使用跨线程 虽然串口是不安全的
//    bool readMes();
//    bool writeMes();
private:
    void loadPortConf();
    bool reconnect();
    void error(QSerialPort::SerialPortError);
private:
    QSerialPort *serialPort_;
    Config config_;
    QTimer *timer_;
    bool userClose_;
    int reconnectIntervalMs_;
};

#endif // SERIALPORT_H
