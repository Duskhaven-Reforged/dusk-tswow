#include <ClientDetours.h>
#include <ClientLua.h>
#include <Editor/EditorObject.h>
#include <Editor/EditorRuntime.h>
#include <Logger.h>
#include <cstdint>
#include <fstream>
#include <string>
#include "CustomLua/Housing/QuatFunctions.h"

using namespace ClientData;

struct HitTestResult
{
    uint32_t guidLow;  // 0x00
    uint32_t guidHigh; // 0x04
    float x;           // 0x08
    float y;           // 0x0C
    float z;           // 0x10
    float dist;        // 0x14
    // float rayStartX;   // 0x18
    // float rayStartY;   // 0x1C
    // float rayStartZ;   // 0x20
    // float rayEndX;     // 0x24
    // float rayEndY;     // 0x28
    // float rayEndZ;     // 0x2C
};

static EditorObject::DecodedGuid lastMouseGUID;
static float lastMouseHitX;
static float lastMouseHitY;
static float lastMouseHitZ;

CLIENT_DETOUR_THISCALL(CGWorldFrame__HitTestPoint, 0x004F9DA0, int, (float a2, float a3, int a4, int a5))
{
    int result = CGWorldFrame__HitTestPoint(self, a2, a3, a4, a5);

    if (a5)
    {
        HitTestResult* lastMouseHit = reinterpret_cast<HitTestResult*>(a5);
        lastMouseGUID = result >= 2 ? EditorObject::DecodeClientGuid(lastMouseHit->guidLow, lastMouseHit->guidHigh)
                                    : EditorObject::DecodeClientGuid(0, 0);
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
    ClientLua::PushString(L, EditorObject::GuidToString(lastMouseGUID.full).c_str());
    ClientLua::PushNumber(L, lastMouseGUID.entry);
    ClientLua::PushNumber(L, lastMouseGUID.low);
    ClientLua::PushNumber(L, lastMouseGUID.type);
    return 4;
}

static CGGameObject_C* SelectedGameObject()
{
    return EditorRuntime::SelectedGameObject();
}

static CGGameObject_C* GameObjectByLuaGuid(lua_State* L, int index)
{
    return EditorObject::GameObjectByGuid(std::stoull(ClientLua::GetString(L, index)));
}

static CGGameObject_C* GameObjectByMouse()
{
    return EditorObject::GameObjectByGuid(lastMouseGUID.full);
}

static void PushGameObjectPosition(lua_State* L, CGGameObject_C* gameObject)
{
    ClientLua::PushNumber(L, gameObject->m_passenger.position.x);
    ClientLua::PushNumber(L, gameObject->m_passenger.position.y);
    ClientLua::PushNumber(L, gameObject->m_passenger.position.z);
}

LUA_FUNCTION(LogMouseoverGobValues, (lua_State*))
{
    CGGameObject_C* gameObject = GameObjectByMouse();
    if (!gameObject)
    {
        LOG_DEBUG << "MouseoverGobValues: no game object moused over";
        return 0;
    }

    uint32_t mapID = *reinterpret_cast<uint32_t*>(0xBD088C);
    Quat q         = unpack_quat(gameObject->m_passenger.compressedRotation);

    float roll, pitch, yaw;
    quat_to_euler(q, roll, pitch, yaw);


    std::ofstream file("gobpositions", std::ios::app);
    if (file.is_open())
    {
        file << lastMouseGUID.entry << "," << mapID << "," << gameObject->m_passenger.position.x << ","
             << gameObject->m_passenger.position.y << "," << gameObject->m_passenger.position.z << "," << yaw << ","
             << q.x << "," << q.y << "," << q.z << "," << q.w << "\n";
        file.close();
    }

    return 0;
}


LUA_FUNCTION(ClearSelectedGob, (lua_State * L))
{
    EditorRuntime::ClearSelection();
    return 0;
}

LUA_FUNCTION(SelectEditorGobByMouse, (lua_State * L))
{
    CGGameObject_C* gameObject = GameObjectByMouse();
    if (!gameObject || !EditorRuntime::SelectGameObject(lastMouseGUID.full))
        return 0;

    ClientLua::PushString(L, EditorObject::GuidToString(lastMouseGUID.full).c_str());
    PushGameObjectPosition(L, gameObject);
    return 4;
}

LUA_FUNCTION(GetSelectedGobGUID, (lua_State * L))
{
    ClientLua::PushString(L, EditorObject::GuidToString(EditorRuntime::CurrentSelectedGobGUID()).c_str());
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

LUA_FUNCTION(GetSelectedGobRotation, (lua_State * L))
{
    CGGameObject_C* gameObject = SelectedGameObject();
    if (!gameObject)
        return 0;

    Quat q = unpack_quat(gameObject->m_passenger.compressedRotation);

    float roll, pitch, yaw;
    quat_to_euler(q, roll, pitch, yaw);

    ClientLua::PushNumber(L, q.x);
    ClientLua::PushNumber(L, q.y);
    ClientLua::PushNumber(L, q.z);
    ClientLua::PushNumber(L, q.w);
    return 4;
}


LUA_FUNCTION(SetGobPositionByGUID, (lua_State * L))
{
    CGGameObject_C* gameObject = GameObjectByLuaGuid(L, 1);
    if (!gameObject)
    {
        ClientLua::PushBoolean(L, false);
        return 1;
    }

    gameObject->m_passenger.position.x = (float)ClientLua::GetNumber(L, 2);
    gameObject->m_passenger.position.y = (float)ClientLua::GetNumber(L, 3);
    gameObject->m_passenger.position.z = (float)ClientLua::GetNumber(L, 4);
    gameObject->UpdateWorldObject(0);

    ClientLua::PushBoolean(L, true);
    return 1;
}

LUA_FUNCTION(SetGobRotationByGUID, (lua_State * L))
{
    CGGameObject_C* gameObject = GameObjectByLuaGuid(L, 1);
    if (!gameObject)
    {
        ClientLua::PushBoolean(L, false);
        return 1;
    }

    float x = (float)ClientLua::GetNumber(L, 2);
    float y = (float)ClientLua::GetNumber(L, 3);
    float z = (float)ClientLua::GetNumber(L, 4);
    float w = (float)ClientLua::GetNumber(L, 5);

    gameObject->m_passenger.compressedRotation = pack_quat({w, x, y, z});
    gameObject->UpdateWorldObject(0);

    ClientLua::PushBoolean(L, true);
    return 1;
}
