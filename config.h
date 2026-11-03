#ifndef CONFIG_H
#define CONFIG_H

#include<QString>
class Config
{
public:
    // Config();
     static Config load(const QString &path="");

     QString serialPort=QStringLiteral("COM4");
     int baudRate = 19200;// 波特率
     int pressureThreshold = 1300;//压力阈值(Pa)
     int pollingIntervalMs = 2000;//轮询间隔(ms)
     int reconnectIntervalMs = 3000;//串口重连间隔

     // ---- 协议选择：custom=自定义帧（原逻辑）  modbus=Modbus RTU 主站 ----
     // 两套协议帧格式与串口参数都不同，同一时刻只能启用一个，切换后需重启
     QString protocol = QStringLiteral("custom");

     // ---- Modbus RTU 参数（protocol=modbus 时才生效）----
     int modbusSlaveAddress = 1;          // 从站地址
     int modbusResponseTimeoutMs = 300;   // 单次应答超时，必须大于设备最坏应答时间
     int modbusRetries = 2;               // 首次之外的额外尝试次数
     int modbusTempRegister = 0;          // 温度：输入寄存器起始地址（读 1 个）
     int modbusPressureRegister = 16;     // 压力：输入寄存器起始地址（连续读 3 个）


};

#endif // CONFIG_H
