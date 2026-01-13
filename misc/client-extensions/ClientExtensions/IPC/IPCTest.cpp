#include "IPCTest.h"
#include "ClientLua.h"
#include <iostream>

int IPCTest::attemptConnect() {
    auto packet = PacketBuilder::CreatePacket(Opcode::CMSG_LOBBY_JOIN);
    packet.writeUInt32(1);
    packet.writeUInt32(5);
    packet.writeString("string");
    packet.Send();


   LOG_INFO <<  "Sent CMSG_LOBBY_JOIN\n";
    return 0;
}

LUA_FUNCTION(TestVCConnect, (lua_State* L)) {
    float ox = ClientLua::GetNumber(L, 1);
    float oy = ClientLua::GetNumber(L, 2);
    float oz = ClientLua::GetNumber(L, 3);
        auto packet = PacketBuilder::CreatePacket(Opcode::CMSG_LOBBY_JOIN);
    packet.writeUInt32(1);
    packet.writeUInt32(5);
    packet.writeString("string");
    packet.Send();


   LOG_INFO <<  "Sent CMSG_LOBBY_JOIN\n";
    return 0;
}
