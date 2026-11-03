#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include<QThread>
#include"serialport.h"
#include"config.h"
#include<QLabel>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
signals:
    void writeStart(uint8_t cmd, uint8_t param, int data);
private:
    void init(const QString &configPath=QString());
    void loadStyle(QWidget *widget,const QString &path);
    void proSetVal(uint8_t param,int data);
    void proWarnStyle(uint8_t param,bool warn);
    void refreshLabel(int index);       // 把「数值 + 状态」合成一行显示
private slots:
    void readDataShow(uint8_t cmd,uint8_t param,int data);


private:
    Serialport *serialport_;
    QThread *thread_;
    Config config_;
    bool warn_[3]={false};
    QLabel *lab_[3];
    int lastVal_[3]={0,0,0};            // 最近一次压力值，用于合成标签文本
    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
