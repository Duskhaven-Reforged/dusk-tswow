#pragma once

#include <ClientMacros.h>
#include <ClientData/Object.h>
#include <ClientData/ObjectFields.h>

#include <cstdint>

namespace ClientData::ObjectManager
{
    CLIENT_FUNCTION(ObjectPtr, 0x4D4DB0, __cdecl, CGObject_C*, (uint64_t guid, uint32_t typeMask))
    CLIENT_FUNCTION(GetActivePlayerObject, 0x4038F0, __cdecl, CGObject_C*, ())

    inline CGObject_C* GetObject(uint64_t guid, ObjectTypeMask typeMask)
    {
        return ObjectPtr(guid, static_cast<uint32_t>(typeMask));
    }
}
