#pragma once

#include <cstdint>

namespace SpellCachePacketExtensions
{
    void Apply();
    void HandleSpellCastPacket(uint32_t opcode, void* packet);
}
