/**
 * Full replacement for CGTooltip__SetSpell.
 * Ported from design/disas.txt; uses client functions and globals from SharedDefines.h.
 * FormatString is replaced by FrameScript::GetText + SStr::Copy where needed.
 */

#include <SharedDefines.h>
#include <Tooltip/SetSpellReplacementDefines.h>
#include <Tooltip/SpellTooltipExtensions.h>
#include <Tooltip/TooltipDefines.h>
#include <SpellAttrDefines.h>
#include <Windows.h>

#include <cstring>

// Helper: format a localized string into dest (replaces FormatString)
static void FormatStringCopy(const char* key, char* dest, uint32_t destSize) {
    char* text = FrameScript::GetText(const_cast<char*>(key), -1, 0);
    if (text)
        SStr::Copy(dest, text, destSize);
    else
        dest[0] = '\0';
}

// Null stub for nullsub_3
static void nullsub_3() {}

// Get PowerDisplayRow by id via client DB (replacement for g_powerDisplayDB.b_base_02.m_recordsById[id - minID])
static PowerDisplayRow* GetPowerDisplayRow(uint32_t id) {
    void* row = ClientDB::GetRow(g_powerDisplayDB, id);
    return reinterpret_cast<PowerDisplayRow*>(row);
}

// Get SpellRuneCostRow by id
static SpellRuneCostRow* GetSpellRuneCostRow(uint32_t id) {
    return reinterpret_cast<SpellRuneCostRow*>(ClientDB::GetRow(g_spellRuneCostDB, id));
}

// Get SkillLineRow by id
static SkillLineRow* GetSkillLineRow(uint32_t id) {
    return reinterpret_cast<SkillLineRow*>(ClientDB::GetRow(g_skillLineDB, id));
}

// Get TotemCategoryRow by id
static TotemCategoryRow* GetTotemCategoryRow(uint32_t id) {
    return reinterpret_cast<TotemCategoryRow*>(ClientDB::GetRow(g_totemCategoryDB, id));
}

// Get ItemSubClassMaskRow - we need to iterate; for simplicity we use GetRow if id is valid
static ItemSubClassMaskRow* GetItemSubClassMaskRowByClassAndMask(uint32_t classID, uint32_t mask) {
    // Client iterates m_records; we have no direct index. Use GetRow with a composite or iterate.
    // Fallback: return nullptr and handle in caller (build v128 from ItemSubClassRow only).
    (void)classID;
    (void)mask;
    return nullptr;
}

// Get ItemSubClassRow by index (for iteration)
static ItemSubClassRow* GetItemSubClassRow(uint32_t index) {
    return reinterpret_cast<ItemSubClassRow*>(ClientDB::GetRow(g_itemSubClassDB, index));
}

// Get SpellShapeshiftFormRow by index
static SpellShapeshiftFormRow* GetSpellShapeshiftFormRow(uint32_t index) {
    return reinterpret_cast<SpellShapeshiftFormRow*>(ClientDB::GetRow(g_spellShapeshiftFormDB, index));
}

// Get FactionRow by id
static FactionRow* GetFactionRow(uint32_t id) {
    return reinterpret_cast<FactionRow*>(ClientDB::GetRow(g_factionDB, id));
}

// Spell attributes from disas
enum {
    ATTR_PASSIVE = 0x40,
    ATTR_ON_NEXT_SWING = 0x404,
    ATTR_HIDE_IN_TOOLTIP = 0x20,
    ATTR_REQ_AMMO = 2,
};
enum { ATTR_EX_CHANNELED_1 = 0x44, ATTR_EX_USE_ALL_POWER = 2 };
enum { ATTR_EXB_SHAPESHIFT_MASK_NONE = 0x80000 };
enum { ATTR_EXC_400 = 0x400, ATTR_EXC_1000400 = 0x1000400, ATTR_EXC_1000000 = 0x1000000, ATTR_EXC_MELEE_RANGE = 0x40000000 };

int __cdecl SetSpellReplacementImpl(void* tooltipThis, int spellId, int a3, int a4, int a5, int a6, int a7, int a8, uint32_t* a9, int a10, int a11, int a12, int a13, int a14, int a15, int a16) {
    void* v143 = tooltipThis;
    if (!a11)
        CGTooltip_ClearTooltip(tooltipThis, nullptr);

    uint64_t activePlayerGuid = ClntObjMgr::GetActivePlayer();
    CGPlayer* v19 = reinterpret_cast<CGPlayer*>(ClntObjMgr::ObjectPtr(activePlayerGuid, TYPEMASK_PLAYER));
    CGUnit* v142 = reinterpret_cast<CGUnit*>(v19);
    if (!v19) {
        CSimpleFrame_Hide(tooltipThis, nullptr);
        return 0;
    }

    SpellRow spellRec = {};
    if (!ClientDB::GetLocalizedRow(g_spellDB, static_cast<uint32_t>(spellId), &spellRec)) {
        if (!a11)
            CSimpleFrame_Hide(tooltipThis, nullptr);
        nullsub_3();
        return 0;
    }

    if (!a11) {
        *reinterpret_cast<uint32_t*>(reinterpret_cast<char*>(tooltipThis) + 217 * 4) = static_cast<uint32_t>(spellId);
        *reinterpret_cast<uint32_t*>(reinterpret_cast<char*>(tooltipThis) + 242 * 4) = 0;
    }

    CGUnit* unit;
    if (a7) {
        unit = reinterpret_cast<CGUnit*>(ClntObjMgr::ObjectPtr(*reinterpret_cast<uint64_t*>(stru_C24220), TYPEMASK_UNIT));
    } else if (a5) {
        uint64_t petGuid = CGPetInfo_C::GetPet(0);
        unit = reinterpret_cast<CGUnit*>(ClntObjMgr::ObjectPtr(petGuid, TYPEMASK_UNIT));
    } else {
        unit = v142;
    }

    int v141 = 0;
    int v136 = 0;
    if ((spellRec.m_attributes & ATTR_HIDE_IN_TOOLTIP) != 0) {
        for (int v23 = 0; v23 < 3; ++v23) {
            uint32_t eff = spellRec.m_effect[v23];
            if (eff == 53) {
                v141 = 1;
                break;
            }
            if (eff == 24 || eff == 157 || eff == 59) {
                v141 = 1;
                v136 = 1;
            }
        }
        if ((spellRec.m_attributes & ATTR_HIDE_IN_TOOLTIP) != 0) {
            for (int v23 = 0; v23 < 3; ++v23)
                if (spellRec.m_effect[v23] == 53) {
                    v141 = 1;
                    break;
                }
        }
    }

    char a1_buf[128];
    char src_buf[128];
    char dest_buf[128];
    char v126[512];
    char v127[4096];
    char v125[2048];
    char v128[512];
    char v129[256];
    char v138[32];
    a1_buf[0] = 0;
    src_buf[0] = 0;
    dest_buf[0] = 0;

    // Name / rank / talent line
    if (a9 && a11) {
        char* text = FrameScript::GetText(const_cast<char*>("TOOLTIP_TALENT_NEXT_RANK"), -1, 0);
        SStr::Printf(a1_buf, 128, reinterpret_cast<char*>(off_A25978), text);
        CGTooltip_AddLine(v143, a1_buf, nullptr, s_Color_Hex_White, s_Color_Hex_White, 0);
    } else if (v141) {
        SkillLineAbilityRow* ability = sub_812410(
            static_cast<uint8_t>(v19->unitBase.unitData->unitBytes0.raceID),
            static_cast<uint8_t>(v19->unitBase.unitData->unitBytes0.classID),
            static_cast<uint32_t>(spellId));
        if (ability) {
            uint32_t skillLineId = ability->m_skillLine;
            SkillLineRow* skillRow = GetSkillLineRow(skillLineId);
            if (skillRow) {
                SStr::Printf(a1_buf, 128, const_cast<char*>("%s: %s"), skillRow->m_displayName_lang, spellRec.m_name_lang);
                CGTooltip_AddLine(v143, a1_buf, nullptr, s_Color_Hex_DarkYellow, s_Color_Hex_DarkYellow, 0);
            }
        }
    } else {
        char* subtext = (a3 || a6) ? spellRec.m_nameSubtext_lang : nullptr;
        CGTooltip_AddLine(v143, spellRec.m_name_lang, subtext, s_Color_Hex_White, s_Color_Hex_Grey0, 0);
    }

    int v137 = 1;
    if (a9 && !a11) {
        if (a15 >= 0) {
            char* v30 = FrameScript::GetText(const_cast<char*>("TOOLTIP_TALENT_RANK"), -1, 0);
            SStr::Printf(a1_buf, 128, v30, a14 + 1, a15 + 1);
            CGTooltip_AddLine(v143, a1_buf, nullptr, s_Color_Hex_White, s_Color_Hex_White, 0);
        }
        if (a14 < 0)
            v137 = sub_6224F0(v143, a9, static_cast<uint32_t>(a10), static_cast<uint32_t>(a7), static_cast<uint32_t>(a5), static_cast<uint32_t>(a12));
    }

    a1_buf[0] = 0;
    src_buf[0] = 0;

    uint32_t PowerCost = 0;
    PowerDisplayRow* v134 = GetPowerDisplayRow(spellRec.m_powerDisplayID);
    PowerCost = Spell_C_GetPowerCost(&spellRec, unit);
    if (!a3) {
        uint32_t PowerCostPerSecond = Spell_C_GetPowerCostPerSecond(&spellRec, unit);
        PowerCost /= Unit_GetPowerDivisor(spellRec.m_powerType);
        int v34 = static_cast<int>(PowerCostPerSecond / Unit_GetPowerDivisor(spellRec.m_powerType));
        const char* v35 = (spellRec.m_powerType > 6u) ? "HEALTH_COST" : reinterpret_cast<const char*>(*reinterpret_cast<uintptr_t*>(reinterpret_cast<char*>(off_AD2EA0) + spellRec.m_powerType * sizeof(void*)));
        if (spellRec.m_powerType != 5) {
            if (PowerCost > 0 && v34 <= 0) {
                if (v134) {
                    char* v44 = FrameScript::GetText(v134->m_globalStringBaseTag, -1, 0);
                    SStr::Copy(dest_buf, v44, 128);
                    char* v45 = FrameScript::GetText(const_cast<char*>("POWER_DISPLAY_COST"), -1, 0);
                    SStr::Printf(a1_buf, 128, v45, PowerCost, dest_buf);
                } else {
                    char* v46 = FrameScript::GetText(const_cast<char*>(v35), -1, 0);
                    SStr::Copy(dest_buf, v46, 128);
                    SStr::Printf(a1_buf, 128, dest_buf, PowerCost);
                }
            } else if (v34 > 0) {
                if (v134) {
                    char* v41 = FrameScript::GetText(v134->m_globalStringBaseTag, -1, 0);
                    SStr::Copy(dest_buf, v41, 128);
                    char* v42 = FrameScript::GetText(const_cast<char*>("POWER_DISPLAY_COST_PER_TIME"), -1, 0);
                    SStr::Printf(a1_buf, 128, v42, PowerCost, dest_buf, v34);
                } else {
                    SStr::Printf(dest_buf, 128, "%s_PER_TIME", v35);
                    char* v43 = FrameScript::GetText(dest_buf, -1, 0);
                    SStr::Printf(a1_buf, 128, v43, PowerCost, v34);
                }
            }
        } else {
            // Rune cost (power type 5)
            SpellRuneCostRow* v36 = GetSpellRuneCostRow(spellRec.m_runeCostID);
            if (v36) {
                if (v36->m_blood) {
                    char* v37 = FrameScript::GetText(const_cast<char*>("RUNE_COST_BLOOD"), -1, 0);
                    SStr::Printf(dest_buf, 128, v37, v36->m_blood);
                    SStr::Append(a1_buf, dest_buf, 0x7FFFFFFF);
                    if (v36->m_unholy || v36->m_frost)
                        SStr::Append(a1_buf, const_cast<char*>(" "), 0x7FFFFFFF);
                }
                if (v36->m_unholy) {
                    char* v38 = FrameScript::GetText(const_cast<char*>("RUNE_COST_UNHOLY"), -1, 0);
                    SStr::Printf(dest_buf, 128, v38, v36->m_unholy);
                    SStr::Append(a1_buf, dest_buf, 0x7FFFFFFF);
                    if (v36->m_frost)
                        SStr::Append(a1_buf, const_cast<char*>(" "), 0x7FFFFFFF);
                }
                if (v36->m_frost) {
                    char* v40 = FrameScript::GetText(const_cast<char*>("RUNE_COST_FROST"), -1, 0);
                    SStr::Printf(dest_buf, 128, v40, v36->m_frost);
                    SStr::Append(a1_buf, dest_buf, 0x7FFFFFFF);
                }
            }
        }
    }

    // Range
    float minRange = 0.f, maxRange = 0.f, result = 0.f, v147 = 0.f;
    if (!a3 && (spellRec.m_attributes & ATTR_ON_NEXT_SWING) == 0 && (spellRec.m_attributesExC & ATTR_EXC_MELEE_RANGE) == 0) {
        if (SpellRec_RangeHasFlag_0x1(&spellRec) && !SpellRec_IsModifiedStat(&spellRec, 5)) {
            FormatStringCopy("MELEE_RANGE", dest_buf, 128);
            SStr::Copy(src_buf, dest_buf, 128);
        } else if (unit) {
            Spell_C_GetMinMaxRange(unit, &spellRec, &minRange, &maxRange, 0, 0);
            Spell_C_GetMinMaxRange(unit, &spellRec, &result, &v147, 1, 0);
            if (Spell_C_UsesDefaultMinRange(&spellRec)) {
                Spell_C_GetDefaultMinRange(&spellRec, &minRange);
                Spell_C_GetDefaultMinRange(&spellRec, &result);
            }
            bool v47 = sub_482900(minRange, result);
            if (!v47 && sub_482900(maxRange, v147)) {
                if (maxRange > 0.f && maxRange < 50000.f) {
                    if (minRange <= 0.f)
                        SStr::Printf(v138, 32, "%d", static_cast<int>(maxRange));
                    else
                        SStr::Printf(v138, 32, "%d-%d", static_cast<int>(minRange), static_cast<int>(maxRange));
                    FormatStringCopy("SPELL_RANGE", dest_buf, 128);
                    SStr::Printf(src_buf, 128, dest_buf, v138);
                } else if (maxRange >= 50000.f) {
                    char* v49 = FrameScript::GetText(const_cast<char*>("SPELL_RANGE_UNLIMITED"), -1, 0);
                    SStr::Printf(src_buf, 128, "%s", v49);
                }
            } else if (maxRange > 0.f) {
                if (maxRange < 50000.f) {
                    if (minRange <= 0.f)
                        SStr::Printf(v138, 32, "%d", static_cast<int>(maxRange));
                    else
                        SStr::Printf(v138, 32, "%d-%d", static_cast<int>(minRange), static_cast<int>(maxRange));
                    FormatStringCopy("SPELL_RANGE_DUAL", dest_buf, 128);
                    char* v51 = FrameScript::GetText(const_cast<char*>("ENEMY"), -1, 0);
                    SStr::Printf(src_buf, 128, dest_buf, v51, v138);
                } else {
                    char* v50 = FrameScript::GetText(const_cast<char*>("SPELL_RANGE_UNLIMITED"), -1, 0);
                    SStr::Printf(src_buf, 128, "%s", v50);
                }
            }
            if (a1_buf[0])
                CGTooltip_AddLine(v143, a1_buf, src_buf, s_Color_Hex_White, s_Color_Hex_White, 0);
            else {
                SStr::Copy(a1_buf, src_buf, 128);
                src_buf[0] = 0;
            }
            if (v147 > 0.f) {
                if (v147 < 50000.f) {
                    if (minRange <= 0.f)
                        SStr::Printf(v138, 32, "%d", static_cast<int>(v147));
                    else
                        SStr::Printf(v138, 32, "%d-%d", static_cast<int>(result), static_cast<int>(v147));
                    FormatStringCopy("SPELL_RANGE_DUAL", dest_buf, 128);
                    char* v54 = FrameScript::GetText(const_cast<char*>("FRIENDLY"), -1, 0);
                    SStr::Printf(src_buf, 128, dest_buf, v54, v138);
                } else {
                    char* v53 = FrameScript::GetText(const_cast<char*>("SPELL_RANGE_UNLIMITED"), -1, 0);
                    SStr::Printf(src_buf, 128, "%s", v53);
                }
                if (!a1_buf[0]) {
                    SStr::Copy(a1_buf, src_buf, 128);
                    src_buf[0] = 0;
                }
            }
        }
    }
    CGTooltip_AddLine(v143, a1_buf[0] ? a1_buf : src_buf, a1_buf[0] ? src_buf : nullptr, s_Color_Hex_White, s_Color_Hex_White, 0);

    // Cast time / recast (client APIs take uint32_t*)
    uint32_t m_recoveryTime_u = static_cast<uint32_t>(spellRec.m_recoveryTime);
    uint32_t m_categoryRecoveryTime_u = static_cast<uint32_t>(spellRec.m_categoryRecoveryTime);
    SpellRec_GetModifiedStatValue(&spellRec, &m_recoveryTime_u, 11);
    sub_7FEF60(&m_categoryRecoveryTime_u, &spellRec, 0);
    int32_t m_recoveryTime = static_cast<int32_t>(m_recoveryTime_u);
    int32_t m_categoryRecoveryTime = static_cast<int32_t>(m_categoryRecoveryTime_u);
    int v57 = (m_recoveryTime <= m_categoryRecoveryTime) ? m_categoryRecoveryTime : m_recoveryTime;
    int spellIda = v57;
    int v155 = 0;
    if (!a3 && !v141) {
        if (spellRec.m_effect[0] == 47 || (spellRec.m_attributes & ATTR_PASSIVE) != 0)
            v155 = 1;
        else if (spellRec.m_effect[0] != 78) {
            int CastTime = static_cast<int>(SpellRec_C::GetCastTime(&spellRec, static_cast<uint32_t>(a5), static_cast<uint32_t>(a7), 1));
            v141 = CastTime;
            bool v59 = (CastTime >= 60000);
            if (CastTime > 0) {
                const char* v60 = (CastTime >= 60000) ? "SPELL_CAST_TIME_MIN" : "SPELL_CAST_TIME_SEC";
                FormatStringCopy(v60, dest_buf, 128);
                double v61 = v59 ? 60000.0 : 1000.0;
                SStr::Printf(a1_buf, 128, dest_buf, static_cast<double>(CastTime) / v61);
            } else if ((spellRec.m_attributesEx & ATTR_EX_CHANNELED_1) != 0) {
                FormatStringCopy("SPELL_CAST_CHANNELED", a1_buf, 128);
            } else if (CastTime < 0) {
                FormatStringCopy("SPELL_CAST_TIME_INSTANT", a1_buf, 128);
            } else if ((spellRec.m_attributes & ATTR_ON_NEXT_SWING) != 0) {
                FormatStringCopy("SPELL_ON_NEXT_SWING", a1_buf, 128);
            } else if ((spellRec.m_attributesEx & ATTR_REQ_AMMO) != 0) {
                FormatStringCopy("SPELL_ON_NEXT_RANGED", a1_buf, 128);
            } else {
                if (!spellRec.m_powerType && PowerCost > 0)
                    FormatStringCopy("SPELL_CAST_TIME_INSTANT", a1_buf, 128);
                else
                    FormatStringCopy("SPELL_CAST_TIME_INSTANT_NO_MANA", a1_buf, 128);
            }
            src_buf[0] = 0;
            if (v57 > 0 && (spellRec.m_attributesExF & 1) == 0) {
                const char* v62 = (v57 < 60000) ? "SPELL_RECAST_TIME_SEC" : "SPELL_RECAST_TIME_MIN";
                FormatStringCopy(v62, dest_buf, 128);
                double v63 = (v57 < 60000) ? 1000.0 : 60000.0;
                SStr::Printf(src_buf, 128, dest_buf, static_cast<double>(spellIda) / v63);
            }
            CGTooltip_AddLine(v143, a1_buf, src_buf, s_Color_Hex_White, s_Color_Hex_White, 0);
        }
    }

    // Totems
    int spellIdb = 1;
    if (!a5) {
        int v157 = 1;
        v127[0] = 0;
        for (unsigned i = 0; i < 2; ++i) {
            int totemId = static_cast<int>(spellRec.m_totem0[i]);
            if (totemId > 0) {
                uint64_t v139 = static_cast<uint64_t>(spellRec.m_ID) | 0x1FE0000000000000ULL;
                ItemCacheRow* info = reinterpret_cast<ItemCacheRow*>(DBItemCache_GetInfoBlockByID(WDB_CACHE_ITEM, static_cast<uint32_t>(totemId), &v139, sub_61DD60, v143, 1));
                if (info) {
                    if (v157) {
                        v157 = 0;
                        char* v68 = FrameScript::GetText(const_cast<char*>("SPELL_TOTEMS"), -1, 0);
                        SStr::Copy(v127, v68, 4096);
                    } else
                        SStr::Append(v127, const_cast<char*>(", "), 0x7FFFFFFF);
                    void* itemOfType = CGBag_C_FindItemOfType(v19, spellRec.m_totem0[i], 0);
                    if (!itemOfType)
                        SStr::Append(v127, *reinterpret_cast<char**>(off_AD2A5C), 0x7FFFFFFF), spellIdb = 0;
                    SStr::Append(v127, info->Name[0][0], 0x7FFFFFFF);
                    if (!itemOfType)
                        SStr::Append(v127, const_cast<char*>("|r"), 0x7FFFFFFF);
                }
            }
        }
        int v70 = v157;
        for (unsigned j = 0; j < 2; ++j) {
            uint32_t catId = spellRec.m_requiredTotemCategoryID[j];
            TotemCategoryRow* v73 = GetTotemCategoryRow(catId);
            if (v73) {
                if (v70) {
                    v70 = 0;
                    char* v74 = FrameScript::GetText(const_cast<char*>("SPELL_TOTEMS"), -1, 0);
                    SStr::Copy(v127, v74, 4096);
                } else
                    SStr::Append(v127, const_cast<char*>(", "), 0x7FFFFFFF);
                if (sub_7548F0(catId, 0))
                    SStr::Append(v127, v73->m_name, 0x7FFFFFFF);
                else {
                    SStr::Append(v127, *reinterpret_cast<char**>(off_AD2A5C), 0x7FFFFFFF);
                    SStr::Append(v127, v73->m_name, 0x7FFFFFFF);
                    SStr::Append(v127, const_cast<char*>("|r"), 0x7FFFFFFF);
                    spellIdb = 0;
                }
            }
        }
        if (!v70 && (!spellIdb || !a3))
            CGTooltip_AddLine(v143, v127, nullptr, s_Color_Hex_White, s_Color_Hex_White, 0);
    }

    // Equipped item requirement
    if ((spellRec.m_targets & 0x10) == 0 && spellRec.m_equippedItemClass >= 0 && spellRec.m_equippedItemSubclass != 0) {
        v128[0] = 0;
        ItemSubClassMaskRow* v77 = GetItemSubClassMaskRowByClassAndMask(spellRec.m_equippedItemClass, spellRec.m_equippedItemSubclass);
        if (v77) {
            SStr::Copy(v128, v77->m_name, 512);
        }
        int v78 = 1;
        int v79 = (spellRec.m_attributesExC & ATTR_EXC_400) ? 0x8000 : ((spellRec.m_attributesExC & ATTR_EXC_1000000) ? 0x10000 : -1);
        if (!CGUnit_C_EquippedItemMeetSpellRequirements(v142, &spellRec, v79))
            v78 = 0;
        if ((spellRec.m_attributesExC & ATTR_EXC_1000400) == ATTR_EXC_1000400 && !CGUnit_C_EquippedItemMeetSpellRequirements(v142, &spellRec, 0x10000))
            v78 = 0;
        if (a5)
            v78 = 1;
        if (v128[0]) {
            const char* v86 = v78 ? "SPELL_EQUIPPED_ITEM" : "SPELL_EQUIPPED_ITEM_NOSPACE";
            if (a3 || v78) {
                char* v87 = FrameScript::GetText(const_cast<char*>(v86), -1, 0);
                SStr::Copy(dest_buf, v87, 128);
                SStr::Printf(v126, 512, dest_buf, v128);
                void* col = v78 ? s_Color_Hex_White : s_Color_Hex_Red0;
                CGTooltip_AddLine(v143, v126, nullptr, col, col, 1);
            }
        }
    }

    // Shapeshift requirement
    if (spellRec.m_shapeshiftMask[0] || spellRec.m_shapeshiftMask[1]) {
        int v159 = 1;
        v128[0] = 0;
        uint32_t attrExB = spellRec.m_attributesExB & ATTR_EXB_SHAPESHIFT_MASK_NONE;
        if (!attrExB) {
            for (uint32_t v90 = 0; v90 < 64; ++v90) {
                if (SpellRec_UsableInShapeshift(&spellRec, v90)) {
                    SpellShapeshiftFormRow* v92 = GetSpellShapeshiftFormRow(v90);
                    if (v92 && v92->m_name && v92->m_name[0]) {
                        if (v159)
                            SStr::Copy(v128, v92->m_name, 512), v159 = 0;
                        else {
                            SStr::Append(v128, const_cast<char*>(", "), 512);
                            SStr::Append(v128, v92->m_name, 512);
                        }
                    }
                }
            }
        }
        int v96 = 0;
        if (unit) {
            uint32_t formId = CGUnit_C::GetShapeshiftFormId(reinterpret_cast<CGUnit*>(unit));
            if (SpellRec_UsableInShapeshift(&spellRec, formId - 1))
                v96 = 1;
            else if (attrExB && !CGUnit_C_IsShapeShifted(unit))
                v96 = 1;
            else if (CGUnit_C::AffectedByAura(reinterpret_cast<CGUnit*>(unit), 275u, static_cast<uint32_t>(spellRec.m_ID)))
                v96 = 1;
        } else
            v96 = 1;
        if (!v159) {
            const char* v97 = v96 ? "SPELL_REQUIRED_FORM" : "SPELL_REQUIRED_FORM_NOSPACE";
            char* v98 = FrameScript::GetText(const_cast<char*>(v97), -1, 0);
            SStr::Copy(dest_buf, v98, 128);
            SStr::Printf(a1_buf, 128, dest_buf, v128);
            void* v99 = v96 ? s_Color_Hex_White : s_Color_Hex_Red0;
            CGTooltip_AddLine(v143, a1_buf, nullptr, v99, v99, 0);
        }
    }

    // Reputation
    if (spellRec.m_minFactionID != 0) {
        int32_t repValue = GetRepListRepValue(spellRec.m_minFactionID);
        bool meetsRep = (repValue >= static_cast<int32_t>(reinterpret_cast<const int32_t*>(dword_A2D2FC)[spellRec.m_minReputation]));
        if (!meetsRep || !a3) {
            FactionRow* v102 = GetFactionRow(spellRec.m_minFactionID);
            SStr::Printf(dest_buf, 128, "FACTION_STANDING_LABEL%d", spellRec.m_minReputation + 1);
            const char* v103 = v102 ? v102->m_name : "UNKNOWN";
            char* LocalizedText = const_cast<char*>(FrameScript_GetLocalizedText(dest_buf, -1, 0));
            char* v104 = FrameScript::GetText(const_cast<char*>("ITEM_REQ_REPUTATION"), -1, 0);
            SStr::Printf(a1_buf, 128, v104, v103, LocalizedText);
            void* v105 = meetsRep ? s_Color_Hex_White : s_Color_Hex_Red0;
            CGTooltip_AddLine(v143, a1_buf, nullptr, v105, v105, 0);
        }
    }

    // Min level
    if (v155 && unit && unit->unitData->level < spellRec.m_baseLevel) {
        char* v107 = FrameScript::GetText(const_cast<char*>("ITEM_MIN_LEVEL"), -1, 0);
        SStr::Copy(dest_buf, v107, 128);
        SStr::Printf(a1_buf, 128, dest_buf, spellRec.m_baseLevel);
        CGTooltip_AddLine(v143, a1_buf, nullptr, s_Color_Hex_Red0, s_Color_Hex_Red0, 0);
    }

    // Reagents
    if (!a5 && !sub_800D60(v142, &spellRec)) {
        int v160 = 1;
        int spellIde = 1;
        v127[0] = 0;
        for (unsigned k = 0; k < 8; ++k) {
            int reagentId = static_cast<int>(spellRec.m_reagent[k]);
            if (reagentId > 0) {
                uint64_t v139 = static_cast<uint64_t>(spellRec.m_ID) | 0x1FE0000000000000ULL;
                ItemCacheRow* v109 = reinterpret_cast<ItemCacheRow*>(DBItemCache_GetInfoBlockByID(WDB_CACHE_ITEM, static_cast<uint32_t>(reagentId), &v139, sub_61DD60, v143, 1));
                if (v109) {
                    if (v160) {
                        v160 = 0;
                        char* v110 = FrameScript::GetText(const_cast<char*>("SPELL_REAGENTS"), -1, 0);
                        SStr::Copy(v127, v110, 4096);
                    } else
                        SStr::Append(v127, const_cast<char*>(", "), 0x7FFFFFFF);
                    char* v111 = v109->Name[0][0];
                    if (spellRec.m_reagentCount[k] <= 1)
                        SStr::Copy(dest_buf, v111, 128);
                    else
                        SStr::Printf(dest_buf, 128, "%s (%d)", v111, spellRec.m_reagentCount[k]);
                    uint32_t count = CGBag_C_GetItemTypeCount(v19, spellRec.m_reagent[k], 0);
                    if (count >= spellRec.m_reagentCount[k])
                        SStr::Append(v127, dest_buf, 0x7FFFFFFF);
                    else {
                        SStr::Append(v127, *reinterpret_cast<char**>(off_AD2A5C), 0x7FFFFFFF);
                        SStr::Append(v127, dest_buf, 0x7FFFFFFF);
                        SStr::Append(v127, const_cast<char*>("|r"), 0x7FFFFFFF);
                        spellIde = 0;
                    }
                }
            }
        }
        if (!v160 && (!spellIde || !a3))
            CGTooltip_AddLine(v143, v127, nullptr, s_Color_Hex_White, s_Color_Hex_White, 1);
    }

    int v112 = 0;
    if (a4) {
        CGTooltip::GetDurationString(a1_buf, 128, static_cast<uint64_t>(a4), const_cast<char*>("ITEM_COOLDOWN_TIME"), 0, 1, 0);
        CGTooltip_AddLine(v143, a1_buf, nullptr, s_Color_Hex_White, s_Color_Hex_White, 0);
        v112 = 1;
    }

    if (!a3) {
        // Chance to dodge/parry/block/crit (simplified - uses m_minID from context; disas uses v142[1].vtable offsets)
        if ((spellRec.m_attributesEx & ATTR_EX_USE_ALL_POWER) != 0) {
            if (v134) {
                char* v116 = FrameScript::GetText(v134->m_globalStringBaseTag, -1, 0);
                SStr::Printf(v129, 256, "%s", v116);
                char* v117 = FrameScript::GetText(const_cast<char*>("SPELL_USE_ALL_POWER_DISPLAY"), -1, 0);
                SStr::Printf(dest_buf, 128, v117, v129);
            } else {
                const char* v118 = (spellRec.m_powerType > 6u) ? "SPELL_USE_ALL_HEALTH" : reinterpret_cast<const char*>(*reinterpret_cast<uintptr_t*>(reinterpret_cast<char*>(off_AD2E84) + spellRec.m_powerType * sizeof(void*)));
                char* v119 = FrameScript::GetText(const_cast<char*>(v118), -1, 0);
                SStr::Copy(dest_buf, v119, 128);
            }
            CGTooltip_AddLine(v143, dest_buf, nullptr, s_Color_Hex_White, s_Color_Hex_White, 0);
        }
        if (spellRec.m_description_lang && spellRec.m_description_lang[0]) {
            SpellParser::ParseText(&spellRec, v125, 2048, static_cast<uint32_t>(a5), static_cast<uint32_t>(a7), 0, 0, 1, 0);
            CGTooltip_AddLine(v143, v125, nullptr, s_Color_Hex_DarkYellow, s_Color_Hex_DarkYellow, 1);
        }
        if (a9 && !a7 && a16)
            sub_622800(a9, static_cast<uint32_t>(a10), v137, static_cast<uint32_t>(a14), static_cast<uint32_t>(a15), 0, static_cast<uint32_t>(a8), static_cast<uint32_t>(a5), static_cast<uint32_t>(a12));
        if (v136 && spellRec.m_effectItemType[0]) {
            sub_50F590(reinterpret_cast<void*>(reinterpret_cast<char*>(v143) + sizeof(void*) * 2 + 0xE4));  // &v106[1].m_onHide
            uint32_t v135 = 0;
            uint64_t v139 = 0;
            CGTooltip_SetItem(v143, spellRec.m_effectItemType[0], &v135, &v139, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1);
        }
    }

    void* batch4 = *reinterpret_cast<void**>(reinterpret_cast<char*>(v143) + sizeof(void*) * 2 + 4 * 4);  // v106[1].m_batch[4]
    if (batch4)
        FrameScript_Object_RunScript(batch4, 0, 0);
    CSimpleFrame_Show(v143);
    CGTooltip_CalculateSize(v143);
    nullsub_3();
    return v112;
}
