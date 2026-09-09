#include "serialport.h"
#include<QDebug>
Serialport::Serialport(QObject *parent):QObject(parent),
    serialPort_(new QSerialPort(this)),
    timer_(new QTimer(this))
{
//    serialPort_=new QSerialPort();
    timer_->setSingleShot(true);
//    config_=Config::load();
    reconnectIntervalMs_=config_.reconnectIntervalMs;
    connect(timer_,&QTimer::timeout,this,&Serialport::reconnect);
    timer_->start(reconnectIntervalMs_);
}

bool Serialport::open(Config &config)
{
    config_=config;
    loadPortConf();
}

bool Serialport::loadPortConf()
{
    qDebug()<<"开始设置串口配置";
    serialPort_->setPortName(config_.serialPort);
    serialPort_->setBaudRate(config_.baudRate);
    serialPort_->setDataBits(QSerialPort::Data8);
    serialPort_->setParity(QSerialPort::NoParity);
    serialPort_->setStopBits(QSerialPort::OneStop);
    serialPort_->setFlowControl(QSerialPort::NoFlowControl);
}

bool Serialport::close()
{
    serialPort_->close();
}

void Serialport::reconnect()
{

}
