#include <CustomLua/CharCreate/CharCreateDefines.h>
#include <ClientDetours.h>
#include <ClientLua.h>
#include <Logger.h>
#include <SharedDefines.h>

LUA_FUNCTION(GetHairStyleSelection, (lua_State * L))
{
    auto component = reinterpret_cast<CharacterComponent*>(*(uint32_t*)0x00B6B1A0);
    ClientLua::PushNumber(L, component->charData.hairStyleId);
    return 1;
}

LUA_FUNCTION(GetHairColorSelection, (lua_State* L))
{
    auto component = reinterpret_cast<CharacterComponent*>(*(uint32_t*)0x00B6B1A0);
    ClientLua::PushNumber(L, component->charData.hairColorId);   
    return 1;
}

LUA_FUNCTION(GetFacialHairSelection, (lua_State * L))
{
    auto component = reinterpret_cast<CharacterComponent*>(*(uint32_t*)0x00B6B1A0);
    ClientLua::PushNumber(L, component->charData.facialHairId);
    return 1;
}

LUA_FUNCTION(GetSkinColorSelection, (lua_State * L))
{
    auto component = reinterpret_cast<CharacterComponent*>(*(uint32_t*)0x00B6B1A0);
    ClientLua::PushNumber(L, component->charData.skinId);
    return 1;
}

LUA_FUNCTION(GetFaceSelection, (lua_State * L))
{
    auto component = reinterpret_cast<CharacterComponent*>(*(uint32_t*)0x00B6B1A0);
    ClientLua::PushNumber(L, component->charData.faceId);
    return 1;
}
