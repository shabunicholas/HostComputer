#include "modbusslave.h"

#include <QTimer>

ModbusRtuSlave::ModbusRtuSlave(QIODevice *transport, quint8 address, QObject *parent)
    : QObject(parent), transport_(transport), address_(address)
{
    if (transport_) {
        connect(transport_, &QIODevice::readyRead, this, &ModbusRtuSlave::onReadyRead);
        connect(transport_, &QIODevice::aboutToClose,[this]() { rx_.clear(); });
    }
}

void ModbusRtuSlave::setAddress(quint8 address)
{
    if (address >= 1 && address <= 247)
        address_ = address;
}

void ModbusRtuSlave::setRegisterRange(quint16 start, quint16 count)
{
    rangeStart_ = start;
    rangeCount_ = count;
}

void ModbusRtuSlave::setHoldingRegister(quint16 address, quint16 value)
{
    holding_.insert(address, value);
}

quint16 ModbusRtuSlave::holdingRegister(quint16 address) const
{
    return holding_.value(address, 0);
}

void ModbusRtuSlave::setInputRegister(quint16 address, quint16 value)
{
    input_.insert(address, value);
}

quint16 ModbusRtuSlave::inputRegister(quint16 address) const
{
    return input_.value(address, 0);
}

void ModbusRtuSlave::setForcedException(quint8 function, quint8 exception)
{
    forcedFunc_ = function;
    forcedException_ = exception;
}

bool ModbusRtuSlave::inRange(quint16 start, quint16 count) const
{
    if (count == 0)
        return false;
    const int first = int(start);
    const int last = first + int(count) - 1;
    const int rangeFirst = int(rangeStart_);
    const int rangeLast = rangeFirst + int(rangeCount_) - 1;
    return first >= rangeFirst && last <= rangeLast;
}

/* rx_ 里是含 CRC 的原始字节流，所以这里算的是「整帧（ADU）长度」 */
int ModbusRtuSlave::expectedRequestLength(const QByteArray &rx) const
{
    if (rx.size() < 2)
        return -1;

    switch (quint8(rx.at(1))) {
    case Modbus::ReadCoils:
    case Modbus::ReadDiscreteInputs:
    case Modbus::ReadHoldingRegisters:
    case Modbus::ReadInputRegisters:
    case Modbus::WriteSingleCoil:
    case Modbus::WriteSingleRegister:
        return 8;                                       // 固定 8 字节请求
    case Modbus::WriteMultipleCoils:
    case Modbus::WriteMultipleRegisters:
        if (rx.size() < 7)
            return -1;
        return 9 + int(quint8(rx.at(6)));               // 9 + 字节数
    default:
        return 8;                                       // 未知功能码：收下最短帧再回异常 01
    }
}

//解析adu
void ModbusRtuSlave::onReadyRead()
{
    if (!transport_)
        return;
    rx_.append(transport_->readAll());

    while (!rx_.isEmpty()) {
        const int need = expectedRequestLength(rx_);
        if (need < 0)
            break;                                      // 信息不足，等下一批
        if (rx_.size() < need)
            break;                                      // 半包，继续等

        const QByteArray frame = rx_.left(need);
        rx_.remove(0, need);

        QByteArray pdu;
        if (!Modbus::stripAndCheckCrc(frame, &pdu)) {
            ++crcErrors_;
            emit requestCrcError();
            // 请求 CRC 错：按规范静默丢弃，由主站超时重发。
            // 同时回退一个字节重新找帧边界（每次净少 1 字节，不会死循环）。
            rx_.prepend(frame.mid(1));
            if (rx_.size() < 2)
                break;
            continue;
        }
        handleFrame(pdu);                               // 传入已校验、且已去掉 CRC 的 PDU
    }
}

//产生异常码
QByteArray ModbusRtuSlave::makeExceptionAdu(quint8 function, quint8 code) const
{
    QByteArray adu;
    adu.append(static_cast<char>(address_));
    adu.append(static_cast<char>(quint8(function | 0x80)));
    adu.append(static_cast<char>(code));
    return Modbus::appendCrc(adu);
}

/* 注意：pdu 已经去掉最后 2 字节 CRC，长度口径与 expectedRequestLength() 不同 */
void ModbusRtuSlave::handleFrame(const QByteArray &pdu)
{
    if (pdu.size() < 2)
        return;

    const quint8 addr = quint8(pdu.at(0));
    const quint8 func = quint8(pdu.at(1));

    ++requests_;
    emit requestReceived(addr, func, pdu);

    const bool broadcast = (addr == 0x00);
    if (!broadcast && addr != address_)
        return;                                         // 不是发给我的，静默忽略

    quint8 exc = Modbus::ExcNone;
    QByteArray body;

    //这个switch只加payload
    switch (func) {
    case Modbus::ReadHoldingRegisters:
    case Modbus::ReadInputRegisters: {
        // pdu = [从站地址][功能码][起始高][起始低][数量高][数量低]，共 6 字节
        if (pdu.size() != 6) {
            exc = Modbus::ExcIllegalDataValue;
            break;
        }
        const quint16 start = quint16((quint8(pdu.at(2)) << 8) | quint8(pdu.at(3)));
        const quint16 count = quint16((quint8(pdu.at(4)) << 8) | quint8(pdu.at(5)));
        if (count < 1 || count > Modbus::MaxReadRegisters) {
            exc = Modbus::ExcIllegalDataValue;
            break;
        }
        if (!inRange(start, count)) {
            exc = Modbus::ExcIllegalDataAddress;
            break;
        }
        body.append(static_cast<char>(quint8(count * 2)));
        const QMap<quint16, quint16> &bank = (func == Modbus::ReadHoldingRegisters)
                                             ? holding_ : input_;
        for (quint16 i = 0; i < count; ++i) {
            const quint16 v = bank.value(quint16(start + i), 0);
            body.append(static_cast<char>(quint8((v >> 8) & 0xFF)));   // 大端
            body.append(static_cast<char>(quint8(v & 0xFF)));
        }
        break;
    }
    case Modbus::WriteSingleRegister: {
        // pdu = [从站地址][功能码][寄存器高][寄存器低][数值高][数值低]，共 6 字节
        if (pdu.size() != 6) {
            exc = Modbus::ExcIllegalDataValue;
            break;
        }
        const quint16 reg = quint16((quint8(pdu.at(2)) << 8) | quint8(pdu.at(3)));
        const quint16 value = quint16((quint8(pdu.at(4)) << 8) | quint8(pdu.at(5)));
        if (!inRange(reg, 1)) {
            exc = Modbus::ExcIllegalDataAddress;
            break;
        }
        holding_.insert(reg, value);
        body = pdu.mid(2, 4);                            // 规范：原样回显地址与数值
        break;
    }
    case Modbus::WriteMultipleRegisters: {
        // pdu = [...][起始 2][数量 2][字节数 1][数据 N]，最短 7 字节
        if (pdu.size() < 7) {
            exc = Modbus::ExcIllegalDataValue;
            break;
        }
        const quint16 start = quint16((quint8(pdu.at(2)) << 8) | quint8(pdu.at(3)));
        const quint16 count = quint16((quint8(pdu.at(4)) << 8) | quint8(pdu.at(5)));
        const int byteCount = int(quint8(pdu.at(6)));
        if (count < 1 || count > Modbus::MaxWriteRegisters
                || byteCount != int(count) * 2 || pdu.size() != 7 + byteCount) {
            exc = Modbus::ExcIllegalDataValue;
            break;
        }
        if (!inRange(start, count)) {
            exc = Modbus::ExcIllegalDataAddress;
            break;
        }
        for (quint16 i = 0; i < count; ++i) {
            const int off = 7 + int(i) * 2;
            const quint16 v = quint16((quint8(pdu.at(off)) << 8) | quint8(pdu.at(off + 1)));
            holding_.insert(quint16(start + i), v);
        }
        body = pdu.mid(2, 4);                            // 规范：回显起始地址与数量
        break;
    }
    default:
        exc = Modbus::ExcIllegalFunction;
        break;
    }

    // 故障注入：强制异常码（用于验证上位机的异常处理分支）
    if (forcedException_ != 0 && func == forcedFunc_)
        exc = forcedException_;

    if (broadcast)
        return;                                         // 广播：执行但不应答

    QByteArray response;
    if (exc != Modbus::ExcNone) {
        response = makeExceptionAdu(func, exc);
        emit exceptionSent(addr, func, exc);
    } else {
        response.append(static_cast<char>(address_));
        response.append(static_cast<char>(func));
        response.append(body);
        response = Modbus::appendCrc(response);
    }
    sendFrame(response, addr);
}

void ModbusRtuSlave::sendFrame(const QByteArray &frame, quint8 slave)
{
    if (!transport_ || !transport_->isOpen())
        return;

    // 故障注入 1：按次数丢应答（主站应当超时重发）
    if (dropEveryNth_ > 0 && (++dropCounter_ % dropEveryNth_) == 0)
        return;

    QByteArray out = frame;

    // 故障注入 2：按次数写错 CRC（主站应当判 CRC 错并重发）
    if (corruptEveryNth_ > 0 && (++corruptCounter_ % corruptEveryNth_) == 0 && !out.isEmpty()) {
        const int last = out.size() - 1;
        out[last] = static_cast<char>(quint8(out.at(last)) ^ 0xFF);
    }

    // 故障注入 3：应答延时（用来测主站超时阈值是否合理）
    if (responseDelayMs_ > 0) {
        QTimer::singleShot(responseDelayMs_, this, [this, out, slave]() {
            if (transport_ && transport_->isOpen()) {
                transport_->write(out);
                emit responseSent(slave, out);
            }
        });
        return;
    }

    transport_->write(out);
    emit responseSent(slave, out);
}
