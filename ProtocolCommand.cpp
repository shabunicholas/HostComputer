#include"ProtocolCommand.h"

#include<QDebug>
QByteArray ProtocolCodec::pack(uint8_t cmd, uint8_t param, const QByteArray &data)
{
    QByteArray array;
    array.append(frame_header);
    array.append(cmd);
    array.append(param);
    array.append(data);
    array.append(frame_tail);

    return array;
}

bool ProtocolCodec::parse(const QByteArray &frame, uint8_t &cmd, uint8_t &param, QByteArray &data)
{
    if(frame.size()<9)return false;
    uint8_t head=frame.at(0);
    uint8_t tail=frame.at(8);

    if(head!=frame_header || tail!=frame_tail){
        return false;
    }

    //这里使用奇偶效验，后面接入moubus协议crc 效验
    //9个字节
    //[包头][命令][参数][data][data][data][data][效验位][包尾]
    int count=0;
    for(int i=1;i<=7;i++){
        uint8_t tem=(uint8_t)frame.at(i);

        //while循环？
        for(int j=0;j<8;j++){
            if(tem & 0x01)count++;
            tem>>=1;
        }
    }

    //奇偶效验，奇数则失败
    bool is=count & 0x01;
    if(is)return false;

    //小端
    cmd=frame.at(1);
    param=frame.at(2);
    data=frame.mid(3,4);
    int data_int= (int)(frame.at(3)) | (frame.at(4)<<8) |
            (frame.at(5)<<16) | (frame.at(6)<<24);
    qDebug()<<"数据(int):"<<data_int;

    return true;
}
