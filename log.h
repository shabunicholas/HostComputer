#ifndef LOG_H
#define LOG_H
#include<QString>
enum  class Level:int{
    NORMAL,
    WARN,
    ERROR,
    NO,
    ALL
};
//也可以定义2个static方法，内部一起调用同一个函数进行写入。后面不够在加，一般也够了吧
//后期可能会集成LOG4CPP库，从而弃用这个类
class Log
{
public:
    Log()=delete;
    static bool updataLog(Level level,const QString &mes);
private:
    //写成单列也行
    static bool wornLog(const QString &name=QString("WARN"),
                 const QString &mes=QString());
    static bool normalLog(const QString &name=QString("NORMAL"),
                const QString &mes=QString());
    static bool errorLog(const QString &name=QString("ERROR"),
                const QString &mes=QString());
    //真正的写入操作
    static bool writeLog(const QString &name=QString(),
                 const QString &mes=QString());
};

#endif // LOG_H
