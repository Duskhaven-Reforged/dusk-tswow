#include <ClientDetours.h>
#include <ClientData/SharedDefines.h>
#include <ClientData/VectorMath.h>
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

static void PushGameObjectRotation(lua_State* L, CGGameObject_C* gameObject)
{
    Quat q = unpack_quat(gameObject->m_passenger.compressedRotation);
    ClientLua::PushNumber(L, q.x);
    ClientLua::PushNumber(L, q.y);
    ClientLua::PushNumber(L, q.z);
    ClientLua::PushNumber(L, q.w);
}

static bool TraceGround(C3Vector const& position, C3Vector& out)
{
    C3Vector start{position.x, position.y, position.z + 5.0f};
    C3Vector end{position.x, position.y, position.z - 100.0f};
    C3Vector hit{};
    float distance = 1.0f;

    if (!TraceLine(&start, &end, &hit, &distance, 0x10111, 0))
        return false;

    out = hit;
    return true;
}

static Quat QuatFromBasis(C3Vector const& xAxis, C3Vector const& yAxis, C3Vector const& zAxis)
{
    float const m00 = xAxis.x;
    float const m01 = yAxis.x;
    float const m02 = zAxis.x;
    float const m10 = xAxis.y;
    float const m11 = yAxis.y;
    float const m12 = zAxis.y;
    float const m20 = xAxis.z;
    float const m21 = yAxis.z;
    float const m22 = zAxis.z;

    float const trace = m00 + m11 + m22;
    if (trace > 0.0f)
    {
        float const s = sqrtf(trace + 1.0f) * 2.0f;
        return normalize_quat({0.25f * s, (m21 - m12) / s, (m02 - m20) / s, (m10 - m01) / s});
    }

    if (m00 > m11 && m00 > m22)
    {
        float const s = sqrtf(1.0f + m00 - m11 - m22) * 2.0f;
        return normalize_quat({(m21 - m12) / s, 0.25f * s, (m01 + m10) / s, (m02 + m20) / s});
    }

    if (m11 > m22)
    {
        float const s = sqrtf(1.0f + m11 - m00 - m22) * 2.0f;
        return normalize_quat({(m02 - m20) / s, (m01 + m10) / s, 0.25f * s, (m12 + m21) / s});
    }

    float const s = sqrtf(1.0f + m22 - m00 - m11) * 2.0f;
    return normalize_quat({(m10 - m01) / s, (m02 + m20) / s, (m12 + m21) / s, 0.25f * s});
}

static bool GroundNormal(C3Vector const& groundPosition, float sampleRadius, C3Vector& out)
{
    if (sampleRadius <= 0.0f)
        return false;

    C3Vector sampleX{};
    C3Vector sampleY{};
    if (!TraceGround({groundPosition.x + sampleRadius, groundPosition.y, groundPosition.z}, sampleX)
        || !TraceGround({groundPosition.x, groundPosition.y + sampleRadius, groundPosition.z}, sampleY))
        return false;

    C3Vector tangentX = VectorMath::Subtract(sampleX, groundPosition);
    C3Vector tangentY = VectorMath::Subtract(sampleY, groundPosition);
    out = VectorMath::Normalize(VectorMath::Cross(tangentX, tangentY));
    if (out.z < 0.0f)
        out = VectorMath::Scale(out, -1.0f);

    return VectorMath::Length(out) > 0.0f;
}

static bool FindGroundAlignedRotation(CGGameObject_C* gameObject, C3Vector const& groundPosition, float sampleRadius,
                                      Quat& out)
{
    C3Vector normal{};
    if (!GroundNormal(groundPosition, sampleRadius, normal))
        return false;

    float roll, pitch, yaw;
    quat_to_euler(unpack_quat(gameObject->m_passenger.compressedRotation), roll, pitch, yaw);

    C3Vector yawForward = {cosf(yaw), sinf(yaw), 0.0f};
    C3Vector xAxis = VectorMath::Normalize(VectorMath::ProjectionOnPlane(yawForward, normal));
    if (VectorMath::Length(xAxis) <= 0.0f)
        return false;

    C3Vector yAxis = VectorMath::Normalize(VectorMath::Cross(normal, xAxis));
    C3Vector zAxis = VectorMath::Normalize(VectorMath::Cross(xAxis, yAxis));

    out = QuatFromBasis(xAxis, yAxis, zAxis);
    return true;
}

static void MoveGameObject(CGGameObject_C* gameObject, C3Vector const& position)
{
    gameObject->m_passenger.position = position;
    gameObject->UpdateWorldObject(0);
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

LUA_FUNCTION(SnapSelectedGobToGround, (lua_State * L))
{
    CGGameObject_C* gameObject = SelectedGameObject();
    if (!gameObject)
    {
        ClientLua::PushBoolean(L, false);
        return 1;
    }

    float const zOffset = (float)ClientLua::GetNumber(L, 1, 0.0);
    float const sampleRadius = (float)ClientLua::GetNumber(L, 2, 1.0);
    C3Vector rawGroundPosition{};
    if (!TraceGround(gameObject->m_passenger.position, rawGroundPosition))
    {
        ClientLua::PushBoolean(L, false);
        return 1;
    }

    Quat groundAlignedRotation{};
    if (FindGroundAlignedRotation(gameObject, rawGroundPosition, sampleRadius, groundAlignedRotation))
        gameObject->m_passenger.compressedRotation = pack_quat(groundAlignedRotation);

    C3Vector groundPosition = {rawGroundPosition.x, rawGroundPosition.y, rawGroundPosition.z + zOffset};
    MoveGameObject(gameObject, groundPosition);

    ClientLua::PushBoolean(L, true);
    PushGameObjectPosition(L, gameObject);
    PushGameObjectRotation(L, gameObject);
    return 8;
}

LUA_FUNCTION(GetSelectedGobRotation, (lua_State * L))
{
    CGGameObject_C* gameObject = SelectedGameObject();
    if (!gameObject)
        return 0;

    PushGameObjectRotation(L, gameObject);
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

    MoveGameObject(gameObject,
                   {(float)ClientLua::GetNumber(L, 2), (float)ClientLua::GetNumber(L, 3),
                    (float)ClientLua::GetNumber(L, 4)});

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
