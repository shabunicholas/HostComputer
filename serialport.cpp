#include "serialport.h"
#include<QDebug>
#include"log.h"
#include"modbusprotocol.h"
//static int reConut=0;
static constexpr uint8_t frame_header=0xEF;
static constexpr uint8_t frame_tail=0xFE;
Serialport::Serialport(QObject *parent):QObject(parent),
    serialPort_(new QSerialPort(this)),
    timer_(new QTimer(this)),
    timerPoll_(new QTimer(this)),
    reConut_(0)
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
    // Modbus RTU 规范要求 11 位/字符（8E1）；自定义帧保持原来的 8N1
    const bool isModbus=config_.protocol.compare(QStringLiteral("modbus"),
                                                 Qt::CaseInsensitive)==0;
    serialPort_->setParity(isModbus ? QSerialPort::EvenParity : QSerialPort::NoParity);
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
    connect(timerPoll_,&QTimer::timeout,this,&Serialport::writePoll);

    open(Config::load());

    const bool isModbus=config_.protocol.compare(QStringLiteral("modbus"),
                                                 Qt::CaseInsensitive)==0;
    if(isModbus){
        // 关键：Modbus 模式下主站自己在 waitForResponse() 里读串口，
        // 绝不能再连 readyRead->readMes，否则两边抢同一批字节，症状是"永远超时"
        modbus_=new ModbusRtuMaster(serialPort_,this);
        ModbusRtuMaster::Config mc=modbus_->config();
        mc.baudRate=config_.baudRate;
        mc.responseTimeoutMs=config_.modbusResponseTimeoutMs;
        mc.retries=config_.modbusRetries;
        modbus_->setConfig(mc);

        connect(modbus_,&ModbusRtuMaster::errorOccurred,this,
                [](quint8 slave,ModbusRtuMaster::Result r,quint8 exc){
            if(r==ModbusRtuMaster::Result::ExceptionResponse){
                Log::updataLog(Level::WARN,QString("Modbus 从站%1异常:%2")
                               .arg(slave).arg(Modbus::exceptionText(exc)));
            }else{
                Log::updataLog(Level::WARN,QString("Modbus 通信失败(从站%1):%2")
                               .arg(slave).arg(ModbusRtuMaster::resultText(r)));
            }
        });
        Log::updataLog(Level::NORMAL,QString("协议=Modbus RTU，从站地址 %1，超时 %2ms")
                       .arg(config_.modbusSlaveAddress)
                       .arg(config_.modbusResponseTimeoutMs));
    }else{
        connect(serialPort_,&QSerialPort::readyRead,this,&Serialport::readMes);
        Log::updataLog(Level::NORMAL,QStringLiteral("协议=自定义帧"));
    }

    qDebug()<<config_.pollingIntervalMs;
    timerPoll_->start(config_.pollingIntervalMs);
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
            //Log::updataLog(Level::NORMAL,QStringLiteral("解析成功"));
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
    //qDebug()<<"写了:"<<size<<"字节 ("<<buf<<")";

}

//bool 会被忽略 除非手动调用
void Serialport::reconnect()
{
    if(userClose_)return;
    if(serialPort_->isOpen()){
        QString str=QString("串口打开了，结束重连:%1").arg(serialPort_->portName());
        Log::updataLog(Level::NORMAL,str);
        timer_->stop();
        return;
    }

    loadPortConf();

    if(serialPort_->open(QIODevice::ReadWrite)){
        QString str=QString("串口重连成功:%1").arg(serialPort_->portName());
        Log::updataLog(Level::NORMAL,str);
        timer_->stop();
        reConut_=0;
        return;
    }
    reConut_++;
    QString str=QString("串口重连失败:%1 尝试重连中...").arg(serialPort_->errorString());
    Log::updataLog(Level::NORMAL,str);

    if(reConut_>=3){
       QString str=QString("重连次数过多(%1次)，停止重连").arg(reConut_);
       Log::updataLog(Level::ERROR,str);
       timer_->stop();
       return;
    }
    QString str1=QString("尝试次数%1").arg(reConut_);
    Log::updataLog(Level::ERROR,str1);
    timer_->start(reconnectIntervalMs_+(reConut_-1)*1000);


}

void Serialport::error(QSerialPort::SerialPortError er)
{
    if(er==QSerialPort::NoError || userClose_){
        return;
    }
    Log::updataLog(Level::WARN,
            QString("串口错误:%1").arg(serialPort_->errorString()));
}

void Serialport::writePoll()
{
    const bool isModbus=config_.protocol.compare(QStringLiteral("modbus"),
                                                 Qt::CaseInsensitive)==0;
    if(isModbus){
        pollModbus();
    }else{
        pollCustom();
    }
}

// 自定义帧协议：原来的盲发轮询，逻辑不动
void Serialport::pollCustom()
{
    if(!serialPort_->isOpen()){
        return;
    }
    //可以加入优先队列,之后把读写拆出去
    //前面是命令，后面是编号(参数)
    uint8_t cmd[][2]={
        {static_cast<uint8_t>(COMMAND::WEN_DU_REQ),0x01},
        {static_cast<uint8_t>(COMMAND::YI_LI_GAN_REQ),0x01},
        {static_cast<uint8_t>(COMMAND::YI_LI_GAN_REQ),0x02},
        {static_cast<uint8_t>(COMMAND::YI_LI_GAN_REQ),0x03},
    };
    size_t size=sizeof(cmd)/sizeof(cmd[0]);

    for(size_t i=0;i<size;i++){
        writeMes(cmd[i][0],cmd[i][1],0);
    }
}

// Modbus RTU：请求-应答模式。主站在工作线程里同步等待，不会卡界面
void Serialport::pollModbus()
{
    if(!modbus_ || !serialPort_->isOpen()){
        return;
    }

    const quint8 slave=quint8(config_.modbusSlaveAddress);
    quint8 exc=0;
    QVector<quint16> regs;

    // 温度：读 1 个输入寄存器
    ModbusRtuMaster::Result r=modbus_->readInputRegisters(
                slave,quint16(config_.modbusTempRegister),1,regs,&exc);
    if(r==ModbusRtuMaster::Result::Ok && regs.size()==1){
        emit readData(static_cast<uint8_t>(COMMAND::WEN_DU_RSP),0x01,int(regs.at(0)));
    }else if(r==ModbusRtuMaster::Result::ExceptionResponse){
        Log::updataLog(Level::WARN,QString("读温度被从站拒绝:%1")
                       .arg(Modbus::exceptionText(exc)));
    }else{
        Log::updataLog(Level::ERROR,QString("读温度失败:%1")
                       .arg(ModbusRtuMaster::resultText(r)));
    }

    // 压力：从指定地址起连续读 3 个
    regs.clear();
    exc=0;
    r=modbus_->readInputRegisters(slave,quint16(config_.modbusPressureRegister),
                                  3,regs,&exc);
    if(r==ModbusRtuMaster::Result::Ok && regs.size()==3){
        for(int i=0;i<3;++i){
            emit readData(static_cast<uint8_t>(COMMAND::YI_LI_GAN_RSP),
                          quint8(i+1),int(regs.at(i)));
        }
    }else if(r==ModbusRtuMaster::Result::ExceptionResponse){
        Log::updataLog(Level::WARN,QString("读压力被从站拒绝:%1")
                       .arg(Modbus::exceptionText(exc)));
    }else{
        Log::updataLog(Level::ERROR,QString("读压力失败:%1")
                       .arg(ModbusRtuMaster::resultText(r)));
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
