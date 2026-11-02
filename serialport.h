#ifndef SERIALPORT_H
#define SERIALPORT_H

#include <QObject>
#include<QSerialPort>
#include<QTimer>
#include<QByteArray>
#include"config.h"
#include"log.h"
#include"ProtocolCommand.h"
#include"modbusmaster.h"
#define MAX_PACK 9
class MainWindow;
class Serialport : public QObject
{
    Q_OBJECT
    friend class MainWindow;
public:
    Serialport(QObject *parent=nullptr);
    bool open(const Config &config);

    void close();


signals:
    //void writeSig(uint8_t cmd,uint8_t param,int data);
    void readSig();
    void readData(uint8_t cmd,uint8_t param,int data);
//暂时不适应信号与曹 使用跨线程 虽然串口是不安全的(还是用信号槽了)
private slots:
    void init();
    void readMes();
    void writeMes(uint8_t cmd,uint8_t param,int data);
private:
    void loadPortConf();
    void reconnect();
    void error(QSerialPort::SerialPortError);
    void writePoll();       // 按 config_.protocol 分发
    void pollCustom();      // 自定义帧协议（原 writePoll 逻辑）
    void pollModbus();      // Modbus RTU 主站轮询
    QByteArray takeOneFrame();
private:
    QSerialPort *serialPort_;
    Config config_;
    QTimer *timer_;
    QTimer *timerPoll_;
    bool userClose_;
    int reconnectIntervalMs_;
    QByteArray buff_;
    int reConut_;
    ModbusRtuMaster *modbus_ = nullptr;   // 仅 protocol=modbus 时创建
};

#endif // SERIALPORT_H
