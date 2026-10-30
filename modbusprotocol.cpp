#include "modbusprotocol.h"



quint16 Modbus::crc16(const quint8 *data, int length)
{
    quint16 crc = 0xFFFF;                       // Modbus 初值
    for (int i = 0; i < length; ++i) {
        crc ^= quint16(data[i]);
        for (int bit = 0; bit < 8; ++bit) {
            if (crc & 0x0001)
                crc = quint16((crc >> 1) ^ 0xA001);   // 反向多项式
            else
                crc = quint16(crc >> 1);
        }
    }
    return crc;
}

quint16 Modbus::crc16(const QByteArray &data)
{
    return crc16(reinterpret_cast<const quint8 *>(data.constData()), int(data.size()));
}

bool Modbus::stripAndCheckCrc(const QByteArray &frame, QByteArray *pduOut)
{
    if (frame.size() < 5)                               // 最短合法应答 5 字节
        return false;

    const QByteArray body = frame.left(frame.size() - 2);
    const quint16 want = quint16(quint8(frame.at(frame.size() - 2)))
                       | quint16(quint8(frame.at(frame.size() - 1)) << 8);
    if (crc16(body) != want)
        return false;

    if (pduOut)
        *pduOut = body;
    return true;
}

QByteArray Modbus::appendCrc(const QByteArray &pdu)
{
    QByteArray out = pdu;
    const quint16 crc = crc16(pdu);
    out.append(static_cast<char>(crc & 0xFF));          // 低字节先发（Modbus 约定）
    out.append(static_cast<char>((crc >> 8) & 0xFF));
    return out;
}

QString Modbus::exceptionText(quint8 code)
{
    switch (code) {
        case ExcNone:                   return QStringLiteral("无异常");
        case ExcIllegalFunction:        return QStringLiteral("01 非法功能码：从站不支持该功能");
        case ExcIllegalDataAddress:     return QStringLiteral("02 非法数据地址：地址超出从站范围");
        case ExcIllegalDataValue:       return QStringLiteral("03 非法数据值：数量或字节数超限");
        case ExcSlaveDeviceFailure:     return QStringLiteral("04 从站设备故障");
        case ExcAcknowledge:            return QStringLiteral("05 已接受，正在处理（长任务）");
        case ExcSlaveDeviceBusy:        return QStringLiteral("06 从站忙，请稍后重试");
        case ExcMemoryParityError:      return QStringLiteral("08 存储奇偶校验错");
        case ExcGatewayPathUnavailable: return QStringLiteral("0A 网关路径不可用");
        case ExcGatewayTargetFailed:    return QStringLiteral("0B 网关目标设备无响应");
        default:                        return QStringLiteral("未知异常码 0x%1")
                                                .arg(int(code), 2, 16, QLatin1Char('0'));
        }
}

double Modbus::interFrameDelayMs(int baudRate)
{
    if (baudRate <= 0)
            return 3.5;
    if (baudRate > 19200)
            return 1.75;                                   // 规范：>19200 固定 1.75ms
    return 3.5 * 11.0 * 1000.0 / double(baudRate);      // 3.5 个字符 = 3.5 × 11 位
}
