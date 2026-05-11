#pragma once

#include <ClientData/GameObject.h>
#include <ClientData/ObjectFields.h>
#include <ClientData/ObjectManager.h>

#include <cstdint>
#include <string>

namespace EditorObject
{
    struct DecodedGuid
    {
        uint64_t full = 0;
        uint16_t type = 0;
        uint32_t entry = 0;
        uint32_t low = 0;
    };

    inline DecodedGuid DecodeClientGuid(uint32_t rawLow, uint32_t rawHigh)
    {
        DecodedGuid guid{};
        guid.full = (uint64_t(rawHigh) << 32) | rawLow;
        guid.type = uint16_t(guid.full >> 48);
        guid.entry = uint32_t((guid.full >> 24) & 0xFFFFFF);
        guid.low = uint32_t(guid.full & 0xFFFFFF);
        return guid;
    }

    inline std::string GuidToString(uint64_t guid)
    {
        return std::to_string(guid);
    }

    inline ClientData::CGGameObject_C* GameObjectByGuid(uint64_t guid)
    {
        return ClientData::AsClientGameObject(ClientData::ObjectManager::GetObject(guid, ClientData::TYPEMASK_OBJECT));
    }
}
