#pragma once

#include <cstdint>

#include "../VirtualHandler.h"
#include "../HandlerRegistry.h"
#include "../../IPC/OpcodeDispatcher.h"
#include "../../IPC/PacketBuilder.h"
#include "../../DiscordManager/DiscordManager.h"
#include "../Opcodes.h"

class OpcodeDispatcher;

class LobbyHandler : public VirtualHandler {
public:

    LobbyHandler() = default;
    ~LobbyHandler() = default;

    void Register(OpcodeDispatcher& dispatcher) override;
    void Unregister(OpcodeDispatcher& dispatcher) override;


private:
};
