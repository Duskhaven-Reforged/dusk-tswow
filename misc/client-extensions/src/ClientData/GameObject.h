#pragma once

#include <SharedDefines.h>
#include <cstddef>
#include <cstdint>

struct CPassenger
{
    uint32_t unk_0000;
    uint32_t unk_0004;
    uint64_t transportGuid;
    C3Vector position;
    uint32_t unk_001C;
    uint64_t compressedRotation;
    uint64_t guid2;
    uint32_t unk_002C;
};

class CGGameObject_C : public CGObject_C {
public:
    uint32_t unk_00D0;
    uint32_t unk_00D4;
    CPassenger m_passenger;
};

inline CGGameObject_C* AsClientGameObject(CGObject_C* object)
{
    if (!object || object->GetTypeID() != TYPEID_GAMEOBJECT)
        return nullptr;

    return reinterpret_cast<CGGameObject_C*>(object);
}
