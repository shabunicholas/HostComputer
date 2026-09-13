#ifndef PROTOCOLCOMMAND_H
#define PROTOCOLCOMMAND_H
#include<cstdint>
enum class COMMAND : uint8_t{
    WEN_DU_REQ=0x01,
    WEN_DU_RSP=0x02
};

#endif // PROTOCOLCOMMAND_H
