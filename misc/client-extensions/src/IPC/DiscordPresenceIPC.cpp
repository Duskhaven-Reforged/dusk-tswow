#include "ClientLua.h"
#include "Logger.h"
#include "PacketBuilder.h"

LUA_FUNCTION(DiscordSetGamePresence, (lua_State* L))
{
    std::string characterName = ClientLua::GetString(L, 1);
    uint32_t characterLevel = static_cast<uint32_t>(ClientLua::GetNumber(L, 2));
    std::string className = ClientLua::GetString(L, 3);
    std::string zoneName = ClientLua::GetString(L, 4);

    auto packet = PacketBuilder::CreatePacket(Opcode::CMSG_DISCORD_SET_GAME_PRESENCE);
    packet.writeString(characterName);
    packet.writeUInt32(characterLevel);
    packet.writeString(className);
    packet.writeString(zoneName);
    packet.Send();

    LOG_INFO << "Sent CMSG_DISCORD_SET_GAME_PRESENCE";
    return 0;
}

LUA_FUNCTION(DiscordClearGamePresence, (lua_State* L))
{
    auto packet = PacketBuilder::CreatePacket(Opcode::CMSG_DISCORD_CLEAR_GAME_PRESENCE);
    packet.Send();

    LOG_INFO << "Sent CMSG_DISCORD_CLEAR_GAME_PRESENCE";
    return 0;
}
