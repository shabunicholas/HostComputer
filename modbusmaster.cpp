#include<QEventLoop>
#include<QTimer>
#include "modbusmaster.h"
#include"modbusprotocol.h"

/*取2个字节的高低位，先按位与 转无符号1字节（防止符号位扩展，污染数据），在转char 这时char
 * 结果需要char,就不用隐式转化类型了*/
namespace {
inline char hiByte(qint16 v){
       return static_cast<char>((static_cast<uint8_t>((v>>8) & 0xFF)));
}
inline char loByte(qint16 v){
       return static_cast<char>(static_cast<uint8_t>(v & 0xFF));
}
}


ModbusRtuMaster::ModbusRtuMaster(QIODevice *transport, QObject *parent):
                 QObject(parent),transport_(transport)
{
    qRegisterMetaType<QVector<quint16>>("QVector<quint16>");
    setConfig(config_);
}

ModbusRtuMaster::~ModbusRtuMaster(){

}

void ModbusRtuMaster::setTransport(QIODevice *transport)
{
    transport_=transport;
}

QIODevice *ModbusRtuMaster::transport() const
{
    return transport_;
}

void ModbusRtuMaster::setConfig(const ModbusRtuMaster::Config &config)
{
    config_=config;
    if(config.responseTimeoutMs<10){
        config_.responseTimeoutMs =10;
    }
    if(config.retries<0){
        config_.retries=0;
    }

    //传输速度太快，可能位1.75ms，最少去5ms兜底
    gapMs_=qMax(5,(int)Modbus::interFrameDelayMs(config_.baudRate)+2);
}

ModbusRtuMaster::Config ModbusRtuMaster::config() const
{
    return  config_;
}


//读保持寄存器
ModbusRtuMaster::Result ModbusRtuMaster::readHoldingRegisters(quint8 slave, quint16 start, quint16 count, QVector<quint16> &out, quint8 *exceptionCode)
{
    return readRegisters(slave,quint8(Modbus::ReadInputRegisters),start,count,out,exceptionCode);
}

//读只读寄存器
ModbusRtuMaster::Result ModbusRtuMaster::readInputRegisters(quint8 slave, quint16 start, quint16 count, QVector<quint16> &out, quint8 *exceptionCode)
{
    return readRegisters(slave,quint8(Modbus::ReadDiscreteInputs),start,count,out,exceptionCode);
}

ModbusRtuMaster::Result ModbusRtuMaster::writeSingleRegister(quint8 slave, quint16 address, quint16 value, quint8 *exceptionCode)
{
    if(exceptionCode){
        *exceptionCode=Modbus::ExcNone;
    }

    QByteArray payload;
    //除了crc其他都是大端
    payload.append(hiByte(address));
    payload.append(loByte(address));
    payload.append(hiByte(value));
    payload.append(loByte(value));

    QByteArray pdu;
    Result lastResult=transact(slave,uint8_t(Modbus::WriteSingleRegister),payload,pdu,exceptionCode);
    //检查从站应答
    if(lastResult!=Result::Ok){
       return lastResult;
    }
    if(pdu.size()!=6 || pdu.mid(2,4)!=payload){
       lastResult=Result::Malformed;
    }


    return lastResult;
}

ModbusRtuMaster::Result ModbusRtuMaster::writeMultipleRegisters(quint8 slave, quint16 start, const QVector<quint16> &values, quint8 *exceptionCode)
{
    if(exceptionCode){
        *exceptionCode=Modbus::ExcNone;
    }
    if (values.isEmpty() || values.size() > Modbus::MaxWriteRegisters){
        return Result::InvalidArgument;
    }
    QByteArray payload;
    int count=values.size();
    payload.append(hiByte(start));
    payload.append(loByte(start));
    payload.append(hiByte(count));
    payload.append(loByte(count));
    payload.append(static_cast<char>(quint8(count * 2)));
    for(int i=0;i<count;i++){
        payload.append(hiByte(values[i]));
        payload.append(hiByte(values[i]));
    }
    QByteArray pdu;
    Result lastResult=transact(slave,quint8(Modbus::WriteSingleRegister),payload,pdu,exceptionCode);
    if(lastResult!=Result::Ok){
       return lastResult;
    }
    if(pdu.size()!=6
          || quint8(pdu.at(2))!=quint8((start>>8) & 0xFF)
          || quint8(pdu.at(3))!=quint8(start & 0xFF)
          || quint8(pdu.at(4))!=quint8((count>>8) & 0xFF)
          || quint8(pdu.at(5))!=quint8(count & 0xFF)){
           lastResult=Result::Malformed;
    }

    return lastResult;
}

double ModbusRtuMaster::successRate() const
{
    if (tx_ == 0)
        return 0.0;
    return double(success_) / double(tx_);
}

void ModbusRtuMaster::resetStatistics()
{
    tx_ = rx_ = retries_ = timeouts_ = crcErrors_ = exceptions_ = success_ = 0;
    emit statisticsChanged();
}

QString ModbusRtuMaster::resultText(ModbusRtuMaster::Result result)
{
    switch (result) {
        case Result::Ok:                return QStringLiteral("成功");
        case Result::NotOpen:           return QStringLiteral("传输未打开或写入失败");
        case Result::InvalidArgument:   return QStringLiteral("本地参数非法（数量超限）");
        case Result::Timeout:           return QStringLiteral("超时应答（重试用尽）");
        case Result::CrcError:          return QStringLiteral("CRC 校验失败（重试用尽）");
        case Result::Malformed:         return QStringLiteral("帧结构错误或应答与请求不匹配");
        case Result::ExceptionResponse: return QStringLiteral("从站返回异常码");
        }
        return QStringLiteral("未知结果");
}

ModbusRtuMaster::Result ModbusRtuMaster::transact(quint8 slave, quint8 func, const QByteArray &payload, QByteArray &responsePdu, quint8 *exceptionCode)
{

    if(exceptionCode){
        *exceptionCode=Modbus::ExcNone;
    }
    if(!transport_ || !transport_->isOpen()){
        return Result::NotOpen;
    }
    Result r=Result::Ok;
    //组数据
    QByteArray frame;
    frame.append(slave);
    frame.append(func);
    frame.append(payload);
    frame=Modbus::appendCrc(frame);

    int count=config_.retries+1;
    for(int i=0;i<count;++i){
        if(i==0){
            drainInput();
        }else{
            retries_++;
            if(config_.discardStaleOnRetry){
                //半双工或者不稳定情况  RS-485
                resyncAfterTimeout();
            }else{
                //全双工 RS-232 内存回环测试（没有物理延迟），或能确定超时时刻线路上一定干净的场景
                drainInput();
            }
        }

        //写出去
        qint64 size=transport_->write(frame);
        if(size!=frame.size()){
           r=Result::NotOpen;
           continue;
        }
        tx_++;
        emit frameSent(slave,frame);
        emit statisticsChanged();

        //拿应答
        QByteArray adu;
        bool complete=false;
        if(!waitForResponse(func,adu,complete)){
            timeouts_++;
            r=Result::Timeout;
            emit errorOccurred(slave, r, Modbus::ExcNone);
            continue;
        }
        rx_++;
        emit frameReceived(slave,adu);

        //取pdu 只去掉了crc效验
        QByteArray pdu;
        if(!Modbus::stripAndCheckCrc(adu,&pdu)){
            crcErrors_++;
            r=Result::CrcError;
            emit errorOccurred(slave, r, Modbus::ExcNone);
            continue;
        }
        if (pdu.size() < 2 || quint8(pdu.at(0)) != slave) {
            r= Result::Malformed;
            continue;
        }
        //异常应答 最高位是1
        const quint8 respFunc=quint8(pdu.at(1));
        if(respFunc & 0x80){
            // 异常应答是「合法的应答」，说明从站收到了请求，绝不能重发
            const quint8 code = pdu.size() >= 3 ? quint8(pdu.at(2))
                                                : quint8(Modbus::ExcSlaveDeviceFailure);
            if (exceptionCode)
                *exceptionCode = code;
            ++exceptions_;
            emit errorOccurred(slave, Result::ExceptionResponse, code);
            emit statisticsChanged();
            return Result::ExceptionResponse;
        }
        if (respFunc != func) {
            r = Result::Malformed;                 // 应答与请求不匹配
            continue;
        }
        success_++;
        responsePdu=pdu;
        emit statisticsChanged();
        return Result::Ok;
    }
    return r;
}

ModbusRtuMaster::Result ModbusRtuMaster::readRegisters(quint8 slave, quint8 function, quint16 start, quint16 count, QVector<quint16> &out, quint8 *exceptionCode)
{
    if(exceptionCode){
       *exceptionCode=Modbus::ExcNone;
    }
    if(count<1 || count>Modbus::MaxReadRegisters){
        return Result::InvalidArgument;
    }
    QByteArray payload;
    payload.append(hiByte(start));
    payload.append(loByte(start));
    payload.append(hiByte(count));
    payload.append(loByte(count));
    QByteArray pdu;
    Result lastResult=transact(slave,function,payload,pdu,exceptionCode);
    if(lastResult!=Result::Ok){
        return lastResult;
    }
    if(pdu.size()<3 || pdu.at(2)!=count*2){
        lastResult=Result::Malformed;
    }

    for(int i=0;i<count;i++){
        const int off=3+i*2;
        quint16 c=quint16((quint8(off))<<8 | quint8(pdu.at(off+1)));
        out.append(c);
    }
    emit registersRead(slave, start, out);
    return lastResult;
}

bool ModbusRtuMaster::waitForResponse(quint8 requestFunc, QByteArray &raw, bool &complete)
{
    //消除不使用参数的警告
    Q_UNUSED(requestFunc);
    if(!transport_ || !transport_->isOpen()){
        return false;
    }
    raw.clear();
    complete=false;
    bool timeout=false;
    //总超时和喂狗
    QTimer total;
    QTimer dog;
    dog.setSingleShot(true);
    total.setSingleShot(true);
    QEventLoop loop;

    const auto onData=[&](){
        raw.append(transport_->readAll());
        int size=expectedResponseLength(raw);
        if(size>0 && raw.size()>=size){
           complete = true;
           loop.quit();
           return;
        }
        dog.start(gapMs_);
    };
    connect(transport_,&QIODevice::readyRead,&loop,onData);
    connect(&total,&QTimer::timeout,[&](){
        timeout=true;
        loop.quit();
    });
    connect(&dog,&QTimer::timeout,[&](){
        complete=!raw.isEmpty();
        loop.quit();
    });

    if(transport_->bytesAvailable()>0){
       raw.append(transport_->readAll());
    }
    if(!complete){
       loop.exec();
    }
    total.start(config_.responseTimeoutMs);
    transport_->disconnect(&loop);

    return complete && !timeout;
}

int ModbusRtuMaster::expectedResponseLength(const QByteArray &rx) const
{
    //连从站地址和功能码都没有
    if(rx.size()<2){
       return -1;
    }
    quint8 fun=quint8(rx.at(1));

    //异常[地址] [功能码|0x80] [异常码] [CRC低] [CRC高]
    if(fun & 0x80)
      return 5;
    switch (fun) {
    case Modbus::ReadHoldingRegisters:
    case Modbus::ReadInputRegisters:
         //从站地址 功能码 数据大小 数据 crc低 crc高
         if(rx.size()<3)return -1;
         return 3+ int(quint8(rx.at(2)))+2;
    case Modbus::WriteSingleRegister:
    case Modbus::WriteMultipleRegisters:
        //从站地址 功能码 起始地址(2) 数量(2) crc低 crc高
        return 8;
    default:
        //交给定时器
        return -1;
    }
}

void ModbusRtuMaster::drainInput()
{
    if (transport_)
        //丢弃残留字节，避免上一轮的迟到应答造成错位
        transport_->readAll();
}

void ModbusRtuMaster::resyncAfterTimeout()
{
    drainInput();
    if(!transport_ || !transport_->isOpen())return;
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer,&QTimer::timeout,[&](){
        loop.quit();
    });
    connect(transport_,&QIODevice::readyRead,[&](){
        //一直丢，直到总线静默为止
        transport_->readAll();
        timer.start(gapMs_);
    });
    timer.start(gapMs_);
    loop.exec();
    transport_->disconnect(&loop);
    drainInput();
}
