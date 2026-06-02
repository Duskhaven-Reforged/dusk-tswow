#include <Spells/SpellCache/Packets/SpellStart.h>

#include <ClientMacros.h>
#include <ClientData/SharedDefines.h>
#include <ClientData/Spell.h>
#include <Logger.h>
#include <Spells/SpellCache/SpellCacheStreaming.h>

#include <array>

namespace
{
    constexpr uint32_t SMSG_SPELL_START = 0x131;
    constexpr uint32_t SMSG_SPELL_GO = 0x132;
    constexpr uint32_t SPELL_CACHE_PAYLOAD_MARKER = 0x53434831; // SCH1
    constexpr uint32_t CAST_FLAG_AMMO = 0x00000020;
    constexpr uint32_t CAST_FLAG_POWER_LEFT_SELF = 0x00000800;
    constexpr uint32_t CAST_FLAG_ADJUST_MISSILE = 0x00020000;
    constexpr uint32_t CAST_FLAG_VISUAL_CHAIN = 0x00080000;
    constexpr uint32_t CAST_FLAG_RUNE_LIST = 0x00200000;
    constexpr uint32_t CAST_FLAG_IMMUNITY = 0x04000000;
    constexpr uint32_t TARGET_FLAG_UNIT = 0x00000002;
    constexpr uint32_t TARGET_FLAG_UNIT_RAID = 0x00000004;
    constexpr uint32_t TARGET_FLAG_UNIT_PARTY = 0x00000008;
    constexpr uint32_t TARGET_FLAG_ITEM = 0x00000010;
    constexpr uint32_t TARGET_FLAG_SOURCE_LOCATION = 0x00000020;
    constexpr uint32_t TARGET_FLAG_DEST_LOCATION = 0x00000040;
    constexpr uint32_t TARGET_FLAG_UNIT_ENEMY = 0x00000080;
    constexpr uint32_t TARGET_FLAG_UNIT_ALLY = 0x00000100;
    constexpr uint32_t TARGET_FLAG_CORPSE_ENEMY = 0x00000200;
    constexpr uint32_t TARGET_FLAG_UNIT_DEAD = 0x00000400;
    constexpr uint32_t TARGET_FLAG_GAMEOBJECT = 0x00000800;
    constexpr uint32_t TARGET_FLAG_TRADE_ITEM = 0x00001000;
    constexpr uint32_t TARGET_FLAG_STRING = 0x00002000;
    constexpr uint32_t TARGET_FLAG_GAMEOBJECT_ITEM = 0x00004000;
    constexpr uint32_t TARGET_FLAG_CORPSE_ALLY = 0x00008000;
    constexpr uint32_t TARGET_FLAG_UNIT_MINIPET = 0x00010000;
    constexpr uint32_t TARGET_FLAG_UNIT_PASSENGER = 0x00100000;
    constexpr uint32_t TARGET_FLAG_UNIT_MASK = TARGET_FLAG_UNIT | TARGET_FLAG_UNIT_RAID | TARGET_FLAG_UNIT_PARTY |
        TARGET_FLAG_UNIT_ENEMY | TARGET_FLAG_UNIT_ALLY | TARGET_FLAG_UNIT_DEAD | TARGET_FLAG_UNIT_MINIPET | TARGET_FLAG_UNIT_PASSENGER;
    constexpr uint32_t TARGET_FLAG_GAMEOBJECT_MASK = TARGET_FLAG_GAMEOBJECT | TARGET_FLAG_GAMEOBJECT_ITEM;
    constexpr uint32_t TARGET_FLAG_CORPSE_MASK = TARGET_FLAG_CORPSE_ALLY | TARGET_FLAG_CORPSE_ENEMY;
    constexpr uint32_t TARGET_FLAG_ITEM_MASK = TARGET_FLAG_TRADE_ITEM | TARGET_FLAG_ITEM | TARGET_FLAG_GAMEOBJECT_ITEM;

    struct SpellStartClientPacket
    {
        uint32_t m_padding;
        uint8_t* m_buffer;
        uint32_t m_base;
        uint32_t m_alloc;
        uint32_t m_size;
        uint32_t m_read;
    };

    bool CanRead(SpellStartClientPacket* packet, uint32_t bytes)
    {
        return packet && packet->m_buffer && packet->m_read >= packet->m_base && packet->m_read + bytes <= packet->m_base + packet->m_size;
    }

    bool ReadU8(SpellStartClientPacket* packet, uint8_t& value)
    {
        if (!CanRead(packet, sizeof(value)))
            return false;

        value = packet->m_buffer[packet->m_read - packet->m_base];
        packet->m_read += sizeof(value);
        return true;
    }

    bool ReadU32(SpellStartClientPacket* packet, uint32_t& value)
    {
        if (!CanRead(packet, sizeof(value)))
            return false;

        uint8_t const* data = packet->m_buffer + packet->m_read - packet->m_base;
        value = uint32_t(data[0]) | (uint32_t(data[1]) << 8) | (uint32_t(data[2]) << 16) | (uint32_t(data[3]) << 24);
        packet->m_read += sizeof(value);
        return true;
    }

    bool ReadU64(SpellStartClientPacket* packet, uint64_t& value)
    {
        if (!CanRead(packet, sizeof(value)))
            return false;

        uint8_t const* data = packet->m_buffer + packet->m_read - packet->m_base;
        value = uint64_t(data[0]) |
            (uint64_t(data[1]) << 8) |
            (uint64_t(data[2]) << 16) |
            (uint64_t(data[3]) << 24) |
            (uint64_t(data[4]) << 32) |
            (uint64_t(data[5]) << 40) |
            (uint64_t(data[6]) << 48) |
            (uint64_t(data[7]) << 56);
        packet->m_read += sizeof(value);
        return true;
    }

    bool Skip(SpellStartClientPacket* packet, uint32_t bytes)
    {
        if (!CanRead(packet, bytes))
            return false;

        packet->m_read += bytes;
        return true;
    }

    bool SkipPackedGuid(SpellStartClientPacket* packet)
    {
        uint8_t mask = 0;
        if (!ReadU8(packet, mask))
            return false;

        uint32_t bytes = 0;
        for (uint8_t bit = 0; bit < 8; ++bit)
            if (mask & (1 << bit))
                ++bytes;

        return Skip(packet, bytes);
    }

    bool ReadPackedGuid(SpellStartClientPacket* packet, uint64_t& guid)
    {
        guid = 0;

        uint8_t mask = 0;
        if (!ReadU8(packet, mask))
            return false;

        for (uint8_t bit = 0; bit < 8; ++bit)
        {
            if ((mask & (1 << bit)) == 0)
                continue;

            uint8_t value = 0;
            if (!ReadU8(packet, value))
                return false;

            guid |= static_cast<uint64_t>(value) << (bit * 8);
        }

        return true;
    }

    bool SkipCString(SpellStartClientPacket* packet)
    {
        if (!packet || !packet->m_buffer)
            return false;

        while (CanRead(packet, 1))
        {
            uint8_t value = 0;
            if (!ReadU8(packet, value))
                return false;

            if (!value)
                return true;
        }

        return false;
    }

    bool SkipTargetData(SpellStartClientPacket* packet, uint32_t& flags)
    {
        flags = 0;
        if (!ReadU32(packet, flags))
            return false;

        if ((flags & (TARGET_FLAG_UNIT_MASK | TARGET_FLAG_GAMEOBJECT_MASK | TARGET_FLAG_CORPSE_MASK)) && !SkipPackedGuid(packet))
            return false;

        if ((flags & TARGET_FLAG_ITEM_MASK) && !SkipPackedGuid(packet))
            return false;

        if ((flags & TARGET_FLAG_SOURCE_LOCATION) && (!SkipPackedGuid(packet) || !Skip(packet, sizeof(float) * 3)))
            return false;

        if ((flags & TARGET_FLAG_DEST_LOCATION) && (!SkipPackedGuid(packet) || !Skip(packet, sizeof(float) * 3)))
            return false;

        if ((flags & TARGET_FLAG_STRING) && !SkipCString(packet))
            return false;

        return true;
    }

    bool TryReadSpellCastInfo(SpellStartClientPacket* packet, uint32_t opcode, uint64_t& casterGuid, std::array<uint64_t, 8>& hitGuids, uint32_t& hitGuidCount, uint32_t& spellId, uint32_t& spellDataHash)
    {
        if (!packet)
            return false;

        uint32_t const savedRead = packet->m_read;
        casterGuid = 0;
        hitGuids.fill(0);
        hitGuidCount = 0;
        spellId = 0;
        spellDataHash = 0;

        uint8_t castId = 0;
        uint32_t castFlags = 0;
        uint32_t castTime = 0;
        uint32_t targetFlags = 0;
        if (!ReadPackedGuid(packet, casterGuid) || !SkipPackedGuid(packet) || !ReadU8(packet, castId) ||
            !ReadU32(packet, spellId) || !ReadU32(packet, castFlags) || !ReadU32(packet, castTime))
        {
            packet->m_read = savedRead;
            return false;
        }

        if (opcode == SMSG_SPELL_GO)
        {
            uint8_t hitCount = 0;
            if (!ReadU8(packet, hitCount))
            {
                packet->m_read = savedRead;
                return false;
            }

            for (uint8_t i = 0; i < hitCount; ++i)
            {
                uint64_t hitGuid = 0;
                if (!ReadU64(packet, hitGuid))
                {
                    packet->m_read = savedRead;
                    return false;
                }

                if (hitGuidCount < hitGuids.size())
                    hitGuids[hitGuidCount++] = hitGuid;
            }

            uint8_t missCount = 0;
            if (!ReadU8(packet, missCount))
            {
                packet->m_read = savedRead;
                return false;
            }

            for (uint8_t i = 0; i < missCount; ++i)
            {
                uint8_t reason = 0;
                if (!Skip(packet, sizeof(uint64_t)) || !ReadU8(packet, reason))
                {
                    packet->m_read = savedRead;
                    return false;
                }

                if (reason == 11 && !Skip(packet, sizeof(uint8_t)))
                {
                    packet->m_read = savedRead;
                    return false;
                }
            }
        }

        if (!SkipTargetData(packet, targetFlags))
        {
            packet->m_read = savedRead;
            return false;
        }

        if ((castFlags & CAST_FLAG_POWER_LEFT_SELF) && !Skip(packet, sizeof(uint32_t)))
        {
            packet->m_read = savedRead;
            return false;
        }

        if (castFlags & CAST_FLAG_RUNE_LIST)
        {
            uint8_t start = 0;
            uint8_t count = 0;
            if (!ReadU8(packet, start) || !ReadU8(packet, count) || !Skip(packet, count))
            {
                packet->m_read = savedRead;
                return false;
            }
        }

        if ((castFlags & CAST_FLAG_ADJUST_MISSILE) && !Skip(packet, sizeof(float) + sizeof(uint32_t)))
        {
            packet->m_read = savedRead;
            return false;
        }

        if ((castFlags & CAST_FLAG_AMMO) && !Skip(packet, sizeof(uint32_t) * 2))
        {
            packet->m_read = savedRead;
            return false;
        }

        if ((castFlags & CAST_FLAG_IMMUNITY) && !Skip(packet, sizeof(uint32_t) * 2))
        {
            packet->m_read = savedRead;
            return false;
        }

        if ((castFlags & CAST_FLAG_VISUAL_CHAIN) && !Skip(packet, sizeof(uint32_t) * 2))
        {
            packet->m_read = savedRead;
            return false;
        }

        if ((targetFlags & TARGET_FLAG_DEST_LOCATION) && !Skip(packet, sizeof(uint8_t)))
        {
            packet->m_read = savedRead;
            return false;
        }

        uint32_t marker = 0;
        uint32_t payloadSpellId = 0;
        uint32_t payloadHash = 0;
        if (packet->m_read + sizeof(uint32_t) * 3 <= packet->m_base + packet->m_size &&
            ReadU32(packet, marker) && ReadU32(packet, payloadSpellId) && ReadU32(packet, payloadHash) &&
            marker == SPELL_CACHE_PAYLOAD_MARKER && payloadSpellId == spellId)
        {
            spellDataHash = payloadHash;
        }

        packet->m_read = savedRead;
        return spellId != 0;
    }
}

void SpellCachePacketExtensions::Apply()
{
    LOG_INFO << "Spell cache packet intercepts enabled";
}

void SpellCachePacketExtensions::HandleSpellCastPacket(uint32_t opcode, void* packet)
{
    if (opcode != SMSG_SPELL_START && opcode != SMSG_SPELL_GO)
        return;

    uint32_t spellId = 0;
    uint32_t spellDataHash = 0;
    uint64_t casterGuid = 0;
    std::array<uint64_t, 8> hitGuids{};
    uint32_t hitGuidCount = 0;
    if (!TryReadSpellCastInfo(static_cast<SpellStartClientPacket*>(packet), opcode, casterGuid, hitGuids, hitGuidCount, spellId, spellDataHash))
        return;
    (void)casterGuid;
    (void)hitGuids;
    (void)hitGuidCount;

    if (!spellDataHash)
    {
        ClientData::SpellRow row{};
        if (SpellCacheStreaming::TryGetSpellRow(spellId, row, false))
            return;
    }

    if (spellDataHash ? SpellCacheStreaming::HasSpell(spellId, spellDataHash) : SpellCacheStreaming::HasSpell(spellId))
        return;

    LOG_INFO << "Spell cache stale from spell cast packet" << opcode << spellId << spellDataHash;
    SpellCacheStreaming::RequestSpell(spellId, spellDataHash);
}
