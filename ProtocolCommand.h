#ifndef PROTOCOLCOMMAND_H
#define PROTOCOLCOMMAND_H
#include<cstdint>
#include<QByteArray>
#define MAX_PACK 9
//static constexpr int MAX_PACK=9;
enum class COMMAND : uint8_t{
    WEN_DU_REQ=0x01,
    WEN_DU_RSP=0x02
};

class ProtocolCodec{
public:
    static QByteArray pack(uint8_t cmd,uint8_t param ,const QByteArray &data=QByteArray());
    static bool parse(const QByteArray &frame,uint8_t &cmd,uint8_t &param,QByteArray &data);
private:
    static constexpr uint8_t frame_header=0xEF;
    static constexpr uint8_t frame_tail=0xFE;
};


#endif // PROTOCOLCOMMAND_H
