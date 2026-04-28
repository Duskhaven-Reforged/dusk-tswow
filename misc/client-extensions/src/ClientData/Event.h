#pragma once

#include <ClientMacros.h>

#include <cstdint>

namespace ClientData
{
    enum EventId
    {
        EVENT_ID_0 = 0,
        EVENT_ID_CHAR = 1,
        EVENT_ID_FOCUS = 2,
        EVENT_ID_3 = 3,
        EVENT_ID_DESTROY = 4,
        EVENT_ON_UPDATE = 5,
        EVENT_ID_IDLE = 6,
        EVENT_ID_POLL = 7,
        EVENT_ID_INITIALIZE = 8,
        EVENT_ID_KEYDOWN = 9,
        EVENT_ID_KEYUP = 10,
        EVENT_ID_KEYDOWN_REPEATING = 11,
        EVENT_ID_MOUSEDOWN = 12,
        EVENT_ID_MOUSEMOVE = 13,
        EVENT_ID_MOUSEMOVE_RELATIVE = 14,
        EVENT_ID_MOUSEUP = 15,
        EVENT_ID_MOUSEMODE_CHANGED = 16,
        EVENT_ID_MOUSEWHEEL = 17,
        EVENT_ID_18 = 18,
        EVENT_ID_19 = 19,
        EVENT_ID_20 = 20,
        EVENT_ID_21 = 21,
        EVENT_ID_22 = 22,
        EVENT_ID_PAINT = 23,
        EVENT_ID_NET_DATA = 24,
        EVENT_ID_NET_CONNECT = 25,
        EVENT_ID_NET_DISCONNECT = 26,
        EVENT_ID_NET_CANTCONNECT = 27,
        EVENT_ID_NET_DESTROY = 28,
        EVENT_ID_NET_AUTH_CHALLENGE = 29,
        EVENT_ID_30 = 30,
        EVENT_ID_31 = 31,
        EVENT_ID_32 = 32,
        EVENT_ID_33 = 33,
        EVENT_ID_IME = 34,
        EVENT_ID_SIZE = 35,
        EVENTIDS = 36,
    };

    enum MouseButton : uint32_t
    {
        MOUSE_BUTTON_NONE = 0x0,
        MOUSE_BUTTON_LEFT = 0x1,
        MOUSE_BUTTON_MIDDLE = 0x2,
        MOUSE_BUTTON_RIGHT = 0x4,
        MOUSE_BUTTON_XBUTTON1 = 0x8,
        MOUSE_BUTTON_XBUTTON2 = 0x10,
        MOUSE_BUTTON_XBUTTON3 = 0x20,
        MOUSE_BUTTON_XBUTTON4 = 0x40,
        MOUSE_BUTTON_XBUTTON5 = 0x80,
        MOUSE_BUTTON_XBUTTON6 = 0x100,
        MOUSE_BUTTON_XBUTTON7 = 0x200,
        MOUSE_BUTTON_XBUTTON8 = 0x400,
        MOUSE_BUTTON_XBUTTON9 = 0x800,
        MOUSE_BUTTON_XBUTTON10 = 0x1000,
        MOUSE_BUTTON_XBUTTON11 = 0x2000,
        MOUSE_BUTTON_XBUTTON12 = 0x4000,
        MOUSE_BUTTON_ALL = 0xFFFFFFFF,
    };

    enum MouseMode
    {
        MOUSE_MODE_NORMAL = 0x0,
        MOUSE_MODE_RELATIVE = 0x1,
        MOUSE_MODES = 0x2,
    };

    struct EventDataMouse
    {
        MouseMode mode;
        MouseButton button;
        uint32_t buttonState;
        uint32_t metaKeyState;
        uint32_t flags;
        float x;
        float y;
        int32_t wheelDistance;
        uint32_t time;
    };

    using EventHandlerFunc = int32_t (*)(const void*, void*);

    CLIENT_FUNCTION(EventRegisterEx, 0x47D3C0, __cdecl, void, (EventId, EventHandlerFunc, void*, float))
    CLIENT_FUNCTION(EventUnregister, 0x47D790, __cdecl, void, (EventId, EventHandlerFunc))
}
