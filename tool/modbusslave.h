#ifndef MODBUSRTUSLAVE_H
#define MODBUSRTUSLAVE_H

/*
 * Modbus RTU 从站（模拟器）
 *
 * 既可以当测试用从站，也可以直接当成「虚拟设备」接到上位机上调试界面。
 * 传输层是 QIODevice：真实串口传 QSerialPort，离线自测传 LoopbackDevice。
 *
 * 已实现功能码：03 读保持寄存器、04 读输入寄存器、06 写单个寄存器、10 写多个寄存器。
 * 其余功能码（线圈 01/02/05/0F）一律回异常码 01，接口按同一模式扩展即可。
 */

#include "modbusprotocol.h"

#include <QByteArray>
#include <QIODevice>
#include <QMap>
#include <QObject>
#include <QtGlobal>

class ModbusRtuSlave : public QObject
{
    Q_OBJECT
public:
    explicit ModbusRtuSlave(QIODevice *transport, quint8 address = 0x01,
                            QObject *parent = nullptr);

    void   setAddress(quint8 address);
    quint8 address() const { return address_; }

    // 合法地址区间，超出即回异常码 02
    void setRegisterRange(quint16 start, quint16 count);

    void    setHoldingRegister(quint16 address, quint16 value);
    quint16 holdingRegister(quint16 address) const;
    void    setInputRegister(quint16 address, quint16 value);   // 模拟采集值
    quint16 inputRegister(quint16 address) const;

    // ---- 故障注入：用来验证主站的超时重发 / CRC 重试 ----
    void setResponseDelayMs(int ms)     { responseDelayMs_ = ms; }
    void setDropResponseEveryNth(int n) { dropEveryNth_ = n; dropCounter_ = 0; }
    void setCorruptCrcEveryNth(int n)   { corruptEveryNth_ = n; corruptCounter_ = 0; }
    void setForcedException(quint8 function, quint8 exception);

    quint32 requestCount() const { return requests_; }
    quint32 crcErrorCount() const { return crcErrors_; }

signals:
    void requestReceived(quint8 slave, quint8 function, const QByteArray &pdu);
    void responseSent(quint8 slave, const QByteArray &frame);
    void exceptionSent(quint8 slave, quint8 function, quint8 exception);
    void requestCrcError();

private slots:
    void onReadyRead();

private:
    int        expectedRequestLength(const QByteArray &rx) const;
    void       handleFrame(const QByteArray &pdu);        // 入参是已去掉 CRC 的 PDU
    void       sendFrame(const QByteArray &frame, quint8 slave);
    QByteArray makeExceptionAdu(quint8 function, quint8 code) const;
    bool       inRange(quint16 start, quint16 count) const;

    QIODevice *transport_ = nullptr;
    quint8     address_ = 0x01;
    quint16    rangeStart_ = 0;
    quint16    rangeCount_ = 1000;

    QMap<quint16, quint16> holding_;   // 保持寄存器（可读写）
    QMap<quint16, quint16> input_;     // 输入寄存器（只读，代表采集值）

    QByteArray rx_;                    // 接收缓冲，含 CRC 的完整帧在此拼装

    int    responseDelayMs_ = 0;
    int    dropEveryNth_ = 0,    dropCounter_ = 0;
    int    corruptEveryNth_ = 0, corruptCounter_ = 0;
    quint8 forcedFunc_ = 0, forcedException_ = 0;

    quint32 requests_ = 0, crcErrors_ = 0;
};

#endif // MODBUSRTUSLAVE_H
