#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include<QThread>
#include"serialport.h"
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
    void init();
    void loadStyle(QWidget *widget,const QString &path);
private slots:
    void readDataShow(uint8_t cmd,uint8_t param,int data);


private:
    Serialport *serialport_;
    QThread *thread_;
    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
