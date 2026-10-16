#ifndef MODBUSRTUMASTER_H
#define MODBUSRTUMASTER_H

/*
 * Modbus RTU 主站
 *
 * 传输层是 QIODevice：真实串口传 QSerialPort（它本身就是 QIODevice），离线自测传 LoopbackDevice。
 *
 * 同步 API 请在「非 GUI 线程」调用（例如上位机里那个 moveToThread 出来的工作线程）：
 * 每次调用最多阻塞 responseTimeoutMs × (retries + 1)，期间用局部事件循环等应答。
 *
 * 可靠性设计：
 *   - 一次只允许一个在途请求（RTU 半双工天然如此）
 *   - 超时 / CRC 错 → 重发；异常应答是「合法应答」→ 不重发
 *   - 重发前等线路静默再清接收缓冲，避免把上一次的迟到应答当成这一次的
 *   - 写操作强制比对从站回显，读操作校验字节数
 */

//#include "modbusprotocol.h"

#include <QByteArray>
#include <QIODevice>
#include <QObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

class ModbusRtuMaster : public QObject
{
    Q_OBJECT
public:
    enum class Result {
        Ok,                 // 成功
        NotOpen,            // 传输未打开 / 写入失败
        InvalidArgument,    // 本地参数非法（数量超限等）
        Timeout,            // 超时且重试用尽
        CrcError,           // CRC 校验失败且重试用尽
        Malformed,          // 帧结构错 / 应答与请求不匹配
        ExceptionResponse,  // 从站返回异常码（合法应答，不重发）
    };
    Q_ENUM(Result)

    struct Config {
        int  baudRate            = 19200;   // 只用于计算 3.5 字符间隔
        int  responseTimeoutMs   = 300;     // 单次等待应答的总超时
        int  retries             = 2;       // 首次之外的额外尝试次数
        bool discardStaleOnRetry = true;    // 重发前等线路静默并清缓冲
    };

    explicit ModbusRtuMaster(QIODevice *transport, QObject *parent = nullptr);
    ~ModbusRtuMaster() override;

    void setTransport(QIODevice *transport);
    QIODevice *transport() const;
    void setConfig(const Config &config);
    Config config() const;

    // ---- 同步 API（在工作线程里用）----
    Result readHoldingRegisters(quint8 slave, quint16 start, quint16 count,
                                QVector<quint16> &out, quint8 *exceptionCode = nullptr);
    Result readInputRegisters(quint8 slave, quint16 start, quint16 count,
                              QVector<quint16> &out, quint8 *exceptionCode = nullptr);
    Result writeSingleRegister(quint8 slave, quint16 address, quint16 value,
                               quint8 *exceptionCode = nullptr);
    Result writeMultipleRegisters(quint8 slave, quint16 start, const QVector<quint16> &values,
                                  quint8 *exceptionCode = nullptr);

    // ---- 统计（可直接显示在界面上）----
    quint32 txFrameCount() const   { return tx_; }
    quint32 rxFrameCount() const   { return rx_; }
    quint32 retryCount() const     { return retries_; }
    quint32 timeoutCount() const   { return timeouts_; }
    quint32 crcErrorCount() const  { return crcErrors_; }
    quint32 exceptionCount() const { return exceptions_; }
    quint32 successCount() const   { return success_; }
    double  successRate() const;    // 0.0 ~ 1.0（按发送帧数算，含重发）
    void resetStatistics();

    static QString resultText(Result result);

signals:
    void frameSent(quint8 slave, const QByteArray &frame);
    void frameReceived(quint8 slave, const QByteArray &frame);
    void errorOccurred(quint8 slave, ModbusRtuMaster::Result result, quint8 exceptionCode);
    void registersRead(quint8 slave, quint16 start, const QVector<quint16> &values);
    void statisticsChanged();

private:
    Result transact(quint8 slave, quint8 func, const QByteArray &payload,
                    QByteArray &responsePdu, quint8 *exceptionCode);
    Result readRegisters(quint8 slave, quint8 function, quint16 start, quint16 count,
                         QVector<quint16> &out, quint8 *exceptionCode);
    bool   waitForResponse(quint8 requestFunc, QByteArray &raw, bool &complete);
    int    expectedResponseLength(const QByteArray &rx) const;
    void   drainInput();
    void   resyncAfterTimeout();

    QIODevice *transport_ = nullptr;
    Config     config_;
    int        gapMs_ = 5;              // 判帧兜底用的静默间隔

    quint32 tx_ = 0, rx_ = 0, retries_ = 0, timeouts_ = 0;
    quint32 crcErrors_ = 0, exceptions_ = 0, success_ = 0;
};

#endif // MODBUSRTUMASTER_H
