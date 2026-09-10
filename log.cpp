#include "log.h"
#include<QDebug>
#include<QFile>
#include<QDir>
#include<QCoreApplication>
#include<QDateTime>
#include<QMutex>
//Log::Log()
//{

//}
static QStringList countLog={"NORMAL","WARN","NO"};
static QMutex lock;
bool Log::updataLog(Level level, const QString &mes)
{
    switch (level) {
      case Level::NORMAL:
        return normalLog(QStringLiteral("NORMAL"),mes);
        break;
      case Level::WARN:
        return wornLog(QStringLiteral("WARN"),mes);
        break;
    case Level::ERROR:
        return errorLog(QStringLiteral("ERROR"),mes);
      case Level::ALL:
    {
        QString str=QString("级别个数:%1[%2]").arg(static_cast<int>(Level::ALL))
                .arg(countLog.join(" "));
        return normalLog(QStringLiteral("NORMAL"),str);
    }
        break;
      default:
        break;
    }

    const QString str=QString("这个未处理日志 请输入[%1 %2]").arg("NORMAL").arg("WARN");
    bool is=wornLog(QStringLiteral("WARN"),str);
    if(is==false)qDebug()<<"写入日志失败,请检查日志文件";
    return is;
}

bool Log::wornLog(const QString &name,const QString &mes)
{
    return writeLog(name,mes);
}

bool Log::normalLog(const QString &name,const QString &mes)
{
    return writeLog(name,mes);
}

bool Log::errorLog(const QString &name, const QString &mes)
{
    return writeLog(name,mes);
}

bool Log::writeLog(const QString &name,const QString &mes)
{
    QMutexLocker locker(&lock);
    bool is=true;
    QString dir=QCoreApplication::applicationDirPath();
    QDir curdir=(dir);
    curdir.cdUp();
    //qDebug()<<"日志文件夹目录:"<<curdir.absolutePath();

    if(!curdir.mkpath("log")){
        qDebug()<<"创建log文件夹失败";
    }

    curdir.cd("log");
    //qDebug()<<curdir.absolutePath();
    const QString logName=name+".txt";
    QFile file(curdir.filePath(logName));

    if(file.open(QIODevice::WriteOnly | QIODevice::Append |QIODevice::Text)){
        QTextStream stream(&file);
        stream.setCodec("UTF-8");
        stream<<QString("[%1]LEVEL:%2 MES:%3\n").
                arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"))
                .arg(name).arg(mes);
    }else{
        qDebug()<<"打开文件失败:"<<file.errorString();
        is=false;
    }
    file.close();
    return is;
}
