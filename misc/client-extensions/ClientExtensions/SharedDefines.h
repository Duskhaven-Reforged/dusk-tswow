#pragma once
#include "Windows.h"
#include "ClientMacros.h"

#include <functional>

enum ObjectTypeMask : uint32_t {
    TYPEMASK_OBJECT         = 0x0001,
    TYPEMASK_ITEM           = 0x0002,
    TYPEMASK_CONTAINER      = 0x0004,
    TYPEMASK_UNIT           = 0x0008,
    TYPEMASK_PLAYER         = 0x0010,
    TYPEMASK_GAMEOBJECT     = 0x0020,
    TYPEMASK_DYNAMICOBJECT  = 0x0040,
    TYPEMASK_CORPSE         = 0x0080,
};

enum Field : uint32_t {
    CURRENT_HP              = 18,
    CURRENT_MANA            = 19,
    CURRENT_RAGE            = 20,
    CURRENT_FOCUS           = 21,
    CURRENT_ENERGY          = 22,
    CURRENT_HAPPINESS       = 23,
    CURRENT_RUNES           = 24,
    CURRENT_RUNIC_POWER     = 25,
    MAX_HP                  = 26,
    MAX_MANA                = 27,
    MAX_RAGE                = 28,
    MAX_FOCUS               = 29,
    MAX_ENERGY              = 30,
    MAX_HAPPINESS           = 31,
    MAX_RUNES               = 32,
    MAX_RUNIC_POWER         = 33,
};

enum SpellFamilyNames : uint32_t {
    SPELLFAMILY_GENERIC     = 0,
    SPELLFAMILY_UNK1        = 1,
    SPELLFAMILY_MAGE        = 3,
    SPELLFAMILY_WARRIOR     = 4,
    SPELLFAMILY_WARLOCK     = 5,
    SPELLFAMILY_PRIEST      = 6,
    SPELLFAMILY_DRUID       = 7,
    SPELLFAMILY_ROGUE       = 8,
    SPELLFAMILY_HUNTER      = 9,
    SPELLFAMILY_PALADIN     = 10,
    SPELLFAMILY_SHAMAN      = 11,
    SPELLFAMILY_UNK2        = 12,
    SPELLFAMILY_POTION      = 13,
    SPELLFAMILY_DEATHKNIGHT = 15,
    SPELLFAMILY_PET         = 17
};

enum AuraType : uint32_t {
    SPELL_AURA_CAN_GLIDE    = 347
};

static uint32_t dummy = 0;

static char* sConnectorPlus = " + ";
static char* sPluralS = "s";
static char* sSpace = " ";

// client functions
CLIENT_FUNCTION(SFileOpenFile, 0x424F80, __stdcall, int, (char const* filename, HANDLE* a2 /*file handle out*/))
CLIENT_FUNCTION(SFileGetFileSize, 0x4218C0, __stdcall, DWORD /*lowest 32 bits in size*/, (HANDLE handle, DWORD* highSize /*highest 32 bits in size*/))
CLIENT_FUNCTION(SFileReadFile, 0x422530, __stdcall, int, (HANDLE handle /*likely a handle*/, char* data, DWORD bytesToRead, DWORD* bytesRead, int overlap /*just set to 0*/, int unk /*just set to 0*/))
CLIENT_FUNCTION(SFileCloseFile, 0x422910, __stdcall, int, (HANDLE a1))

CLIENT_FUNCTION(ClntObjMgrGetActivePlayer, 0x4D3790, __cdecl, uint64_t, ())
CLIENT_FUNCTION(ClntObjMgrObjectPtr, 0x4D4DB0, __cdecl, void*, (uint64_t, uint32_t))

CLIENT_FUNCTION(FrameScript_GetText, 0x819D40, __cdecl, char*, (char*, int, int))
CLIENT_FUNCTION(SStrPrintf, 0x76F070, __cdecl, int, (char*, uint32_t, char*, uint32_t))
CLIENT_FUNCTION(SStrCopy_0, 0x76EF70, __stdcall, unsigned char, (char*, char*, uint32_t))

CLIENT_FUNCTION(ClientDB__GetRow, 0x65C290, __thiscall, void*, (void*, uint32_t))
CLIENT_FUNCTION(ClientDB__GetLocalizedRow, 0x4CFD20, __thiscall, int, (void*, uint32_t, void*))

CLIENT_FUNCTION(SpellParserParseText, 0x57ABC0, __cdecl, void, (void*, void*, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t))

CLIENT_FUNCTION(CGUnit_C__GetAuraCount, 0x4F8850, __thiscall, uint32_t, (void*))
CLIENT_FUNCTION(CGUnit_C__HasAuraBySpellId, 0x7282A0, __thiscall, bool, (void*, uint32_t))

// functions
static int32_t GetPlayerField(uint32_t* ActivePlayer, uint32_t field) {
    return *reinterpret_cast<int32_t*>(*(ActivePlayer + 52) + 4 * field);
};

static void OverwriteUInt32AtAddress(uint32_t position, uint32_t newValue) {
    DWORD flOldProtect = 0;

    VirtualProtect((LPVOID)position, 0x4, PAGE_EXECUTE_READWRITE, &flOldProtect);
    *reinterpret_cast<uint32_t*>(position) = newValue;
    VirtualProtect((LPVOID)position, 0x4, flOldProtect, &flOldProtect);
};

static void WriteBytesAtAddress(void* position, uint8_t byte, size_t size) {
    DWORD flOldProtect = 0;

    VirtualProtect((LPVOID)position, size, PAGE_EXECUTE_READWRITE, &flOldProtect);
    memset(position, byte, size);
    VirtualProtect((LPVOID)position, 0x4, flOldProtect, &flOldProtect);
}

// Aleist3r: use bigger number as address1
// if the jump/call address is earlier in the memory (e.g. you're jumping from dll code back to wow.exe address), use backwards = true
// TODO: investigate what I did wrong with code, math was off
static uint32_t CalculateAddress(uint32_t address1, uint32_t address2, bool backwards = false) {
    if (!backwards)
        return address1 - address2;
    else
        return address2 - address1;
}
