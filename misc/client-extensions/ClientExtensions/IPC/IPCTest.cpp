#include "IPCTest.h"
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
