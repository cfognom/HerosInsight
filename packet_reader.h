#pragma once

#include <GWCA/Packets/Opcodes.h>
#include <GWCA/Packets/StoC.h>

namespace HerosInsight::PacketReader
{
    void Initialize();
    void Terminate();
    uint32_t GetPing();
}