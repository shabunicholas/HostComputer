#include "mainwindow.h"
#include "ui_mainwindow.h"
#include<QDebug>
#include<QFile>
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

void MainWindow::init()
{
    //loadStyle(ui->progressBar,":/qss/pro.qss");
    pro_[0]=ui->progressBar;
    pro_[1]=ui->progressBar_2;
    pro_[2]=ui->progressBar_3;
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
        Log::updataLog(Level::WARN,QString("目前只有1，2，3,当前输入为:%1").arg(param));
        return;
    }

    pro_[param-1]->setValue(data);

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
        proSetVal(param,data);
        break;
    default:
        break;
    }
}



