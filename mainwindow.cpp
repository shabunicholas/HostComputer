#include "mainwindow.h"
#include "ui_mainwindow.h"
#include<QDebug>
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
    serialport_=new Serialport();
    thread_=new QThread();
    serialport_->moveToThread(thread_);

    connect(thread_,&QThread::started,serialport_,&Serialport::init);
    connect(serialport_,&Serialport::readData,this,&MainWindow::readDataShow);

    connect(thread_,&QThread::finished,thread_,&QThread::deleteLater);
    connect(thread_,&QThread::finished,serialport_,&Serialport::deleteLater);
    thread_->start();
}

void MainWindow::readDataShow(uint8_t cmd, uint8_t param, int data)
{
    qDebug()<<"cmd:"<<cmd<<" param:"<<param<<" data:"<<data;
}

