#pragma once

#include <cstdint>
#include <ClientMacros.h>

namespace ClientData
{
    struct WoWTime
    {
        int32_t minute;
        int32_t hour;
        int32_t weekDay;
        int32_t monthDay;
        int32_t month;
        int32_t year;
        int32_t flags;
    };

    struct WoWClientDB
    {
        void* funcTable;
        int32_t isLoaded;
        int32_t numRows;
        int32_t maxIndex;
        int32_t minIndex;
        int32_t stringTable;
        void* funcTable2;
        int32_t* FirstRow;
        int32_t* Rows;
    };

    struct TSLink
    {
        TSLink* m_prevlink;
        void* m_next;
    };

    struct TSList
    {
        intptr_t m_linkoffset;
        TSLink m_terminator;
    };

    struct RCString
    {
        void** vtable;
        uint32_t ukn1;
        char* string;
    };

    struct CVar;
    typedef char(__cdecl* CVarCallback)(CVar*, const char*, const char*, const char*);
    struct CVar
    {
        uint32_t pad[0x18];
        uint32_t m_category;
        uint32_t m_flags;
        RCString m_stringValue;
        float m_numberValue;
        int32_t m_intValue;
        int32_t m_modified;
        RCString m_defaultValue;
        RCString m_resetValue;
        RCString m_latchedValue;
        RCString m_help;
        CVarCallback m_callback;
        void* m_arg;
    };

    struct TerrainClickEvent
    {
        uint64_t GUID;
        float x, y, z;
        uint32_t button;
    };

    struct ZoneLightData
    {
        int32_t mapID;
        int32_t lightID;
        void* pointData;
        int32_t pointNum;
        float minX;
        float minY;
        float maxX;
        float maxY;
    };
}
