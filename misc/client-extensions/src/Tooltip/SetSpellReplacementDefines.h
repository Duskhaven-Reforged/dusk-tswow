#pragma once

#include <ClientMacros.h>
#include <SharedDefines.h>

/*
 * Client symbols for full CGTooltip__SetSpell replacement.
 * Addresses from user-provided list; CGTooltip__SetSpell entry from IDA (verify per build).
 */

/* --- CGTooltip__SetSpell entry: patch target (verify in IDA; function contains 0x623C71) --- */
static const uint32_t CGTooltip__SetSpell_ADDR = 0x6238A0;

/* Replacement: static member so MSVC allows __thiscall; address used for JMP patch. */
struct SetSpellReplacementFn {
    static int __thiscall Call(void* this_, int spellId, int a3, int a4, int a5, int a6, int a7, int a8,
        uint32_t* a9, int a10, int a11, int a12, int a13, int a14, int a15, int a16);
};

/* --- CGTooltip --- */
CLIENT_FUNCTION(CGTooltip_ClearTooltip, 0x61C620, __thiscall, void, (void* this_, void*))
CLIENT_FUNCTION(CGTooltip_AddLine, 0x61FEC0, __thiscall, void, (void* this_, char* line1, char* line2, void* color1, void* color2, uint32_t))
CLIENT_FUNCTION(CGTooltip_SetItem, 0x6277F0, __thiscall, void, (void* this_, uint32_t itemId, uint32_t* a3, void* guid, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t))
CLIENT_FUNCTION(CGTooltip_CalculateSize, 0x61CAF0, __thiscall, void, (void* this_))
CLIENT_FUNCTION(CGTooltip_GetDurationString, 0x61A9E0, __cdecl, void, (char* dest, uint32_t size, uint64_t duration, char* key, uint32_t a, uint32_t b, uint32_t c))

/* --- CSimpleFrame --- */
CLIENT_FUNCTION(CSimpleFrame_Hide, 0x48F620, __thiscall, void, (void* this_, void*))
CLIENT_FUNCTION(CSimpleFrame_Show, 0x48F660, __thiscall, void, (void* this_))

/* --- Spell / Unit (Spell_C_, Unit) --- */
CLIENT_FUNCTION(Spell_C_GetPowerCost, 0x8012F0, __cdecl, uint32_t, (SpellRow* spell, void* unit))
CLIENT_FUNCTION(Spell_C_GetPowerCostPerSecond, 0x7FF100, __cdecl, uint32_t, (SpellRow* spell, void* unit))
CLIENT_FUNCTION(Spell_C_GetMinMaxRange, 0x7FF480, __cdecl, void, (void* unit, SpellRow* spell, float* minRange, float* maxRange, int, int))
CLIENT_FUNCTION(Spell_C_UsesDefaultMinRange, 0x7FF3C0, __cdecl, int, (SpellRow* spell))
CLIENT_FUNCTION(Spell_C_GetDefaultMinRange, 0x7FF400, __cdecl, void, (SpellRow* spell, float* minRange))
CLIENT_FUNCTION(Unit_GetPowerDivisor, 0x7FDE00, __cdecl, int, (uint32_t powerType))

/* --- SpellRec --- */
CLIENT_FUNCTION(SpellRec_RangeHasFlag_0x1, 0x7FF380, __cdecl, int, (SpellRow* spell))
CLIENT_FUNCTION(SpellRec_IsModifiedStat, 0x800770, __cdecl, int, (SpellRow* spell, int stat))
CLIENT_FUNCTION(SpellRec_GetModifiedStatValue, 0x7FDB50, __cdecl, void, (SpellRow* spell, uint32_t* value, uint32_t stat))
CLIENT_FUNCTION(SpellRec_UsableInShapeshift, 0x7FE850, __cdecl, int, (SpellRow* spell, int formIndex))

/* --- Subs --- */
CLIENT_FUNCTION(sub_482900, 0x482900, __cdecl, int, (float, float))
CLIENT_FUNCTION(sub_6224F0, 0x6224F0, __cdecl, int, (void* tooltip, void* a9, int a10, int a7, int a5, int a12))
CLIENT_FUNCTION(sub_7FEF60, 0x7FEF60, __cdecl, void, (uint32_t* out, SpellRow* spell, int))
CLIENT_FUNCTION(sub_61DD60, 0x61DD60, __cdecl, void, (void*, void*, void*, void*, int))
CLIENT_FUNCTION(sub_7548F0, 0x7548F0, __cdecl, int, (uint32_t categoryId, int))
CLIENT_FUNCTION(sub_622800, 0x622800, __cdecl, void, (void* a9, int a10, int v137, int a14, int a15, int, int a8, int a5, int a12))
CLIENT_FUNCTION(sub_50F590, 0x50F590, __cdecl, void, (void*))
CLIENT_FUNCTION(sub_800D60, 0x800D60, __cdecl, int, (void* player, SpellRow* spell))

/* --- Item / Bag / Unit --- */
CLIENT_FUNCTION(DBItemCache_GetInfoBlockByID, 0x67CA30, __cdecl, void*, (int cacheType, uint32_t itemId, void* a3, void* callback, void* tooltip, int))
CLIENT_FUNCTION(CGBag_C_FindItemOfType, 0x754A20, __cdecl, void*, (void* bag, uint32_t itemId, int))
CLIENT_FUNCTION(CGBag_C_GetItemTypeCount, 0x500100, __cdecl, uint32_t, (void* bag, uint32_t itemId, int))
CLIENT_FUNCTION(CGUnit_C_EquippedItemMeetSpellRequirements, 0x754D00, __cdecl, int, (void* unit, SpellRow* spell, int mask))
CLIENT_FUNCTION(CGUnit_C_IsShapeShifted, 0x721CA0, __cdecl, int, (void* unit))

/* --- FrameScript / Rep --- */
CLIENT_FUNCTION(GetRepListRepValue, 0x5D05B0, __cdecl, int, (uint32_t factionId))
CLIENT_FUNCTION(FrameScript_GetLocalizedText, 0x7225E0, __cdecl, const char*, (const char* key, int, int))
CLIENT_FUNCTION(FrameScript_Object_RunScript, 0x81A2C0, __cdecl, void, (void* scriptRef, int, int))

/* FormatString: used for MELEE_RANGE, SPELL_RANGE, SPELL_RANGE_DUAL, SPELL_CAST_TIME_*, SPELL_RECAST_TIME_*. Resolve in IDA if build fails. */
CLIENT_FUNCTION(FormatString, 0x61A190, __cdecl, void, (int a1, const char* keyOrId, int a3, char* dest, uint32_t destSize))

/* WDB cache type for DBItemCache_GetInfoBlockByID */
static const int WDB_CACHE_ITEM = 0;

/* --- Globals --- */
CLIENT_ADDRESS(void, g_spellDB, 0x00AD49D0)
CLIENT_ADDRESS(void, stru_C24220, 0x00C24220)
CLIENT_ADDRESS(void, off_A25978, 0x00A25978)
CLIENT_ADDRESS(void, off_AD2EA0, 0x00AD2EA0)
CLIENT_ADDRESS(void, off_AD2A5C, 0x00AD2A5C)
CLIENT_ADDRESS(void, off_AD2E84, 0x00AD2E84)
CLIENT_ADDRESS(void, dword_A2D2FC, 0x00A2D2FC)
CLIENT_ADDRESS(void, s_Color_Hex_White, 0x00AD2D30)
CLIENT_ADDRESS(void, s_Color_Hex_Grey0, 0x00AD2D38)
CLIENT_ADDRESS(void, s_Color_Hex_DarkYellow, 0x00AD2D2C)
CLIENT_ADDRESS(void, s_Color_Hex_Red0, 0x00AD2D34)
CLIENT_ADDRESS(void, g_powerDisplayDB, 0x00AD43A0)
CLIENT_ADDRESS(void, g_spellRuneCostDB, 0x00AD49AC)
CLIENT_ADDRESS(void, g_skillLineDB, 0x00AD45E0)
CLIENT_ADDRESS(void, g_totemCategoryDB, 0x00AD4C7C)
CLIENT_ADDRESS(void, g_itemSubClassMaskDB, 0x00AD3F20)
CLIENT_ADDRESS(void, g_itemSubClassDB, 0x00AD3F44)
CLIENT_ADDRESS(void, g_spellShapeshiftFormDB, 0x00AD49F4)
CLIENT_ADDRESS(void, g_factionDB, 0x00AD3860)

/* DB layout: b_base_01.b_base.m_minID/m_maxID, b_base_02.m_recordsById or m_records. Offsets for 3.3.5; verify in IDA. */
inline int32_t DB_MIN_ID(void* db) { return *reinterpret_cast<int32_t*>(reinterpret_cast<char*>(db) + 8); }
inline int32_t DB_MAX_ID(void* db) { return *reinterpret_cast<int32_t*>(reinterpret_cast<char*>(db) + 12); }
inline int32_t DB_NUM_RECORDS(void* db) { return *reinterpret_cast<int32_t*>(reinterpret_cast<char*>(db) + 16); }
inline void* DB_RECORDS_BY_ID(void* db) { return *reinterpret_cast<void**>(reinterpret_cast<char*>(db) + 20); }
inline void* DB_RECORDS(void* db) { return *reinterpret_cast<void**>(reinterpret_cast<char*>(db) + 20); }

/* Record types for tooltip (minimal; match client layout). */
struct TotemCategoryRec { char pad[8]; char* m_name; };
struct ItemSubClassMaskRec { int32_t m_classID; int32_t m_mask; char* m_name; };
struct ItemSubClassRec { int32_t m_classID; int32_t m_subClassID; char* m_verboseName; char* m_displayName; };
struct SpellShapeshiftFormRec { char pad[8]; char* m_name; };
struct FactionRec { char pad[8]; char* m_name; };
struct ItemCacheName { char* Name[1][1]; };

typedef TotemCategoryRec TotemCategoryRow;
typedef ItemSubClassMaskRec ItemSubClassMaskRow;
typedef ItemSubClassRec ItemSubClassRow;
typedef SpellShapeshiftFormRec SpellShapeshiftFormRow;
typedef FactionRec FactionRow;
typedef ItemCacheName ItemCacheRow;
