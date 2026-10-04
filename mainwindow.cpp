#include "mainwindow.h"
#include "ui_mainwindow.h"
#include<QDebug>
#include<QFile>
#define MAX_DATA 1200
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
    //loadStyle(ui->progressBar,":/qss/pro.qss");
    config_=Config::load(configPath);
    pro_[0]=ui->progressBar;
    pro_[1]=ui->progressBar_2;
    pro_[2]=ui->progressBar_3;
    lab_[0]=ui->label_1;
    lab_[1]=ui->label_2;
    lab_[2]=ui->label_3;
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

void MainWindow::proSetVal(uint8_t param,int data)
{
    if(param<1 || param >3){
        Log::updataLog(Level::WARN,QString("proSetVal 目前只有1，2，3,当前输入为:%1").arg(param));
        return;
    }

    pro_[param-1]->setValue(data);

}

void MainWindow::proWarnStyle(uint8_t param, bool warn)
{
    if(param<1 || param>3){
        Log::updataLog(Level::WARN,
                       QString("proWarnStyle 目前只有1，2，3,当前输入为:%1").arg(param));
        return;
    }

    QString color = warn ? "#E74C3C" : "#3498DB";

    QString qss = QString(
           "QProgressBar {"
           "    border: none;"
           "    border-radius: 3px;"
           "    background-color: #E8E8E8;"
           "    text-align: center;"
           "    color: #333;"
           "    font-size: 12px;"
           "}"
           "QProgressBar::chunk {"
           "    border-radius: 6px;"
           "    background-color: %1;"
           "}"
       ).arg(color);

       pro_[param - 1]->setStyleSheet(qss);
       if(warn){
           lab_[param-1]->setText(QString("%1号警告").arg(param));
           lab_[param - 1]->setStyleSheet("color: red;");
       }else{
           lab_[param-1]->setText(QString("%1号正常").arg(param));
           lab_[param - 1]->setStyleSheet("color: green;");
       }

}

void MainWindow::readDataShow(uint8_t cmd, uint8_t param, int data)
{
    qDebug()<<"cmd:"<<cmd<<" param:"<<param<<" data:"<<data;
    ui->textEdit->append(QString("cmd:%1,param:%2,data:%3")
                          .arg(cmd).arg(param).arg(data));

    switch (cmd) {
    case static_cast<uint8_t>(COMMAND::WEN_DU_RSP):
        ui->label_wd->setText(QString("温度:%1").arg(data));
        break;
    case static_cast<uint8_t>(COMMAND::YI_LI_GAN_RSP):
    {
        if(data>MAX_DATA || data<0){
            qDebug()<<QString("无效数据:%1").arg(data);
            Log::updataLog(Level::ERROR,QString("无效数据:%1").arg(data));
            return;
        }
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



