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

float lastMouseHitX;
float lastMouseHitY;
float lastMouseHitZ;

CLIENT_DETOUR_THISCALL(CGWorldFrame__HitTestPoint, 0x004F9DA0, int, (float a2, float a3, int a4, int a5))
{
    int result = CGWorldFrame__HitTestPoint(self, a2, a3, a4, a5);

    if (a5)
    {
        HitTestResult* lastMouseHit = reinterpret_cast<HitTestResult*>(a5);
        lastMouseHitX               = lastMouseHit->x;
        lastMouseHitY               = lastMouseHit->y;
        lastMouseHitZ               = lastMouseHit->z;
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