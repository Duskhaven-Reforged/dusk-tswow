#include <ClientDetours.h>
#include <ClientLua.h>
#include <FSRoot.h>
#include <Logger.h>
#include <SharedDefines.h>
#include <Windows.h>
#include <World/WMOLogging.h>
#include <cstdint>
#include <string>
#include <vector>
#include "CustomLua/Housing/QuatFunctions.cpp"

struct HitTestResult
{
    uint32_t guidLow;  // 0x00
    uint32_t guidHigh; // 0x04
    float x;           // 0x08
    float y;           // 0x0C
    float z;           // 0x10
    float dist;        // 0x14
    float rayStartX;   // 0x18
    float rayStartY;   // 0x1C
    float rayStartZ;   // 0x20
    float rayEndX;     // 0x24
    float rayEndY;     // 0x28
    float rayEndZ;     // 0x2C
};

struct DecodedGuid
{
    uint64_t full;
    uint16_t type;
    uint32_t entry;
    uint32_t low;
};

static DecodedGuid DecodeClientGuid(uint32_t rawLow, uint32_t rawHigh)
{
    DecodedGuid g{};
    g.full  = (uint64_t(rawHigh) << 32) | rawLow;
    g.type  = (uint16_t)(g.full >> 48);
    g.entry = (uint32_t)((g.full >> 24) & 0xFFFFFF);
    g.low   = (uint32_t)(g.full & 0xFFFFFF);
    return g;
}

const char* GuidToString(uint64_t guid)
{
    static char buffer[32];
    snprintf(buffer, sizeof(buffer), "%llu", (unsigned long long)guid);
    return buffer;
}

DecodedGuid lastMouseGUID;
float lastMouseHitX;
float lastMouseHitY;
float lastMouseHitZ;

CLIENT_DETOUR_THISCALL(CGWorldFrame__HitTestPoint, 0x004F9DA0, int, (float a2, float a3, int a4, int a5))
{
    int result = CGWorldFrame__HitTestPoint(self, a2, a3, a4, a5);

    if (a5)
    {
        HitTestResult* lastMouseHit = reinterpret_cast<HitTestResult*>(a5);
        lastMouseGUID =
            result >= 2 ? DecodeClientGuid(lastMouseHit->guidLow, lastMouseHit->guidHigh) : DecodeClientGuid(0, 0);
        lastMouseHitX = lastMouseHit->x;
        lastMouseHitY = lastMouseHit->y;
        lastMouseHitZ = lastMouseHit->z;
    }

    return result;
}

LUA_FUNCTION(GetMouseWorldPosition, (lua_State * L))
{
    ClientLua::PushNumber(L, lastMouseHitX);
    ClientLua::PushNumber(L, lastMouseHitY);
    ClientLua::PushNumber(L, lastMouseHitZ);
    return 3;
}

LUA_FUNCTION(GetLastMouseoverGUID, (lua_State * L))
{
    ClientLua::PushString(L, GuidToString(lastMouseGUID.full));
    ClientLua::PushNumber(L, lastMouseGUID.entry);
    ClientLua::PushNumber(L, lastMouseGUID.low);
    ClientLua::PushNumber(L, lastMouseGUID.type);
    return 4;
}

struct TestGameObject
{
    char pad1[0xE8];
    C3Vector position;
    uint32_t unk;
    uint64_t packedQuaternion;
};

LUA_FUNCTION(RotateGobByMouse, (lua_State * L))
{
    CGObject_C* obj = static_cast<CGObject_C*>(ClntObjMgr::ObjectPtr(lastMouseGUID.full, TYPEMASK_OBJECT));

    if (!obj || obj->GetTypeID() != TYPEID_GAMEOBJECT)
        return 0;

    TestGameObject* gameObject = reinterpret_cast<TestGameObject*>(obj);

    gameObject->packedQuaternion =
        add_euler_delta_to_packed_quat(gameObject->packedQuaternion, ClientLua::GetNumber(L, 1),
                                       ClientLua::GetNumber(L, 2), ClientLua::GetNumber(L, 3));

    obj->UpdateWorldObject(0);

    float roll, pitch, yaw;
    quat_to_euler(unpack_quat(gameObject->packedQuaternion), roll, pitch, yaw);

    ClientLua::PushNumber(L, yaw);
    ClientLua::PushNumber(L, pitch);
    ClientLua::PushNumber(L, roll);

    return 3;
}

LUA_FUNCTION(MoveGobByMouse, (lua_State * L))
{
    // TODO: use a GUID passed in
    CGObject_C* obj = static_cast<CGObject_C*>(ClntObjMgr::ObjectPtr(lastMouseGUID.full, TYPEMASK_OBJECT));
    if (obj && (obj->GetTypeID() == TYPEID_GAMEOBJECT))
    {
        TestGameObject* gameObject = reinterpret_cast<TestGameObject*>(obj);
        LOG_DEBUG << gameObject->position.x << " " << gameObject->position.y << " " << gameObject->position.z;
        gameObject->position.x += ClientLua::GetNumber(L, 1);
        gameObject->position.y += ClientLua::GetNumber(L, 2);
        gameObject->position.z += ClientLua::GetNumber(L, 3);
        obj->UpdateWorldObject(0);
        ClientLua::PushNumber(L, gameObject->position.x);
        ClientLua::PushNumber(L, gameObject->position.y);
        ClientLua::PushNumber(L, gameObject->position.z);
    }
    return 3;
}