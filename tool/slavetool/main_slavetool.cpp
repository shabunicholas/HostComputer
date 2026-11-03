/*
 * Modbus RTU 虚拟从站工具
 *
 * 把 modbusslave.cpp 里那套从站核心接到真实串口上，当成一台"假设备"，
 * 用来跟上位机联调——不需要真机、不需要下位机。
 *
 * 典型用法（配合 com0com 虚拟串口对 COM10 <-> COM11）：
 *   上位机跑 COM10，本工具跑 COM11：
 *       ModbusSlaveTool.exe --port COM11 --baud 19200 --parity even --slave 1
 *
 * 运行中按键（回车生效，不用重启）：
 *   d  开/关「丢应答」        —— 每帧都丢，看主站超时重发
 *   c  开/关「应答 CRC 写错」  —— 看主站判 CRC 错并重发
 *   s  开/关「慢应答 +500ms」  —— 看主站超时阈值够不够
 *   x  开/关「强制异常码 04」  —— 看主站怎么处理异常应答（不该重发）
 *   r  寄存器值立刻随机跳变一次
 *   v  打印当前寄存器值
 *   q  退出
 */

#include "modbusprotocol.h"
#include "modbusslave.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QRandomGenerator>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTimer>
#include <cmath>
#include <cstdio>

#ifdef Q_OS_WIN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <conio.h>
#  include <windows.h>
#endif

// ---------------------------------------------------------------- 小工具
// 统一按 UTF-8 字节输出（配合 main 里的 SetConsoleOutputCP），避免中文在控制台乱码。
// 不用 QTextStream：它默认按系统 ANSI 编码转换，会和 UTF-8 控制台打架。
static void say(const QString &text)
{
    std::printf("%s\n", text.toUtf8().constData());
    std::fflush(stdout);
}

static QString stamp()
{
    return QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"));
}

static QString hexOf(const QByteArray &data)
{
    QString s;
    for (int i = 0; i < data.size(); ++i) {
        s += QStringLiteral("%1 ").arg(quint8(data.at(i)), 2, 16, QLatin1Char('0')).toUpper();
    }
    return s.trimmed();
}

static quint16 be16(const QByteArray &d, int off)
{
    return quint16((quint8(d.at(off)) << 8) | quint8(d.at(off + 1)));
}

// 把一帧 PDU（已去 CRC）翻译成人话，方便对着屏幕看
static QString describePdu(const QByteArray &pdu)
{
    if (pdu.size() < 2)
        return QStringLiteral("长度不足");

    const quint8 func = quint8(pdu.at(1));
    if (func & 0x80)
        return QStringLiteral("异常应答 码=%1").arg(Modbus::exceptionText(
            pdu.size() >= 3 ? quint8(pdu.at(2)) : 0xFF));

    switch (func) {
    case Modbus::ReadHoldingRegisters:
        return QStringLiteral("读保持寄存器 起始=0x%1 数量=%2")
            .arg(pdu.size() >= 6 ? be16(pdu, 2) : 0, 4, 16, QLatin1Char('0'))
            .arg(pdu.size() >= 6 ? be16(pdu, 4) : 0);
    case Modbus::ReadInputRegisters:
        return QStringLiteral("读输入寄存器 起始=0x%1 数量=%2")
            .arg(pdu.size() >= 6 ? be16(pdu, 2) : 0, 4, 16, QLatin1Char('0'))
            .arg(pdu.size() >= 6 ? be16(pdu, 4) : 0);
    case Modbus::WriteSingleRegister:
        return QStringLiteral("写单个寄存器 地址=0x%1 值=%2")
            .arg(pdu.size() >= 6 ? be16(pdu, 2) : 0, 4, 16, QLatin1Char('0'))
            .arg(pdu.size() >= 6 ? be16(pdu, 4) : 0);
    case Modbus::WriteMultipleRegisters:
        return QStringLiteral("写多个寄存器 起始=0x%1 数量=%2")
            .arg(pdu.size() >= 6 ? be16(pdu, 2) : 0, 4, 16, QLatin1Char('0'))
            .arg(pdu.size() >= 6 ? be16(pdu, 4) : 0);
    default:
        return QStringLiteral("功能码 0x%1（从站不支持）")
            .arg(func, 2, 16, QLatin1Char('0'));
    }
}

static QString availablePortsText()
{
    QStringList names;
    const auto ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : ports)
        names << info.portName();
    return names.isEmpty() ? QStringLiteral("(无)") : names.join(QStringLiteral(", "));
}

// ---------------------------------------------------------------- 参数
struct Options
{
    QString port = QStringLiteral("COM2");
    int baud = 19200;
    int dataBits = 8;
    int stopBits = 1;
    QSerialPort::Parity parity = QSerialPort::EvenParity;
    int slave = 1;
    int tempReg = 0x0000;
    int pressReg = 0x0010;
    int pressCount = 3;
};

static bool parseParity(const QString &text, QSerialPort::Parity *parity)
{
    const QString t = text.toLower();
    if (t == QStringLiteral("none") || t == QStringLiteral("n")) { *parity = QSerialPort::NoParity;   return true; }
    if (t == QStringLiteral("even") || t == QStringLiteral("e")) { *parity = QSerialPort::EvenParity; return true; }
    if (t == QStringLiteral("odd")  || t == QStringLiteral("o")) { *parity = QSerialPort::OddParity;  return true; }
    return false;
}

static QString parityName(QSerialPort::Parity p)
{
    switch (p) {
    case QSerialPort::NoParity:   return QStringLiteral("N");
    case QSerialPort::EvenParity: return QStringLiteral("E");
    case QSerialPort::OddParity:  return QStringLiteral("O");
    default:                      return QStringLiteral("?");
    }
}

static void printUsage()
{
    say(QStringLiteral(
        "用法: ModbusSlaveTool.exe [--port COM11] [--baud 19200] [--parity even|none|odd]\n"
        "                          [--databits 8] [--stopbits 1|2] [--slave 1]\n"
        "                          [--temp 0] [--press 16] [--presscount 3]"));
}

// ---------------------------------------------------------------- 从站外壳
class SlaveTool : public QObject
{
    Q_OBJECT
public:
    SlaveTool(const Options &opt, QObject *parent = nullptr)
        : QObject(parent), opt_(opt)
    {
        port_ = new QSerialPort(this);
        slave_ = new ModbusRtuSlave(port_, quint8(opt_.slave), this);
        slave_->setRegisterRange(0, 200);

        // 让保持寄存器一开始有值，方便主站测 03/06
        for (quint16 i = 0; i < 20; ++i)
            slave_->setHoldingRegister(i, quint16(100 + i));

        connect(slave_, &ModbusRtuSlave::requestReceived,
                this, &SlaveTool::onRequest);
        connect(slave_, &ModbusRtuSlave::responseSent,
                this, &SlaveTool::onResponse);
        connect(slave_, &ModbusRtuSlave::exceptionSent,
                this, &SlaveTool::onException);
        connect(slave_, &ModbusRtuSlave::requestCrcError,
                this, &SlaveTool::onRequestCrcError);
    }

    bool start()
    {
        port_->setPortName(opt_.port);
        port_->setBaudRate(opt_.baud);
        port_->setDataBits(opt_.dataBits == 7 ? QSerialPort::Data7 : QSerialPort::Data8);
        port_->setParity(opt_.parity);
        port_->setStopBits(opt_.stopBits == 2 ? QSerialPort::TwoStop : QSerialPort::OneStop);
        port_->setFlowControl(QSerialPort::NoFlowControl);

        // 键盘先启动：即使串口没连上，也能按 q 退出
        keys_ = new QTimer(this);
        connect(keys_, &QTimer::timeout, this, &SlaveTool::pollKeyboard);
        keys_->start(100);

        if (!openPort()) {
            say(QStringLiteral("!! 打开串口失败: %1 (%2)").arg(opt_.port, port_->errorString()));
            say(QStringLiteral("   本机可用串口: %1").arg(availablePortsText()));
            say(QStringLiteral("   常见原因：端口号写错 / 被别的程序占用 / 虚拟串口对没建好"));
            say(QStringLiteral("   程序不会退出，每 2 秒自动重试；按 q 退出。"));
            retry_ = new QTimer(this);
            connect(retry_, &QTimer::timeout, this, &SlaveTool::retryOpen);
            retry_->start(2000);
        }

        sim_ = new QTimer(this);
        connect(sim_, &QTimer::timeout, this, &SlaveTool::updateSimulatedValues);
        sim_->start(1000);
        updateSimulatedValues();
        return true;
    }

private slots:
    void retryOpen()
    {
        if (port_->isOpen()) {
            retry_->stop();
            return;
        }
        if (openPort()) {
            retry_->stop();
            say(QStringLiteral(">> 串口已连上: %1").arg(opt_.port));
        }
    }

    void onRequest(quint8 slave, quint8 function, const QByteArray &pdu)
    {
        Q_UNUSED(slave)
        Q_UNUSED(function)
        say(QStringLiteral("[%1] <- %2   %3")
            .arg(stamp(), hexOf(pdu), describePdu(pdu)));
    }

    void onResponse(quint8 slave, const QByteArray &frame)
    {
        Q_UNUSED(slave)
        say(QStringLiteral("[%1] -> %2")
            .arg(stamp(), hexOf(frame)));
    }

    void onException(quint8 slave, quint8 function, quint8 code)
    {
        Q_UNUSED(slave)
        say(QStringLiteral("[%1] -> 异常应答 功能码0x%2 码=%3")
            .arg(stamp())
            .arg(function, 2, 16, QLatin1Char('0'))
            .arg(Modbus::exceptionText(code)));
    }

    void onRequestCrcError()
    {
        say(QStringLiteral("[%1] !! 收到的请求 CRC 错，按规范静默丢弃（主站会超时重发）")
            .arg(stamp()));
    }

    void updateSimulatedValues()
    {
        ++tick_;

        // 温度：22.0~28.0℃（按 0.1℃ 精度存整数 → 220~280）
        const double tempC = 25.0 + 3.0 * std::sin(double(tick_) / 10.0);
        slave_->setInputRegister(quint16(opt_.tempReg), quint16(tempC * 10.0));

        // 压力：1200 / 1500 / 1800 附近波动
        for (int i = 0; i < opt_.pressCount; ++i) {
            const double base = 1200.0 + i * 300.0;
            const double wave = 60.0 * std::sin(double(tick_) / 7.0 + i);
            const int noise = QRandomGenerator::global()->bounded(21) - 10;
            slave_->setInputRegister(quint16(opt_.pressReg + i),
                                     quint16(base + wave + noise));
        }
    }

    void pollKeyboard()
    {
#ifdef Q_OS_WIN
        while (_kbhit()) {
            const int key = _getch();
            handleKey(key);
        }
#endif
    }

private:
    void handleKey(int key)
    {
        switch (key) {
        case 'd': case 'D':
            dropAll_ = !dropAll_;
            slave_->setDropResponseEveryNth(dropAll_ ? 1 : 0);
            say(QStringLiteral(">> 丢应答: %1").arg(dropAll_ ? QStringLiteral("开") : QStringLiteral("关")));
            break;
        case 'c': case 'C':
            corruptAll_ = !corruptAll_;
            slave_->setCorruptCrcEveryNth(corruptAll_ ? 1 : 0);
            say(QStringLiteral(">> CRC 写错: %1").arg(corruptAll_ ? QStringLiteral("开") : QStringLiteral("关")));
            break;
        case 's': case 'S':
            slowAll_ = !slowAll_;
            slave_->setResponseDelayMs(slowAll_ ? 500 : 0);
            say(QStringLiteral(">> 慢应答 +500ms: %1").arg(slowAll_ ? QStringLiteral("开") : QStringLiteral("关")));
            break;
        case 'x': case 'X':
            forcedAll_ = !forcedAll_;
            if (forcedAll_)
                slave_->setForcedException(quint8(Modbus::ReadInputRegisters),
                                           quint8(Modbus::ExcSlaveDeviceFailure));
            else
                slave_->setForcedException(0, 0);
            say(QStringLiteral(">> 强制异常码 04(读输入寄存器): %1")
                .arg(forcedAll_ ? QStringLiteral("开") : QStringLiteral("关")));
            break;
        case 'r': case 'R':
            tick_ += 100;
            updateSimulatedValues();
            say(QStringLiteral(">> 寄存器值已跳变"));
            break;
        case 'v': case 'V':
            dumpRegisters();
            break;
        case 'q': case 'Q':
            say(QStringLiteral(">> 退出"));
            QCoreApplication::quit();
            break;
        default:
            break;
        }
    }

    void dumpRegisters()
    {
        say(QStringLiteral("---- 输入寄存器（主站读这个）----"));
        for (int r = opt_.tempReg; r < opt_.tempReg + 1; ++r)
            say(QStringLiteral("  0x%1 = %2   (温度, 值×0.1℃)")
                .arg(r, 4, 16, QLatin1Char('0'))
                .arg(slave_->inputRegister(quint16(r))));
        for (int i = 0; i < opt_.pressCount; ++i) {
            const int r = opt_.pressReg + i;
            say(QStringLiteral("  0x%1 = %2   (压力%3)")
                .arg(r, 4, 16, QLatin1Char('0'))
                .arg(slave_->inputRegister(quint16(r)))
                .arg(i + 1));
        }
        say(QStringLiteral("---- 保持寄存器 0x0000~0x0013（可被主站写）----"));
        say(QStringLiteral("  0x0000 = %1   0x0001 = %2   0x0002 = %3")
            .arg(slave_->holdingRegister(0))
            .arg(slave_->holdingRegister(1))
            .arg(slave_->holdingRegister(2)));
    }

    bool openPort()
    {
        if (port_->isOpen())
            return true;
        if (!port_->open(QIODevice::ReadWrite))
            return false;
        printBanner();
        return true;
    }

    void printBanner()
    {
        say(QStringLiteral("======================================================"));
        say(QStringLiteral(" Modbus RTU 虚拟从站"));
        say(QStringLiteral("   串口: %1   波特率: %2   数据位: %3   校验: %4   停止位: %5")
            .arg(opt_.port)
            .arg(opt_.baud)
            .arg(opt_.dataBits)
            .arg(parityName(opt_.parity))
            .arg(opt_.stopBits));
        say(QStringLiteral("   从站地址: %1   合法寄存器范围: 0~199").arg(opt_.slave));
        say(QStringLiteral("   温度: 输入寄存器 0x%1（1 个）")
            .arg(opt_.tempReg, 4, 16, QLatin1Char('0')));
        say(QStringLiteral("   压力: 输入寄存器 0x%1 ~ 0x%2（%3 个连续）")
            .arg(opt_.pressReg, 4, 16, QLatin1Char('0'))
            .arg(opt_.pressReg + opt_.pressCount - 1, 4, 16, QLatin1Char('0'))
            .arg(opt_.pressCount));
        say(QStringLiteral("------------------------------------------------------"));
        say(QStringLiteral(" 按键: d=丢应答  c=CRC写错  s=慢应答  x=强制异常04"));
        say(QStringLiteral("       r=值跳变  v=看寄存器  q=退出   （回车生效）"));
        say(QStringLiteral("======================================================"));
    }

    Options opt_;
    QSerialPort *port_ = nullptr;
    ModbusRtuSlave *slave_ = nullptr;
    QTimer *sim_ = nullptr;
    QTimer *keys_ = nullptr;
    QTimer *retry_ = nullptr;
    int tick_ = 0;
    bool dropAll_ = false;
    bool corruptAll_ = false;
    bool slowAll_ = false;
    bool forcedAll_ = false;
};

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    SetConsoleOutputCP(CP_UTF8);        // 中文输出不乱码
#endif
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("ModbusSlaveTool"));

    Options opt;
    const QStringList args = QCoreApplication::arguments();
    for (int i = 1; i < args.size(); ++i) {
        const QString a = args.at(i);
        const bool hasNext = (i + 1 < args.size());
        const QString v = hasNext ? args.at(i + 1) : QString();

        if (a == QStringLiteral("--help") || a == QStringLiteral("-h")) {
            printUsage();
            return 0;
        } else if (a == QStringLiteral("--port") && hasNext) {
            opt.port = v; ++i;
        } else if (a == QStringLiteral("--baud") && hasNext) {
            opt.baud = v.toInt(); ++i;
        } else if (a == QStringLiteral("--parity") && hasNext) {
            if (!parseParity(v, &opt.parity)) {
                say(QStringLiteral("!! 无法识别的校验位: %1").arg(v));
                return 2;
            }
            ++i;
        } else if (a == QStringLiteral("--databits") && hasNext) {
            opt.dataBits = v.toInt(); ++i;
        } else if (a == QStringLiteral("--stopbits") && hasNext) {
            opt.stopBits = v.toInt(); ++i;
        } else if (a == QStringLiteral("--slave") && hasNext) {
            opt.slave = v.toInt(); ++i;
        } else if (a == QStringLiteral("--temp") && hasNext) {
            opt.tempReg = v.toInt(); ++i;
        } else if (a == QStringLiteral("--press") && hasNext) {
            opt.pressReg = v.toInt(); ++i;
        } else if (a == QStringLiteral("--presscount") && hasNext) {
            opt.pressCount = v.toInt(); ++i;
        } else {
            say(QStringLiteral("!! 未知参数: %1").arg(a));
            printUsage();
            return 2;
        }
    }

    SlaveTool tool(opt);
    tool.start();          // 打不开串口也不退出，内部会每 2 秒重试

    return app.exec();
}

#include "main_slavetool.moc"
