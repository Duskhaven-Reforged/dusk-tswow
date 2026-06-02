#include <ClientDetours.h>
#include <Logger.h>
#include <Spells/SpellCache/SpellCacheStreaming.h>

#include <cstdint>

namespace
{
    constexpr uint32_t SMSG_AURA_UPDATE_ALL = 0x495;
    constexpr uint32_t SMSG_AURA_UPDATE = 0x496;
    constexpr uint32_t SPELL_CACHE_PAYLOAD_MARKER = 0x53434831; // SCH1
    constexpr uint32_t SPELL_CACHE_PAYLOAD_BYTES = sizeof(uint32_t) * 3;

    struct AuraUpdateClientPacket
    {
        uint32_t m_padding;
        uint8_t* m_buffer;
        uint32_t m_base;
        uint32_t m_alloc;
        uint32_t m_size;
        uint32_t m_read;
    };

    bool ReadTailU32(AuraUpdateClientPacket* packet, uint32_t offsetFromTail, uint32_t& value)
    {
        if (!packet || !packet->m_buffer || packet->m_size < offsetFromTail + sizeof(uint32_t))
            return false;

        uint32_t const packetOffset = packet->m_size - offsetFromTail - sizeof(uint32_t);
        uint8_t const* data = packet->m_buffer + packetOffset;
        value = uint32_t(data[0]) | (uint32_t(data[1]) << 8) | (uint32_t(data[2]) << 16) | (uint32_t(data[3]) << 24);
        return true;
    }

    void HandleAuraUpdateSpellCachePayload(uint32_t opcode, AuraUpdateClientPacket* packet)
    {
        if (opcode != SMSG_AURA_UPDATE && opcode != SMSG_AURA_UPDATE_ALL)
            return;

        uint32_t marker = 0;
        uint32_t spellId = 0;
        uint32_t spellDataHash = 0;
        if (!ReadTailU32(packet, sizeof(uint32_t) * 2, marker) ||
            !ReadTailU32(packet, sizeof(uint32_t), spellId) ||
            !ReadTailU32(packet, 0, spellDataHash) ||
            marker != SPELL_CACHE_PAYLOAD_MARKER ||
            spellId == 0 ||
            spellDataHash == 0)
        {
            return;
        }

        packet->m_size -= SPELL_CACHE_PAYLOAD_BYTES;
        if (packet->m_read > packet->m_base + packet->m_size)
            packet->m_read = packet->m_base + packet->m_size;

        if (SpellCacheStreaming::HasSpell(spellId, spellDataHash))
            return;

        LOG_INFO << "Spell cache stale from aura update packet" << opcode << spellId << spellDataHash;
        SpellCacheStreaming::RequestSpell(spellId, spellDataHash);
    }
}

CLIENT_DETOUR(SpellCacheAuraUpdatePacket, 0x7300A0, __cdecl, int, (void* param, uint32_t opcode, int timestamp, AuraUpdateClientPacket* packet))
{
    HandleAuraUpdateSpellCachePayload(opcode, packet);
    return SpellCacheAuraUpdatePacket(param, opcode, timestamp, packet);
}
