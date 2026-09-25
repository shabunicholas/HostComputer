#ifndef SERIALPORT_H
#define SERIALPORT_H

#include <QObject>
#include<QSerialPort>
#include<QTimer>
#include<QByteArray>
#include"config.h"
#include"log.h"
#include"ProtocolCommand.h"
#define MAX_PACK 9

class Serialport : public QObject
{
    Q_OBJECT
public:
    Serialport(QObject *parent=nullptr);
    bool open(Config &config);

    void close();

signals:
    void writeSig(uint8_t cmd,uint8_t param,int data);
    void readSig(uint8_t cmd,uint8_t param,int data);
//暂时不适应信号与曹 使用跨线程 虽然串口是不安全的(还是用信号槽了)
private slots:
    void readMes();
    void writeMes(uint8_t cmd,uint8_t param,int data);
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
    QByteArray buff_;
};

#endif // SERIALPORT_H
