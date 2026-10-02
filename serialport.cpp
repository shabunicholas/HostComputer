#include "serialport.h"
#include<QDebug>
#include"log.h"
static int reConut=0;
static constexpr uint8_t frame_header=0xEF;
static constexpr uint8_t frame_tail=0xFE;
Serialport::Serialport(QObject *parent):QObject(parent),
    serialPort_(new QSerialPort(this)),
    timer_(new QTimer(this)),
    timerPoll_(new QTimer(this))
{
//    serialPort_=new QSerialPort();
    timer_->setSingleShot(true);
//    config_=Config::load();
    reconnectIntervalMs_=2000;
//    connect(timer_,&QTimer::timeout,this,&Serialport::reconnect);
//    connect(serialPort_,&QSerialPort::errorOccurred,this,&Serialport::error);
    //timer_->start(reconnectIntervalMs_);
}

bool Serialport::open(const Config &config)
{
    userClose_=false;
    config_=config;
    qDebug()<<"congif_ name"<<config_.serialPort;

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

void Serialport::init()
{
    connect(timer_,&QTimer::timeout,this,&Serialport::reconnect);
    connect(serialPort_,&QSerialPort::errorOccurred,this,&Serialport::error);
    //connect(this,&Serialport::readSig,this,&Serialport::readMes);
    connect(serialPort_,&QSerialPort::readyRead,this,&Serialport::readMes);
    //connect(this,&Serialport::writeSig,this,&Serialport::writeMes);
    connect(timerPoll_,&QTimer::timeout,this,&Serialport::writePoll);
    open(Config::load());

    timerPoll_->start(3000);
}

void Serialport::readMes()
{
    //buff_=serialPort_->readAll();
    buff_.append(serialPort_->readAll());
    //定义一个处理分包粘包的函数
    qDebug()<<"buff:"<<buff_;
    while(true){
        QByteArray frame= takeOneFrame();
        if(frame.isEmpty())return;
        uint8_t cmd=0;
        uint8_t param=0;
        int data=0;
        if(ProtocolCodec::parse(frame,cmd,param,data)){
            emit readData(cmd, param,data);
            Log::updataLog(Level::NORMAL,QStringLiteral("解析成功"));
        }else{
            Log::updataLog(Level::WARN,QStringLiteral("解析失败"));
        }
    }
}

void Serialport::writeMes(uint8_t cmd,uint8_t param,int data)
{

    QByteArray buf=ProtocolCodec::pack(cmd,param,data);

    qint64 size= serialPort_->write(buf);
    if(size==-1){
        Log::updataLog(Level::ERROR,QString("串口写入失败:%1").arg(
                           serialPort_->errorString()));
        return;
    }
    qDebug()<<"写了:"<<size<<"字节 ("<<buf<<")";

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
       QString str=QString("尝试次数%1").arg(reConut);
       Log::updataLog(Level::ERROR,str);
       timer_->start(reconnectIntervalMs_+(reConut-1)*1000);
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

void Serialport::writePoll()
{
    //前面是命令，后面是编号(参数)
    uint8_t cmd[][2]={
        {static_cast<uint8_t>(COMMAND::WEN_DU_REQ),0x01},
    };
    size_t size=sizeof(cmd)/sizeof(cmd[0]);

    for(size_t i=0;i<size;i++){
        writeMes(cmd[i][0],cmd[i][1],0x5);
    }
}

QByteArray Serialport::takeOneFrame()
{
    if(buff_.isEmpty())return QByteArray();
    while(!buff_.isEmpty()){
        int index=buff_.indexOf(static_cast<char>(frame_header));
        //去掉无效头
        if(index>=0){
          buff_=buff_.mid(index);
        }else{
            buff_.clear();
            return QByteArray();
        }

        if(buff_.size()<MAX_PACK)break;

        QByteArray frame=buff_.left(MAX_PACK);

        if(static_cast<uint8_t>(frame.at(MAX_PACK-1))!=frame_tail){
           //重新寻找下一个包头
           buff_.remove(0,1);
           continue;
        }
        //找到了一个包
        buff_.remove(0,MAX_PACK);
        return frame;
    }
    return QByteArray();
}
