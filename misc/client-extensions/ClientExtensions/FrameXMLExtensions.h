#pragma once

#include <vector>
#include <cstddef>
#include <cstdint>
#include "ClientLua.h"
#include "ClientDetours.h"
#include "Logger.h"

struct Event {
    uint32_t hash;
    uint32_t gap4[4];
    const char* name;
    uint32_t gap18[12];
    uint32_t field48;
    uint32_t field4C;
    uint32_t field50;
};

struct EventList {
    size_t reserve;
    size_t size;
    Event** buf;
};

namespace RCString_C {
    uint32_t __stdcall hash(const char* str);
}

class FrameXMLExtensions {
public:
    static void registerEvent(const char* str);
    static void LoadNewEvents();
    static void Apply();
    static void EmitEvent(const char* str);
    static int GetEventIdByName(const char* eventName);
    static const std::vector<const char*>& getCustomEvents() { return s_customEvents; }
    static std::vector<const char*> s_customEvents;    
};
