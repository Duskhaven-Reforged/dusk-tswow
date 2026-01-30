#pragma once
#include "Opcodes.h"
class OpcodeDispatcher;

class VirtualHandler {
public:
    virtual ~VirtualHandler() = default;

    virtual void Register(OpcodeDispatcher& dispatcher) = 0;
    virtual void Unregister(OpcodeDispatcher& dispatcher) = 0;
};
