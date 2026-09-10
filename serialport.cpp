#include "serialport.h"
#include<QDebug>
#include"log.h"
static int reConut=0;
Serialport::Serialport(QObject *parent):QObject(parent),
    serialPort_(new QSerialPort(this)),
    timer_(new QTimer(this))
{
//    serialPort_=new QSerialPort();
    timer_->setSingleShot(true);
//    config_=Config::load();
    reconnectIntervalMs_=2000;
    connect(timer_,&QTimer::timeout,this,&Serialport::reconnect);
    connect(serialPort_,&QSerialPort::errorOccurred,this,&Serialport::error);
    //timer_->start(reconnectIntervalMs_);
}

bool Serialport::open(Config &config)
{
    userClose_=false;
    config_=config;
    reconnectIntervalMs_=config.reconnectIntervalMs;
    if(serialPort_->isOpen()){
       QString str=QString("串口已经打开了:%1").arg(serialPort_->portName());
       Log::updataLog(Level::NORMAL,str);
       return true;
    }
    loadPortConf();

    if(!serialPort_->open(QIODevice::ReadWrite)){
        QString str=QString("串口打开失败:%1").arg(serialPort_->errorString());
        Log::updataLog(Level::NORMAL,str);
        timer_->start(reconnectIntervalMs_);
        return false;
    }
    QString str=QString("串口打开成功:%1").arg(serialPort_->portName());
    Log::updataLog(Level::NORMAL,str);
    return true;
}

void Serialport::loadPortConf()
{
    qDebug()<<"开始设置串口配置";
    serialPort_->setPortName(config_.serialPort);
    serialPort_->setBaudRate(config_.baudRate);
    serialPort_->setDataBits(QSerialPort::Data8);
    serialPort_->setParity(QSerialPort::NoParity);
    serialPort_->setStopBits(QSerialPort::OneStop);
    serialPort_->setFlowControl(QSerialPort::NoFlowControl);
}

void Serialport::close()
{
    userClose_=true;
    timer_->stop();
    if(serialPort_->isOpen()){
        serialPort_->close();
    }

}

//bool 会被忽略 除非手动调用
bool Serialport::reconnect()
{
    if(userClose_)return false;
    if(serialPort_->isOpen()){
        QString str=QString("串口打开了，结束重连:%1").arg(serialPort_->portName());
        Log::updataLog(Level::NORMAL,str);
        timer_->stop();
        return true;
    }

    loadPortConf();

    if(serialPort_->open(QIODevice::ReadWrite)){
        QString str=QString("串口重连成功:%1").arg(serialPort_->portName());
        Log::updataLog(Level::NORMAL,str);
        timer_->stop();
        reConut=0;
        return true;
    }
    reConut++;
    QString str=QString("串口重连失败:%1 尝试重连中...").arg(serialPort_->errorString());
    Log::updataLog(Level::NORMAL,str);

    if(reConut<=3){
       QString str=QString("尝试次数%1，请重启程序:%2").arg(reConut)
               .arg(serialPort_->errorString());
       Log::updataLog(Level::ERROR,str);
       timer_->start(reconnectIntervalMs_);
    }

    return false;
}

void Serialport::error(QSerialPort::SerialPortError er)
{
    if(er==QSerialPort::NoError || userClose_){
        return;
    }

    if(!timer_->isActive()){
       timer_->start(reconnectIntervalMs_);
    }
}
