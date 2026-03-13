#include <CDBCMgr/CDBCMgr.h>
#include <CDBCMgr/CDBCDefs/SpellAdditionalCostData.h>
#include <CDBCMgr/CDBCDefs/SpellEffectScalars.h>
#include <Character/CharacterExtensions.h>
#include <Tooltip/SpellTooltipExtensions.h>
#include <Logger.h>
#include <SpellAttrDefines.h>

#include <Windows.h>
#include <algorithm>

void TooltipExtensions::Apply() {
    SpellTooltipVariableExtension();
    SpellTooltipSetSpellExtension();
    //SpellTooltipPowerCostExtension();
    //SpellTooltipCooldownExtension();
    //SpellTooltipRemainingCooldownExtension();
}

void TooltipExtensions::SpellTooltipVariableExtension() {
    DWORD flOldProtect = 0;

    // change pointer to table with variables
    Util::OverwriteUInt32AtAddress(0x576B63, reinterpret_cast<uint32_t>(&spellVariables));
    // update number of entries value
    Util::OverwriteUInt32AtAddress(0x576B7C, (sizeof(spellVariables) / 4));
    // copy table of pointers from address to spellVariables vector and add new entries
    memcpy(&spellVariables, (const void*)0xACE8F8, sizeof(uint32_t) * 140);
    SetNewVariablePointers();
    // change pointer of GetVariableTableValue to pointer to extended function
    Util::OverwriteUInt32AtAddress(0x578E8B, Util::CalculateAddress(reinterpret_cast<uint32_t>(&GetVariableValueEx), 0x578E8F));
}

void GetSpellScalarsForEffect(int SpellId, int idx, float& ap, float& sp, float& bv)
{
    auto scalars = GlobalCDBCMap.getCDBC("SpellEffectScalars");
    for (auto scalar : scalars)
    {
        if (SpellEffectScalarsRow* row = std::any_cast<SpellEffectScalarsRow>(&scalar.second))
        {
            if (row->spellID == SpellId && row->effectIdx == idx) {
                ap += row->ap;
                sp += row->sp;
                bv += row->bv;

                break;
            }
        }
    }
}

float ApplyScalarsForPlayer(CGPlayer* activePlayer, SpellRow* spell, int index, float ap, float sp, float bv)
{
    float total = 0.0;
    if (ap) {
        uint8_t attType = (spell->m_equippedItemClass == 2 && spell->m_equippedItemSubclass & 262156 && spell->m_defenseType != 2) ? 2 : spell->m_attributesExC & SPELL_ATTR3_MAIN_HAND ? 0 : 1;
        total += ap * CharacterDefines::GetTotalAttackPowerValue(attType, activePlayer);
    }
    if (sp) {
        int32_t spBonus = 0;
        uint32_t schoolMask = spell->m_schoolMask;
        for (uint32_t i = SPELL_SCHOOL_HOLY; i < MAX_SPELL_SCHOOL; ++i) {
            if (schoolMask & (1 << i)) {
                int32_t tempBonus = activePlayer->PlayerData->SPPos[i];
                if (tempBonus > spBonus)
                    spBonus = tempBonus;
            }
        }
        total += sp * spBonus;
    }
    if (bv) {
        total += bv * static_cast<float>(activePlayer->PlayerData->shieldBlock);
    }

    return total;
}

// Client-side color pointers used by tooltip rendering helpers.
static void* const sColorHexWhite      = reinterpret_cast<void*>(0xAD2D30);
static void* const sColorHexGrey0      = reinterpret_cast<void*>(0xAD2D38);
static void* const sColorHexDarkYellow = reinterpret_cast<void*>(0xAD2D2C);

int TooltipExtensions::GetVariableValueEx(void* _this, uint32_t edx, uint32_t spellVariable, uint32_t a3, SpellRow* spell, uint32_t a5, uint32_t a6, uint32_t a7, uint32_t a8, uint32_t a9) {
    uint32_t result = 0;

    if (spellVariable < SPELLVARIABLE_hp)
        result = CFormula::GetVariableValue(_this, spellVariable, a3, spell, a5, a6, a7, a8, a9);
    else {
        float value = 0.f;
        CGPlayer* activePlayer = reinterpret_cast<CGPlayer*>(ClntObjMgr::ObjectPtr(ClntObjMgr::GetActivePlayer(), TYPEMASK_PLAYER));

        if (activePlayer) {
            // Arrays for current and max power fields
            if (spellVariable >= SPELLVARIABLE_power1 && spellVariable <= SPELLVARIABLE_power7) {
                uint32_t var = spellVariable - SPELLVARIABLE_power1;
                value = static_cast<float>(activePlayer->unitBase.unitData->unitCurrPowers[var]);
            }
            else if (spellVariable >= SPELLVARIABLE_POWER1 && spellVariable <= SPELLVARIABLE_POWER7) {
                uint32_t var = spellVariable - SPELLVARIABLE_POWER1;
                value = static_cast<float>(activePlayer->unitBase.unitData->unitMaxPowers[var]);
            }
            else {
                switch (spellVariable) {
                    case SPELLVARIABLE_hp:
                        value = static_cast<float>(activePlayer->unitBase.unitData->unitCurrHealth);
                        break;
                    case SPELLVARIABLE_HP:
                        value = static_cast<float>(activePlayer->unitBase.unitData->unitMaxHealth);
                        break;
                    case SPELLVARIABLE_ppl1:
                        value = spell->m_effectRealPointsPerLevel[0];
                        break;
                    case SPELLVARIABLE_ppl2:
                        value = spell->m_effectRealPointsPerLevel[1];
                        break;
                    case SPELLVARIABLE_ppl3:
                        value = spell->m_effectRealPointsPerLevel[2];
                        break;
                    case SPELLVARIABLE_PPL1:
                        value = spell->m_effectRealPointsPerLevel[0] * activePlayer->unitBase.unitData->level;
                        break;
                    case SPELLVARIABLE_PPL2:
                        value = spell->m_effectRealPointsPerLevel[1] * activePlayer->unitBase.unitData->level;
                        break;
                    case SPELLVARIABLE_PPL3:
                        value = spell->m_effectRealPointsPerLevel[2] * activePlayer->unitBase.unitData->level;
                        break;
                    case SPELLVARIABLE_mastery1:
                        value = CharacterDefines::getMasteryForSpec(0);
                        break;
                    case SPELLVARIABLE_mastery2:
                        value = CharacterDefines::getMasteryForSpec(1);
                        break;
                    case SPELLVARIABLE_mastery3:
                        value = CharacterDefines::getMasteryForSpec(2);
                        break;
                    case SPELLVARIABLE_mastery4:
                        value = CharacterDefines::getMasteryForSpec(3);
                        break;
                    case SPELLVARIABLE_MASTERY:
                        value = CharacterDefines::getMasteryRatingSpec(CharacterExtensions::SpecToIndex(CharacterDefines::getCharActiveSpec()));
                        break;
                    case SPELLVARIABLE_dpct:
                        value = activePlayer->PlayerData->dodgePct;
                        break;
                    case SPELLVARIABLE_ppct:
                        value = activePlayer->PlayerData->parryPct;
                        break;
                    case SPELLVARIABLE_bon1: {
                        float ap = 0.0;
                        float sp = 0.0;
                        float bv = 0.0;
                        GetSpellScalarsForEffect(spell->m_ID, 0, ap, sp, bv);
                        value = ApplyScalarsForPlayer(activePlayer, spell, 0, ap, sp, bv);
                        value += spell->m_effectRealPointsPerLevel[0] * activePlayer->unitBase.unitData->level;
                    } break;
                    case SPELLVARIABLE_bon2: {
                        float ap = 0.0;
                        float sp = 0.0;
                        float bv = 0.0;
                        GetSpellScalarsForEffect(spell->m_ID, 1, ap, sp, bv);
                        value = ApplyScalarsForPlayer(activePlayer, spell, 1, ap, sp, bv);
                        value += spell->m_effectRealPointsPerLevel[1] * activePlayer->unitBase.unitData->level;
                    }
                    break;
                    case SPELLVARIABLE_bon3: {
                        float ap = 0.0;
                        float sp = 0.0;
                        float bv = 0.0;
                        GetSpellScalarsForEffect(spell->m_ID, 2, ap, sp, bv);
                        value = ApplyScalarsForPlayer(activePlayer, spell, 2, ap, sp, bv);
                        value += spell->m_effectRealPointsPerLevel[2] * activePlayer->unitBase.unitData->level;
                    }
                    break;
                    default:
                        *reinterpret_cast<uint32_t*>(_this) = 1;
                        break;
                }
            }
        }

        result = a3;
        uint32_t* offset = reinterpret_cast<uint32_t*>(a3 + 128);
        --*offset;
        *reinterpret_cast<float*>(a3 + 4 * *offset) = value;
    }

    return result;
}

void TooltipExtensions::SetNewVariablePointers() {
    const char* tooltipSpellVariablesExtensions[] = {
        "hp", "HP", "ppl1", "ppl2", "ppl3", "PPL1", "PPL2", "PPL3",
        "power1", "power2", "power3", "power4", "power5", "power6", "power7",
        "POWER1", "POWER2", "POWER3", "POWER4", "POWER5", "POWER6", "POWER7",
        "mastery1", "mastery2", "mastery3", "mastery4", "MASTERY",
        "dpct", "ppct", "bon1", "bon2", "bon3"
    };

    for (size_t i = 0; i < sizeof(tooltipSpellVariablesExtensions) / sizeof(tooltipSpellVariablesExtensions[0]); i++)
        spellVariables[140 + i] = reinterpret_cast<uint32_t>(tooltipSpellVariablesExtensions[i]);
}

void TooltipExtensions::SpellTooltipPowerCostExtension() {
    uint8_t patchBytes[] = {
        0x57, 0x51, 0x56, 0x8B, 0x4D, 0x2C, 0x51, 0x8D, 0x95, 0x78, 0xFB, 0xFF, 0xFF, 0x8D, 0x8D, 0x20,
        0xFF, 0xFF, 0xFF, 0x52, 0x51, 0xE8, 0xFC, 0xFF, 0x00, 0x00, 0xE9, 0x3A, 0x01, 0x00, 0x00
    };

    Util::OverwriteBytesAtAddress(0x623D8A, patchBytes, sizeof(patchBytes));
    Util::OverwriteUInt32AtAddress(0x623DA0, Util::CalculateAddress(reinterpret_cast<uint32_t>(&SetPowerCostTooltip), 0x623DA4));
}

void TooltipExtensions::SetPowerCostTooltip(char* dest, SpellRow* spell, uint32_t powerCost, uint32_t powerCostPerSec, char* powerString, PowerDisplayRow* powerDisplayRow) {
    SpellAdditionalCostDataRow* additionalCostRow = GlobalCDBCMap.getRow<SpellAdditionalCostDataRow>("SpellAdditionalCostData", spell->m_ID);
    SpellAdditionalAttributesRow* customAttributesRow = GlobalCDBCMap.getRow<SpellAdditionalAttributesRow>("SpellAdditionalAttributes", spell->m_ID);
    bool hasPowerCost = (powerCost != 0 || powerCostPerSec != 0);
    char buffer[128];

    if (!customAttributesRow || !(customAttributesRow->customAttr2 & SPELL_ATTR2_CU_DO_NOT_DISPLAY_POWER_COST)) {
        if (powerCost && !powerCostPerSec) {
            if (powerDisplayRow) {
                SStr::Copy(buffer, FrameScript::GetText(powerDisplayRow->m_globalStringBaseTag, -1, 0), 128);
                SStr::Printf(dest, 128, FrameScript::GetText("POWER_DISPLAY_COST", -1, 0), powerCost, buffer);
            }
            else {
                SStr::Copy(buffer, FrameScript::GetText(powerString, -1, 0), 128);
                SStr::Printf(dest, 128, buffer, powerCost);
            }
        }
        else if (powerCostPerSec > 0) {
            if (powerDisplayRow) {
                SStr::Copy(buffer, FrameScript::GetText(powerDisplayRow->m_globalStringBaseTag, -1, 0), 128);
                SStr::Printf(dest, 128, FrameScript::GetText("POWER_DISPLAY_COST_PER_TIME", -1, 0), powerCost, buffer, powerCostPerSec);
            }
            else {
                SStr::Printf(buffer, 128, "%s_PER_TIME", powerString);
                SStr::Printf(dest, 128, FrameScript::GetText(buffer, -1, 0), powerCost, powerCostPerSec);
            }
        }

        if (additionalCostRow && additionalCostRow->cost) {
            if (hasPowerCost)
                SStr::Append(dest, " + ", 0x7FFFFFFF);

            SStr::Printf(buffer, 128, "%d %s", additionalCostRow->cost, additionalCostRow->resourceName);
            SStr::Append(dest, buffer, 0x7FFFFFFF);

            if (additionalCostRow->flag == 1 && additionalCostRow->cost != 1)
                SStr::Append(dest, sPluralS, 0x7FFFFFFF);
        }
    }
}

void TooltipExtensions::SpellTooltipCooldownExtension() {
    uint8_t patchBytes[] = {
        0x8B, 0x4D, 0x2C, 0x8B, 0x45, 0xE4, 0x51, 0x50, 0x8D, 0x8D, 0x20, 0xFE, 0xFF, 0xFF, 0x51, 0x8B,
        0x55, 0x1C, 0x8B, 0x45, 0x14, 0x52, 0x50, 0x8D, 0x4D, 0x18, 0x51, 0x8D, 0x95, 0x78, 0xFB, 0xFF,
        0xFF, 0x8D, 0x8D, 0x20, 0xFF, 0xFF, 0xFF, 0x52, 0x51, 0xE8, 0x00, 0x00, 0x00, 0x00, 0xE9, 0xD8,
        0x01, 0x00, 0x00
    };

    Util::OverwriteBytesAtAddress(0x62443B, patchBytes, sizeof(patchBytes));
    Util::OverwriteUInt32AtAddress(0x624465, Util::CalculateAddress(reinterpret_cast<uint32_t>(&SetSpellCooldownTooltip), 0x624469));
}

void TooltipExtensions::SetSpellCooldownTooltip(char* dest, SpellRow* spell, uintptr_t* a7, uint32_t a6, uint32_t a8, char* src, void* _this, uint32_t powerCost) {
    const uint32_t MILLISECONDS_IN_MINUTE = 60000;
    const uint32_t MILLISECONDS_IN_SECOND = 1000;

    if (spell->m_effect[0] == SPELL_EFFECT_TRADE_SKILL || (spell->m_attributes & SPELL_ATTR0_PASSIVE) != 0) {
        *a7 = 1;
        return;
    }
    if (spell->m_effect[0] == SPELL_EFFECT_ATTACK) {
        return;
    }

    char buffer[128];
    SpellAdditionalAttributesRow* customAttributesRow = GlobalCDBCMap.getRow<SpellAdditionalAttributesRow>("SpellAdditionalAttributes", spell->m_ID);
    double castTime     = SpellRec_C::GetCastTime(spell, a6, a8, 1);
    bool treatAsInstant = castTime <= 250 ? HasAttribute(spell, SPELL_ATTR2_CU_LOW_TIME_TREAT_AS_INSTANT) :
                                                HasAttribute(spell, SPELL_ATTR2_CU_TREAT_AS_INSTANT);

    if (castTime && !treatAsInstant) {
        bool isLongCast = castTime >= MILLISECONDS_IN_MINUTE;
        char* castFlag = isLongCast ? "SPELL_CAST_TIME_MIN" : "SPELL_CAST_TIME_SEC";
        double divisor = isLongCast ? MILLISECONDS_IN_MINUTE : MILLISECONDS_IN_SECOND;

        SStr::Copy(buffer, FrameScript::GetText(castFlag, -1, 0), 128);
        SStr::Printf(dest, 128, buffer, castTime / divisor);
    } else {
        char* castFlag = "SPELL_CAST_TIME_INSTANT_NO_MANA";
        if ((spell->m_attributesEx & (SPELL_ATTR1_CHANNELED_1 | SPELL_ATTR1_CHANNELED_2)) != 0) {
            castFlag = "SPELL_CAST_CHANNELED";
        }
        else if ((spell->m_attributes & (SPELL_ATTR0_ON_NEXT_SWING | SPELL_ATTR0_ON_NEXT_SWING_2)) != 0) {
            castFlag = "SPELL_ON_NEXT_SWING";
        }
        else if ((spell->m_attributesEx & SPELL_ATTR0_REQ_AMMO) != 0) {
            castFlag = "SPELL_ON_NEXT_RANGED";
        }
        else if (!spell->m_powerType && powerCost > 0) {
            castFlag = "SPELL_CAST_TIME_INSTANT";
        }   
        SStr::Copy(dest, FrameScript::GetText(castFlag, -1, 0), 128);
    }

    double recoveryTime = 0;
    auto it = CharacterDefines::spellChargeMap.find(spell->m_ID);

    auto isCharged = false;
    if (it != CharacterDefines::spellChargeMap.end()) {
        CharacterDefines::SpellCharge temp = it->second;
        recoveryTime = temp.cooldown;
        isCharged = temp.maxCharges > 1;
    }
    else
        recoveryTime = spell->m_categoryRecoveryTime > spell->m_recoveryTime ? spell->m_categoryRecoveryTime : spell->m_recoveryTime;

    if (recoveryTime > 0) {
        bool isLongRecovery = recoveryTime >= MILLISECONDS_IN_MINUTE;
        char* str = isLongRecovery ? "SPELL_RECAST_TIME_MIN" : "SPELL_RECAST_TIME_SEC"; // sets to % min/sec
        double divider = isLongRecovery ? MILLISECONDS_IN_MINUTE : MILLISECONDS_IN_SECOND;

        SStr::Copy(buffer, FrameScript::GetText(str, -1, 0), 128);
        if (isCharged) {
            SStr::Append(buffer, FrameScript::GetText("SPELL_RECAST_RECHARGE", -1, 0), 128);
        }
        else {
            SStr::Append(buffer, FrameScript::GetText("SPELL_RECAST_COOLDOWN", -1, 0), 128);
        }
        SStr::Printf(src, 128, buffer, recoveryTime / divider);
    }
    else {
        *src = 0;
    }

    void* ptr = reinterpret_cast<void*>(0xAD2D30);
    sub_61FEC0(_this, dest, src, ptr, ptr, 0);
}

void TooltipExtensions::SpellTooltipRemainingCooldownExtension() {
    uint8_t patchBytes[] = {
        0x8B, 0x45, 0x10, 0x89, 0xF9, 0x8D, 0x95, 0x78, 0xFB, 0xFF, 0xFF, 0x50, 0x51, 0x52, 0x8D, 0x95,
        0x20, 0xFF, 0xFF, 0xFF, 0x52, 0xE8, 0x00, 0x00, 0x00, 0x00, 0x31, 0xDB, 0xBB, 0x01, 0x00, 0x00,
        0x00, 0xEB, 0x24
    };

    Util::OverwriteBytesAtAddress(0x624FF0, patchBytes, sizeof(patchBytes));
    Util::OverwriteUInt32AtAddress(0x625006, Util::CalculateAddress(reinterpret_cast<uint32_t>(&SetSpellRemainingCooldownTooltip), 0x62500A));
}

void TooltipExtensions::SetSpellRemainingCooldownTooltip(char* dest, SpellRow* spell, void* _this, uint32_t currentCooldown) {
    void* ptr = reinterpret_cast<void*>(0xAD2D30);
    uint32_t recoveryTime = 0;
    auto it = CharacterDefines::spellChargeMap.find(spell->m_ID);

    if (it != CharacterDefines::spellChargeMap.end())
    {
        CharacterDefines::SpellCharge temp = it->second;
        if (temp.remainingCooldown >= currentCooldown) {
            uint32_t currAsync = OsGetAsyncTimeMs();

            if (temp.remainingCooldown > (currAsync - temp.async))
                recoveryTime = temp.remainingCooldown + (temp.async - currAsync);
            else
                recoveryTime = 0;

            temp.remainingCooldown = recoveryTime;
            temp.async = currAsync;
            it->second = temp;
        }
        else
            recoveryTime = currentCooldown;
    }
    else
        recoveryTime = currentCooldown;

    if (recoveryTime) {
        CGTooltip::GetDurationString(dest, 128, recoveryTime, "ITEM_COOLDOWN_TIME", 0, 1, 0);
        sub_61FEC0(_this, dest, 0, ptr, ptr, 0);
    }
}

void TooltipExtensions::SpellTooltipSetSpellExtension() {
    // Overwrite the CGTooltip__SetSpell entry with a call into our fastcall hook.
    // 0x006238A0 is the original address of CGTooltip__SetSpell.
    // We install a small stub: pushad/save, move ecx/edx as-is, and call SetSpellTooltipHook.
    // For safety and simplicity here, we patch the entry point to an absolute call to our hook
    // followed by a return with the original stack-cleanup size (0x3C bytes).

    uint8_t patchBytes[] = {
        0xE9,0,0,0,0
    };

    // Write the bytes first, then fix up the relative call target.
    Util::OverwriteBytesAtAddress(0x6238A0, patchBytes, sizeof(patchBytes));
    Util::OverwriteUInt32AtAddress(
        0x6238A1,
        Util::CalculateAddress(reinterpret_cast<uint32_t>(&SetSpellTooltipHook), 0x6238A5));
}

int __fastcall TooltipExtensions::SetSpellTooltipHook(
    void* thisPtr,
    void* edx,
    int spellId,
    int a3,
    int a4,
    int a5,
    int a6,
    int a7,
    int a8,
    uint32_t* a9,
    int a10,
    int a11,
    int a12,
    int a13,
    int a14,
    int a15,
    int a16)
{
    return SetSpellTooltipImpl(
        thisPtr,
        spellId,
        a3,
        a4,
        a5,
        a6,
        a7,
        a8,
        a9,
        a10,
        a11,
        a12,
        a13,
        a14,
        a15,
        a16);
}

int TooltipExtensions::SetSpellTooltipImpl(
    void* tooltip,
    int spellId,
    int a3,
    int a4,
    int a5,
    int a6,
    int a7,
    int a8,
    uint32_t* a9,
    int a10,
    int a11,
    int a12,
    int a13,
    int a14,
    int a15,
    int a16)
{
    // Early out if we do not have a tooltip object.
    if (!tooltip)
        return 0;

    // Clear tooltip if this is not an update call.
    if (!a11) {
        CGTooltipInternal::ClearTooltip(tooltip);
    }

    // Resolve active player and unit context.
    CGPlayer* activePlayer = reinterpret_cast<CGPlayer*>(
        ClntObjMgr::ObjectPtr(ClntObjMgr::GetActivePlayer(), TYPEMASK_PLAYER));
    if (!activePlayer) {
        CSimpleFrame::Hide(tooltip);
        return 0;
    }

    CGUnit* unit = &activePlayer->unitBase;
    if (a7) {
        // target unit from stru_C24220 – not currently exposed; fall back to player.
        unit = &activePlayer->unitBase;
    } else if (a5) {
        uint64_t petGuid = CGPetInfo_C::GetPet(0);
        unit = reinterpret_cast<CGUnit*>(ClntObjMgr::ObjectPtr(petGuid, TYPEMASK_UNIT));
        if (!unit)
            unit = &activePlayer->unitBase;
    } else {
        unit = &activePlayer->unitBase;
    }

    // Fetch spell row from the client spell DB (matches original CGTooltip__SetSpell).
    WoWClientDB* spellDB = reinterpret_cast<WoWClientDB*>(0x00AD49D0); // g_spellDB
    SpellRow spellRow{};
    if (!ClientDB::GetLocalizedRow(spellDB, static_cast<uint32_t>(spellId), &spellRow)) {
        if (!a11) {
            CSimpleFrame::Hide(tooltip);
        }
        return 0;
    }
    SpellRow* spell = &spellRow;

    // Basic line buffers.
    char lineLeft[128] = {};
    char lineRight[128] = {};

    // Top name line: spell name and optional subtext.
    {
        char* name = spell->m_name_lang;
        char* subText = nullptr;
        if (a3 || a6) {
            subText = spell->m_nameSubtext_lang;
        }

        if (name && *name) {
            SStr::Copy(lineLeft, name, sizeof(lineLeft));
            if (subText && *subText) {
                SStr::Copy(lineRight, subText, sizeof(lineRight));
            } else {
                lineRight[0] = 0;
            }

            sub_61FEC0(
                tooltip,
                lineLeft,
                lineRight[0] ? lineRight : nullptr,
                sColorHexWhite,
                sColorHexGrey0,
                0);
        }
    }

    // Cost and range on the same row (left = cost, right = range). Matches disas.txt lines 280-364.
    {
        char costStr[128] = {};
        char rangeStr[128] = {};

        uint32_t powerCost = Spell_C::GetPowerCost(spell, unit);
        if (!a3) {
            uint32_t powerCostPerSec = Spell_C::GetPowerCostPerSecond(spell, unit);
            uint32_t divisor = Unit_C::GetPowerDivisor(spell->m_powerType);
            if (divisor > 1) {
                powerCost /= divisor;
                powerCostPerSec /= divisor;
            }

            // disas 289: if ( spellRec.m_powerType != 5 ) -> power cost; else -> rune cost (330-364).
            if (spell->m_powerType == POWER_RUNES) {
                // disas 331-334: v36 = g_spellRuneCostDB.b_base_02.m_recordsById[spellRec.m_runeCostID - g_spellRuneCostDB.b_base_01.b_base.m_minID]
                if (spell->m_runeCostID != 0) {
                    const uintptr_t runeCostDB = 0x00AD49AC;
                    // WowClientDB layout: b_base_01 at +0 (WowClientDB_Base: m_maxID +12, m_minID +16), b_base_02 at +0x14 (m_recordsById at +8).
                    uint32_t runeMinID = *reinterpret_cast<uint32_t*>(runeCostDB + 16);
                    uint32_t runeMaxID = *reinterpret_cast<uint32_t*>(runeCostDB + 12);
                    LOG_DEBUG << "SpellRuneCost: runeCostID=" << spell->m_runeCostID << " minID=" << runeMinID << " maxID=" << runeMaxID;

                    if (spell->m_runeCostID >= runeMinID && spell->m_runeCostID <= runeMaxID) {
                        uint32_t runeIndex = spell->m_runeCostID - runeMinID;
                        SpellRuneCostRow** table =
                            *reinterpret_cast<SpellRuneCostRow***>(runeCostDB + 0x20);
                        
                        SpellRuneCostRow* runeRow = nullptr;
                        if (table)
                            runeRow = table[runeIndex];

                        if (runeRow) {
                            LOG_DEBUG << "RowID=" << runeRow->m_ID
                                << " blood=" << runeRow->m_blood
                                << " unholy=" << runeRow->m_unholy
                                << " frost=" << runeRow->m_frost;

                            char runeBuff[128] = {};
                            if (runeRow->m_blood) {
                                char* txt = FrameScript::GetText(const_cast<char*>("RUNE_COST_DEATH"), -1, 0);
                                SStr::Printf(runeBuff, 128, txt, runeRow->m_blood);
                                SStr::Append(costStr, runeBuff, 0x7FFFFFFF);
                                if (runeRow->m_blood > 1)
                                    SStr::Append(costStr, sPluralS, 0x7FFFFFFF);

                                if (runeRow->m_runicPower < 0) {
                                    int32_t m_Amount = -runeRow->m_runicPower / 10;
                                    SStr::Append(costStr, sConnectorPlus, 0x7FFFFFFF);
                                    char* txt = FrameScript::GetText(const_cast<char*>("RUNIC_POWER_COST"), -1, 0);
                                    SStr::Printf(runeBuff, 128, txt, m_Amount);
                                    SStr::Append(costStr, runeBuff, 0x7FFFFFFF);
                                }
                            }
                        }
                    }
                }
            } else {
                // Non-rune: PowerCost > 0 and v34<=0 -> one-off (POWER_DISPLAY_COST or v35); else per-time or skip (disas 291-328).
                // PowerDisplay: index = m_powerDisplayID - minID (disas 269-276).
                if (powerCost > 0 || powerCostPerSec > 0) {
                    const uintptr_t powerDisplayDB = 0x00AD43A0;
                    const uintptr_t powerBase = powerDisplayDB + 4;
                    uint32_t powerMinID = *reinterpret_cast<uint32_t*>(powerBase + 16);
                    uint32_t powerMaxID = *reinterpret_cast<uint32_t*>(powerBase + 12);
                    PowerDisplayRow* powerDisplayRow = nullptr;
                    if (spell->m_powerDisplayID >= powerMinID && spell->m_powerDisplayID <= powerMaxID) {
                        uint32_t powerIndex = spell->m_powerDisplayID - powerMinID;
                        powerDisplayRow = reinterpret_cast<PowerDisplayRow*>(
                            ClientDB::GetRow(reinterpret_cast<void*>(powerDisplayDB), powerIndex));
                    }
                    const char* powerStringKey = "HEALTH_COST";
                    if (spell->m_powerType <= POWER_RUNIC_POWER) {
                        static const char* powerKeys[] = {
                            "MANA_COST", "RAGE_COST", "FOCUS_COST", "ENERGY_COST",
                            "HAPPINESS_COST", "RUNE_COST", "RUNIC_POWER_COST", "UNKNOWN",
                        };
                        powerStringKey = powerKeys[spell->m_powerType];
                    }
                    SetPowerCostTooltip(
                        costStr,
                        spell,
                        powerCost,
                        powerCostPerSec,
                        const_cast<char*>(powerStringKey),
                        powerDisplayRow);
                }
            }
        }

        // Build range in rangeStr. Skip when a3 or certain attributes; use MELEE_RANGE for range index 1 (5 yd).
        bool skipRange = a3 || (spell->m_attributes & 0x404) != 0 || (spell->m_attributesExC & 0x40000000) != 0;
        if (!skipRange) {
            if (SpellRec_RangeHasFlag_0x1(spell) && !SpellRec_IsModifiedStat(spell, 5)) {
                const char* melee = FrameScript::GetText(const_cast<char*>("MELEE_RANGE"), -1, 0);
                if (melee) SStr::Copy(rangeStr, const_cast<char*>(melee), sizeof(rangeStr));
            } else {
                float minRange = 0.0f, maxRange = 0.0f;
                float minRangeFriendly = 0.0f, maxRangeFriendly = 0.0f;
                Spell_C::GetMinMaxRange(unit, spell, &minRange, &maxRange, 0, 0);
                Spell_C::GetMinMaxRange(unit, spell, &minRangeFriendly, &maxRangeFriendly, 1, 0);
                if (Spell_C::UsesDefaultMinRange(spell)) {
                    Spell_C::GetDefaultMinRange(spell, &minRange);
                    Spell_C::GetDefaultMinRange(spell, &minRangeFriendly);
                }

                char rangeNum[32] = {};
                const float unlim = 50000.0f;
                bool sameRange = (minRange == minRangeFriendly && maxRange == maxRangeFriendly);

                if (sameRange) {
                    if (maxRange > 0.0f && maxRange < unlim) {
                        if (minRange <= 0.0f)
                            SStr::Printf(rangeNum, sizeof(rangeNum), "%d", static_cast<int>(maxRange));
                        else
                            SStr::Printf(rangeNum, sizeof(rangeNum), "%d-%d", static_cast<int>(minRange), static_cast<int>(maxRange));
                        const char* fmt = FrameScript::GetText(const_cast<char*>("SPELL_RANGE"), -1, 0);
                        if (fmt) SStr::Printf(rangeStr, sizeof(rangeStr), const_cast<char*>(fmt), rangeNum);
                    } else if (maxRange >= unlim) {
                        const char* u = FrameScript::GetText(const_cast<char*>("SPELL_RANGE_UNLIMITED"), -1, 0);
                        if (u) SStr::Copy(rangeStr, const_cast<char*>(u), sizeof(rangeStr));
                    }
                } else {
                    if (maxRange > 0.0f && maxRange < unlim) {
                        if (minRange <= 0.0f)
                            SStr::Printf(rangeNum, sizeof(rangeNum), "%d", static_cast<int>(maxRange));
                        else
                            SStr::Printf(rangeNum, sizeof(rangeNum), "%d-%d", static_cast<int>(minRange), static_cast<int>(maxRange));
                        const char* dualFmt = FrameScript::GetText(const_cast<char*>("SPELL_RANGE_DUAL"), -1, 0);
                        const char* enemy = FrameScript::GetText(const_cast<char*>("ENEMY"), -1, 0);
                        if (dualFmt && enemy) {
                            SStr::Printf(rangeStr, sizeof(rangeStr), const_cast<char*>(dualFmt), enemy, rangeNum);
                            sub_61FEC0(tooltip, const_cast<char*>(" "), rangeStr, sColorHexWhite, sColorHexWhite, 0);
                            rangeStr[0] = '\0';
                        }
                    } else if (maxRange >= unlim) {
                        const char* u = FrameScript::GetText(const_cast<char*>("SPELL_RANGE_UNLIMITED"), -1, 0);
                        if (u) {
                            SStr::Copy(rangeStr, const_cast<char*>(u), sizeof(rangeStr));
                            sub_61FEC0(tooltip, const_cast<char*>(" "), rangeStr, sColorHexWhite, sColorHexWhite, 0);
                            rangeStr[0] = '\0';
                        }
                    }
                    if (maxRangeFriendly > 0.0f && maxRangeFriendly < unlim) {
                        rangeNum[0] = '\0';
                        if (minRangeFriendly <= 0.0f)
                            SStr::Printf(rangeNum, sizeof(rangeNum), "%d", static_cast<int>(maxRangeFriendly));
                        else
                            SStr::Printf(rangeNum, sizeof(rangeNum), "%d-%d", static_cast<int>(minRangeFriendly), static_cast<int>(maxRangeFriendly));
                        const char* dualFmt = FrameScript::GetText(const_cast<char*>("SPELL_RANGE_DUAL"), -1, 0);
                        const char* friendly = FrameScript::GetText(const_cast<char*>("FRIENDLY"), -1, 0);
                        if (dualFmt && friendly) SStr::Printf(rangeStr, sizeof(rangeStr), const_cast<char*>(dualFmt), friendly, rangeNum);
                    } else if (maxRangeFriendly >= unlim) {
                        const char* u = FrameScript::GetText(const_cast<char*>("SPELL_RANGE_UNLIMITED"), -1, 0);
                        if (u) SStr::Copy(rangeStr, const_cast<char*>(u), sizeof(rangeStr));
                    }
                }
            }
        }

        // When no cost, range goes on the left (original: SStrCopy(a1, src, 128); src[0]=0).
        char* lineLeftFinal = costStr;
        char* lineRightFinal = rangeStr[0] ? rangeStr : nullptr;
        if (!costStr[0] && rangeStr[0]) {
            lineLeftFinal = rangeStr;
            lineRightFinal = nullptr;
        }
        sub_61FEC0(
            tooltip,
            lineLeftFinal,
            lineRightFinal,
            sColorHexWhite,
            sColorHexWhite,
            0);
    }
    {
        char castBuf[128] = {};
        char cdBuf[128] = {};
        uintptr_t flag = 0;

        SetSpellCooldownTooltip(
            castBuf,
            spell,
            &flag,
            a5,
            a7,
            cdBuf,
            tooltip,
            Spell_C::GetPowerCost(spell, unit));
    }

    // Description text.
    if (spell->m_description_lang && *spell->m_description_lang) {
        char desc[2048] = {};
        SpellParser::ParseText(
            spell,
            desc,
            sizeof(desc),
            a5,
            a7,
            0,
            0,
            1,
            0);

        sub_61FEC0(
            tooltip,
            desc,
            nullptr,
            sColorHexDarkYellow,
            sColorHexDarkYellow,
            1);
    }

    // Finalize: show and size tooltip.
    CSimpleFrame::Show(tooltip);
    CGTooltipInternal::CalculateSize(tooltip);

    LOG_DEBUG << "Tooltip finalized";
    return 1;
}
