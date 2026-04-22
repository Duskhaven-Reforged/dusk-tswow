#pragma once

#include <ClientMacros.h>
#include <lua.hpp>
#include <string>

namespace ClientLua
{
    int AddFunction(char const* fnName, lua_CFunction fn, std::string const& file, size_t line);

    lua_State* State();
    // Registers lua that will fire on reload
    void RegisterLua(std::string const& lua, std::string const& filename = "", size_t line = 0);

    CLIENT_FUNCTION(GetTop, 0x0084DBD0, __cdecl, int, (lua_State * L))
    CLIENT_FUNCTION(SetTop, 0x0084DBF0, __cdecl, void, (lua_State * L, int idx))
    CLIENT_FUNCTION(Remove, 0x0084DC50, __cdecl, void, (lua_State * L, int idx))
    CLIENT_FUNCTION(Insert, 0x0084DCC0, __cdecl, void, (lua_State * L, int idx))
    CLIENT_FUNCTION(PushValue, 0x0084DE50, __cdecl, void, (lua_State * L, int idx))
    CLIENT_FUNCTION(Type, 0x0084DEB0, __cdecl, int, (lua_State * L, int idx))
    CLIENT_FUNCTION(IsNumber, 0x0084DF20, __cdecl, int, (lua_State * L, int idx))
    CLIENT_FUNCTION(IsString, 0x0084DF60, __cdecl, int, (lua_State * L, int idx))
    CLIENT_FUNCTION(Equal, 0x0084DFE0, __cdecl, int, (lua_State * L, int idx1, int idx2))
    CLIENT_FUNCTION(ToNumber, 0x0084E030, __cdecl, double, (lua_State * L, int idx))
    CLIENT_FUNCTION(ToInteger, 0x0084E070, __cdecl, int, (lua_State * L, int idx))
    CLIENT_FUNCTION(ToBoolean, 0x0084E0B0, __cdecl, int, (lua_State * L, int idx))
    CLIENT_FUNCTION(ToLString, 0x0084E0E0, __cdecl, char*, (lua_State * L, int idx, size_t* len))
    CLIENT_FUNCTION(ToUserdata, 0x0084E1C0, __cdecl, void*, (lua_State * L, int idx))
    CLIENT_FUNCTION(ToThread, 0x0084E1F0, __cdecl, lua_State*, (lua_State * L, int idx))
    CLIENT_FUNCTION(ToPointer, 0x0084E210, __cdecl, void*, (lua_State * L, int idx))
    CLIENT_FUNCTION(PushNil, 0x0084E280, __cdecl, void, (lua_State * L))
    CLIENT_FUNCTION(PushNumber, 0x0084E2A0, __cdecl, void, (lua_State * L, double n))
    CLIENT_FUNCTION(PushInteger, 0x0084E2D0, __cdecl, void, (lua_State * L, int n))
    CLIENT_FUNCTION(PushFString, 0x0084E300, __cdecl, const char*, (lua_State * L, const char* fmt, ...))
    CLIENT_FUNCTION(PushString, 0x0084E350, __cdecl, void, (lua_State * L, const char* s))
    CLIENT_FUNCTION(PushCClosure, 0x0084E400, __cdecl, void, (lua_State * L, lua_CFunction fn, int n))
    CLIENT_FUNCTION(PushBoolean, 0x0084E4D0, __cdecl, void, (lua_State * L, int b))
    CLIENT_FUNCTION(GetTable, 0x0084E560, __cdecl, void, (lua_State * L, int idx))
    CLIENT_FUNCTION(FindTable, 0x0084E590, __cdecl, const char*,
                    (lua_State * L, int idx, const char* fname, int szhint))
    CLIENT_FUNCTION(RawGet, 0x0084E600, __cdecl, void, (lua_State * L, int idx))
    CLIENT_FUNCTION(RawGetI, 0x0084E670, __cdecl, void, (lua_State * L, int idx, int n))
    CLIENT_FUNCTION(CreateTable, 0x0084E6E0, __cdecl, void, (lua_State * L, int narr, int nrec))
    CLIENT_FUNCTION(SetField, 0x0084E900, __cdecl, void, (lua_State * L, int idx, const char* k))
    CLIENT_FUNCTION(RawSet, 0x0084E970, __cdecl, void, (lua_State * L, int idx))
    CLIENT_FUNCTION(SetMetatable, 0x0084EA90, __cdecl, int, (lua_State * L, int objindex))
    CLIENT_FUNCTION(PCall, 0x0084EC50, __cdecl, int, (lua_State * L, int nargs, int nresults, int errfunc))
    CLIENT_FUNCTION(GC, 0x0084ED50, __cdecl, int, (lua_State * L, int what, int data))
    CLIENT_FUNCTION(GetField, 0x0084F3B0, __cdecl, void, (lua_State * L, int idx, const char* k))
    CLIENT_FUNCTION(LoadBuffer, 0x0084F860, __cdecl, int,
                    (lua_State * L, const char* buff, size_t sz, const char* name))
    CLIENT_FUNCTION(OpenLib, 0x0084FC00, __cdecl, void,
                    (lua_State * L, const char* libname, const luaL_Reg* l, int nup))
    CLIENT_FUNCTION(NewState, 0x00855370, __cdecl, lua_State*, (lua_Alloc f, void* ud))
    // CLIENT_FUNCTION(Next,             0x00854690, __cdecl, int,         (lua_State* L))
    CLIENT_FUNCTION(LuaNext, 0x0084EF50, __cdecl, int, (lua_State * L, int idx))

    CLIENT_FUNCTION(DisplayError, 0x84F280, __cdecl, void, (lua_State * L, char*, ...))
    CLIENT_FUNCTION(DoString, 0x00819210, __cdecl, void, (char const* code, lua_State* L))

    std::string GetString(lua_State* L, int32_t offset, std::string const& defValue = "");
    double GetNumber(lua_State* L, int32_t offset, double defValue = 0);
    void allowOutOfBoundsPointer();
    void customLua();

    static bool isInDevMode = true;
} // namespace ClientLua

// do NOT refactor this name
// without also changing the name in client_header_builder.cpp

#define LUA_FUNCTION(__lua_function_name, arg)                                                     \
    int __lua_function_name##Fn##arg;                                                              \
    int __lua_function_name##__Result =                                                            \
        ClientLua::AddFunction(#__lua_function_name, __lua_function_name##Fn, __FILE__, __LINE__); \
    int __lua_function_name##Fn##arg
