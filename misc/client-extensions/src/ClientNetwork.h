#pragma once

#include <CustomPacketRead.h>
#include <CustomPacketWrite.h>

#include <functional>

class ClientNetwork {
public:
    static int OnCustomPacket(opcode_t opcode, std::function<void(CustomPacketRead*)> callback);
    static void SendCustomPacket(opcode_t opcode, totalSize_t size, std::function<void(CustomPacketWrite&)> writer);
private:
    static void initialize();
    friend class Main;
};

// do NOT change this without also changing the names in client_header_builder.cpp
#define ON_CUSTOM_PACKET(__listener_name, opcode, callback) \
    int __listener_name##__Result = ClientNetwork::OnCustomPacket(opcode,callback);
