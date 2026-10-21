#ifndef MODBUSPROTOCOL_H
#define MODBUSPROTOCOL_H

/*
 * Modbus RTU 协议层：CRC16、功能码、异常码、帧工具。
 * 无状态、无 QObject、不依赖 QtSerialPort —— 只用到 QtCore 的 QByteArray。
 *
 * ⚠ 读代码时最容易踩的地方（本模块实测踩过一次）：
 *     ADU / frame ：线上完整帧 = [从站地址][功能码][数据...][CRC 低][CRC 高]
 *     PDU / pdu   ：去掉最后 2 字节 CRC 之后的帧
 *   所有 size() 判断都要看清比的是哪一种：读请求的 ADU 是 8 字节，PDU 只有 6 字节。
 */

#include <QByteArray>
#include <QString>
#include <QtGlobal>

namespace Modbus {

// ---------------- 功能码 ----------------
enum FunctionCode {
    ReadCoils                = 0x01,
    ReadDiscreteInputs       = 0x02,
    ReadHoldingRegisters     = 0x03,
    ReadInputRegisters       = 0x04,
    WriteSingleCoil          = 0x05,
    WriteSingleRegister      = 0x06,
    WriteMultipleCoils       = 0x0F,
    WriteMultipleRegisters   = 0x10,
};

// ---------------- 异常码（Modbus 规范全量）----------------
enum ExceptionCode {
    ExcNone                   = 0x00,
    ExcIllegalFunction        = 0x01,   // 不支持的功能码
    ExcIllegalDataAddress     = 0x02,   // 地址超出从站范围
    ExcIllegalDataValue       = 0x03,   // 数量/字节数超限
    ExcSlaveDeviceFailure     = 0x04,   // 从站执行失败
    ExcAcknowledge            = 0x05,   // 已接受，正在处理
    ExcSlaveDeviceBusy        = 0x06,   // 从站忙
    ExcMemoryParityError      = 0x08,   // 存储奇偶校验错
    ExcGatewayPathUnavailable = 0x0A,   // 网关路径不可用
    ExcGatewayTargetFailed    = 0x0B,   // 网关目标无响应
};

// ---------------- 协议工具 ----------------
quint16 crc16(const quint8 *data, int length);
quint16 crc16(const QByteArray &data);

// 校验并剥离 CRC：成功时把去掉 CRC 的 PDU 写入 pduOut
bool stripAndCheckCrc(const QByteArray &frame, QByteArray *pduOut = nullptr);

// 给 PDU 追加 CRC（低字节先发）
QByteArray appendCrc(const QByteArray &pdu);

QString exceptionText(quint8 code);

// 3.5 字符静默间隔（毫秒）：波特率 > 19200 时规范固定 1.75ms，否则 3.5 × 11 位 / 波特率
double interFrameDelayMs(int baudRate);

// 合法数量上限（Modbus 规范）
enum { MaxReadRegisters = 125, MaxWriteRegisters = 123 };

} // namespace Modbus

#endif // MODBUSPROTOCOL_H
