#include "log.h"
#include<QDebug>
#include<QFile>
//Log::Log()
//{

//}

bool Log::updataLog(Level level, const QString &mes)
{
    switch (level) {
      case Level::NORMAL:
        return normalLog(QStringLiteral("NORMAL"),mes);
        break;
      case Level::WORN:
        return wornLog(QStringLiteral("WORN"),mes);
        break;
      case Level::ALL:
        qDebug()<<"一共有多少个级别:"<<Level::ALL;
        break;
      default:
        break;
    }

    const QString str=QString("没有这个日志级别请输入[%1 %2]").arg("NORMAL").arg("WORN");
    bool is=wornLog(QStringLiteral("WRON"),str);
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

bool Log::writeLog(const QString &name,const QString &mes)
{
    const QString str=QString("当前日志为【level:%1 mes:%2】").arg(name).arg(mes);
    qDebug()<<str;
    return true;
}
