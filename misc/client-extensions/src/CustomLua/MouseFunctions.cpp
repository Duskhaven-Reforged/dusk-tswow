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

struct Quat
{
    float w, x, y, z;
};

static inline float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static inline Quat normalize_quat(Quat q)
{
    float lenSq = q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z;

    if (lenSq <= 0.0f)
        return {1.0f, 0.0f, 0.0f, 0.0f};

    float invLen = 1.0f / sqrtf(lenSq);

    return {q.w * invLen, q.x * invLen, q.y * invLen, q.z * invLen};
}

static inline Quat multiply_quat(Quat a, Quat b)
{
    return {a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z, a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
            a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x, a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w};
}

unsigned long long pack_quat(Quat q)
{
    q = normalize_quat(q);

    // Your packing drops the sign of w, so force w positive.
    if (q.w < 0.0f)
    {
        q.w = -q.w;
        q.x = -q.x;
        q.y = -q.y;
        q.z = -q.z;
    }

    float x = clampf(q.x, -1.0f, 1.0f);
    float y = clampf(q.y, -1.0f, 1.0f);
    float z = clampf(q.z, -1.0f, 1.0f);

    int xi = (int)(x * 2097152.0f + (x >= 0.0f ? 0.5f : -0.5f));
    int yi = (int)(y * 1048576.0f + (y >= 0.0f ? 0.5f : -0.5f));
    int zi = (int)(z * 1048576.0f + (z >= 0.0f ? 0.5f : -0.5f));

    if (xi > 2097151)
        xi = 2097151;
    if (xi < -2097152)
        xi = -2097152;

    if (yi > 1048575)
        yi = 1048575;
    if (yi < -1048576)
        yi = -1048576;

    if (zi > 1048575)
        zi = 1048575;
    if (zi < -1048576)
        zi = -1048576;

    unsigned long long xu = (unsigned long long)(unsigned)xi & 0x3FFFFFULL;
    unsigned long long yu = (unsigned long long)(unsigned)yi & 0x1FFFFFULL;
    unsigned long long zu = (unsigned long long)(unsigned)zi & 0x1FFFFFULL;

    return (xu << 42) | (yu << 21) | zu;
}

Quat euler_to_quat(float roll, float pitch, float yaw)
{
    float hr = roll * 0.5f;
    float hp = pitch * 0.5f;
    float hy = yaw * 0.5f;

    float cr = cosf(hr), sr = sinf(hr);
    float cp = cosf(hp), sp = sinf(hp);
    float cy = cosf(hy), sy = sinf(hy);

    Quat q;

    q.w = cr * cp * cy + sr * sp * sy;
    q.x = sr * cp * cy - cr * sp * sy;
    q.y = cr * sp * cy + sr * cp * sy;
    q.z = cr * cp * sy - sr * sp * cy;

    return normalize_quat(q);
}

Quat unpack_quat(unsigned long long packed)
{
    int xi = (int)((packed >> 42) & 0x3FFFFFULL);
    int yi = (int)((packed >> 21) & 0x1FFFFFULL);
    int zi = (int)(packed & 0x1FFFFFULL);

    // Sign-extend packed signed fields.
    if (xi & 0x200000)
        xi |= ~0x3FFFFF;
    if (yi & 0x100000)
        yi |= ~0x1FFFFF;
    if (zi & 0x100000)
        zi |= ~0x1FFFFF;

    float x = (float)xi / 2097152.0f;
    float y = (float)yi / 1048576.0f;
    float z = (float)zi / 1048576.0f;

    float ww = 1.0f - x * x - y * y - z * z;
    float w  = ww > 0.0f ? sqrtf(ww) : 0.0f;

    return normalize_quat({w, x, y, z});
}

unsigned long long add_euler_delta_to_packed_quat(unsigned long long packed, float deltaRoll, float deltaPitch,
                                                  float deltaYaw)
{
    Quat current = unpack_quat(packed);
    Quat delta   = euler_to_quat(deltaRoll, deltaPitch, deltaYaw);

    // Local-space delta:
    Quat result = multiply_quat(current, delta);

    // For world-space delta, use this instead:
    // Quat result = multiply_quat(delta, current);

    return pack_quat(result);
}

LUA_FUNCTION(RotateGobByMouse, (lua_State * L))
{
    // TODO: use a GUID passed in
    CGObject_C* obj = static_cast<CGObject_C*>(ClntObjMgr::ObjectPtr(lastMouseGUID.full, TYPEMASK_OBJECT));
    if (obj && (obj->GetTypeID() == TYPEID_GAMEOBJECT))
    {
        TestGameObject* gameObject = reinterpret_cast<TestGameObject*>(obj);
        gameObject->packedQuaternion =
            add_euler_delta_to_packed_quat(gameObject->packedQuaternion, ClientLua::GetNumber(L, 1),
                                           ClientLua::GetNumber(L, 2), ClientLua::GetNumber(L, 3));
        obj->UpdateWorldObject(0);
    }
    return 0;
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