#ifndef LOG_H
#define LOG_H
#include<QString>
enum  class Level:int{
    NORMAL,
    WORN,
    NO,
    ALL
};
//也可以定义2个static方法，内部一起调用同一个函数进行写入。后面不够在加，一般也够了吧
//后期可能会集成LOG4CPP库，从而弃用这个类
class Log
{
public:
    Log()=default;
    static bool updataLog(Level level,const QString &mes);
private:
    //写成单列也行
    static bool wornLog(const QString &name=QStringLiteral("WORN"),
                 const QString &mes=QString());
    static bool normalLog(const QString &name=QStringLiteral("NORMAL"),
                const QString &mes=QString());
    static bool writeLog(const QString &name=QString(),
                 const QString &mes=QString());
};

#endif // LOG_H
