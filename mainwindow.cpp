#include "mainwindow.h"
#include "ui_mainwindow.h"
#include<QDebug>
#include<QFile>
// MAX_DATA 已移除：不再用写死量程上限拦截压力数据（理由见 readDataShow 里的注释）
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , serialport_(nullptr),thread_(nullptr),ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    init();
}

MainWindow::~MainWindow()
{
    if(thread_){
        thread_->quit();
        thread_->wait();
        thread_=nullptr;
    }
    delete ui;
}

void MainWindow::init(const QString &configPath)
{
    config_=Config::load(configPath);
    lab_[0]=ui->label_1;
    lab_[1]=ui->label_2;
    lab_[2]=ui->label_3;
    // 温度标签字号跟三路压力保持一致（文字在 readDataShow 里更新）
    ui->label_wd->setStyleSheet(QStringLiteral("font-size:22px; color:#2C3E50; padding:6px 0;"));
    serialport_=new Serialport();
    thread_=new QThread();
    serialport_->moveToThread(thread_);

    connect(thread_,&QThread::started,serialport_,&Serialport::init);
    connect(serialport_,&Serialport::readData,this,&MainWindow::readDataShow);

    connect(thread_,&QThread::finished,thread_,&QThread::deleteLater);
    connect(thread_,&QThread::finished,serialport_,&Serialport::deleteLater);
    thread_->start();
}

void MainWindow::loadStyle(QWidget *widget, const QString &path)
{
    if(!widget)return;
    QFile file(path);

    if(file.open(QIODevice::ReadOnly | QIODevice::Text)){
        const QString style=QString::fromUtf8(file.readAll());
        widget->setStyleSheet(style);
    }else{
        Log::updataLog(Level::ERROR,QString("加载样式失败widget:%1,error:%2")
                       .arg(widget->objectName()).arg(file.errorString()));
    }

}

void MainWindow::refreshLabel(int index)
{
    if(index<0 || index>2 || !lab_[index]){
        return;
    }

    const bool isWarn=warn_[index];
    lab_[index]->setText(QString("%1号    %2 Pa    %3")
                         .arg(index+1)
                         .arg(lastVal_[index])
                         .arg(isWarn ? QStringLiteral("超限") : QStringLiteral("正常")));
    // 字号一次给足（22px ≈ 16.5pt），超限时再加粗变红；上下留点内边距避免三行挤在一起
    lab_[index]->setStyleSheet(isWarn
        ? QStringLiteral("color:#C0392B; font-weight:bold; font-size:22px; padding:6px 0;")
        : QStringLiteral("color:#1E8449; font-size:22px; padding:6px 0;"));
}

void MainWindow::proSetVal(uint8_t param,int data)
{
    if(param<1 || param >3){
        Log::updataLog(Level::WARN,QString("proSetVal 目前只有1，2，3,当前输入为:%1").arg(param));
        return;
    }

    lastVal_[param-1]=data;
    refreshLabel(param-1);
}

void MainWindow::proWarnStyle(uint8_t param, bool warn)
{
    if(param<1 || param>3){
        Log::updataLog(Level::WARN,
                       QString("proWarnStyle 目前只有1，2，3,当前输入为:%1").arg(param));
        return;
    }

    warn_[param-1]=warn;
    refreshLabel(param-1);
}

void MainWindow::readDataShow(uint8_t cmd, uint8_t param, int data)
{
    qDebug()<<"cmd:"<<cmd<<" param:"<<param<<" data:"<<data;
    ui->textEdit->append(QString("cmd:%1,param:%2,data:%3")
                          .arg(cmd).arg(param).arg(data));

    switch (cmd) {
    case static_cast<uint8_t>(COMMAND::WEN_DU_RSP):
        // 设备按 0.1℃ 存储（如 276 表示 27.6℃），系数由 [Scale] TempScale 配置
        ui->label_wd->setText(QString("温度：%1 ℃")
                              .arg(data * config_.tempScale, 0, 'f', 1));
        break;
    case static_cast<uint8_t>(COMMAND::YI_LI_GAN_RSP):
    {
        if(param<1 || param >3){
            Log::updataLog(Level::WARN,QString("非法参数,有效为1~3,当前输入为:%1").arg(param));
            return;
        }
        // 这里原来有个 data>MAX_DATA(1200) 的拦截，已移除：
        // 数据完整性已由 CRC 校验 + 超时重发保证，应用层再卡一个硬编码上限
        // 只会误杀正常数据（阈值 2000 永远等不到，报警功能形同废掉）。
        proSetVal(param,data);

        bool is_warn= data>=config_.pressureThreshold ? true : false;
        if(is_warn !=warn_[param-1]){
           warn_[param-1]=is_warn;
           if(is_warn){
               Log::updataLog(Level::WARN,QString("%1号[%2]发出警告!")
                              .arg(param).arg(data));
           }else{
               Log::updataLog(Level::NORMAL,QString("%1号[%2]警告解除!")
                              .arg(param).arg(data));
           }
        }

        proWarnStyle(param,is_warn);
        break;
    }
    default:
        break;
    }
}



