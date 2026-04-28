#include <ClientDetours.h>
#include <ClientData/GameObject.h>
#include <ClientData/GizmoPick.h>
#include <ClientData/ObjectManager.h>
#include <ClientLua.h>
#include <Logger.h>
#include <SharedDefines.h>

#include <cstdio>
#include <cstdint>
#include <string>

#include "CustomLua/Housing/QuatFunctions.h"

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

    return AsClientGameObject(ClientData::ObjectManager::GetObject(selectedGameObjectGuid, TYPEMASK_OBJECT));
}

static CGGameObject_C* GameObjectByGuid(uint64_t guid)
{
    return AsClientGameObject(ClientData::ObjectManager::GetObject(guid, TYPEMASK_OBJECT));
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

static void SetGameObjectPosition(CGGameObject_C* gameObject, float x, float y, float z)
{
    gameObject->m_passenger.position.x = x;
    gameObject->m_passenger.position.y = y;
    gameObject->m_passenger.position.z = z;
    gameObject->UpdateWorldObject(0);
}

static void MoveGameObject(CGGameObject_C* gameObject, float x, float y, float z)
{
    SetGameObjectPosition(gameObject, gameObject->m_passenger.position.x + x, gameObject->m_passenger.position.y + y,
                          gameObject->m_passenger.position.z + z);
}

static void SetGameObjectRotation(CGGameObject_C* gameObject, uint64_t compressedRotation)
{
    gameObject->m_passenger.compressedRotation = compressedRotation;
    gameObject->UpdateWorldObject(0);
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

LUA_FUNCTION(PickSelectedGobTranslationAxis, (lua_State * L))
{
    CGGameObject_C* gameObject = SelectedGameObject();
    if (!gameObject)
        return 0;

    ClientData::Axis axis = ClientData::PickTranslationGizmo(
        LastMouseRayStart(), LastMouseRayEnd(), gameObject->m_passenger.position,
        static_cast<float>(ClientLua::GetNumber(L, 1, 1.0)));
    ClientLua::PushNumber(L, static_cast<int32_t>(axis));
    return 1;
}

LUA_FUNCTION(MoveSelectedGobAxis, (lua_State * L))
{
    CGGameObject_C* gameObject = SelectedGameObject();
    if (!gameObject)
        return 0;

    ClientData::Axis axis = static_cast<ClientData::Axis>(static_cast<int32_t>(ClientLua::GetNumber(L, 1, -1.0)));
    C3Vector direction = ClientData::AxisDirection(axis);
    float amount = static_cast<float>(ClientLua::GetNumber(L, 2, 0.0));

    MoveGameObject(gameObject, direction.x * amount, direction.y * amount, direction.z * amount);

    PushGameObjectPosition(L, gameObject);
    return 3;
}

LUA_FUNCTION(RotateSelectedGobAxis, (lua_State * L))
{
    CGGameObject_C* gameObject = SelectedGameObject();
    if (!gameObject)
        return 0;

    ClientData::Axis axis = static_cast<ClientData::Axis>(static_cast<int32_t>(ClientLua::GetNumber(L, 1, -1.0)));
    C3Vector direction = ClientData::AxisDirection(axis);
    float angle = static_cast<float>(ClientLua::GetNumber(L, 2, 0.0));

    SetGameObjectRotation(gameObject,
                          add_axis_delta_to_packed_quat(gameObject->m_passenger.compressedRotation, direction.x,
                                                        direction.y, direction.z, angle));

    PushGameObjectRotation(L, gameObject);
    return 3;
}

LUA_FUNCTION(PickSelectedGobRotationAxis, (lua_State * L))
{
    CGGameObject_C* gameObject = SelectedGameObject();
    if (!gameObject)
        return 0;

    ClientData::Axis axis = ClientData::PickRotationGizmo(
        LastMouseRayStart(), LastMouseRayEnd(), gameObject->m_passenger.position,
        static_cast<float>(ClientLua::GetNumber(L, 1, 1.0)));
    ClientLua::PushNumber(L, static_cast<int32_t>(axis));
    return 1;
}

LUA_FUNCTION(RotateGobByGUID, (lua_State * L))
{
    CGGameObject_C* gameObject = GameObjectByLuaGuid(L, 1);
    if (!gameObject)
        return 0;

    SetGameObjectRotation(gameObject,
                          add_euler_delta_to_packed_quat(gameObject->m_passenger.compressedRotation,
                                                         ClientLua::GetNumber(L, 2), ClientLua::GetNumber(L, 3),
                                                         ClientLua::GetNumber(L, 4)));
    PushGameObjectRotation(L, gameObject);

    return 3;
}

LUA_FUNCTION(RotateGobByMouse, (lua_State * L))
{
    CGGameObject_C* gameObject = GameObjectByMouse();
    if (!gameObject)
        return 0;

    SetGameObjectRotation(gameObject,
                          add_euler_delta_to_packed_quat(gameObject->m_passenger.compressedRotation,
                                                         ClientLua::GetNumber(L, 1), ClientLua::GetNumber(L, 2),
                                                         ClientLua::GetNumber(L, 3)));
    PushGameObjectRotation(L, gameObject);

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

LUA_FUNCTION(RotateGobByGUIDWorld, (lua_State * L))
{
    CGGameObject_C* gameObject = GameObjectByLuaGuid(L, 1);
    if (!gameObject)
        return 0;

    SetGameObjectRotation(gameObject,
                          add_world_euler_delta_to_packed_quat(gameObject->m_passenger.compressedRotation,
                                                               ClientLua::GetNumber(L, 2),
                                                               ClientLua::GetNumber(L, 3),
                                                               ClientLua::GetNumber(L, 4)));
    PushGameObjectRotation(L, gameObject);

    return 3;
}

LUA_FUNCTION(RotateGobByMouseAxis, (lua_State * L))
{
    CGGameObject_C* gameObject = GameObjectByMouse();
    if (!gameObject)
        return 0;

    SetGameObjectRotation(gameObject,
                          add_axis_delta_to_packed_quat(gameObject->m_passenger.compressedRotation,
                                                        ClientLua::GetNumber(L, 1), ClientLua::GetNumber(L, 2),
                                                        ClientLua::GetNumber(L, 3), ClientLua::GetNumber(L, 4)));
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

LUA_FUNCTION(SetGobPositionByGUID, (lua_State * L))
{
    CGGameObject_C* gameObject = GameObjectByLuaGuid(L, 1);
    if (!gameObject)
        return 0;

    SetGameObjectPosition(gameObject, ClientLua::GetNumber(L, 2), ClientLua::GetNumber(L, 3),
                          ClientLua::GetNumber(L, 4));

    PushGameObjectPosition(L, gameObject);
    return 3;
}

LUA_FUNCTION(SetGobPositionByMouse, (lua_State * L))
{
    CGGameObject_C* gameObject = GameObjectByMouse();
    if (!gameObject)
        return 0;

    SetGameObjectPosition(gameObject, ClientLua::GetNumber(L, 1), ClientLua::GetNumber(L, 2),
                          ClientLua::GetNumber(L, 3));

    PushGameObjectPosition(L, gameObject);
    return 3;
}

LUA_FUNCTION(MoveGobByGUID, (lua_State * L))
{
    CGGameObject_C* gameObject = GameObjectByLuaGuid(L, 1);
    if (!gameObject)
        return 0;

    LOG_DEBUG << gameObject->m_passenger.position.x << " " << gameObject->m_passenger.position.y << " "
              << gameObject->m_passenger.position.z;
    MoveGameObject(gameObject, ClientLua::GetNumber(L, 2), ClientLua::GetNumber(L, 3), ClientLua::GetNumber(L, 4));
    PushGameObjectPosition(L, gameObject);

    return 3;
}

LUA_FUNCTION(MoveGobByMouse, (lua_State * L))
{
    CGGameObject_C* gameObject = GameObjectByMouse();
    if (!gameObject)
        return 0;

    LOG_DEBUG << gameObject->m_passenger.position.x << " " << gameObject->m_passenger.position.y << " "
              << gameObject->m_passenger.position.z;
    MoveGameObject(gameObject, ClientLua::GetNumber(L, 1), ClientLua::GetNumber(L, 2), ClientLua::GetNumber(L, 3));
    PushGameObjectPosition(L, gameObject);

    return 3;
}
