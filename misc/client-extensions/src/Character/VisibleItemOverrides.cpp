#include <Character/VisibleItemOverrides.h>

#include <ClientDetours.h>
#include <ClientLua.h>
#include <ClientNetwork.h>
#include <Logger.h>
#include <SharedDefines.h>

#include <CustomPacketRead.h>

#include <array>
#include <cstdint>
#include <sstream>
#include <string>
#include <unordered_map>

namespace
{
    constexpr opcode_t VISIBLE_ITEM_OVERRIDE_OPCODE = 0x7A10;
    constexpr size_t MAX_VISIBLE_ITEM_SLOTS = 19;
    constexpr bool DEBUG_VISIBLE_ITEM_OVERRIDES = true;

    struct VisibleItemOverride
    {
        uint32_t entryId = 0;
        uint32_t enchantPacked = 0;
        uint32_t transmogEntry = 0;
        int32_t leftShoulderDisplay = -1;
        int32_t rightShoulderDisplay = -1;
        uint32_t flags = 0;
        bool active = false;
    };

    using VisibleItemOverrideSlots = std::array<VisibleItemOverride, MAX_VISIBLE_ITEM_SLOTS>;

    std::unordered_map<uint64_t, VisibleItemOverrideSlots> s_visibleItemOverrides;

    std::string EscapeLuaString(const std::string& input)
    {
        std::string escaped;
        escaped.reserve(input.size() + 8);
        for (char c : input)
        {
            switch (c)
            {
                case '\\':
                    escaped += "\\\\";
                    break;
                case '"':
                    escaped += "\\\"";
                    break;
                default:
                    escaped += c;
                    break;
            }
        }
        return escaped;
    }

    void DebugVisibleItemOverride(
        uint64_t guid,
        uint8_t slot,
        bool hasItem,
        uint32_t entryId,
        uint32_t transmogEntry,
        int32_t leftShoulderDisplay,
        int32_t rightShoulderDisplay)
    {
        if (!DEBUG_VISIBLE_ITEM_OVERRIDES)
            return;

        std::ostringstream message;
        message << "[VisibleItemOverride] guid=" << guid
                << " slot=" << uint32_t(slot)
                << " hasItem=" << uint32_t(hasItem ? 1 : 0)
                << " entry=" << entryId
                << " transmog=" << transmogEntry
                << " left=" << leftShoulderDisplay
                << " right=" << rightShoulderDisplay;

        const std::string text = message.str();
        LOG_INFO << text;

        lua_State* state = ClientLua::State();
        if (state)
        {
            const std::string escaped = EscapeLuaString(text);
            const std::string lua =
                "if DEFAULT_CHAT_FRAME then DEFAULT_CHAT_FRAME:AddMessage(\"" + escaped + "\") end";
            ClientLua::DoString(lua.c_str(), state);
        }
    }

    uint64_t GetPlayerGuid(void* self)
    {
        if (!self)
            return 0;

        CGPlayer* player = reinterpret_cast<CGPlayer*>(self);
        if (!player->unitBase.objectBase.ObjectData)
            return 0;

        return player->unitBase.objectBase.ObjectData->OBJECT_FIELD_GUID;
    }

    void StoreVisibleItemOverride(CustomPacketRead* packet)
    {
        if (!packet)
            return;

        uint64_t guid = packet->Read<uint64_t>(0);
        uint8_t slot = packet->Read<uint8_t>(0);
        bool hasItem = packet->Read<uint8_t>(0) != 0;

        if (!guid || slot >= MAX_VISIBLE_ITEM_SLOTS)
            return;

        uint32_t entryId = 0;
        uint32_t transmogEntry = 0;
        int32_t leftShoulderDisplay = -1;
        int32_t rightShoulderDisplay = -1;

        VisibleItemOverride& overrideData = s_visibleItemOverrides[guid][slot];
        if (!hasItem)
        {
            overrideData = {};
            overrideData.leftShoulderDisplay = -1;
            overrideData.rightShoulderDisplay = -1;
            DebugVisibleItemOverride(guid, slot, false, 0, 0, -1, -1);
            return;
        }

        entryId = packet->Read<uint32_t>(0);
        const uint16_t permanentEnchant = packet->Read<uint16_t>(0);
        const uint16_t temporaryEnchant = packet->Read<uint16_t>(0);
        transmogEntry = packet->Read<uint32_t>(0);
        leftShoulderDisplay = packet->Read<int32_t>(-1);
        rightShoulderDisplay = packet->Read<int32_t>(-1);

        overrideData.entryId = entryId;
        overrideData.transmogEntry = transmogEntry;
        overrideData.leftShoulderDisplay = leftShoulderDisplay;
        overrideData.rightShoulderDisplay = rightShoulderDisplay;
        overrideData.flags = packet->Read<uint32_t>(0);
        overrideData.enchantPacked = uint32_t(permanentEnchant) | (uint32_t(temporaryEnchant) << 16);
        overrideData.active = true;

        DebugVisibleItemOverride(
            guid,
            slot,
            true,
            entryId,
            transmogEntry,
            leftShoulderDisplay,
            rightShoulderDisplay);
    }

    CLIENT_DETOUR_THISCALL(CGPlayer_C__GetVisibleItemEntryId_Override, 0x6DE330, uint32_t, (int slot))
    {
        if (slot >= 0 && slot < int(MAX_VISIBLE_ITEM_SLOTS))
        {
            uint64_t guid = GetPlayerGuid(self);
            auto itr = s_visibleItemOverrides.find(guid);
            if (itr != s_visibleItemOverrides.end())
            {
                VisibleItemOverride& overrideData = itr->second[slot];
                if (overrideData.active)
                    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&overrideData.entryId));
            }
        }

        return CGPlayer_C__GetVisibleItemEntryId_Override(self, slot);
    }
}

void VisibleItemOverrides::Apply()
{
    ClientNetwork::OnCustomPacket(VISIBLE_ITEM_OVERRIDE_OPCODE, [](CustomPacketRead* packet)
    {
        StoreVisibleItemOverride(packet);
    });
}
