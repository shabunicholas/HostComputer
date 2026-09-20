#include"ProtocolCommand.h"

#include<QDebug>

static int count_one(uint8_t c){
    int count=0;


    for(int i=0;i<8;i++){
        if(c & 0x01)count++;
        c>>=1;
    }

    return count;
}

QByteArray ProtocolCodec::pack(uint8_t cmd, uint8_t param, int data)
{
    QByteArray array;
    array.append(static_cast<char>(frame_header));
    array.append(static_cast<char>(cmd));
    array.append(static_cast<char>(param));
    array.append(static_cast<char>(data & 0xFF));
    array.append(static_cast<char>((data>>8) & 0xFF));
    array.append(static_cast<char>((data>>16) & 0xFF));
    array.append(static_cast<char>((data>>24) & 0xFF));

    int count=0;
    count+=count_one(cmd);
    count+=count_one(param);
    count+=count_one(static_cast<uint8_t>(data & 0xFF));
    count+=count_one(static_cast<uint8_t>((data>>8) & 0xFF));
    count+=count_one(static_cast<uint8_t>((data>>16) & 0xFF));
    count+=count_one(static_cast<uint8_t>((data>>24) & 0xFF));

    array.append(count & 0x01);
    array.append(static_cast<char>(frame_tail));

    return array;
}

bool ProtocolCodec::parse(const QByteArray &frame, uint8_t &cmd, uint8_t &param, int &data)
{
    if(frame.size()<9)return false;
    uint8_t head=static_cast<uint8_t>(frame.at(0));
    uint8_t tail=static_cast<uint8_t>(frame.at(8));

    if(head!=frame_header || tail!=frame_tail){
        return false;
    }

    //这里使用奇偶效验，后面接入moubus协议crc 效验
    //9个字节
    //[包头][命令][参数][data][data][data][data][效验位][包尾]
    int count=0;
    for(int i=1;i<=7;i++){

        count+=count_one(static_cast<uint8_t>(frame.at(i)));
    }

    //奇偶效验，奇数则失败
    bool is=count & 0x01;
    if(is)return false;

    //小端
    cmd=static_cast<uint8_t>(frame.at(1));
    param=static_cast<uint8_t>(frame.at(2));
    data=  static_cast<int>( (static_cast<uint8_t>(frame.at(3)) |
            (static_cast<uint8_t>(frame.at(4))<<8) |
            (static_cast<uint8_t>(frame.at(5))<<16) |
            (static_cast<uint8_t>(frame.at(6))<<24)) );
    qDebug()<<"数据:"<<data;

    return true;
}
