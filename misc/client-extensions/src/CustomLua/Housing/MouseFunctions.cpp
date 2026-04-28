#include <ClientDetours.h>
#include <ClientData/GameObject.h>
#include <Editor/GizmoPick.h>
#include <ClientData/ObjectManager.h>
#include <ClientLua.h>
#include <Logger.h>
#include <ClientData/SharedDefines.h>

#include <cstdio>
#include <cstdint>
#include <string>

#include "CustomLua/Housing/QuatFunctions.h"
#include <ClientData/MathTypes.h>
#include <ClientData/ObjectFields.h>

using namespace ClientData;

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

static const char* GuidToString(uint64_t guid)
{
    static char buffer[32];
    snprintf(buffer, sizeof(buffer), "%llu", (unsigned long long)guid);
    return buffer;
}

static DecodedGuid lastMouseGUID;
static float lastMouseHitX;
static float lastMouseHitY;
static float lastMouseHitZ;
static float lastMouseRayStartX;
static float lastMouseRayStartY;
static float lastMouseRayStartZ;
static float lastMouseRayEndX;
static float lastMouseRayEndY;
static float lastMouseRayEndZ;
static uint64_t selectedGameObjectGuid;

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
        lastMouseRayStartX = lastMouseHit->rayStartX;
        lastMouseRayStartY = lastMouseHit->rayStartY;
        lastMouseRayStartZ = lastMouseHit->rayStartZ;
        lastMouseRayEndX = lastMouseHit->rayEndX;
        lastMouseRayEndY = lastMouseHit->rayEndY;
        lastMouseRayEndZ = lastMouseHit->rayEndZ;
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

LUA_FUNCTION(GetMouseWorldRay, (lua_State * L))
{
    ClientLua::PushNumber(L, lastMouseRayStartX);
    ClientLua::PushNumber(L, lastMouseRayStartY);
    ClientLua::PushNumber(L, lastMouseRayStartZ);
    ClientLua::PushNumber(L, lastMouseRayEndX);
    ClientLua::PushNumber(L, lastMouseRayEndY);
    ClientLua::PushNumber(L, lastMouseRayEndZ);
    return 6;
}

LUA_FUNCTION(GetLastMouseoverGUID, (lua_State * L))
{
    ClientLua::PushString(L, GuidToString(lastMouseGUID.full));
    ClientLua::PushNumber(L, lastMouseGUID.entry);
    ClientLua::PushNumber(L, lastMouseGUID.low);
    ClientLua::PushNumber(L, lastMouseGUID.type);
    return 4;
}

static C3Vector LastMouseRayStart()
{
    return {lastMouseRayStartX, lastMouseRayStartY, lastMouseRayStartZ};
}

static C3Vector LastMouseRayEnd()
{
    return {lastMouseRayEndX, lastMouseRayEndY, lastMouseRayEndZ};
}

static CGGameObject_C* SelectedGameObject()
{
    if (!selectedGameObjectGuid)
        return nullptr;

    return AsClientGameObject(ObjectManager::GetObject(selectedGameObjectGuid, TYPEMASK_OBJECT));
}

static CGGameObject_C* GameObjectByGuid(uint64_t guid)
{
    return AsClientGameObject(ObjectManager::GetObject(guid, TYPEMASK_OBJECT));
}

static CGGameObject_C* GameObjectByLuaGuid(lua_State* L, int index)
{
    return GameObjectByGuid(std::stoull(ClientLua::GetString(L, index)));
}

static CGGameObject_C* GameObjectByMouse()
{
    return GameObjectByGuid(lastMouseGUID.full);
}

static void PushGameObjectPosition(lua_State* L, CGGameObject_C* gameObject)
{
    ClientLua::PushNumber(L, gameObject->m_passenger.position.x);
    ClientLua::PushNumber(L, gameObject->m_passenger.position.y);
    ClientLua::PushNumber(L, gameObject->m_passenger.position.z);
}

static void PushGameObjectRotation(lua_State* L, CGGameObject_C* gameObject)
{
    float roll, pitch, yaw;
    quat_to_euler(unpack_quat(gameObject->m_passenger.compressedRotation), roll, pitch, yaw);
    ClientLua::PushNumber(L, yaw);
    ClientLua::PushNumber(L, pitch);
    ClientLua::PushNumber(L, roll);
}

LUA_FUNCTION(SelectGobByMouse, (lua_State * L))
{
    CGGameObject_C* gameObject = GameObjectByMouse();
    if (!gameObject)
    {
        selectedGameObjectGuid = 0;
        return 0;
    }

    selectedGameObjectGuid = lastMouseGUID.full;
    ClientLua::PushString(L, GuidToString(selectedGameObjectGuid));
    PushGameObjectPosition(L, gameObject);
    return 4;
}

LUA_FUNCTION(ClearSelectedGob, (lua_State * L))
{
    selectedGameObjectGuid = 0;
    return 0;
}

LUA_FUNCTION(GetSelectedGobGUID, (lua_State * L))
{
    ClientLua::PushString(L, GuidToString(selectedGameObjectGuid));
    return 1;
}

LUA_FUNCTION(GetSelectedGobPosition, (lua_State * L))
{
    CGGameObject_C* gameObject = SelectedGameObject();
    if (!gameObject)
        return 0;

    PushGameObjectPosition(L, gameObject);
    return 3;
}

LUA_FUNCTION(GetGobRotByMouse, (lua_State * L))
{
    // TODO: use a GUID passed in
    CGGameObject_C* gameObject = GameObjectByMouse();
    if (!gameObject)
        return 0;

    PushGameObjectRotation(L, gameObject);

    return 3;
}

LUA_FUNCTION(GetGobPositionByMouse, (lua_State * L))
{
    CGGameObject_C* gameObject = GameObjectByMouse();
    if (!gameObject)
        return 0;

    PushGameObjectPosition(L, gameObject);
    return 3;
}