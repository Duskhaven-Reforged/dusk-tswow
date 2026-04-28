#pragma once

#include <ClientMacros.h>
#include <Util.h>
#include <ClientData/MathTypes.h>
#include <ClientData/GameEnums.h>
#include <ClientData/DBCRowTypes.h>
#include <ClientData/Spell.h>
#include <ClientData/ObjectFields.h>
#include <ClientData/Units.h>
#include <ClientData/Players.h>
#include <ClientData/System.h>

#include <ClientData/Object.h>

#include <cmath>

using namespace ClientData;

struct lua_State;

// Client functions and constants
namespace CGChat {
    CLIENT_FUNCTION(AddChatMessage, 0x509DD0, __cdecl, bool, (char*, uint32_t, uint32_t, uint32_t, uint32_t*, uint32_t, char*, uint64_t, uint32_t, uint64_t, uint32_t, uint32_t, uint32_t*))
}

namespace CGGameUI {
    CLIENT_FUNCTION(DisplayError, 0x5216F0, __cdecl, void, (uint32_t, ...))
}

namespace CGPetInfo_C {
    CLIENT_FUNCTION(GetPet, 0x5D3390, __cdecl, uint64_t, (uint32_t))
}

namespace CGUnit_C {
    CLIENT_FUNCTION(GetFacing, 0x6E6F60, __thiscall, double, (CGUnit*))
    CLIENT_FUNCTION(GetShapeshiftFormId, 0x71AF70, __thiscall, uint32_t, (CGUnit*))
    CLIENT_FUNCTION(HasAuraBySpellId, 0x7282A0, __thiscall, bool, (CGUnit*, uint32_t))
    CLIENT_FUNCTION(GetAuraCount, 0x004F8850, __thiscall, int, (CGUnit*))
    CLIENT_FUNCTION(GetAura, 0x00556E10, __thiscall, AuraData*, (CGUnit*, uint32_t))
    CLIENT_FUNCTION(GetAuraFlags, 0x00565510, __thiscall, uint8_t, (CGUnit*, uint32_t))
    CLIENT_FUNCTION(AffectedByAura, 0x007283A0, __thiscall, char, (CGUnit*, uint32_t, uint32_t))
    CLIENT_FUNCTION(HasAuraMatchingSpellClass, 0x7283A0, __thiscall, bool, (CGUnit*, uint32_t, SpellRow*))
    CLIENT_FUNCTION(ShouldFadeIn, 0x716650, __thiscall, bool, (CGUnit*))
    CLIENT_FUNCTION(GetDistanceToPos, 0x004F61D0, __thiscall, float, (CGUnit*, C3Vector*))
}

namespace CGWorldFrame_C {
    CLIENT_FUNCTION(TranslateToMapCoords, 0x544140, __cdecl, bool, (C3Vector*, uint32_t, float*, float*, uint32_t, bool, uint32_t))
    CLIENT_FUNCTION(Intersect, 0x0077F310, __cdecl, char, (C3Vector*, C3Vector*, C3Vector*, float*, int, int))
}

namespace ClientDB {
    CLIENT_FUNCTION(GetRow, 0x65C290, __thiscall, void*, (void*, uint32_t))
    CLIENT_FUNCTION(GetLocalizedRow, 0x4CFD20, __thiscall, int, (void*, uint32_t, void*))
}

namespace ClientPacket {
    CLIENT_FUNCTION(MSG_SET_ACTION_BUTTON, 0x5AA390, __cdecl, void, (uint32_t, bool, bool))
}

namespace ClntObjMgr {
    CLIENT_FUNCTION(GetActivePlayer, 0x4D3790, __cdecl, uint64_t, ())
    CLIENT_FUNCTION(GetActivePlayerObj, 0x004038F0, __cdecl, CGPlayer*, ())
    CLIENT_FUNCTION(GetUnitFromName, 0x60C1F0, __cdecl, CGUnit*, (char*))
    CLIENT_FUNCTION(ObjectPtr, 0x4D4DB0, __cdecl, void*, (uint64_t, uint32_t))
}

namespace CVar_C {
    CLIENT_FUNCTION(sub_766940, 0x766940, __thiscall, void, (void*, int, char, char, char, char))
    CLIENT_FUNCTION(Register, 0x00767FC0, __cdecl, CVar*, (char*, char*, int, char*, CVarCallback, int, char, int, char))
}

namespace DNInfo {
    CLIENT_FUNCTION(AddZoneLight, 0x7ED150, __thiscall, void, (void*, int32_t, float))
    CLIENT_FUNCTION(GetDNInfoPtr, 0x7ECEF0, __stdcall, void*, ())
}

namespace FrameScript {
    CLIENT_FUNCTION(GetParam, 0x815500, __cdecl, bool, (lua_State*, int, int))
    CLIENT_FUNCTION(GetText, 0x819D40, __cdecl, char*, (char*, int, int))
    CLIENT_FUNCTION(SignalEvent, 0x81B530, __cdecl, int, (uint32_t, char*, ...))
}

namespace NTempest {
    CLIENT_FUNCTION(DistanceSquaredFromEdge, 0x7F9C90, __cdecl, bool, (int32_t, void*, C2Vector*, float*))
}

namespace SpellParser {
    CLIENT_FUNCTION(ParseText, 0x57ABC0, __cdecl, void, (void*, void*, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t))
}

namespace Spell_C {
    CLIENT_FUNCTION(SpellFailed, 0x808200, __cdecl, void, (void*, SpellRow*, uint32_t, int32_t, int32_t, uint32_t))
    CLIENT_FUNCTION(IsTargeting, 0x007FD620, __cdecl, bool, ())
    CLIENT_FUNCTION(CanTargetTerrain, 0x007FD750, __cdecl, bool, ())
    CLIENT_FUNCTION(CanTargetUnits, 0x007FD650, __cdecl, bool, ())
    CLIENT_FUNCTION(GetSpellRange, 0x00802C30, __cdecl, void, (void* a1, unsigned int spellId, float* minDist, float* maxDist, void* a5))
    CLIENT_FUNCTION(GetPowerCost, 0x008012F0, __cdecl, uint32_t, (SpellRow*, CGUnit*))
    CLIENT_FUNCTION(GetPowerCostPerSecond, 0x007FF100, __cdecl, uint32_t, (SpellRow*, CGUnit*))
    CLIENT_FUNCTION(GetMinMaxRange, 0x007FF480, __cdecl, void, (CGUnit*, SpellRow*, float*, float*, uint32_t, uint32_t))
    CLIENT_FUNCTION(UsesDefaultMinRange, 0x007FF3C0, __cdecl, bool, (SpellRow*))
    CLIENT_FUNCTION(GetDefaultMinRange, 0x007FF400, __cdecl, void, (SpellRow*, float*))
}

CLIENT_FUNCTION(SpellRec_RangeHasFlag_0x1, 0x007FF380, __cdecl, bool, (SpellRow*))
CLIENT_FUNCTION(SpellRec_IsModifiedStat, 0x00800770, __cdecl, bool, (SpellRow*, uint32_t))

namespace Unit_C {
    CLIENT_FUNCTION(GetPowerDivisor, 0x007FDE00, __cdecl, uint32_t, (uint32_t))
}

namespace SpellRec_C {
    CLIENT_FUNCTION(GetLevel, 0x7FF070, __cdecl, uint32_t, (SpellRow*, uint32_t, uint32_t))
    CLIENT_FUNCTION(GetCastTime, 0x7FF180, __cdecl, uint32_t, (SpellRow*, uint32_t, uint32_t, uint32_t))
    CLIENT_FUNCTION(ModifySpellValueInt, 0x7FDB50, __cdecl, void, (SpellRow*, uint32_t*, uint32_t))
    CLIENT_FUNCTION(GetModifiedStatValue, 0x7FDB50, __cdecl, void, (SpellRow*, int32_t*, uint32_t))
}

namespace SErr {
    CLIENT_FUNCTION(PrepareAppFatal, 0x772A80, __cdecl, void, (uint32_t, const char*, ...))
}

namespace SFile {
    // Defs cherrypicked from StormLib: https://github.com/ladislav-zezula/StormLib
    CLIENT_FUNCTION(OpenFileEx, 0x424B50, __stdcall, bool, (HANDLE, const char*, uint32_t, HANDLE*))
    CLIENT_FUNCTION(ReadFile, 0x422530, __stdcall, bool, (HANDLE, void*, uint32_t, uint32_t*, uint32_t*, uint32_t))
    CLIENT_FUNCTION(CloseFile, 0x422910, __stdcall, void, (HANDLE))
    CLIENT_FUNCTION(OpenFile, 0x424F80, __stdcall, int, (char const*, HANDLE*))
    CLIENT_FUNCTION(GetFileSize, 0x4218C0, __stdcall, DWORD, (HANDLE, DWORD*))
}

namespace SMem {
    CLIENT_FUNCTION(Alloc, 0x76E540, __stdcall, void*, (uint32_t, const char*, uint32_t, uint32_t))
    CLIENT_FUNCTION(Free, 0x76E5A0, __stdcall, bool, (void*, const char*, uint32_t, uint32_t))
}

namespace SStr {
    CLIENT_FUNCTION(Printf, 0x76F070, __cdecl, int, (char*, uint32_t, char*, ...))
    CLIENT_FUNCTION(Copy, 0x76ED20, __stdcall, char*, (char*, char*, uint32_t))
    CLIENT_FUNCTION(Append, 0x76EF70, __stdcall, char*, (char*, char*, uint32_t))
    CLIENT_FUNCTION(Len, 0x76EE30, __stdcall, char*, (char*))
    CLIENT_FUNCTION(Chr, 0x76E6E0, __cdecl, char*, (char*, char))
}

namespace SysMsg {
    CLIENT_FUNCTION(Printf, 0x4B5040, __cdecl, int, (uint32_t, uint32_t, char*, ...))
}

namespace World {
    CLIENT_FUNCTION(LoadMap, 0x781430, __cdecl, void, (char*, C3Vector*, uint32_t))
    CLIENT_FUNCTION(UnloadMap, 0x783180, __cdecl, void, ())
}

CLIENT_FUNCTION(OsGetAsyncTimeMs, 0x86AE20, __cdecl, uint64_t, ())

CLIENT_FUNCTION(sub_6B1080, 0x6B1080, __cdecl, uint8_t, ())
CLIENT_FUNCTION(sub_6E22C0, 0x6E22C0, __thiscall, uint32_t, (void*, uint32_t))
CLIENT_FUNCTION(sub_812410, 0x812410, __cdecl, SkillLineAbilityRow*, (uint32_t, uint32_t, uint32_t))
CLIENT_FUNCTION(DBItemCache_GetInfoBlockByID, 0x67CA30, __thiscall, void*, (void*, uint32_t, void*, void*, void*, int))
CLIENT_FUNCTION(CGBag_C__FindItemOfType, 0x754A20, __thiscall, void*, (void*, uint32_t, int))
CLIENT_FUNCTION(Player_HasTotemCategory, 0x7548F0, __thiscall, char, (void*, uint32_t, int))
CLIENT_FUNCTION(LookupEntryById, 0x6224F0, __thiscall, int, (void*, uint32_t*, int, int, int, int))
CLIENT_FUNCTION(CGUnit_C__EquippedItemMeetSpellRequirements, 0x6DE230, __thiscall, void*, (CGUnit*, SpellRow*, int))
CLIENT_FUNCTION(TerrainClick, 0x00527830, __cdecl, void, (TerrainClickEvent*))
CLIENT_FUNCTION(SStrCmpI, 0x0076E780, __stdcall, int, (char* text1, const char* text2, int length))
CLIENT_FUNCTION(TraceLine, 0x007A3B70, __cdecl, char, (C3Vector* start, C3Vector* end, C3Vector* hitPoint, float* distance, uint32_t flag, uint32_t optional))

namespace CGTooltipInternal {
    CLIENT_FUNCTION(ClearTooltip, 0x61C620, __thiscall, void, (void*))
    CLIENT_FUNCTION(CalculateSize, 0x61CAF0, __thiscall, void, (void*))
    CLIENT_FUNCTION(SetItem, 0x6277F0, __thiscall, void, (void*, int, unsigned int, void*, void*, int, int, int, int, int, int, int, int, int, int, int))
}

namespace CSimpleFrame {
    CLIENT_FUNCTION(Hide, 0x0048F620, __thiscall, void, (void*))
    CLIENT_FUNCTION(Show, 0x0048F660, __thiscall, void, (void*))
}

// Spell tooltip replacement: item load callback (pass to DBItemCache_GetInfoBlockByID).
CLIENT_FUNCTION(CGTooltip_HandleItemLoad, 0x61DD60, __cdecl, void, (void*, void*))

// Spell tooltip: target unit when a7; read *(uint64_t*)kStruC24220.
static constexpr uintptr_t kStruC24220 = 0xC24220;
static constexpr uintptr_t kDwordA2D2FC = 0xA2D2FC;
// Faction DB; m_recordsById at +0x1C, minID/maxID in base.
static constexpr uintptr_t kGFactionDB = 0xAD3860;
// Spell shapeshift form DB; m_numRecords at +8, m_records at +0x1C.
static constexpr uintptr_t kGSpellShapeshiftFormDB = 0xAD49F4;

CLIENT_FUNCTION(Player_CanCastSpellInCurrentForm, 0x800D60, __cdecl, int, (int, int))
CLIENT_FUNCTION(SpellRec__UsableInShapeshift, 0x7FE850, __cdecl, uint8_t, (SpellRow*, int))
CLIENT_FUNCTION(CGUnit_C__GetShapeshiftFormId, 0x71AF70, __thiscall, int, (CGUnit*))
CLIENT_FUNCTION(CGUnit_C__IsShapeShifted, 0x721CA0, __thiscall, uint8_t, (CGUnit*))
CLIENT_FUNCTION(CGUnit_C__AffectedByAura, 0x7283A0, __thiscall, uint8_t, (CGUnit*, uint32_t, SpellRow*))
CLIENT_FUNCTION(GetRepListRepValue, 0x5D05B0, __cdecl, int32_t, (uint32_t))
CLIENT_FUNCTION(FrameScript__GetLocalizedText, 0x7225E0, __thiscall, const char*, (void*, const char*, int))
CLIENT_FUNCTION(TalentTooltip_AddActionLines, 0x622800, __cdecl, void, (uint32_t*, int, int, int, int, int, int, int, int))
CLIENT_FUNCTION(CGTooltipItemData_Reset, 0x50F590, __cdecl, void, (void*))
CLIENT_FUNCTION(FrameScript_Object__RunScript, 0x81A2C0, __cdecl, void, (void*, int, int))
CLIENT_FUNCTION(CGBag_C__GetItemTypeCount, 0x754D00, __thiscall, uint32_t, (void*, uint32_t, int))
