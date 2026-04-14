#include <ClientDetours.h>
#include <ClientLua.h>
#include <Logger.h>
#include <SharedDefines.h>

#include <array>
#include <cctype>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
    constexpr uint32_t kTextureObjectTypeCache = 0x00B4793C;
    constexpr uint32_t kFrameScriptObjectType = 0x00D3F778;
    constexpr uint32_t kPixelArgb8888 = 2;

    struct OpaqueBLPFile
    {
        std::array<uint8_t, 0x4B4> storage;
    };

    struct OpaqueTgaFile
    {
        std::array<uint8_t, 0x44> storage;
    };

    struct CImVector
    {
        uint8_t b;
        uint8_t g;
        uint8_t r;
        uint8_t a;
    };

    struct TSGrowableArray_CImVector
    {
        uint32_t m_alloc;
        uint32_t m_count;
        CImVector* m_data;
        uint32_t m_chunk;
    };

    struct MaskedTextureKey
    {
        std::string sourceBase;
        std::string maskBase;
        uint32_t width;
        uint32_t height;

        bool operator==(MaskedTextureKey const& other) const
        {
            return width == other.width
                && height == other.height
                && sourceBase == other.sourceBase
                && maskBase == other.maskBase;
        }
    };

    struct MaskedTextureKeyHash
    {
        size_t operator()(MaskedTextureKey const& key) const
        {
            size_t h = std::hash<std::string>{}(key.sourceBase);
            h ^= std::hash<std::string>{}(key.maskBase) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<uint32_t>{}(key.width) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<uint32_t>{}(key.height) + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };

    struct MaskedTextureCacheEntry
    {
        TSGrowableArray_CImVector image;
        int handle;
        std::string debugName;
    };

    static std::unordered_map<MaskedTextureKey, std::unique_ptr<MaskedTextureCacheEntry>, MaskedTextureKeyHash> s_maskedTextureCache;
    static std::unordered_map<uintptr_t, std::string> s_textureMaskByObject;

    CLIENT_FUNCTION(CScriptObject__GetScriptObjectByNameEx, 0x0048B5F0, __cdecl, int, (char* name, int objectType))
    CLIENT_FUNCTION(LuaTypeEx, 0x0084DEB0, __cdecl, int, (lua_State* L, int index))
    CLIENT_FUNCTION(LuaRawGetIEx, 0x0084E670, __cdecl, void, (lua_State* L, int index, int n))
    CLIENT_FUNCTION(LuaToUserDataEx, 0x0084E1C0, __cdecl, void*, (lua_State* L, int index))
    CLIENT_FUNCTION(LuaSetTopEx, 0x0084DBF0, __cdecl, void, (lua_State* L, int index))
    CLIENT_FUNCTION(LuaLErrorEx, 0x0084F280, __cdecl, int, (lua_State* L, char* fmt, ...))
    CLIENT_FUNCTION(Sub481520_SetTextureHandle, 0x00481520, __thiscall, int, (void* textureObject, int textureHandle))
    CLIENT_FUNCTION(Sub4B58D0_CBLPFileCtor, 0x004B58D0, __thiscall, void, (OpaqueBLPFile* file))
    CLIENT_FUNCTION(CBLPFile__OpenEx, 0x006AFF10, __thiscall, int, (OpaqueBLPFile* file, const char* filename, int))
    CLIENT_FUNCTION(Sub6AF990_LoadBLP, 0x006AF990, __thiscall, int, (OpaqueBLPFile* file, int pixelFormat, int mip, int* outPixels, uint32_t* outInfo))
    CLIENT_FUNCTION(Sub6AF6E0_FreeBLPDecode, 0x006AF6E0, __thiscall, void, (OpaqueBLPFile* file, int))
    CLIENT_FUNCTION(CBLPFile__CloseEx, 0x006AE8B0, __thiscall, void, (OpaqueBLPFile* file))
    CLIENT_FUNCTION(CTgaFile__OpenEx, 0x006AAFB0, __thiscall, int, (OpaqueTgaFile* file, const char* filename, int))
    CLIENT_FUNCTION(CTgaFile__LoadImageDataEx, 0x006AB4B0, __thiscall, int, (OpaqueTgaFile* file, int flags))
    CLIENT_FUNCTION(Sub6AA820_TgaImageBytes, 0x006AA820, __thiscall, uint8_t*, (OpaqueTgaFile* file))
    CLIENT_FUNCTION(CTgaFile__CloseEx, 0x006AAF40, __thiscall, void, (OpaqueTgaFile* file))
    CLIENT_FUNCTION(CGxTexFlags__constructorEx, 0x00681BE0, __thiscall, uint32_t*, (uint32_t* flags, int, int, int, int, int, int, unsigned int, int, int, int))
    CLIENT_FUNCTION(TSGrowableArray_CImVector__SetCountEx, 0x00616CA0, __thiscall, void, (TSGrowableArray_CImVector* vec, uint32_t count))
    CLIENT_FUNCTION(TextureCreate_0_Ex, 0x004B9200, __cdecl, int, (uint32_t width, uint32_t height, int, int, uint32_t flags, TSGrowableArray_CImVector* vec, int callback, const char* name, int))

    static std::string NormalizeTexturePath(std::string path)
    {
        for (char& ch : path) {
            if (ch == '/')
                ch = '\\';
        }
        return path;
    }

    static bool EndsWithInsensitive(std::string const& value, std::string const& suffix)
    {
        if (value.size() < suffix.size())
            return false;

        size_t offset = value.size() - suffix.size();
        for (size_t i = 0; i < suffix.size(); ++i) {
            char lhs = static_cast<char>(tolower(static_cast<unsigned char>(value[offset + i])));
            char rhs = static_cast<char>(tolower(static_cast<unsigned char>(suffix[i])));
            if (lhs != rhs)
                return false;
        }
        return true;
    }

    static std::string AppendExtensionIfMissing(std::string const& base, char const* extension)
    {
        if (EndsWithInsensitive(base, ".blp") || EndsWithInsensitive(base, ".tga"))
            return base;
        return base + extension;
    }

    static int GetTextureObjectType()
    {
        uint32_t& cached = *reinterpret_cast<uint32_t*>(kTextureObjectTypeCache);
        if (!cached)
            cached = ++(*reinterpret_cast<uint32_t*>(kFrameScriptObjectType));
        return static_cast<int>(cached);
    }

    static void* ResolveTextureObject(lua_State* L, int index)
    {
        if (ClientLua::IsString(L, index)) {
            char* name = ClientLua::ToLString(L, index, nullptr);
            if (!name)
                return nullptr;
            return reinterpret_cast<void*>(CScriptObject__GetScriptObjectByNameEx(name, GetTextureObjectType()));
        }

        if (LuaTypeEx(L, index) != LUA_TTABLE)
            return nullptr;

        LuaRawGetIEx(L, index, 0);
        void* object = LuaToUserDataEx(L, -1);
        LuaSetTopEx(L, -2);
        if (!object)
            return nullptr;

        using TypeCheckFn = uint8_t(__thiscall*)(void*, int);
        auto typeCheck = reinterpret_cast<TypeCheckFn>(*reinterpret_cast<uint32_t*>(*reinterpret_cast<uint32_t*>(object) + 0x10));
        if (!typeCheck || !typeCheck(object, GetTextureObjectType()))
            return nullptr;

        return object;
    }

    static bool LoadBLPToRGBA(std::string const& path, std::vector<CImVector>& pixels, uint32_t& width, uint32_t& height)
    {
        OpaqueBLPFile blp = {};
        Sub4B58D0_CBLPFileCtor(&blp);

        std::string filename = AppendExtensionIfMissing(NormalizeTexturePath(path), ".blp");
        if (!CBLPFile__OpenEx(&blp, filename.c_str(), 1)) {
            CBLPFile__CloseEx(&blp);
            return false;
        }

        int pixelBytes = 0;
        uint32_t info[4] = {};
        bool loaded = Sub6AF990_LoadBLP(&blp, kPixelArgb8888, 0, &pixelBytes, info) != 0;
        if (!loaded || pixelBytes == 0) {
            Sub6AF6E0_FreeBLPDecode(&blp, 0);
            CBLPFile__CloseEx(&blp);
            return false;
        }

        width = *reinterpret_cast<uint32_t*>(blp.storage.data() + 0x10);
        height = *reinterpret_cast<uint32_t*>(blp.storage.data() + 0x14);
        uint32_t pixelCount = width * height;

        uint8_t* srcPixels = reinterpret_cast<uint8_t*>(pixelBytes);
        pixels.resize(pixelCount);
        memcpy(pixels.data(), srcPixels, pixelCount * sizeof(CImVector));

        Sub6AF6E0_FreeBLPDecode(&blp, 0);
        CBLPFile__CloseEx(&blp);
        return true;
    }

    static bool LoadMaskAlphaFromTga(std::string const& path, uint32_t pixelCount, std::vector<uint8_t>& alphaOut)
    {
        OpaqueTgaFile tga = {};
        std::string filename = AppendExtensionIfMissing(NormalizeTexturePath(path), ".tga");
        if (!CTgaFile__OpenEx(&tga, filename.c_str(), 1))
            return false;

        bool ok = false;
        if (CTgaFile__LoadImageDataEx(&tga, 0)) {
            uint8_t* src = Sub6AA820_TgaImageBytes(&tga);
            if (src) {
                alphaOut.resize(pixelCount);
                memcpy(alphaOut.data(), src, pixelCount);
                ok = true;
            }
        }

        CTgaFile__CloseEx(&tga);
        return ok;
    }

    static uint8_t SelectMaskByte(uint8_t const* px, bool preferAlpha)
    {
        if (preferAlpha)
            return px[3];

        uint8_t b = px[0];
        uint8_t g = px[1];
        uint8_t r = px[2];
        return static_cast<uint8_t>((static_cast<uint32_t>(r) + static_cast<uint32_t>(g) + static_cast<uint32_t>(b)) / 3u);
    }

    static bool LoadMaskAlphaFromBlp(std::string const& path, uint32_t targetWidth, uint32_t targetHeight, std::vector<uint8_t>& alphaOut)
    {
        OpaqueBLPFile blp = {};
        Sub4B58D0_CBLPFileCtor(&blp);
        uint32_t pixelCount = targetWidth * targetHeight;

        std::string filename = AppendExtensionIfMissing(NormalizeTexturePath(path), ".blp");
        if (!CBLPFile__OpenEx(&blp, filename.c_str(), 1)) {
            CBLPFile__CloseEx(&blp);
            return false;
        }

        int pixelBytes = 0;
        uint32_t info[4] = {};
        bool ok = false;
        if (Sub6AF990_LoadBLP(&blp, kPixelArgb8888, 0, &pixelBytes, info) != 0 && pixelBytes != 0) {
            uint32_t width = *reinterpret_cast<uint32_t*>(blp.storage.data() + 0x10);
            uint32_t height = *reinterpret_cast<uint32_t*>(blp.storage.data() + 0x14);
            if (width != 0 && height != 0) {
                uint8_t* src = reinterpret_cast<uint8_t*>(pixelBytes);
                uint32_t srcCount = width * height;
                uint8_t alphaMin = 255;
                uint8_t alphaMax = 0;
                uint8_t rgbMin = 255;
                uint8_t rgbMax = 0;
                for (uint32_t i = 0; i < srcCount; ++i) {
                    uint8_t const* px = src + i * 4;
                    uint8_t a = px[3];
                    uint8_t rgb = static_cast<uint8_t>((static_cast<uint32_t>(px[0]) + static_cast<uint32_t>(px[1]) + static_cast<uint32_t>(px[2])) / 3u);
                    if (a < alphaMin) alphaMin = a;
                    if (a > alphaMax) alphaMax = a;
                    if (rgb < rgbMin) rgbMin = rgb;
                    if (rgb > rgbMax) rgbMax = rgb;
                }
                bool preferAlpha = (alphaMax > alphaMin) && (alphaMin != 255 || alphaMax != 255);
                alphaOut.resize(pixelCount);
                if (width * height == pixelCount) {
                    for (uint32_t i = 0; i < pixelCount; ++i)
                        alphaOut[i] = SelectMaskByte(src + i * 4, preferAlpha);
                    ok = true;
                } else {
                    if (targetWidth != 0 && targetHeight != 0) {
                        for (uint32_t y = 0; y < targetHeight; ++y) {
                            uint32_t srcY = (static_cast<uint64_t>(y) * height) / targetHeight;
                            for (uint32_t x = 0; x < targetWidth; ++x) {
                                uint32_t srcX = (static_cast<uint64_t>(x) * width) / targetWidth;
                                alphaOut[y * targetWidth + x] = SelectMaskByte(src + ((srcY * width + srcX) * 4), preferAlpha);
                            }
                        }
                        ok = true;
                    }
                }
                LOG_DEBUG << "SetTextureWithMask mask=" << filename
                    << " src=" << width << "x" << height
                    << " alphaRange=" << uint32_t(alphaMin) << "-" << uint32_t(alphaMax)
                    << " rgbRange=" << uint32_t(rgbMin) << "-" << uint32_t(rgbMax)
                    << " preferAlpha=" << (preferAlpha ? 1 : 0);
            }
        }

        Sub6AF6E0_FreeBLPDecode(&blp, 0);
        CBLPFile__CloseEx(&blp);
        return ok;
    }

    static bool LoadMaskAlpha(std::string const& maskBase, uint32_t width, uint32_t height, std::vector<uint8_t>& alphaOut)
    {
        uint32_t pixelCount = width * height;
        bool useSmall = width <= 64 && height <= 64;

        std::array<std::string, 4> candidates = {
            useSmall ? maskBase + "Small" : maskBase,
            useSmall ? maskBase : maskBase + "Small",
            maskBase,
            maskBase + "Small"
        };

        for (std::string const& candidate : candidates) {
            if (LoadMaskAlphaFromTga(candidate, pixelCount, alphaOut))
                return true;
            if (LoadMaskAlphaFromBlp(candidate, width, height, alphaOut))
                return true;
        }

        return false;
    }

    static std::unique_ptr<MaskedTextureCacheEntry> CreateMaskedTextureEntry(std::string const& sourceBase, std::string const& maskBase)
    {
        std::vector<CImVector> pixels;
        uint32_t width = 0;
        uint32_t height = 0;
        if (!LoadBLPToRGBA(sourceBase, pixels, width, height))
            return nullptr;

        if (width == 0 || height == 0 || pixels.empty())
            return nullptr;

        std::vector<uint8_t> alpha;
        if (!LoadMaskAlpha(maskBase, width, height, alpha))
            return nullptr;

        if (alpha.size() != pixels.size())
            return nullptr;

        for (size_t i = 0; i < pixels.size(); ++i)
            pixels[i].a = alpha[i];

        auto entry = std::make_unique<MaskedTextureCacheEntry>();
        memset(&entry->image, 0, sizeof(entry->image));
        entry->handle = 0;
        entry->debugName = "MaskedTexture:" + NormalizeTexturePath(sourceBase) + "|" + NormalizeTexturePath(maskBase);
        TSGrowableArray_CImVector__SetCountEx(&entry->image, static_cast<uint32_t>(pixels.size()));
        if (!entry->image.m_data || entry->image.m_count != pixels.size())
            return nullptr;

        memcpy(entry->image.m_data, pixels.data(), pixels.size() * sizeof(CImVector));

        uint32_t flags = 0;
        uint32_t* builtFlags = CGxTexFlags__constructorEx(&flags, 1, 0, 0, 0, 0, 0, 1u, 0, 0, 0);
        if (!builtFlags)
            return nullptr;

        entry->handle = TextureCreate_0_Ex(width, height, 2, 2, *builtFlags, &entry->image, 0x00616B90, entry->debugName.c_str(), 0);
        if (!entry->handle)
            return nullptr;

        return entry;
    }

    static int GetOrCreateMaskedTextureHandle(std::string const& sourceBase, std::string const& maskBase)
    {
        std::vector<CImVector> sourcePixels;
        uint32_t width = 0;
        uint32_t height = 0;
        if (!LoadBLPToRGBA(sourceBase, sourcePixels, width, height))
            return 0;

        MaskedTextureKey key{ NormalizeTexturePath(sourceBase), NormalizeTexturePath(maskBase), width, height };
        auto it = s_maskedTextureCache.find(key);
        if (it != s_maskedTextureCache.end())
            return it->second->handle;

        auto entry = CreateMaskedTextureEntry(sourceBase, maskBase);
        if (!entry)
            return 0;

        int handle = entry->handle;
        s_maskedTextureCache.emplace(std::move(key), std::move(entry));
        return handle;
    }

    static bool ApplyMaskedTextureToObject(void* textureObject, std::string const& sourceBase, std::string const& maskBase)
    {
        int handle = GetOrCreateMaskedTextureHandle(sourceBase, maskBase);
        if (!handle)
            return false;

        Sub481520_SetTextureHandle(textureObject, handle);
        return true;
    }
}

LUA_FUNCTION(FireActionBarSlotUpdateEvent, (lua_State* L)) {
    uint8_t slotID = ClientLua::GetNumber(L, 1);

    if (slotID < 144)
        FrameScript::SignalEvent(EVENT_ACTIONBAR_SLOT_CHANGED, "%d", slotID + 1);

    return 0;
}

LUA_FUNCTION(FireTalentUpdateEvent, (lua_State* L)) {
    FrameScript::SignalEvent(EVENT_PLAYER_TALENT_UPDATE, 0);

    return 0;
}

LUA_FUNCTION(FlashGameWindow, (lua_State* L)) {
    HWND activeWindow = *(HWND*)0xD41620;

    if (activeWindow && GetForegroundWindow() != activeWindow) {
        FLASHWINFO flashInfo;

        flashInfo.cbSize = sizeof(flashInfo);
        flashInfo.hwnd = activeWindow;
        flashInfo.dwFlags = FLASHW_TIMERNOFG | FLASHW_TRAY;
        flashInfo.uCount = -1;
        flashInfo.dwTimeout = 500;

        FlashWindowEx(&flashInfo);
    }

    return 0;
}

LUA_FUNCTION(FindSpellActionBarSlot, (lua_State* L)) {
    uint32_t spellID = ClientLua::GetNumber(L, 1);
    uintptr_t* actionBarSpellIDs = reinterpret_cast<uintptr_t*>(0xC1E358);
    uint8_t count = 0;

    for (uint8_t i = 0; i < 144; i++)
        if (actionBarSpellIDs[i] == spellID) {
            ClientLua::PushNumber(L, i);
            count++;
        }

    if (!count) {
        ClientLua::PushNil(L);
        return 1;
    }
    else
        return count;
}

LUA_FUNCTION(ReplaceActionBarSpell, (lua_State* L)) {
    uint32_t oldSpellID = ClientLua::GetNumber(L, 1);
    uint32_t newSpellID = ClientLua::GetNumber(L, 2);
    uintptr_t* actionBarSpellIDs = reinterpret_cast<uintptr_t*>(0xC1E358);
    uintptr_t* actionButtons = reinterpret_cast<uintptr_t*>(0xC1DED8);

    for (uint8_t i = 0; i < 144; i++)
        if (actionBarSpellIDs[i] == oldSpellID) {
            actionBarSpellIDs[i] = newSpellID;
            ClientPacket::MSG_SET_ACTION_BUTTON(i, 1, 0);

            for (uint8_t j = i + 72; j < 144; j += 12) {
                if (!actionButtons[j]) {
                    actionBarSpellIDs[i] = newSpellID;
                    actionButtons[j] = 1;
                    ClientPacket::MSG_SET_ACTION_BUTTON(j, 1, 0);
                }
            }
        }

    return 0;
}

LUA_FUNCTION(SetSpellInActionBarSlot, (lua_State* L)) {
    uint32_t spellID = ClientLua::GetNumber(L, 1);
    uint8_t slotID = ClientLua::GetNumber(L, 2);
    uintptr_t* actionBarSpellIDs = reinterpret_cast<uintptr_t*>(0xC1E358);
    uintptr_t* actionButtons = reinterpret_cast<uintptr_t*>(0xC1DED8);

    if (slotID < 144) {
        if (!actionButtons[slotID])
            actionButtons[slotID] = 1;

        actionBarSpellIDs[slotID] = spellID;
        ClientPacket::MSG_SET_ACTION_BUTTON(slotID, 1, 0);
    }

    return 0;
}

LUA_FUNCTION(SetTextureWithMask, (lua_State* L)) {
    void* textureObject = ResolveTextureObject(L, 1);
    if (!textureObject)
        return LuaLErrorEx(L, const_cast<char*>("SetTextureWithMask(): expected texture object or texture name"));

    std::string source = ClientLua::GetString(L, 2, "");
    std::string mask = ClientLua::GetString(L, 3, "");
    if (source.empty() || mask.empty())
        return LuaLErrorEx(L, const_cast<char*>("SetTextureWithMask(): expected source texture and mask texture"));

    s_textureMaskByObject[reinterpret_cast<uintptr_t>(textureObject)] = mask;
    if (!ApplyMaskedTextureToObject(textureObject, source, mask))
        return LuaLErrorEx(L, const_cast<char*>("SetTextureWithMask(): failed to build masked texture from '%s' with mask '%s'"), const_cast<char*>(source.c_str()), const_cast<char*>(mask.c_str()));
    return 0;
}

CLIENT_DETOUR(Lua_SetPortraitToTexture, 0x00516970, __cdecl, int, (lua_State * L))
{
    std::string mask = ClientLua::GetString(L, 3, "");
    if (mask.empty())
        return Lua_SetPortraitToTexture(L);

    void* textureObject = ResolveTextureObject(L, 1);
    if (!textureObject)
        return LuaLErrorEx(L, const_cast<char*>("SetPortraitToTexture(): expected texture object or texture name"));

    std::string source = ClientLua::GetString(L, 2, "");
    if (source.empty())
        return LuaLErrorEx(L, const_cast<char*>("SetPortraitToTexture(): expected source texture"));

    s_textureMaskByObject[reinterpret_cast<uintptr_t>(textureObject)] = mask;
    if (!ApplyMaskedTextureToObject(textureObject, source, mask))
        return LuaLErrorEx(L, const_cast<char*>("SetPortraitToTexture(): failed to build masked texture from '%s' with mask '%s'"), const_cast<char*>(source.c_str()), const_cast<char*>(mask.c_str()));
    return 0;
}
