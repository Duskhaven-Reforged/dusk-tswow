#pragma optimize("", off)
#include "FrameXMLExtensions.h"
#include <cstring>
#include <iostream>

std::vector<const char*> FrameXMLExtensions::s_customEvents;
CLIENT_FUNCTION(FireEvent_inner, 0x0081AA00, __cdecl, void, (int eventId, lua_State* L, int nargs))

void FrameXMLExtensions::registerEvent(const char* str) {
    s_customEvents.emplace_back(str);
}

void FrameXMLExtensions::LoadNewEvents() {
    FrameXMLExtensions::registerEvent("TEST_EVENT");
}

void FrameXMLExtensions::Apply() {
   FrameXMLExtensions::LoadNewEvents();
}

void FrameXMLExtensions::EmitEvent(const char* str) {
    LOG_DEBUG << "pre-id";
    int id = FrameXMLExtensions::GetEventIdByName(str);
    LOG_DEBUG << id;

    ClientLua::PushString(ClientLua::State(), str);
    FireEvent_inner(id, ClientLua::State(), 1);
    ClientLua::SetTop(ClientLua::State(), -2);
    LOG_DEBUG << "post-submit";
}

LUA_FUNCTION(EmitFrameEventByName, (lua_State* L)) {
    FrameXMLExtensions::EmitEvent(ClientLua::GetString(L, 1).c_str());
    return 0;
}
inline EventList* GetEventList() { return (EventList*)0x00D3F7D0; }
int FrameXMLExtensions::GetEventIdByName(const char* eventName)
{
    EventList* eventList = GetEventList();
    if (eventList->size == 0)
        return -1;
    LOG_DEBUG << eventList->size;

    uint32_t hash = RCString_C::hash(eventName);
    for (size_t i = 0; i < eventList->size; i++) {
        Event* event = eventList->buf[i];
        if(event && event->name)
            LOG_DEBUG << event->name;
        if (event && strcmp(event->name, eventName) == 0)
            return i;
    }
    return -1;
}

namespace RCString_C {
    inline uint32_t __stdcall hash(const char* str) {
        return ((decltype(&hash))0x0076F640)(str);
    }
}

CLIENT_DETOUR(FrameScript_FillEvents, 0x0081B5F0, __cdecl, void, (const char** list, size_t count)) {
    LOG_DEBUG << "Loading New FrameScript Events";
    std::vector<const char*> events;
    events.reserve(count + FrameXMLExtensions::s_customEvents.size());
    events.insert(events.end(), &list[0], &list[count]);
    events.insert(events.end(), FrameXMLExtensions::s_customEvents.begin(), FrameXMLExtensions::s_customEvents.end());
    FrameScript_FillEvents(events.data(), events.size());
    LOG_DEBUG << "Loaded New FrameScript Events";
}
#pragma optimize("", on)