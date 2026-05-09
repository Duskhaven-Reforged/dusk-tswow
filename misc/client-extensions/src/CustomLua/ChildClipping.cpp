#include <ClientDetours.h>
#include <ClientLua.h>
#include <Logger.h>
#include <lua.hpp>

#include <array>
#include <cstdint>
#include <cstring>
#include <unordered_set>

namespace
{
    constexpr uint32_t kFrameScriptObjectTypeCache = 0x00B49984;
    constexpr uint32_t kFrameScriptObjectTypeSeed = 0x00D3F778;

    constexpr size_t kParentOffset = 37 * sizeof(uint32_t);
    constexpr size_t kLayoutOffset = 32;
    constexpr size_t kChildListOffset = 160 * sizeof(uint32_t);

    constexpr size_t kFakeFlagsOffset = 24 * sizeof(uint32_t);
    constexpr size_t kFakeLeftOffset = 57 * sizeof(uint32_t);
    constexpr size_t kFakeTopOffset = 58 * sizeof(uint32_t);
    constexpr size_t kFakeRightOffset = 59 * sizeof(uint32_t);
    constexpr size_t kFakeBottomOffset = 60 * sizeof(uint32_t);
    constexpr size_t kFakeScrollChildOffset = 672;

    constexpr uint32_t kHasHitRectFlag = 0x100;

    struct FakeScrollFrame
    {
        std::array<uint8_t, kFakeScrollChildOffset + sizeof(uint32_t)> bytes = {};
    };

    using TypeCheckFn = uint8_t(__thiscall*)(void*, int);
    using FrameRenderFn = void(__thiscall*)(void*);

    std::unordered_set<void*>& ClippedParents()
    {
        static std::unordered_set<void*> parents;
        return parents;
    }

    CLIENT_FUNCTION(CScriptObject__GetScriptObjectByNameEx, 0x0048B5F0, __cdecl, int, (char* name, int objectType))
    CLIENT_FUNCTION(GetFrameObjectTypeEx, 0x0049CAB0, __cdecl, int, ())
    CLIENT_FUNCTION(LuaTypeEx, 0x0084DEB0, __cdecl, int, (lua_State* L, int index))
    CLIENT_FUNCTION(LuaRawGetIEx, 0x0084E670, __cdecl, void, (lua_State* L, int index, int n))
    CLIENT_FUNCTION(LuaToUserDataEx, 0x0084E1C0, __cdecl, void*, (lua_State* L, int index))
    CLIENT_FUNCTION(LuaSetTopEx, 0x0084DBF0, __cdecl, void, (lua_State* L, int index))
    CLIENT_FUNCTION(CLayoutFrame__GetRectEx, 0x00489230, __thiscall, int, (void* layoutFrame, float* rect))
    CLIENT_FUNCTION(CSimpleFrame__SetBeingScrolledEx, 0x00490F60, __thiscall, int, (void* frame, int isScrolled, int inherited))
    CLIENT_FUNCTION(CRenderBatch__QueueCallbackEx, 0x004858E0, __thiscall, void, (void* renderBatch, void(__cdecl* cb)(void*), void* context))
    CLIENT_FUNCTION(CSimpleScrollFrame__RenderScrollChildEx, 0x0096B610, __cdecl, void, (void* frameLike))

    int GetFrameObjectType()
    {
        uint32_t& cached = *reinterpret_cast<uint32_t*>(kFrameScriptObjectTypeCache);
        if (!cached)
            cached = ++(*reinterpret_cast<uint32_t*>(kFrameScriptObjectTypeSeed));
        return cached ? static_cast<int>(cached) : GetFrameObjectTypeEx();
    }

    void* ResolveFrameObject(lua_State* L, int index)
    {
        if (ClientLua::IsString(L, index)) {
            char* name = ClientLua::ToLString(L, index, nullptr);
            if (!name)
                return nullptr;
            return reinterpret_cast<void*>(CScriptObject__GetScriptObjectByNameEx(name, GetFrameObjectType()));
        }

        if (LuaTypeEx(L, index) != LUA_TTABLE)
            return nullptr;

        LuaRawGetIEx(L, index, 0);
        void* object = LuaToUserDataEx(L, -1);
        LuaSetTopEx(L, -2);
        if (!object)
            return nullptr;

        auto typeCheck = reinterpret_cast<TypeCheckFn>(*reinterpret_cast<uint32_t*>(*reinterpret_cast<uint32_t*>(object) + 0x10));
        return (typeCheck && typeCheck(object, GetFrameObjectType())) ? object : nullptr;
    }

    template <typename T>
    T ReadField(void* base, size_t offset)
    {
        T value {};
        std::memcpy(&value, reinterpret_cast<uint8_t*>(base) + offset, sizeof(T));
        return value;
    }

    template <typename T>
    void WriteField(FakeScrollFrame& fake, size_t offset, T value)
    {
        std::memcpy(fake.bytes.data() + offset, &value, sizeof(T));
    }

    uintptr_t FirstChildNode(void* frame)
    {
        return ReadField<uint32_t>(frame, kChildListOffset);
    }

    uintptr_t NextNode(uintptr_t node)
    {
        return *reinterpret_cast<uint32_t*>(node + 4);
    }

    void* NodeFrame(uintptr_t node)
    {
        return *reinterpret_cast<void**>(node + 8);
    }

    void UpdateDirectChildScrollState(void* parent, bool enabled)
    {
        for (uintptr_t node = FirstChildNode(parent); node && (node & 1) == 0; node = NextNode(node)) {
            if (void* child = NodeFrame(node))
                CSimpleFrame__SetBeingScrolledEx(child, enabled ? 1 : 0, enabled ? 1 : 0);
        }
    }

    void SetChildClippingEnabled(void* frame, bool enabled)
    {
        auto& clippedParents = ClippedParents();
        bool const alreadyEnabled = clippedParents.count(frame) != 0;
        if (alreadyEnabled == enabled)
            return;

        if (enabled) {
            clippedParents.insert(frame);
            UpdateDirectChildScrollState(frame, true);
        } else {
            UpdateDirectChildScrollState(frame, false);
            clippedParents.erase(frame);
        }

        LOG_DEBUG << "Child clipping " << (enabled ? "enabled" : "disabled") << " for frame @" << frame;
    }

    bool BuildFakeScrollFrame(void* parent, void* child, FakeScrollFrame& fake)
    {
        float rect[4] = {};
        if (!CLayoutFrame__GetRectEx(reinterpret_cast<uint8_t*>(parent) + kLayoutOffset, rect))
            return false;

        WriteField<uint32_t>(fake, kFakeFlagsOffset, kHasHitRectFlag);
        WriteField<float>(fake, kFakeLeftOffset, rect[0]);
        WriteField<float>(fake, kFakeTopOffset, rect[1]);
        WriteField<float>(fake, kFakeRightOffset, rect[2]);
        WriteField<float>(fake, kFakeBottomOffset, rect[3]);
        WriteField<uint32_t>(fake, kFakeScrollChildOffset, reinterpret_cast<uint32_t>(child));
        return true;
    }

    void __cdecl RenderClippedChildren(void* parent)
    {
        if (!parent || ClippedParents().count(parent) == 0)
            return;

        for (uintptr_t node = FirstChildNode(parent); node && (node & 1) == 0; node = NextNode(node)) {
            void* child = NodeFrame(node);
            if (!child)
                continue;

            FakeScrollFrame fake;
            if (!BuildFakeScrollFrame(parent, child, fake))
                continue;

            CSimpleScrollFrame__RenderScrollChildEx(fake.bytes.data());
        }
    }
}

LUA_FUNCTION(SetFrameChildClipping, (lua_State* L))
{
    void* frame = ResolveFrameObject(L, 1);
    if (!frame) {
        ClientLua::DisplayError(L, "Usage: SetFrameChildClipping(frame, enabled)");
        return 0;
    }

    bool enabled = ClientLua::GetNumber(L, 2, 1) != 0.0;
    SetChildClippingEnabled(frame, enabled);
    return 0;
}

LUA_FUNCTION(GetFrameChildClipping, (lua_State* L))
{
    void* frame = ResolveFrameObject(L, 1);
    ClientLua::PushBoolean(L, frame && ClippedParents().count(frame) != 0);
    return 1;
}

CLIENT_DETOUR_THISCALL(CSimpleFrame__OnFrameRender_ClipChildren, 0x00490840, char*, (int renderBatch, int layer))
{
    char* result = CSimpleFrame__OnFrameRender_ClipChildren(self, renderBatch, layer);
    auto& clippedParents = ClippedParents();
    if (layer == 4 && !clippedParents.empty() && clippedParents.count(self) != 0)
        CRenderBatch__QueueCallbackEx(reinterpret_cast<void*>(renderBatch), RenderClippedChildren, self);
    return result;
}

CLIENT_DETOUR_THISCALL(CSimpleFrame__SetParent_ClipChildren, 0x004911B0, int, (void* newParent))
{
    void* oldParent = ReadField<void*>(self, kParentOffset);
    int result = CSimpleFrame__SetParent_ClipChildren(self, newParent);

    auto& clippedParents = ClippedParents();
    bool const oldClipped = oldParent && !clippedParents.empty() && clippedParents.count(oldParent) != 0;
    bool const newClipped = newParent && !clippedParents.empty() && clippedParents.count(newParent) != 0;

    if (oldClipped != newClipped)
        CSimpleFrame__SetBeingScrolledEx(self, newClipped ? 1 : 0, newClipped ? 1 : 0);

    return result;
}
