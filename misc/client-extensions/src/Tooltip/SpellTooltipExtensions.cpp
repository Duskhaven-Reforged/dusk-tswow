#include <CDBCMgr/CDBCMgr.h>
#include <CDBCMgr/CDBCDefs/SpellAdditionalCostData.h>
#include <CDBCMgr/CDBCDefs/SpellEffect.h>
#include <CDBCMgr/CDBCDefs/SpellEffectScalars.h>
#include <Character/CharacterExtensions.h>
#include <Tooltip/SpellTooltipExtensions.h>
#include <Spells/Spells.h>
#include <ClientData/Spell.h>
#include <Spells/SpellCache/SpellCacheAuthority.h>
#include <Spells/SpellCache/SpellCacheStreaming.h>
#include <Logger.h>

#include <Windows.h>
#include <algorithm>
#include <any>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

using namespace ClientData;

static constexpr uint32_t kTooltipDamageTypeBleed = 0x80000000;

void TooltipExtensions::Apply() {
    SpellTooltipVariableExtension();
    SpellTooltipSetSpellExtension();
    //SpellTooltipPowerCostExtension();
    //SpellTooltipCooldownExtension();
    //SpellTooltipRemainingCooldownExtension();
}

void TooltipExtensions::SpellTooltipVariableExtension() {
    size_t extendedVariableCount = 0;
    TooltipVariableExtensions::GetExtendedSpellVariableNames(extendedVariableCount);
    const uint32_t variableTableCount = SPELLVARIABLE_STOCK_COUNT + static_cast<uint32_t>(extendedVariableCount);

    // change pointer to table with variables
    Util::OverwriteUInt32AtAddress(0x576B63, reinterpret_cast<uint32_t>(&spellVariables));
    // update number of entries value
    Util::OverwriteUInt32AtAddress(0x576B7C, variableTableCount);
    // copy table of pointers from address to spellVariables vector and add new entries
    memcpy(&spellVariables, (const void*)0xACE8F8, sizeof(uint32_t) * SPELLVARIABLE_STOCK_COUNT);
    SetNewVariablePointers();
    // change pointer of GetVariableTableValue to pointer to extended function
    Util::OverwriteUInt32AtAddress(0x578E8B, Util::CalculateAddress(reinterpret_cast<uint32_t>(&GetVariableValueEx), 0x578E8F));
}

float GetSpellPowerBonusForSpell(CGPlayer* activePlayer, SpellRow* spell)
{
    if (!activePlayer || !spell)
        return 0.0f;

    int32_t spBonus = 0;
    uint32_t schoolMask = spell->m_schoolMask;
    for (uint32_t i = SPELL_SCHOOL_HOLY; i < MAX_SPELL_SCHOOL; ++i) {
        if (schoolMask & (1 << i)) {
            int32_t tempBonus = activePlayer->PlayerData->SPPos[i];
            if (tempBonus > spBonus)
                spBonus = tempBonus;
        }
    }

    return static_cast<float>(spBonus);
}

float GetAttackPowerBonusForSpell(CGPlayer* activePlayer, SpellRow* spell)
{
    if (!activePlayer || !spell)
        return 0.0f;

    uint8_t attType = (spell->m_equippedItemClass == 2 && spell->m_equippedItemSubclass & 262156 && spell->m_defenseType != 2)
        ? 2
        : spell->m_attributesExC & SPELL_ATTR3_MAIN_HAND ? 0 : 1;
    return static_cast<float>(CharacterDefines::GetTotalAttackPowerValue(attType, activePlayer));
}

float ApplyScalarValuesForPlayer(CGPlayer* activePlayer, SpellRow* spell, float sp, float ap, float bv, int mode)
{
    if (!activePlayer || !spell)
        return 0.0f;

    float apValue = ap * GetAttackPowerBonusForSpell(activePlayer, spell);
    float spValue = sp * GetSpellPowerBonusForSpell(activePlayer, spell);
    float bvValue = bv * static_cast<float>(activePlayer->PlayerData->shieldBlock);

    if (mode == 1)
        return (std::max)(apValue, spValue) + bvValue;

    return apValue + spValue + bvValue;
}

float ApplyScalarRowForPlayer(CGPlayer* activePlayer, SpellRow* spell, const SpellEffectScalarsRow& row)
{
    return ApplyScalarValuesForPlayer(activePlayer, spell, row.sp, row.ap, row.bv, row.mode);
}

float GetSpellScalarsForEffect(CGPlayer* activePlayer, SpellRow* spell, int idx)
{
    if (!activePlayer || !spell)
        return 0.0f;

    float total = 0.0f;
    auto scalars = GlobalCDBCMap.getCDBC("SpellEffectScalars");
    for (auto scalar : scalars)
    {
        if (SpellEffectScalarsRow* row = std::any_cast<SpellEffectScalarsRow>(&scalar.second))
        {
            if (row->spellID == spell->m_ID && row->effectIdx == idx)
                total += ApplyScalarRowForPlayer(activePlayer, spell, *row);
        }
    }

    return total;
}

std::string FormatScalarCoefficient(float value)
{
    char buffer[32] = {};
    std::snprintf(buffer, sizeof(buffer), "%.2f", value);
    return buffer;
}

std::string FormatFormulaNumber(float value, int decimals = 2)
{
    char format[16] = {};
    std::snprintf(format, sizeof(format), "%%.%df", decimals);

    char buffer[64] = {};
    std::snprintf(buffer, sizeof(buffer), format, value);
    size_t len = std::strlen(buffer);
    while (len > 0 && buffer[len - 1] == '0')
        buffer[--len] = '\0';
    if (len > 0 && buffer[len - 1] == '.')
        buffer[--len] = '\0';
    return buffer;
}

std::string FormatScalarPercent(float value)
{
    return FormatFormulaNumber(value * 100.0f, 2) + "%";
}

const char* TooltipAttackPowerIcon()
{
    return "|TInterface\\Icons\\Ability_MeleeDamage:0|t";
}

const char* TooltipSpellPowerIcon()
{
    return "|TInterface\\Icons\\Spell_Holy_MagicalSentry:0|t";
}

const char* TooltipBlockValueIcon()
{
    return "|TInterface\\Icons\\INV_Shield_06:0|t";
}

const char* TooltipFormulaBaseColor()
{
    return "|cffffffff";
}

const char* TooltipFormulaAttackPowerColor()
{
    return "|cffd99545";
}

const char* TooltipFormulaSpellPowerColor()
{
    return "|cff4fa6d8";
}

std::string ColorFormulaText(const char* color, const std::string& text)
{
    if (!color || text.empty())
        return text;

    return std::string(color) + text + "|r";
}

void AppendFormulaTerm(std::string& formula, const std::string& term, const char* formulaColor = nullptr)
{
    if (term.empty())
        return;

    if (!formula.empty())
        formula += ColorFormulaText(formulaColor, " + ");
    formula += term;
}

std::string BuildScalarFormula(float sp, float ap, float bv, int mode, const char* formulaColor)
{
    std::string formula;
    std::string apTerm = ap != 0.0f
        ? ColorFormulaText(TooltipFormulaAttackPowerColor(), FormatScalarPercent(ap) + " " + TooltipAttackPowerIcon())
        : "";
    std::string spTerm = sp != 0.0f
        ? ColorFormulaText(TooltipFormulaSpellPowerColor(), FormatScalarPercent(sp) + " " + TooltipSpellPowerIcon())
        : "";

    if (mode == 1 && !apTerm.empty() && !spTerm.empty())
        AppendFormulaTerm(
            formula,
            ColorFormulaText(formulaColor, "max(") + apTerm + ColorFormulaText(formulaColor, ", ") + spTerm + ColorFormulaText(formulaColor, ")"),
            formulaColor);
    else
    {
        AppendFormulaTerm(formula, apTerm, formulaColor);
        AppendFormulaTerm(formula, spTerm, formulaColor);
    }

    if (bv != 0.0f)
        AppendFormulaTerm(formula, ColorFormulaText(formulaColor, FormatScalarPercent(bv) + " " + TooltipBlockValueIcon()), formulaColor);

    return formula;
}

std::string BuildSpellScalarFormulaForEffect(SpellRow* spell, int idx, const char* formulaColor)
{
    if (!spell)
        return "";

    std::string formula;
    auto scalars = GlobalCDBCMap.getCDBC("SpellEffectScalars");
    for (auto scalar : scalars)
    {
        if (SpellEffectScalarsRow* row = std::any_cast<SpellEffectScalarsRow>(&scalar.second))
        {
            if (row->spellID != spell->m_ID || row->effectIdx != idx)
                continue;

            AppendFormulaTerm(formula, BuildScalarFormula(row->sp, row->ap, row->bv, row->mode, formulaColor), formulaColor);
        }
    }

    return formula;
}

std::string BuildStreamedEffectScalarFormula(const SpellEffectCacheRow& effect, const char* formulaColor)
{
    return BuildScalarFormula(
        effect.effectSpellPowerBonus,
        effect.effectAttackPowerBonus,
        effect.effectBlockValueBonus,
        effect.effectScalingMode,
        formulaColor);
}

bool TryGetOwnedTooltipSpellMod(SpellRow* spell, int modOp, int& flat, int& pct)
{
    flat = 0;
    pct = 100;
    if (!spell)
        return false;

    return QuestTextParser::GetModifiedStats(spell, modOp, &flat, &pct) != 0;
}

void ApplyOwnedTooltipSpellMod(SpellRow* spell, int modOp, float& value)
{
    if (spell)
        QuestTextParser::ModifySpellValue(spell, &value, modOp);
}

void ApplyOwnedTooltipSpellModWithFormula(SpellRow* spell, int modOp, float& value, std::string& formula)
{
    int flat = 0;
    int pct = 100;
    if (!TryGetOwnedTooltipSpellMod(spell, modOp, flat, pct))
        return;

    value = (static_cast<float>(flat) + value) * static_cast<float>(pct) * 0.01f;
    if (formula.empty())
        return;

    if (flat)
    {
        char flatBuffer[32] = {};
        std::snprintf(flatBuffer, sizeof(flatBuffer), "%+d", flat);
        formula = "(" + formula + " " + flatBuffer + ")";
    }

    if (pct != 100)
    {
        char pctBuffer[32] = {};
        std::snprintf(pctBuffer, sizeof(pctBuffer), "%.2f", static_cast<float>(pct) * 0.01f);
        formula = std::string(pctBuffer) + "(" + formula + ")";
    }
}

struct OwnedTooltipValueFormulaParts
{
    bool enabled = false;
    float baseValue = 0.0f;
    std::string scalarFormula;
    int flatModTotal = 0;
    float percentMultiplier = 1.0f;
};

void ApplyOwnedTooltipSpellModWithParts(SpellRow* spell, int modOp, float& value, OwnedTooltipValueFormulaParts& parts)
{
    int flat = 0;
    int pct = 100;
    if (!TryGetOwnedTooltipSpellMod(spell, modOp, flat, pct))
        return;

    parts.flatModTotal += flat;
    parts.percentMultiplier *= static_cast<float>(pct) * 0.01f;
    value = (value + static_cast<float>(flat)) * static_cast<float>(pct) * 0.01f;
}

std::string BuildOwnedTooltipValueFormula(const OwnedTooltipValueFormulaParts& parts, const char* formulaColor)
{
    if (!parts.enabled)
        return "";

    if (parts.scalarFormula.empty() && parts.flatModTotal == 0 && parts.percentMultiplier == 1.0f)
        return "";

    std::string inner;
    if (parts.baseValue != 0.0f)
        AppendFormulaTerm(inner, ColorFormulaText(TooltipFormulaBaseColor(), FormatFormulaNumber(parts.baseValue, 0)), formulaColor);
    if (parts.flatModTotal != 0)
        AppendFormulaTerm(inner, ColorFormulaText(formulaColor, FormatFormulaNumber(static_cast<float>(parts.flatModTotal), 0)), formulaColor);
    AppendFormulaTerm(inner, parts.scalarFormula, formulaColor);
    if (inner.empty())
        inner = ColorFormulaText(formulaColor, "0");

    std::string formula;
    if (parts.percentMultiplier != 1.0f)
        formula = ColorFormulaText(formulaColor, "(" + FormatFormulaNumber(parts.percentMultiplier, 3) + "(") + inner + ColorFormulaText(formulaColor, "))");
    else
        formula = ColorFormulaText(formulaColor, "(") + inner + ColorFormulaText(formulaColor, ")");

    return formula;
}

bool IsOwnedTooltipHealingEffect(uint32_t effectType)
{
    return effectType == SpellEffects::Heal
        || effectType == SpellEffects::HealMaxHealth
        || effectType == SpellEffects::HealMechanical
        || effectType == SpellEffects::HealPct;
}

bool IsOwnedTooltipHealAmountEffect(uint32_t effectType)
{
    return effectType == SpellEffects::Heal || effectType == SpellEffects::HealMechanical;
}

bool IsOwnedTooltipDamageEffect(uint32_t effectType)
{
    return effectType == SpellEffects::SchoolDamage || effectType == SpellEffects::HealthLeech;
}

bool IsTooltipFormulaExpanded()
{
    return (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
}

struct OwnedSpellTooltipRefreshState
{
    bool active = false;
    bool shiftDown = false;
    bool refreshing = false;
    void* tooltip = nullptr;
    int spellId = 0;
    int a3 = 0;
    int a4 = 0;
    int a5 = 0;
    int a6 = 0;
    int a7 = 0;
    int a8 = 0;
    bool hasA9 = false;
    uint32_t a9Value = 0;
    int a10 = 0;
    int a12 = 0;
    int a13 = 0;
    int a14 = 0;
    int a15 = 0;
    int a16 = 0;
    bool cacheRefreshPending = false;
    std::vector<uint32_t> awaitedSpellIds;
};

OwnedSpellTooltipRefreshState s_ownedSpellTooltipRefresh;

void MarkOwnedSpellTooltipAwaitingSpell(uint32_t spellId)
{
    if (!spellId || !s_ownedSpellTooltipRefresh.active)
        return;

    if (std::find(
        s_ownedSpellTooltipRefresh.awaitedSpellIds.begin(),
        s_ownedSpellTooltipRefresh.awaitedSpellIds.end(),
        spellId) == s_ownedSpellTooltipRefresh.awaitedSpellIds.end())
    {
        s_ownedSpellTooltipRefresh.awaitedSpellIds.push_back(spellId);
    }
}

void MarkOwnedSpellTooltipCacheRefresh(uint32_t spellId)
{
    if (!spellId || !s_ownedSpellTooltipRefresh.active)
        return;

    if (std::find(
        s_ownedSpellTooltipRefresh.awaitedSpellIds.begin(),
        s_ownedSpellTooltipRefresh.awaitedSpellIds.end(),
        spellId) != s_ownedSpellTooltipRefresh.awaitedSpellIds.end())
    {
        s_ownedSpellTooltipRefresh.cacheRefreshPending = true;
    }
}

float GetNativeEffectMinPoints(SpellRow* spell, uint32_t effectIndex, uint32_t level, uint32_t a4, uint32_t a5, uint32_t a8)
{
    float minPoints = 0.0f;
    float maxPoints = 0.0f;
    QuestTextParser::GetMinMaxPoints(spell, effectIndex, &minPoints, &maxPoints, level, a4, a5, a8);
    return minPoints;
}

void PushFormulaValue(uint32_t a3, float value)
{
    uint32_t* offset = reinterpret_cast<uint32_t*>(a3 + 128);
    --*offset;
    *reinterpret_cast<float*>(a3 + 4 * *offset) = value;
}

void ClearSpellEffectSlot(SpellRow& spell, uint32_t slot)
{
    spell.m_effect[slot] = 0;
    spell.m_effectDieSides[slot] = 0;
    spell.m_effectRealPointsPerLevel[slot] = 0.0f;
    spell.m_effectBasePoints[slot] = 0;
    spell.m_effectMechanic[slot] = 0;
    spell.m_implicitTargetA[slot] = 0;
    spell.m_implicitTargetB[slot] = 0;
    spell.m_effectRadiusIndex[slot] = 0;
    spell.m_effectAura[slot] = 0;
    spell.m_effectAuraPeriod[slot] = 0;
    spell.m_effectAmplitude[slot] = 0.0f;
    spell.m_effectChainTargets[slot] = 0;
    spell.m_effectItemType[slot] = 0;
    spell.m_effectMiscValue[slot] = 0;
    spell.m_effectMiscValueB[slot] = 0;
    spell.m_effectTriggerSpell[slot] = 0;
    spell.m_effectPointsPerCombo[slot] = 0.0f;
    spell.m_effectSpellClassMask[slot][0] = 0;
    spell.m_effectSpellClassMask[slot][1] = 0;
    spell.m_effectSpellClassMask[slot][2] = 0;
    spell.m_effectChainAmplitude[slot] = 0.0f;
    spell.m_effectBonusCoefficient[slot] = 0.0f;
}

void ProjectEffectIntoFirstSlot(SpellRow* sourceSpell, const SpellEffectCacheRow& effect, SpellRow& projected)
{
    projected = *sourceSpell;

    for (uint32_t slot = 0; slot < 3; ++slot)
        ClearSpellEffectSlot(projected, slot);

    projected.m_effect[0] = effect.effect;
    projected.m_effectDieSides[0] = static_cast<uint32_t>(effect.effectDieSides);
    projected.m_effectRealPointsPerLevel[0] = effect.effectRealPointsPerLevel;
    projected.m_effectBasePoints[0] = static_cast<uint32_t>(effect.effectBasePoints);
    projected.m_effectMechanic[0] = effect.effectMechanic;
    projected.m_implicitTargetA[0] = effect.effectImplicitTargetA;
    projected.m_implicitTargetB[0] = effect.effectImplicitTargetB;
    projected.m_effectRadiusIndex[0] = effect.effectRadiusIndex;
    projected.m_effectAura[0] = effect.effectApplyAuraName;
    projected.m_effectAuraPeriod[0] = effect.effectAmplitude;
    projected.m_effectAmplitude[0] = effect.effectMultipleValue;
    projected.m_effectChainTargets[0] = effect.effectChainTargets;
    projected.m_effectItemType[0] = static_cast<uint32_t>(effect.effectItemType);
    projected.m_effectMiscValue[0] = static_cast<uint32_t>(effect.effectMiscValue);
    projected.m_effectMiscValueB[0] = static_cast<uint32_t>(effect.effectMiscValueB);
    projected.m_effectTriggerSpell[0] = effect.effectTriggerSpell;
    projected.m_effectPointsPerCombo[0] = effect.effectPointsPerCombo;
    projected.m_effectSpellClassMask[0][0] = effect.effectSpellClassMaskA;
    projected.m_effectSpellClassMask[0][1] = effect.effectSpellClassMaskB;
    projected.m_effectSpellClassMask[0][2] = effect.effectSpellClassMaskC;
    projected.m_effectChainAmplitude[0] = effect.effectChainAmplitude;
    projected.m_effectBonusCoefficient[0] = effect.effectBonusMultiplier;
}

float GetStreamedEffectMinPoints(SpellRow* spell, const SpellEffectCacheRow& effect, uint32_t level, uint32_t a4, uint32_t a5, uint32_t a8)
{
    SpellRow projected = {};
    ProjectEffectIntoFirstSlot(spell, effect, projected);
    return GetNativeEffectMinPoints(&projected, 0, level, a4, a5, a8);
}

struct EffectFormulaVariable
{
    uint32_t effectIndex = 0;
    uint32_t nativeSlotOneVariable = 0;
};

bool TryMapStockEffectFormulaVariable(uint32_t spellVariable, EffectFormulaVariable& out)
{
    struct Range
    {
        uint32_t first;
        uint32_t count;
        uint32_t nativeSlotOneVariable;
    };

    static const Range ranges[] = {
        { 32, 3, 32 },   // m
        { 35, 3, 35 },   // M
        { 38, 3, 38 },   // a
        { 41, 3, 41 },   // A
        { 50, 3, 50 },   // x
        { 53, 3, 53 },   // X
        { 56, 3, 56 },   // t
        { 59, 3, 59 },   // T
        { 66, 3, 66 },   // b
        { 69, 3, 69 },   // B
        { 76, 3, 76 },   // e
        { 79, 3, 79 },   // E
        { 84, 3, 84 },   // f
        { 87, 3, 87 },   // F
        { 90, 3, 90 },   // q
        { 93, 3, 93 },   // Q
        { 156, 3, 156 }, // bc
        { 159, 3, 159 }, // BC
    };

    for (const Range& range : ranges) {
        if (spellVariable >= range.first && spellVariable < range.first + range.count) {
            out.effectIndex = spellVariable - range.first;
            out.nativeSlotOneVariable = range.nativeSlotOneVariable;
            return true;
        }
    }

    return false;
}

bool TryMapExtendedEffectFormulaVariable(uint32_t spellVariable, EffectFormulaVariable& out)
{
    if (spellVariable < SPELLVARIABLE_BASELINE_EFFECT_FIRST)
        return false;

    uint32_t offset = spellVariable - SPELLVARIABLE_BASELINE_EFFECT_FIRST;
    uint32_t group = offset / SPELLVARIABLE_BASELINE_EFFECT_GROUP_SIZE;
    if (group >= SPELLVARIABLE_BASELINE_EFFECT_GROUP_COUNT)
        return false;

    static const uint32_t nativeSlotOneVariables[] = {
        32, 35, 38, 41, 50, 53, 56, 59, 66,
        69, 76, 79, 84, 87, 90, 93, 156, 159
    };

    out.effectIndex = offset % SPELLVARIABLE_BASELINE_EFFECT_GROUP_SIZE;
    out.nativeSlotOneVariable = nativeSlotOneVariables[group];
    return true;
}

bool TryHandleStreamedEffectFormulaVariable(
    void* _this,
    uint32_t spellVariable,
    uint32_t a3,
    SpellRow* spell,
    uint32_t a5,
    uint32_t a6,
    uint32_t a7,
    uint32_t a8,
    uint32_t a9)
{
    if (!spell || !SpellCacheStreaming::HasSpell(spell->m_ID))
        return false;

    EffectFormulaVariable mapped = {};
    if (!TryMapStockEffectFormulaVariable(spellVariable, mapped) &&
        !TryMapExtendedEffectFormulaVariable(spellVariable, mapped)) {
        return false;
    }

    const SpellEffectCacheRow* effect = nullptr;
    if (!SpellCacheStreaming::TryGetSpellEffect(spell->m_ID, mapped.effectIndex, effect) || !effect) {
        PushFormulaValue(a3, 0.0f);
        return true;
    }

    SpellRow projected = {};
    ProjectEffectIntoFirstSlot(spell, *effect, projected);
    CFormula::GetVariableValue(_this, mapped.nativeSlotOneVariable, a3, &projected, a5, a6, a7, a8, a9);
    return true;
}

bool TryGetCustomEffectIndex(uint32_t spellVariable, uint32_t first1, uint32_t first4, uint32_t& effectIndex)
{
    if (spellVariable >= first1 && spellVariable < first1 + 3) {
        effectIndex = spellVariable - first1;
        return true;
    }

    if (spellVariable >= first4 && spellVariable < first4 + 29) {
        effectIndex = 3 + (spellVariable - first4);
        return true;
    }

    return false;
}

bool TryGetEffectRealPointsPerLevel(SpellRow* spell, uint32_t effectIndex, float& value)
{
    if (!spell || effectIndex >= SPELLVARIABLE_EFFECT_SLOT_COUNT)
        return false;

    if (SpellCacheStreaming::HasSpell(spell->m_ID)) {
        const SpellEffectCacheRow* effect = nullptr;
        if (!SpellCacheStreaming::TryGetSpellEffect(spell->m_ID, effectIndex, effect) || !effect) {
            value = 0.0f;
            return true;
        }

        value = effect->effectRealPointsPerLevel;
        return true;
    }

    if (effectIndex < 3) {
        value = spell->m_effectRealPointsPerLevel[effectIndex];
        return true;
    }

    return false;
}

bool TryGetEffectBonusValue(CGPlayer* activePlayer, SpellRow* spell, uint32_t effectIndex, uint32_t level, uint32_t a4, uint32_t a5, uint32_t a8, float& value)
{
    if (!spell || effectIndex >= SPELLVARIABLE_EFFECT_SLOT_COUNT)
        return false;

    if (SpellCacheStreaming::HasSpell(spell->m_ID)) {
        const SpellEffectCacheRow* effect = nullptr;
        if (!SpellCacheStreaming::TryGetSpellEffect(spell->m_ID, effectIndex, effect) || !effect) {
            value = 0.0f;
            return true;
        }

        value = ApplyScalarValuesForPlayer(
            activePlayer,
            spell,
            effect->effectSpellPowerBonus,
            effect->effectAttackPowerBonus,
            effect->effectBlockValueBonus,
            effect->effectScalingMode);
        value += GetStreamedEffectMinPoints(spell, *effect, level, a4, a5, a8);
        return true;
    }

    value = GetSpellScalarsForEffect(activePlayer, spell, static_cast<int>(effectIndex));

    if (effectIndex < 3) {
        value += GetNativeEffectMinPoints(spell, effectIndex, level, a4, a5, a8);
        return true;
    }

    return false;
}

// Client-side color pointers used by tooltip rendering helpers.
static void* const sColorHexWhite      = reinterpret_cast<void*>(0xAD2D30);
static void* const sColorHexGrey0      = reinterpret_cast<void*>(0xAD2D38);
static void* const sColorHexDarkYellow = reinterpret_cast<void*>(0xAD2D2C);
static void* const sColorHexRed0       = reinterpret_cast<void*>(0xAD2D34);

namespace
{
    static constexpr uint32_t kTSSpellIdStart = 80000;
    int s_ownedTooltipExpressionEvaluationDepth = 0;

    struct TooltipSpellRadiusRow
    {
        uint32_t id;
        float radius;
        float radiusPerLevel;
        float radiusMax;
    };

    struct TooltipSpellDurationRow
    {
        uint32_t id;
        int32_t duration;
        int32_t durationPerLevel;
        int32_t maxDuration;
    };

    std::unordered_map<uint32_t, TooltipSpellRadiusRow> s_tooltipSpellRadii;
    std::unordered_map<uint32_t, TooltipSpellDurationRow> s_tooltipSpellDurations;
    bool s_tooltipSpellRadiiLoaded = false;
    bool s_tooltipSpellDurationsLoaded = false;

    template <typename Row>
    bool LoadSimpleDbcRows(const char* path, uint32_t expectedFields, uint32_t maxRecords, std::vector<Row>& rows)
    {
        HANDLE file = nullptr;
        if (!SFile::OpenFile(path, &file) || !file)
            return false;

        uint32_t header[5] = {};
        uint32_t bytesRead = 0;
        if (!SFile::ReadFile(file, header, sizeof(header), &bytesRead, nullptr, 0) || bytesRead != sizeof(header))
        {
            SFile::CloseFile(file);
            return false;
        }

        uint32_t const records = header[1];
        uint32_t const fields = header[2];
        uint32_t const recordSize = header[3];
        if (fields != expectedFields || recordSize != sizeof(Row) || records > maxRecords)
        {
            SFile::CloseFile(file);
            return false;
        }

        rows.resize(records);
        bool ok = SFile::ReadFile(file, rows.data(), records * sizeof(Row), &bytesRead, nullptr, 0) &&
            bytesRead == records * sizeof(Row);
        SFile::CloseFile(file);
        return ok;
    }

    void LoadTooltipSpellRadii()
    {
        if (s_tooltipSpellRadiiLoaded)
            return;

        s_tooltipSpellRadiiLoaded = true;
        std::vector<TooltipSpellRadiusRow> rows;
        if (!LoadSimpleDbcRows("DBFilesClient\\SpellRadius.dbc", 4, 4096, rows))
            return;

        for (TooltipSpellRadiusRow const& row : rows)
            s_tooltipSpellRadii[row.id] = row;
    }

    void LoadTooltipSpellDurations()
    {
        if (s_tooltipSpellDurationsLoaded)
            return;

        s_tooltipSpellDurationsLoaded = true;
        std::vector<TooltipSpellDurationRow> rows;
        if (!LoadSimpleDbcRows("DBFilesClient\\SpellDuration.dbc", 4, 4096, rows))
            return;

        for (TooltipSpellDurationRow const& row : rows)
            s_tooltipSpellDurations[row.id] = row;
    }

    bool MacroNameEquals(const std::string& actual, const char* expected)
    {
        size_t const expectedLen = std::strlen(expected);
        if (actual.size() != expectedLen)
            return false;

        for (size_t i = 0; i < expectedLen; ++i)
        {
            if (std::tolower(static_cast<unsigned char>(actual[i])) !=
                std::tolower(static_cast<unsigned char>(expected[i])))
                return false;
        }

        return true;
    }

    uint32_t s_ownedTooltipTextParseDepth = 0;
    bool s_ownedTooltipHasLastPluralValue = false;
    float s_ownedTooltipLastPluralValue = 0.0f;

    void RecordOwnedTooltipPluralValue(float value)
    {
        s_ownedTooltipHasLastPluralValue = true;
        s_ownedTooltipLastPluralValue = value;
    }

    void AppendOwnedTooltipNumber(std::string& out, float value, bool integerValue)
    {
        RecordOwnedTooltipPluralValue(value);
        char buffer[64] = {};
        if (integerValue)
            std::snprintf(buffer, sizeof(buffer), "%d", static_cast<int32_t>(value));
        else
            std::snprintf(buffer, sizeof(buffer), "%.1f", value);
        out.append(buffer);
    }

    void AppendOwnedTooltipDuration(std::string& out, uint32_t milliseconds)
    {
        if (!milliseconds)
        {
            out.push_back('0');
            return;
        }

        char buffer[128] = {};
        uint32_t value = milliseconds;
        const char* unit = "ms";
        if (milliseconds >= 3600000 && milliseconds % 3600000 == 0)
        {
            value = milliseconds / 3600000;
            unit = value == 1 ? "hour" : "hours";
        }
        else if (milliseconds >= 60000 && milliseconds % 60000 == 0)
        {
            value = milliseconds / 60000;
            unit = value == 1 ? "min" : "min";
        }
        else if (milliseconds >= 1000)
        {
            value = milliseconds / 1000;
            unit = value == 1 ? "sec" : "sec";
        }

        std::snprintf(buffer, sizeof(buffer), "%u %s", value, unit);
        out.append(buffer);
    }

    bool TryParseMacroIndex(const char* text, size_t textLen, size_t& pos, uint32_t& index)
    {
        index = 0;
        while (pos < textLen && std::isspace(static_cast<unsigned char>(text[pos])))
            ++pos;

        if (pos >= textLen || text[pos] != '(')
            return false;

        ++pos;
        while (pos < textLen && std::isspace(static_cast<unsigned char>(text[pos])))
            ++pos;

        if (pos >= textLen || !std::isdigit(static_cast<unsigned char>(text[pos])))
            return false;

        char* end = nullptr;
        unsigned long ordinal = std::strtoul(text + pos, &end, 10);
        if (!end || end == text + pos || ordinal < 1 || ordinal > SPELLVARIABLE_EFFECT_SLOT_COUNT)
            return false;

        pos = static_cast<size_t>(end - text);
        while (pos < textLen && std::isspace(static_cast<unsigned char>(text[pos])))
            ++pos;

        if (pos >= textLen || text[pos] != ')')
            return false;

        ++pos;
        index = static_cast<uint32_t>(ordinal - 1);
        return true;
    }

    bool OwnedTooltipNeedsEffectIndex(const std::string& macro)
    {
        return MacroNameEquals(macro, "Value")
            || MacroNameEquals(macro, "TickValue")
            || MacroNameEquals(macro, "BasePoints")
            || MacroNameEquals(macro, "Min")
            || MacroNameEquals(macro, "Max")
            || MacroNameEquals(macro, "MultipleValue")
            || MacroNameEquals(macro, "Period")
            || MacroNameEquals(macro, "Radius")
            || MacroNameEquals(macro, "ChainTargets")
            || MacroNameEquals(macro, "ChainAmplitude")
            || MacroNameEquals(macro, "Misc")
            || MacroNameEquals(macro, "MiscA")
            || MacroNameEquals(macro, "MiscB")
            || MacroNameEquals(macro, "TriggerSpell")
            || MacroNameEquals(macro, "PointsPerCombo")
            || MacroNameEquals(macro, "BonusCoefficient")
            || MacroNameEquals(macro, "Coefficient")
            || MacroNameEquals(macro, "PointsPerLevel");
    }

    float OwnedTooltipRadiusValue(uint32_t radiusIndex)
    {
        if (!radiusIndex)
            return 0.0f;

        LoadTooltipSpellRadii();
        auto it = s_tooltipSpellRadii.find(radiusIndex);
        if (it == s_tooltipSpellRadii.end())
            return static_cast<float>(radiusIndex);

        return it->second.radius;
    }

    uint32_t OwnedTooltipDurationMs(SpellRow* spell)
    {
        if (!spell || !spell->m_durationIndex)
            return 0;

        LoadTooltipSpellDurations();
        auto it = s_tooltipSpellDurations.find(spell->m_durationIndex);
        if (it == s_tooltipSpellDurations.end())
            return 0;

        return it->second.duration > 0 ? static_cast<uint32_t>(it->second.duration) : 0;
    }

    std::string NormalizeMacroName(const std::string& name)
    {
        std::string normalized;
        normalized.reserve(name.size());
        for (char ch : name)
            normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        return normalized;
    }

    bool ParseOwnedTooltipTextToString(SpellRow* spell, const char* source, size_t textLen, std::string& out);

    bool TryParseMacroArguments(const char* text, size_t textLen, size_t& pos, std::vector<uint32_t>& args)
    {
        args.clear();
        while (pos < textLen && std::isspace(static_cast<unsigned char>(text[pos])))
            ++pos;

        if (pos >= textLen || text[pos] != '(')
            return true;

        ++pos;
        while (pos < textLen)
        {
            while (pos < textLen && std::isspace(static_cast<unsigned char>(text[pos])))
                ++pos;

            if (pos < textLen && text[pos] == ')')
            {
                ++pos;
                return true;
            }

            if (pos >= textLen || !std::isdigit(static_cast<unsigned char>(text[pos])))
                return false;

            char* end = nullptr;
            unsigned long value = std::strtoul(text + pos, &end, 10);
            if (!end || end == text + pos || value > UINT32_MAX)
                return false;

            args.push_back(static_cast<uint32_t>(value));
            pos = static_cast<size_t>(end - text);

            while (pos < textLen && std::isspace(static_cast<unsigned char>(text[pos])))
                ++pos;

            if (pos < textLen && text[pos] == ',')
            {
                ++pos;
                continue;
            }

            if (pos < textLen && text[pos] == ')')
            {
                ++pos;
                return true;
            }

            return false;
        }

        return false;
    }

    struct OwnedTooltipMacroContext
    {
        SpellRow* currentSpell = nullptr;
        SpellRow targetStorage = {};
        SpellRow* targetSpell = nullptr;
        const SpellEffectCacheRow* effect = nullptr;
        std::vector<uint32_t> args;
        uint32_t effectIndex = 0;
        bool targetUsesStreamedEffects = false;
    };

    struct OwnedTooltipExpressionColorState
    {
        bool hasValue = false;
        bool hasDamage = false;
        bool conflict = false;
        uint32_t schoolMask = 0;
    };

    OwnedTooltipExpressionColorState s_ownedTooltipExpressionColor;

    uint32_t OwnedTooltipPrimarySchoolMask(uint32_t schoolMask)
    {
        static constexpr uint32_t kSchoolPhysical = 0x01;
        static constexpr uint32_t kSchoolHoly = 0x02;
        static constexpr uint32_t kSchoolFire = 0x04;
        static constexpr uint32_t kSchoolNature = 0x08;
        static constexpr uint32_t kSchoolFrost = 0x10;
        static constexpr uint32_t kSchoolShadow = 0x20;
        static constexpr uint32_t kSchoolArcane = 0x40;

        if (schoolMask & kSchoolFire)
            return kSchoolFire;
        if (schoolMask & kSchoolFrost)
            return kSchoolFrost;
        if (schoolMask & kSchoolNature)
            return kSchoolNature;
        if (schoolMask & kSchoolShadow)
            return kSchoolShadow;
        if (schoolMask & kSchoolArcane)
            return kSchoolArcane;
        if (schoolMask & kSchoolHoly)
            return kSchoolHoly;
        if (schoolMask & kSchoolPhysical)
            return kSchoolPhysical;

        return 0;
    }

    const char* OwnedTooltipSchoolColor(uint32_t schoolMask)
    {
        static constexpr uint32_t kSchoolPhysical = 0x01;
        static constexpr uint32_t kSchoolHoly = 0x02;
        static constexpr uint32_t kSchoolFire = 0x04;
        static constexpr uint32_t kSchoolNature = 0x08;
        static constexpr uint32_t kSchoolFrost = 0x10;
        static constexpr uint32_t kSchoolShadow = 0x20;
        static constexpr uint32_t kSchoolArcane = 0x40;

        schoolMask = OwnedTooltipPrimarySchoolMask(schoolMask);
        if (schoolMask == kSchoolFire)
            return "|cffff7d0a";
        if (schoolMask == kSchoolFrost)
            return "|cff69ccf0";
        if (schoolMask == kSchoolNature)
            return "|cff9dff9d";
        if (schoolMask == kSchoolShadow)
            return "|cff7b4fb3";
        if (schoolMask == kSchoolArcane)
            return "|cffd17bff";
        if (schoolMask == kSchoolHoly)
            return "|cfffff569";
        if (schoolMask == kSchoolPhysical)
            return "|cffc79c6e";

        return nullptr;
    }

    uint32_t OwnedTooltipSpellSchoolMask(SpellRow* spell)
    {
        static constexpr uint32_t kSchoolPhysical = 0x01;

        if (!spell)
            return 0;

        uint32_t schoolMask = OwnedTooltipPrimarySchoolMask(spell->m_schoolMask);
        if (schoolMask)
            return schoolMask;

        return kSchoolPhysical;
    }

    const char* OwnedTooltipSchoolName(uint32_t schoolMask)
    {
        static constexpr uint32_t kSchoolPhysical = 0x01;
        static constexpr uint32_t kSchoolHoly = 0x02;
        static constexpr uint32_t kSchoolFire = 0x04;
        static constexpr uint32_t kSchoolNature = 0x08;
        static constexpr uint32_t kSchoolFrost = 0x10;
        static constexpr uint32_t kSchoolShadow = 0x20;
        static constexpr uint32_t kSchoolArcane = 0x40;

        schoolMask = OwnedTooltipPrimarySchoolMask(schoolMask);
        if (schoolMask == kSchoolFire)
            return "Fire";
        if (schoolMask == kSchoolFrost)
            return "Frost";
        if (schoolMask == kSchoolNature)
            return "Nature";
        if (schoolMask == kSchoolShadow)
            return "Shadow";
        if (schoolMask == kSchoolArcane)
            return "Arcane";
        if (schoolMask == kSchoolHoly)
            return "Holy";
        if (schoolMask == kSchoolPhysical)
            return "Physical";

        return nullptr;
    }

    const char* OwnedTooltipDamageTypeColor(uint32_t damageType)
    {
        if (damageType == kTooltipDamageTypeBleed)
            return "|cffb03030";

        return OwnedTooltipSchoolColor(damageType);
    }

    const char* OwnedTooltipDamageTypeName(uint32_t damageType)
    {
        if (damageType == kTooltipDamageTypeBleed)
            return "Bleed";

        return OwnedTooltipSchoolName(damageType);
    }

    bool OwnedTooltipValueIsDamage(OwnedTooltipMacroContext& ctx, uint32_t effectType);
    bool OwnedTooltipValueIsHealing(OwnedTooltipMacroContext& ctx, uint32_t effectType);
    bool OwnedTooltipValueIsHealAmount(OwnedTooltipMacroContext& ctx, uint32_t effectType);
    uint32_t OwnedTooltipValueDamageType(OwnedTooltipMacroContext& ctx, uint32_t effectType);

    const char* OwnedTooltipValueColor(OwnedTooltipMacroContext& ctx, uint32_t effectType)
    {
        if (s_ownedTooltipExpressionEvaluationDepth != 0)
            return nullptr;

        if (OwnedTooltipValueIsHealing(ctx, effectType))
            return "|cff9dff9d";

        if (OwnedTooltipValueIsDamage(ctx, effectType) && ctx.targetSpell)
            return OwnedTooltipDamageTypeColor(OwnedTooltipValueDamageType(ctx, effectType));

        return nullptr;
    }

    std::string OwnedTooltipValueSuffix(OwnedTooltipMacroContext& ctx, uint32_t effectType)
    {
        if (s_ownedTooltipExpressionEvaluationDepth != 0)
            return "";

        if (OwnedTooltipValueIsHealAmount(ctx, effectType))
            return " Health";

        if (!OwnedTooltipValueIsDamage(ctx, effectType) || !ctx.targetSpell)
            return "";

        uint32_t const damageType = OwnedTooltipValueDamageType(ctx, effectType);
        const char* damageName = OwnedTooltipDamageTypeName(damageType);
        if (!damageName)
            return "";

        return std::string(" ") + damageName + " damage";
    }

    void RecordOwnedTooltipExpressionValue(OwnedTooltipMacroContext& ctx, uint32_t effectType)
    {
        if (s_ownedTooltipExpressionEvaluationDepth == 0)
            return;

        s_ownedTooltipExpressionColor.hasValue = true;
        if (!OwnedTooltipValueIsDamage(ctx, effectType) || !ctx.targetSpell)
        {
            s_ownedTooltipExpressionColor.conflict = true;
            return;
        }

        uint32_t const damageType = OwnedTooltipValueDamageType(ctx, effectType);
        if (!damageType)
        {
            s_ownedTooltipExpressionColor.conflict = true;
            return;
        }

        if (!s_ownedTooltipExpressionColor.hasDamage)
        {
            s_ownedTooltipExpressionColor.hasDamage = true;
            s_ownedTooltipExpressionColor.schoolMask = damageType;
            return;
        }

        if (s_ownedTooltipExpressionColor.schoolMask != damageType)
            s_ownedTooltipExpressionColor.conflict = true;
    }

    bool ResolveOwnedTooltipMacroSpell(OwnedTooltipMacroContext& ctx, uint32_t spellId)
    {
        if (ctx.currentSpell && ctx.currentSpell->m_ID == spellId)
        {
            ctx.targetSpell = ctx.currentSpell;
            ctx.targetUsesStreamedEffects = spellId >= kTSSpellIdStart || SpellCacheStreaming::HasSpell(spellId);
            return true;
        }

        if (SpellCacheStreaming::TryGetSpellRow(spellId, ctx.targetStorage, true))
        {
            ctx.targetSpell = &ctx.targetStorage;
            ctx.targetUsesStreamedEffects = spellId >= kTSSpellIdStart || SpellCacheStreaming::HasSpell(spellId);
            return true;
        }

        ctx.targetSpell = nullptr;
        ctx.targetUsesStreamedEffects = spellId >= kTSSpellIdStart;
        return false;
    }

    bool ResolveOwnedTooltipSpellMacro(OwnedTooltipMacroContext& ctx)
    {
        if (ctx.args.empty())
        {
            ctx.targetSpell = ctx.currentSpell;
            return ctx.targetSpell != nullptr;
        }

        if (ctx.args.size() != 1)
            return false;

        return ResolveOwnedTooltipMacroSpell(ctx, ctx.args[0]);
    }

    bool ResolveOwnedTooltipEffectMacro(OwnedTooltipMacroContext& ctx)
    {
        uint32_t spellId = 0;
        uint32_t ordinal = 0;

        if (ctx.args.size() == 1)
        {
            if (!ctx.currentSpell)
                return false;
            spellId = ctx.currentSpell->m_ID;
            ordinal = ctx.args[0];
        }
        else if (ctx.args.size() == 2)
        {
            spellId = ctx.args[0];
            ordinal = ctx.args[1];
        }
        else
            return false;

        if (ordinal < 1 || ordinal > SPELLVARIABLE_EFFECT_SLOT_COUNT)
            return false;

        ctx.effectIndex = ordinal - 1;
        ResolveOwnedTooltipMacroSpell(ctx, spellId);
        ctx.effect = nullptr;
        if (!SpellCacheStreaming::TryGetSpellEffect(spellId, ctx.effectIndex, ctx.effect))
        {
            ctx.effect = nullptr;
            if (ctx.targetUsesStreamedEffects || spellId >= kTSSpellIdStart || ctx.effectIndex >= 3)
            {
                MarkOwnedSpellTooltipAwaitingSpell(spellId);
                SpellCacheStreaming::RequestSpell(spellId, UINT32_MAX);
            }
        }

        return true;
    }

    bool AppendMacroString(std::string& out, const char* value)
    {
        if (value)
            out.append(value);
        return true;
    }

    bool AppendMacroNumber(std::string& out, float value, bool integerValue)
    {
        AppendOwnedTooltipNumber(out, value, integerValue);
        return true;
    }

    bool AppendMacroRadius(std::string& out, float value)
    {
        AppendOwnedTooltipNumber(out, value, true);
        out.append(" yards");
        return true;
    }

    void AppendOwnedTooltipError(std::string& out)
    {
        out.append("|cffff0000ERROR|r");
    }

    void AppendOwnedTooltipExpressionNumber(std::string& out, double value)
    {
        RecordOwnedTooltipPluralValue(static_cast<float>(value));
        char buffer[64] = {};
        std::snprintf(buffer, sizeof(buffer), "%.3f", value);
        size_t len = std::strlen(buffer);
        while (len > 0 && buffer[len - 1] == '0')
            buffer[--len] = '\0';
        if (len > 0 && buffer[len - 1] == '.')
            buffer[--len] = '\0';
        out.append(buffer);
    }

    struct OwnedTooltipExpressionParser
    {
        const char* text = nullptr;
        size_t textLen = 0;
        size_t pos = 0;

        void SkipSpaces()
        {
            while (pos < textLen && std::isspace(static_cast<unsigned char>(text[pos])))
                ++pos;
        }

        bool ParseNumber(double& out)
        {
            SkipSpaces();
            if (pos >= textLen)
                return false;

            char* end = nullptr;
            out = std::strtod(text + pos, &end);
            if (!end || end == text + pos)
                return false;

            pos = static_cast<size_t>(end - text);
            return pos <= textLen;
        }

        bool ParseFactor(double& out)
        {
            SkipSpaces();
            if (pos >= textLen)
                return false;

            if (text[pos] == '+')
            {
                ++pos;
                return ParseFactor(out);
            }

            if (text[pos] == '-')
            {
                ++pos;
                if (!ParseFactor(out))
                    return false;
                out = -out;
                return true;
            }

            if (text[pos] == '(')
            {
                ++pos;
                if (!ParseExpression(out))
                    return false;
                SkipSpaces();
                if (pos >= textLen || text[pos] != ')')
                    return false;
                ++pos;
                return true;
            }

            return ParseNumber(out);
        }

        bool ParseTerm(double& out)
        {
            if (!ParseFactor(out))
                return false;

            while (true)
            {
                SkipSpaces();
                if (pos >= textLen || (text[pos] != '*' && text[pos] != '/'))
                    return true;

                char const op = text[pos++];
                double rhs = 0.0;
                if (!ParseFactor(rhs))
                    return false;

                if (op == '*')
                    out *= rhs;
                else
                {
                    if (rhs == 0.0)
                        return false;
                    out /= rhs;
                }
            }
        }

        bool ParseExpression(double& out)
        {
            if (!ParseTerm(out))
                return false;

            while (true)
            {
                SkipSpaces();
                if (pos >= textLen || (text[pos] != '+' && text[pos] != '-'))
                    return true;

                char const op = text[pos++];
                double rhs = 0.0;
                if (!ParseTerm(rhs))
                    return false;

                if (op == '+')
                    out += rhs;
                else
                    out -= rhs;
            }
        }
    };

    bool TryEvaluateOwnedTooltipExpression(const std::string& expression, double& value)
    {
        OwnedTooltipExpressionParser parser;
        parser.text = expression.c_str();
        parser.textLen = expression.size();
        if (!parser.ParseExpression(value))
            return false;

        parser.SkipSpaces();
        return parser.pos == parser.textLen;
    }

    void AppendOwnedTooltipTimeValue(std::string& out, float milliseconds)
    {
        if (milliseconds <= 0.0f)
        {
            out.append("0 sec");
            return;
        }

        float divisor = 1000.0f;
        const char* unit = "sec";
        if (milliseconds >= 3600000.0f)
        {
            divisor = 3600000.0f;
            unit = "hour";
        }
        else if (milliseconds >= 60000.0f)
        {
            divisor = 60000.0f;
            unit = "min";
        }

        double scaled = static_cast<double>(milliseconds) / static_cast<double>(divisor);
        scaled = static_cast<double>(static_cast<int64_t>(scaled * 1000.0)) / 1000.0;

        char buffer[64] = {};
        std::snprintf(buffer, sizeof(buffer), "%.3f", scaled);
        size_t len = std::strlen(buffer);
        while (len > 0 && buffer[len - 1] == '0')
            buffer[--len] = '\0';
        if (len > 0 && buffer[len - 1] == '.')
            buffer[--len] = '\0';

        out.append(buffer);
        out.push_back(' ');
        out.append(unit);
        if (std::strcmp(unit, "hour") == 0 && std::strcmp(buffer, "1") != 0)
            out.push_back('s');
    }

    bool MacroDuration(OwnedTooltipMacroContext& ctx, std::string& out)
    {
        if (!ResolveOwnedTooltipSpellMacro(ctx))
            return AppendMacroString(out, "");
        AppendOwnedTooltipDuration(out, OwnedTooltipDurationMs(ctx.targetSpell));
        return true;
    }

    bool MacroCooldown(OwnedTooltipMacroContext& ctx, std::string& out)
    {
        if (!ResolveOwnedTooltipSpellMacro(ctx) || !ctx.targetSpell)
            return AppendMacroNumber(out, 0.0f, true);
        AppendOwnedTooltipDuration(out, (std::max)(ctx.targetSpell->m_recoveryTime, ctx.targetSpell->m_categoryRecoveryTime));
        return true;
    }

    bool MacroName(OwnedTooltipMacroContext& ctx, std::string& out)
    {
        if (!ResolveOwnedTooltipSpellMacro(ctx) || !ctx.targetSpell)
            return AppendMacroString(out, "");
        return AppendMacroString(out, ctx.targetSpell->m_name_lang);
    }

    bool MacroIcon(OwnedTooltipMacroContext& ctx, std::string& out)
    {
        if (!ResolveOwnedTooltipSpellMacro(ctx) || !ctx.targetSpell)
            return AppendMacroString(out, "");

        SpellIconRow* iconRow = reinterpret_cast<SpellIconRow*>(
            ClientDB::GetRow(reinterpret_cast<void*>(0xAD48A4), ctx.targetSpell->m_spellIconID));
        return AppendMacroString(out, iconRow ? iconRow->m_textureFilename : "");
    }

    bool MacroStacks(OwnedTooltipMacroContext& ctx, std::string& out)
    {
        if (!ResolveOwnedTooltipSpellMacro(ctx) || !ctx.targetSpell)
            return AppendMacroNumber(out, 0.0f, true);
        return AppendMacroNumber(out, static_cast<float>(ctx.targetSpell->m_cumulativeAura), true);
    }

    bool TryAppendOwnedTooltipAsTimeMacro(SpellRow* spell, const char* text, size_t textLen, size_t& pos, std::string& out)
    {
        while (pos < textLen && std::isspace(static_cast<unsigned char>(text[pos])))
            ++pos;

        if (pos >= textLen || text[pos] != '(')
            return false;

        size_t const innerStart = ++pos;
        uint32_t depth = 1;
        while (pos < textLen)
        {
            char const ch = text[pos++];
            if (ch == '(')
                ++depth;
            else if (ch == ')')
            {
                if (--depth == 0)
                {
                    size_t const innerLen = (pos - 1) - innerStart;
                    std::string expanded;
                    if (!ParseOwnedTooltipTextToString(spell, text + innerStart, innerLen, expanded))
                        return false;

                    char* end = nullptr;
                    float milliseconds = std::strtof(expanded.c_str(), &end);
                    if (!end || end == expanded.c_str())
                        milliseconds = 0.0f;

                    AppendOwnedTooltipTimeValue(out, milliseconds);
                    return true;
                }
            }
        }

        return false;
    }

    bool TryFindOwnedTooltipMathBlockEnd(const char* text, size_t textLen, size_t start, bool doubleBrace, size_t& innerStart, size_t& innerLen, size_t& endPos)
    {
        if (doubleBrace)
        {
            innerStart = start + 2;
            size_t depth = 1;
            for (size_t pos = innerStart; pos + 1 < textLen; ++pos)
            {
                if (text[pos] == '{' && text[pos + 1] == '{')
                {
                    ++depth;
                    ++pos;
                    continue;
                }

                if (text[pos] == '}' && text[pos + 1] == '}')
                {
                    if (--depth == 0)
                    {
                        innerLen = pos - innerStart;
                        endPos = pos + 2;
                        return true;
                    }
                    ++pos;
                }
            }
            return false;
        }

        innerStart = start + 2;
        size_t depth = 1;
        for (size_t pos = innerStart; pos < textLen; ++pos)
        {
            if (text[pos] == '{')
                ++depth;
            else if (text[pos] == '}')
            {
                if (--depth == 0)
                {
                    innerLen = pos - innerStart;
                    endPos = pos + 1;
                    return true;
                }
            }
        }

        return false;
    }

    bool TryAppendOwnedTooltipMathBlock(SpellRow* spell, const char* text, size_t textLen, size_t& pos, std::string& out)
    {
        bool const atBrace = pos + 1 < textLen && text[pos] == '@' && text[pos + 1] == '{';
        bool const doubleBrace = pos + 1 < textLen && text[pos] == '{' && text[pos + 1] == '{';
        if (!atBrace && !doubleBrace)
            return false;

        size_t innerStart = 0;
        size_t innerLen = 0;
        size_t endPos = pos;
        if (!TryFindOwnedTooltipMathBlockEnd(text, textLen, pos, doubleBrace, innerStart, innerLen, endPos))
        {
            AppendOwnedTooltipError(out);
            pos = textLen;
            return true;
        }

        std::string expanded;
        OwnedTooltipExpressionColorState const previousColorState = s_ownedTooltipExpressionColor;
        s_ownedTooltipExpressionColor = {};
        ++s_ownedTooltipExpressionEvaluationDepth;
        bool const expandedOk = ParseOwnedTooltipTextToString(spell, text + innerStart, innerLen, expanded);
        --s_ownedTooltipExpressionEvaluationDepth;
        OwnedTooltipExpressionColorState const mathColorState = s_ownedTooltipExpressionColor;
        s_ownedTooltipExpressionColor = previousColorState;
        if (!expandedOk)
        {
            AppendOwnedTooltipError(out);
            pos = endPos;
            return true;
        }

        double value = 0.0;
        if (!TryEvaluateOwnedTooltipExpression(expanded, value))
            AppendOwnedTooltipError(out);
        else
        {
            const char* color = (mathColorState.hasDamage && !mathColorState.conflict)
                ? OwnedTooltipDamageTypeColor(mathColorState.schoolMask)
                : nullptr;
            if (color)
                out.append(color);
            AppendOwnedTooltipExpressionNumber(out, value);
            if (color)
                out.append("|r");
        }

        pos = endPos;
        return true;
    }

    float NativeEffectField(OwnedTooltipMacroContext& ctx, const char* field, bool& integerValue)
    {
        integerValue = true;
        if (!ctx.targetSpell || ctx.effectIndex >= 3)
            return 0.0f;

        uint32_t i = ctx.effectIndex;
        if (std::strcmp(field, "value") == 0 || std::strcmp(field, "min") == 0)
            return static_cast<float>(ctx.targetSpell->m_effectBasePoints[i] + 1);
        if (std::strcmp(field, "basepoints") == 0)
            return static_cast<float>(ctx.targetSpell->m_effectBasePoints[i]);
        if (std::strcmp(field, "max") == 0)
            return static_cast<float>(ctx.targetSpell->m_effectBasePoints[i] + ctx.targetSpell->m_effectDieSides[i]);
        if (std::strcmp(field, "multiplevalue") == 0)
        {
            integerValue = false;
            float value = ctx.targetSpell->m_effectAmplitude[i];
            ApplyOwnedTooltipSpellMod(ctx.targetSpell, SpellMods::ValueMult, value);
            return value;
        }
        if (std::strcmp(field, "period") == 0 || std::strcmp(field, "periodms") == 0)
        {
            float value = static_cast<float>((ctx.targetSpell->m_procTypeMask & 1) ? 5000 : ctx.targetSpell->m_effectAuraPeriod[i]);
            if (!(ctx.targetSpell->m_procTypeMask & 1))
                ApplyOwnedTooltipSpellMod(ctx.targetSpell, SpellMods::Period, value);
            return value;
        }
        if (std::strcmp(field, "radius") == 0)
        {
            integerValue = false;
            float value = OwnedTooltipRadiusValue(ctx.targetSpell->m_effectRadiusIndex[i]);
            ApplyOwnedTooltipSpellMod(ctx.targetSpell, SpellMods::Radius, value);
            return value;
        }
        if (std::strcmp(field, "chaintargets") == 0)
            return static_cast<float>(ctx.targetSpell->m_effectChainTargets[i]);
        if (std::strcmp(field, "chainamplitude") == 0)
        {
            integerValue = false;
            return ctx.targetSpell->m_effectChainAmplitude[i];
        }
        if (std::strcmp(field, "misc") == 0 || std::strcmp(field, "misca") == 0)
            return static_cast<float>(ctx.targetSpell->m_effectMiscValue[i]);
        if (std::strcmp(field, "miscb") == 0)
            return static_cast<float>(ctx.targetSpell->m_effectMiscValueB[i]);
        if (std::strcmp(field, "triggerspell") == 0)
            return static_cast<float>(ctx.targetSpell->m_effectTriggerSpell[i]);
        if (std::strcmp(field, "pointspercombo") == 0)
        {
            integerValue = false;
            return ctx.targetSpell->m_effectPointsPerCombo[i];
        }
        if (std::strcmp(field, "bonuscoefficient") == 0 || std::strcmp(field, "coefficient") == 0)
        {
            integerValue = false;
            float value = ctx.targetSpell->m_effectBonusCoefficient[i];
            ApplyOwnedTooltipSpellMod(ctx.targetSpell, SpellMods::ScalingRatio, value);
            return value;
        }
        if (std::strcmp(field, "pointsperlevel") == 0)
        {
            integerValue = false;
            return ctx.targetSpell->m_effectRealPointsPerLevel[i];
        }
        if (std::strcmp(field, "targets") == 0) {
            float value = static_cast<float>(ctx.targetSpell->m_maxTargets);
            ApplyOwnedTooltipSpellMod(ctx.targetSpell, SpellMods::TargetCount, value);
            return value;
        }

        return 0.0f;
    }

    uint32_t OwnedTooltipEffectType(OwnedTooltipMacroContext& ctx)
    {
        if (ctx.effect)
            return ctx.effect->effect;

        if (ctx.targetUsesStreamedEffects)
            return 0;

        if (ctx.targetSpell && ctx.effectIndex < 3)
            return ctx.targetSpell->m_effect[ctx.effectIndex];

        return 0;
    }

    uint32_t OwnedTooltipEffectAuraType(OwnedTooltipMacroContext& ctx)
    {
        if (ctx.effect)
            return ctx.effect->effectApplyAuraName;

        if (ctx.targetUsesStreamedEffects)
            return 0;

        if (ctx.targetSpell && ctx.effectIndex < 3)
            return ctx.targetSpell->m_effectAura[ctx.effectIndex];

        return 0;
    }

    uint32_t OwnedTooltipEffectMechanic(OwnedTooltipMacroContext& ctx)
    {
        if (ctx.effect)
            return ctx.effect->effectMechanic;

        if (ctx.targetUsesStreamedEffects)
            return 0;

        if (ctx.targetSpell && ctx.effectIndex < 3)
            return ctx.targetSpell->m_effectMechanic[ctx.effectIndex];

        return 0;
    }

    bool OwnedTooltipValueIsBleed(OwnedTooltipMacroContext& ctx, uint32_t effectType)
    {
        if (!OwnedTooltipValueIsDamage(ctx, effectType))
            return false;

        uint32_t const effectMechanic = OwnedTooltipEffectMechanic(ctx);
        if (effectMechanic == SpellMechanics::Bleed)
            return true;

        return ctx.targetSpell && ctx.targetSpell->m_mechanic == SpellMechanics::Bleed;
    }

    bool IsOwnedTooltipPeriodicAura(uint32_t auraType)
    {
        return auraType == Auras::PeriodicDamage || auraType == Auras::PeriodicHeal ||
               auraType == Auras::PeriodicEnergize || auraType == Auras::PeriodicLeech ||
               auraType == Auras::PeriodicHealthFunnel || auraType == Auras::PeriodicManaLeech ||
               auraType == Auras::PeriodicDamagePct || auraType == Auras::PeriodicDummy ||
               auraType == Auras::PeriodicTriggerSpellWithValue;
    }

    bool OwnedTooltipEffectIsPeriodic(OwnedTooltipMacroContext& ctx)
    {
        return OwnedTooltipEffectType(ctx) == SpellEffects::ApplyAura
            && IsOwnedTooltipPeriodicAura(OwnedTooltipEffectAuraType(ctx));
    }

    bool OwnedTooltipValueIsDamage(OwnedTooltipMacroContext& ctx, uint32_t effectType)
    {
        if (IsOwnedTooltipDamageEffect(effectType))
            return true;

        return effectType == SpellEffects::ApplyAura && OwnedTooltipEffectAuraType(ctx) == Auras::PeriodicDamage;
    }

    bool OwnedTooltipValueIsHealing(OwnedTooltipMacroContext& ctx, uint32_t effectType)
    {
        if (IsOwnedTooltipHealingEffect(effectType))
            return true;

        return effectType == SpellEffects::ApplyAura && OwnedTooltipEffectAuraType(ctx) == Auras::PeriodicHeal;
    }

    bool OwnedTooltipValueIsHealAmount(OwnedTooltipMacroContext& ctx, uint32_t effectType)
    {
        if (IsOwnedTooltipHealAmountEffect(effectType))
            return true;

        return effectType == SpellEffects::ApplyAura && OwnedTooltipEffectAuraType(ctx) == Auras::PeriodicHeal;
    }

    uint32_t OwnedTooltipValueDamageType(OwnedTooltipMacroContext& ctx, uint32_t effectType)
    {
        if (OwnedTooltipValueIsBleed(ctx, effectType))
            return kTooltipDamageTypeBleed;

        return ctx.targetSpell ? OwnedTooltipSpellSchoolMask(ctx.targetSpell) : 0;
    }

    uint32_t OwnedTooltipEffectPeriodMs(OwnedTooltipMacroContext& ctx)
    {
        float period = 0.0f;
        if (ctx.targetSpell && (ctx.targetSpell->m_procTypeMask & 1))
            period = 5000.0f;
        else if (ctx.effect)
            period = static_cast<float>(ctx.effect->effectAmplitude);
        else if (ctx.targetUsesStreamedEffects)
            period = 0.0f;
        else if (ctx.targetSpell && ctx.effectIndex < 3)
            period = static_cast<float>(ctx.targetSpell->m_effectAuraPeriod[ctx.effectIndex]);

        if (period <= 0.0f)
            return 0;

        if (!(ctx.targetSpell && (ctx.targetSpell->m_procTypeMask & 1)))
            ApplyOwnedTooltipSpellMod(ctx.targetSpell, SpellMods::Period, period);

        return period > 0.0f ? static_cast<uint32_t>(period) : 0;
    }

    uint32_t OwnedTooltipEffectDurationMs(OwnedTooltipMacroContext& ctx)
    {
        if (!ctx.targetSpell)
            return 0;

        float duration = static_cast<float>(OwnedTooltipDurationMs(ctx.targetSpell));
        if (duration <= 0.0f)
            return 0;

        ApplyOwnedTooltipSpellMod(ctx.targetSpell, SpellMods::Duration, duration);
        if (duration <= 0.0f)
            return 0;

        if (duration > 30000.0f)
            duration = 30000.0f;

        return static_cast<uint32_t>(duration);
    }

    struct OwnedTooltipTickInfo
    {
        uint32_t wholeTicks = 1;
        float amountMultiplier = 1.0f;
    };

    float OwnedTooltipEffectHastedPeriodMs(uint32_t period)
    {
        if (!period)
            return 0.0f;

        CGPlayer* activePlayer = reinterpret_cast<CGPlayer*>(ClntObjMgr::ObjectPtr(ClntObjMgr::GetActivePlayer(), TYPEMASK_PLAYER));
        if (!activePlayer || !activePlayer->unitBase.unitData)
            return static_cast<float>(period);

        float castSpeed = activePlayer->unitBase.unitData->modCastSpell;
        if (castSpeed <= 0.0f)
            return static_cast<float>(period);

        return static_cast<float>(period) * castSpeed;
    }

    OwnedTooltipTickInfo OwnedTooltipEffectTickInfo(OwnedTooltipMacroContext& ctx)
    {
        OwnedTooltipTickInfo info;
        if (!ctx.targetSpell || !OwnedTooltipEffectIsPeriodic(ctx))
            return info;

        uint32_t const duration = OwnedTooltipEffectDurationMs(ctx);
        uint32_t const period = OwnedTooltipEffectPeriodMs(ctx);
        if (!duration || !period)
            return info;

        float const hastedPeriod = OwnedTooltipEffectHastedPeriodMs(period);
        uint32_t const runtimePeriod = hastedPeriod > 0.0f ? static_cast<uint32_t>(hastedPeriod) : period;
        if (!runtimePeriod)
            return info;

        info.wholeTicks = duration / runtimePeriod;
        if (ctx.targetSpell && (ctx.targetSpell->m_attributesExE & Spellattr5::StartPeriodicAtApply))
            ++info.wholeTicks;
        info.wholeTicks = (std::max)(1u, info.wholeTicks);

        uint32_t const auraType = OwnedTooltipEffectAuraType(ctx);
        if ((auraType == Auras::PeriodicDamage || auraType == Auras::PeriodicHeal) && hastedPeriod > 0.0f)
        {
            float exactTicks = static_cast<float>(duration) / hastedPeriod;
            if (ctx.targetSpell && (ctx.targetSpell->m_attributesExE & Spellattr5::StartPeriodicAtApply))
                exactTicks += 1.0f;

            if (exactTicks > static_cast<float>(info.wholeTicks))
                info.amountMultiplier = exactTicks / static_cast<float>(info.wholeTicks);
        }

        return info;
    }

    uint32_t OwnedTooltipActivePlayerLevel()
    {
        CGPlayer* activePlayer = reinterpret_cast<CGPlayer*>(ClntObjMgr::ObjectPtr(ClntObjMgr::GetActivePlayer(), TYPEMASK_PLAYER));
        return activePlayer && activePlayer->unitBase.unitData ? activePlayer->unitBase.unitData->level : 0;
    }

    float OwnedTooltipEffectMinPoints(OwnedTooltipMacroContext& ctx)
    {
        if (ctx.effect)
            return GetStreamedEffectMinPoints(ctx.targetSpell, *ctx.effect, OwnedTooltipActivePlayerLevel(), 0, 0, 0);

        if (ctx.targetUsesStreamedEffects)
            return 0.0f;

        if (ctx.targetSpell && ctx.effectIndex < 3)
            return GetNativeEffectMinPoints(ctx.targetSpell, ctx.effectIndex, OwnedTooltipActivePlayerLevel(), 0, 0, 0);

        return 0.0f;
    }

    float OwnedTooltipEffectScalarValue(OwnedTooltipMacroContext& ctx)
    {
        CGPlayer* activePlayer = reinterpret_cast<CGPlayer*>(ClntObjMgr::ObjectPtr(ClntObjMgr::GetActivePlayer(), TYPEMASK_PLAYER));
        if (ctx.effect)
            return ApplyScalarValuesForPlayer(
                activePlayer,
                ctx.targetSpell,
                ctx.effect->effectSpellPowerBonus,
                ctx.effect->effectAttackPowerBonus,
                ctx.effect->effectBlockValueBonus,
                ctx.effect->effectScalingMode);

        if (ctx.targetUsesStreamedEffects)
            return 0.0f;

        return GetSpellScalarsForEffect(activePlayer, ctx.targetSpell, static_cast<int>(ctx.effectIndex));
    }

    void AppendOwnedTooltipValueMacro(OwnedTooltipMacroContext& ctx, std::string& out, bool totalPeriodic)
    {
        float const baseValue = OwnedTooltipEffectMinPoints(ctx);
        float value = baseValue + OwnedTooltipEffectScalarValue(ctx);
        uint32_t const effectType = OwnedTooltipEffectType(ctx);
        RecordOwnedTooltipExpressionValue(ctx, effectType);
        OwnedTooltipValueFormulaParts formulaParts;
        const char* valueColor = OwnedTooltipValueColor(ctx, effectType);

        if (IsTooltipFormulaExpanded() && s_ownedTooltipExpressionEvaluationDepth == 0)
        {
            formulaParts.enabled = true;
            formulaParts.baseValue = baseValue;
            formulaParts.scalarFormula = ctx.effect
                ? BuildStreamedEffectScalarFormula(*ctx.effect, valueColor)
                : BuildSpellScalarFormulaForEffect(ctx.targetSpell, static_cast<int>(ctx.effectIndex), valueColor);
        }

        ApplyOwnedTooltipSpellModWithParts(ctx.targetSpell, SpellMods::AllEffects, value, formulaParts);
        if (OwnedTooltipValueIsHealing(ctx, effectType))
            ApplyOwnedTooltipSpellModWithParts(ctx.targetSpell, SpellMods::Healing, value, formulaParts);
        else if (OwnedTooltipValueIsDamage(ctx, effectType))
            ApplyOwnedTooltipSpellModWithParts(ctx.targetSpell, SpellMods::Damage, value, formulaParts);

        OwnedTooltipTickInfo const tickInfo = OwnedTooltipEffectTickInfo(ctx);
        if (tickInfo.amountMultiplier != 1.0f)
        {
            value = std::round(value * tickInfo.amountMultiplier);
            if (formulaParts.enabled)
                formulaParts.percentMultiplier *= tickInfo.amountMultiplier;
        }

        if (totalPeriodic && tickInfo.wholeTicks > 1)
        {
            value *= static_cast<float>(tickInfo.wholeTicks);
            if (formulaParts.enabled)
                formulaParts.percentMultiplier *= static_cast<float>(tickInfo.wholeTicks);
        }

        value = std::fabs(value);
        std::string formula = BuildOwnedTooltipValueFormula(formulaParts, valueColor);
        std::string suffix = OwnedTooltipValueSuffix(ctx, effectType);
        if (valueColor)
            out.append(valueColor);
        AppendOwnedTooltipNumber(out, value, true);
        if (!formula.empty())
        {
            if (valueColor)
                out.append("|r");
            out.append(" = ");
            out.append(formula);
            if (valueColor && !suffix.empty())
                out.append(valueColor);
        }
        out.append(suffix);
        if (valueColor)
            out.append("|r");
    }

    bool MacroEffectNumber(OwnedTooltipMacroContext& ctx, std::string& out, const char* field)
    {
        if (!ResolveOwnedTooltipEffectMacro(ctx))
            return false;

        bool integerValue = true;
        float value = 0.0f;
        if (std::strcmp(field, "value") == 0 || std::strcmp(field, "min") == 0)
        {
            AppendOwnedTooltipValueMacro(ctx, out, true);
            return true;
        }
        if (std::strcmp(field, "tickvalue") == 0)
        {
            AppendOwnedTooltipValueMacro(ctx, out, false);
            return true;
        }

        if (ctx.effect)
        {
            if (std::strcmp(field, "basepoints") == 0)
                value = static_cast<float>(ctx.effect->effectBasePoints);
            else if (std::strcmp(field, "max") == 0)
                value = static_cast<float>(ctx.effect->effectBasePoints + ctx.effect->effectDieSides);
            else if (std::strcmp(field, "multiplevalue") == 0)
            {
                value = ctx.effect->effectMultipleValue;
                ApplyOwnedTooltipSpellMod(ctx.targetSpell, SpellMods::ValueMult, value);
                integerValue = false;
            }
            else if (std::strcmp(field, "period") == 0 || std::strcmp(field, "periodms") == 0)
            {
                value = static_cast<float>((ctx.targetSpell && (ctx.targetSpell->m_procTypeMask & 1)) ? 5000 : ctx.effect->effectAmplitude);
                if (!(ctx.targetSpell && (ctx.targetSpell->m_procTypeMask & 1)))
                    ApplyOwnedTooltipSpellMod(ctx.targetSpell, SpellMods::Period, value);
            }
            else if (std::strcmp(field, "radius") == 0)
            {
                value = OwnedTooltipRadiusValue(ctx.effect->effectRadiusIndex);
                ApplyOwnedTooltipSpellMod(ctx.targetSpell, SpellMods::Radius, value);
                integerValue = false;
            }
            else if (std::strcmp(field, "chaintargets") == 0)
                value = static_cast<float>(ctx.effect->effectChainTargets);
            else if (std::strcmp(field, "chainamplitude") == 0)
            {
                value = ctx.effect->effectChainAmplitude;
                integerValue = false;
            }
            else if (std::strcmp(field, "misc") == 0 || std::strcmp(field, "misca") == 0)
                value = static_cast<float>(ctx.effect->effectMiscValue);
            else if (std::strcmp(field, "miscb") == 0)
                value = static_cast<float>(ctx.effect->effectMiscValueB);
            else if (std::strcmp(field, "triggerspell") == 0)
                value = static_cast<float>(ctx.effect->effectTriggerSpell);
            else if (std::strcmp(field, "pointspercombo") == 0)
            {
                value = ctx.effect->effectPointsPerCombo;
                integerValue = false;
            }
            else if (std::strcmp(field, "bonuscoefficient") == 0 || std::strcmp(field, "coefficient") == 0)
            {
                value = ctx.effect->effectBonusMultiplier;
                ApplyOwnedTooltipSpellMod(ctx.targetSpell, SpellMods::ScalingRatio, value);
                integerValue = false;
            }
            else if (std::strcmp(field, "pointsperlevel") == 0)
            {
                value = ctx.effect->effectRealPointsPerLevel;
                integerValue = false;
            }
            else
                return false;
        }
        else if (ctx.targetUsesStreamedEffects)
            value = 0.0f;
        else
            value = NativeEffectField(ctx, field, integerValue);

        if (std::strcmp(field, "period") == 0)
        {
            AppendOwnedTooltipTimeValue(out, value);
            return true;
        }

        if (std::strcmp(field, "radius") == 0)
            return AppendMacroRadius(out, value);

        return AppendMacroNumber(out, value, integerValue);
    }

    bool MacroSchoolDamage(OwnedTooltipMacroContext& ctx, std::string& out)
    {
        if (!ResolveOwnedTooltipSpellMacro(ctx))
        {
            if (ctx.args.size() == 1)
            {
                uint32_t const spellId = ctx.args[0];
                MarkOwnedSpellTooltipAwaitingSpell(spellId);
                SpellCacheStreaming::RequestSpell(spellId, UINT32_MAX);
                return true;
            }

            return false;
        }

        if (!ctx.targetSpell)
            return true;

        uint32_t const schoolMask = OwnedTooltipSpellSchoolMask(ctx.targetSpell);
        const char* schoolName = OwnedTooltipSchoolName(schoolMask);
        if (!schoolName)
            return true;

        const char* schoolColor = s_ownedTooltipExpressionEvaluationDepth == 0
            ? OwnedTooltipSchoolColor(schoolMask)
            : nullptr;
        if (schoolColor)
            out.append(schoolColor);
        out.append(schoolName);
        out.append(" damage");
        if (schoolColor)
            out.append("|r");
        return true;
    }

    bool MacroMastery(OwnedTooltipMacroContext& ctx, std::string& out)
    {
        if (ctx.args.size() > 1)
            return false;

        uint32_t specIndex = ctx.args.empty() ? 0 : ctx.args[0];
        if (specIndex < 1 || specIndex > 4)
            return false;

        return AppendMacroNumber(out, CharacterDefines::getMasteryForSpec(specIndex - 1), false);
    }

    bool MacroAttackPower(OwnedTooltipMacroContext& ctx, std::string& out)
    {
        if (!ResolveOwnedTooltipSpellMacro(ctx))
            return false;

        CGPlayer* activePlayer = reinterpret_cast<CGPlayer*>(ClntObjMgr::ObjectPtr(ClntObjMgr::GetActivePlayer(), TYPEMASK_PLAYER));
        return AppendMacroNumber(out, GetAttackPowerBonusForSpell(activePlayer, ctx.targetSpell), true);
    }

    bool MacroProcChance(OwnedTooltipMacroContext& ctx, std::string& out)
    {
        if (!ResolveOwnedTooltipSpellMacro(ctx))
            return false;

        return AppendMacroNumber(out, ctx.targetSpell ? static_cast<float>(ctx.targetSpell->m_procChance) : 0.0f, true);
    }

    bool MacroProcCharges(OwnedTooltipMacroContext& ctx, std::string& out)
    {
        if (!ResolveOwnedTooltipSpellMacro(ctx))
            return false;

        return AppendMacroNumber(out, ctx.targetSpell ? static_cast<float>(ctx.targetSpell->m_procCharges) : 0.0f, true);
    }

    bool MacroMaxTargets(OwnedTooltipMacroContext& ctx, std::string& out)
    {
        if (!ResolveOwnedTooltipSpellMacro(ctx))
            return false;

        return AppendMacroNumber(out, ctx.targetSpell ? static_cast<float>(ctx.targetSpell->m_maxTargets) : 0.0f, true);
    }

    using OwnedTooltipMacroHandler = bool (*)(OwnedTooltipMacroContext&, std::string&);

    bool MacroValue(OwnedTooltipMacroContext& ctx, std::string& out) { return MacroEffectNumber(ctx, out, "value"); }
    bool MacroTickValue(OwnedTooltipMacroContext& ctx, std::string& out) { return MacroEffectNumber(ctx, out, "tickvalue"); }
    bool MacroBasePoints(OwnedTooltipMacroContext& ctx, std::string& out) { return MacroEffectNumber(ctx, out, "basepoints"); }
    bool MacroMin(OwnedTooltipMacroContext& ctx, std::string& out) { return MacroEffectNumber(ctx, out, "min"); }
    bool MacroMax(OwnedTooltipMacroContext& ctx, std::string& out) { return MacroEffectNumber(ctx, out, "max"); }
    bool MacroMultipleValue(OwnedTooltipMacroContext& ctx, std::string& out) { return MacroEffectNumber(ctx, out, "multiplevalue"); }
    bool MacroPeriod(OwnedTooltipMacroContext& ctx, std::string& out) { return MacroEffectNumber(ctx, out, "period"); }
    bool MacroPeriodMs(OwnedTooltipMacroContext& ctx, std::string& out) { return MacroEffectNumber(ctx, out, "periodms"); }
    bool MacroRadius(OwnedTooltipMacroContext& ctx, std::string& out) { return MacroEffectNumber(ctx, out, "radius"); }
    bool MacroChainTargets(OwnedTooltipMacroContext& ctx, std::string& out) { return MacroEffectNumber(ctx, out, "chaintargets"); }
    bool MacroChainAmplitude(OwnedTooltipMacroContext& ctx, std::string& out) { return MacroEffectNumber(ctx, out, "chainamplitude"); }
    bool MacroMisc(OwnedTooltipMacroContext& ctx, std::string& out) { return MacroEffectNumber(ctx, out, "misc"); }
    bool MacroMiscA(OwnedTooltipMacroContext& ctx, std::string& out) { return MacroEffectNumber(ctx, out, "misca"); }
    bool MacroMiscB(OwnedTooltipMacroContext& ctx, std::string& out) { return MacroEffectNumber(ctx, out, "miscb"); }
    bool MacroTriggerSpell(OwnedTooltipMacroContext& ctx, std::string& out) { return MacroEffectNumber(ctx, out, "triggerspell"); }
    bool MacroPointsPerCombo(OwnedTooltipMacroContext& ctx, std::string& out) { return MacroEffectNumber(ctx, out, "pointspercombo"); }
    bool MacroBonusCoefficient(OwnedTooltipMacroContext& ctx, std::string& out) { return MacroEffectNumber(ctx, out, "bonuscoefficient"); }
    bool MacroCoefficient(OwnedTooltipMacroContext& ctx, std::string& out) { return MacroEffectNumber(ctx, out, "coefficient"); }
    bool MacroPointsPerLevel(OwnedTooltipMacroContext& ctx, std::string& out) { return MacroEffectNumber(ctx, out, "pointsperlevel"); }
    bool MacroTargetcount(OwnedTooltipMacroContext& ctx, std::string& out) { return MacroEffectNumber(ctx, out, "targets"); }

    const std::unordered_map<std::string, OwnedTooltipMacroHandler>& OwnedTooltipMacroDefinitions()
    {
        static const std::unordered_map<std::string, OwnedTooltipMacroHandler> definitions = {
            { "duration", MacroDuration },
            { "cooldown", MacroCooldown },
            { "name", MacroName },
            { "icon", MacroIcon },
            { "stacks", MacroStacks },
            { "schooldamage", MacroSchoolDamage },
            { "mastery", MacroMastery },
            { "attackpower", MacroAttackPower },
            { "procchance", MacroProcChance },
            { "proccharges", MacroProcCharges },
            { "maxtargets", MacroMaxTargets },
            { "value", MacroValue },
            { "tickvalue", MacroTickValue },
            { "basepoints", MacroBasePoints },
            { "min", MacroMin },
            { "max", MacroMax },
            { "multiplevalue", MacroMultipleValue },
            { "period", MacroPeriod },
            { "periodms", MacroPeriodMs },
            { "radius", MacroRadius },
            { "radious", MacroRadius },
            { "chaintargets", MacroChainTargets },
            { "chainamplitude", MacroChainAmplitude },
            { "misc", MacroMisc },
            { "misca", MacroMiscA },
            { "miscb", MacroMiscB },
            { "triggerspell", MacroTriggerSpell },
            { "pointspercombo", MacroPointsPerCombo },
            { "bonuscoefficient", MacroBonusCoefficient },
            { "coefficient", MacroCoefficient },
            { "pointsperlevel", MacroPointsPerLevel },
            { "Targets", MacroTargetcount },
        };

        return definitions;
    }

    bool TryAppendOwnedTooltipMacro(SpellRow* spell, const char* text, size_t textLen, size_t& pos, std::string& out)
    {
        if (pos >= textLen || text[pos] != '@')
            return false;

        size_t const macroStart = pos;
        size_t nameStart = pos + 1;
        if (nameStart >= textLen || !std::isalpha(static_cast<unsigned char>(text[nameStart])))
            return false;

        size_t nameEnd = nameStart + 1;
        while (nameEnd < textLen && std::isalpha(static_cast<unsigned char>(text[nameEnd])))
            ++nameEnd;

        std::string macro(text + nameStart, nameEnd - nameStart);
        size_t afterName = nameEnd;
        if (NormalizeMacroName(macro) == "astime")
        {
            if (!TryAppendOwnedTooltipAsTimeMacro(spell, text, textLen, afterName, out))
                return false;
            pos = afterName;
            return true;
        }

        std::vector<uint32_t> args;
        if (!TryParseMacroArguments(text, textLen, afterName, args))
            return false;

        std::string normalizedMacro = NormalizeMacroName(macro);
        auto const& definitions = OwnedTooltipMacroDefinitions();
        auto it = definitions.find(normalizedMacro);
        if (it == definitions.end())
            return false;

        OwnedTooltipMacroContext ctx;
        ctx.currentSpell = spell;
        ctx.args = args;
        if (!it->second(ctx, out))
            return false;

        if (normalizedMacro == "schooldamage" && afterName < textLen && std::isspace(static_cast<unsigned char>(text[afterName])))
        {
            static constexpr const char* kColorEnd = "|r";
            static constexpr size_t kColorEndLen = 2;
            if (out.size() >= kColorEndLen && out.compare(out.size() - kColorEndLen, kColorEndLen, kColorEnd) == 0)
            {
                out.push_back(text[afterName++]);
            }
        }

        pos = afterName;
        return true;
    }

    bool TryAppendOwnedTooltipPluralToken(const char* text, size_t textLen, size_t& pos, std::string& out)
    {
        if (pos + 1 >= textLen || text[pos] != '$' || text[pos + 1] != 'l')
            return false;

        size_t singularStart = pos + 2;
        size_t colon = singularStart;
        while (colon < textLen && text[colon] != ':' && text[colon] != ';')
            ++colon;

        if (colon >= textLen || text[colon] != ':')
            return false;

        size_t pluralStart = colon + 1;
        size_t end = pluralStart;
        while (end < textLen && text[end] != ';')
            ++end;

        if (end >= textLen)
            return false;

        bool const singular = s_ownedTooltipHasLastPluralValue
            && std::fabs(std::fabs(s_ownedTooltipLastPluralValue) - 1.0f) < 0.0001f;
        if (singular)
            out.append(text + singularStart, colon - singularStart);
        else
            out.append(text + pluralStart, end - pluralStart);

        pos = end + 1;
        return true;
    }

    bool ParseOwnedTooltipTextToString(SpellRow* spell, const char* source, size_t textLen, std::string& out)
    {
        out.clear();
        if (!spell || !source)
            return false;

        bool const rootParse = s_ownedTooltipTextParseDepth == 0;
        if (rootParse)
        {
            s_ownedTooltipHasLastPluralValue = false;
            s_ownedTooltipLastPluralValue = 0.0f;
        }

        ++s_ownedTooltipTextParseDepth;
        out.reserve(textLen);
        for (size_t pos = 0; pos < textLen;)
        {
            if (TryAppendOwnedTooltipMathBlock(spell, source, textLen, pos, out))
                continue;

            if (TryAppendOwnedTooltipMacro(spell, source, textLen, pos, out))
                continue;

            if (TryAppendOwnedTooltipPluralToken(source, textLen, pos, out))
                continue;

            out.push_back(source[pos++]);
        }

        --s_ownedTooltipTextParseDepth;
        return true;
    }

    bool ShouldUseOwnedSpellTooltipParser(SpellRow* spell)
    {
        if (!spell)
            return false;

        if (spell->m_ID >= kTSSpellIdStart)
            return true;

        return SpellCacheStreaming::GetSpellEffectCount(spell->m_ID) > 3;
    }

    bool OwnedTooltipDescriptionHasValueMacro(const char* description)
    {
        if (!description)
            return false;

        for (const char* pos = description; *pos; ++pos)
        {
            if (pos[0] != '@')
                continue;

            if (pos[1] == '{' || std::isalpha(static_cast<unsigned char>(pos[1])))
                return true;
        }

        return false;
    }

    void RecordOwnedSpellTooltipRefreshState(
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
        int a16,
        SpellRow* spell)
    {
        if (s_ownedSpellTooltipRefresh.refreshing)
            return;

        if (a11 || !ShouldUseOwnedSpellTooltipParser(spell) || !OwnedTooltipDescriptionHasValueMacro(spell ? spell->m_description_lang : nullptr))
        {
            s_ownedSpellTooltipRefresh.active = false;
            return;
        }

        s_ownedSpellTooltipRefresh.active = true;
        s_ownedSpellTooltipRefresh.shiftDown = IsTooltipFormulaExpanded();
        s_ownedSpellTooltipRefresh.tooltip = tooltip;
        s_ownedSpellTooltipRefresh.spellId = spellId;
        s_ownedSpellTooltipRefresh.a3 = a3;
        s_ownedSpellTooltipRefresh.a4 = a4;
        s_ownedSpellTooltipRefresh.a5 = a5;
        s_ownedSpellTooltipRefresh.a6 = a6;
        s_ownedSpellTooltipRefresh.a7 = a7;
        s_ownedSpellTooltipRefresh.a8 = a8;
        s_ownedSpellTooltipRefresh.hasA9 = a9 != nullptr;
        s_ownedSpellTooltipRefresh.a9Value = a9 ? *a9 : 0;
        s_ownedSpellTooltipRefresh.a10 = a10;
        s_ownedSpellTooltipRefresh.a12 = a12;
        s_ownedSpellTooltipRefresh.a13 = a13;
        s_ownedSpellTooltipRefresh.a14 = a14;
        s_ownedSpellTooltipRefresh.a15 = a15;
        s_ownedSpellTooltipRefresh.a16 = a16;
        s_ownedSpellTooltipRefresh.cacheRefreshPending = false;
        s_ownedSpellTooltipRefresh.awaitedSpellIds.clear();
    }

    void RecordPendingSpellTooltipRefreshState(
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
        int a12,
        int a13,
        int a14,
        int a15,
        int a16)
    {
        if (s_ownedSpellTooltipRefresh.refreshing || !tooltip || spellId <= 0)
            return;

        s_ownedSpellTooltipRefresh.active = true;
        s_ownedSpellTooltipRefresh.shiftDown = IsTooltipFormulaExpanded();
        s_ownedSpellTooltipRefresh.tooltip = tooltip;
        s_ownedSpellTooltipRefresh.spellId = spellId;
        s_ownedSpellTooltipRefresh.a3 = a3;
        s_ownedSpellTooltipRefresh.a4 = a4;
        s_ownedSpellTooltipRefresh.a5 = a5;
        s_ownedSpellTooltipRefresh.a6 = a6;
        s_ownedSpellTooltipRefresh.a7 = a7;
        s_ownedSpellTooltipRefresh.a8 = a8;
        s_ownedSpellTooltipRefresh.hasA9 = a9 != nullptr;
        s_ownedSpellTooltipRefresh.a9Value = a9 ? *a9 : 0;
        s_ownedSpellTooltipRefresh.a10 = a10;
        s_ownedSpellTooltipRefresh.a12 = a12;
        s_ownedSpellTooltipRefresh.a13 = a13;
        s_ownedSpellTooltipRefresh.a14 = a14;
        s_ownedSpellTooltipRefresh.a15 = a15;
        s_ownedSpellTooltipRefresh.a16 = a16;
        s_ownedSpellTooltipRefresh.cacheRefreshPending = false;
        s_ownedSpellTooltipRefresh.awaitedSpellIds.clear();
        MarkOwnedSpellTooltipAwaitingSpell(static_cast<uint32_t>(spellId));
    }

    bool ParseOwnedSpellTooltipText(SpellRow* spell, const char* source, char* dest, size_t destSize)
    {
        if (!spell || !source || !dest || destSize == 0)
            return false;

        size_t const textLen = std::strlen(source);
        std::string out;
        if (!ParseOwnedTooltipTextToString(spell, source, textLen, out))
            return false;

        if (out.size() >= destSize)
            out.resize(destSize - 1);

        std::memcpy(dest, out.data(), out.size());
        dest[out.size()] = '\0';
        return dest[0] != '\0';
    }

    bool SafeParseSpellTooltipDescription(SpellRow* spell, char* dest, size_t destSize, int a5, int a7)
    {
        if (!spell || !dest || destSize == 0) {
            return false;
        }

        dest[0] = '\0';
        if (ShouldUseOwnedSpellTooltipParser(spell))
            return ParseOwnedSpellTooltipText(spell, spell->m_description_lang, dest, destSize);

#ifdef _MSC_VER
        __try {
            TooltipVariableExtensions::ParseText(spell, dest, static_cast<uint32_t>(destSize), a5, a7, 0, 0, 1, 0);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            LOG_ERROR << "SpellTooltip parse failed for spellId=" << spell->m_ID;
            dest[0] = '\0';
            return false;
        }
#else
        TooltipVariableExtensions::ParseText(spell, dest, static_cast<uint32_t>(destSize), a5, a7, 0, 0, 1, 0);
#endif
        return dest[0] != '\0';
    }

    uint32_t GetSpellChargeRemainingCooldown(uint32_t spellId, uint32_t fallbackCooldown)
    {
        auto it = CharacterDefines::spellChargeMap.find(spellId);
        if (it == CharacterDefines::spellChargeMap.end())
            return fallbackCooldown;

        CharacterDefines::SpellCharge& charge = it->second;
        uint32_t remainingCooldown = fallbackCooldown;

        if (charge.remainingCooldown >= fallbackCooldown)
        {
            uint32_t currAsync = OsGetAsyncTimeMs();

            if (charge.remainingCooldown > (currAsync - charge.async))
                remainingCooldown = charge.remainingCooldown + (charge.async - currAsync);
            else
                remainingCooldown = 0;

            charge.remainingCooldown = remainingCooldown;
            charge.async = currAsync;
        }

        return remainingCooldown;
    }

    uint32_t GetSpellTooltipCooldown(SpellRow* spell, bool& isCharged)
    {
        uint32_t recoveryTime = spell->m_categoryRecoveryTime > spell->m_recoveryTime
            ? spell->m_categoryRecoveryTime
            : spell->m_recoveryTime;

        isCharged = false;

        auto it = CharacterDefines::spellChargeMap.find(spell->m_ID);
        if (it == CharacterDefines::spellChargeMap.end())
            return recoveryTime;

        CharacterDefines::SpellCharge const& charge = it->second;
        if (charge.cooldown)
            recoveryTime = charge.cooldown;
        else if (charge.remainingCooldown)
            recoveryTime = charge.remainingCooldown;

        isCharged = charge.maxCharges > 1;
        return recoveryTime;
    }
}

int TooltipExtensions::GetVariableValueEx(void* _this, uint32_t edx, uint32_t spellVariable, uint32_t a3, SpellRow* spell, uint32_t a5, uint32_t a6, uint32_t a7, uint32_t a8, uint32_t a9) {
    uint32_t result = 0;

    if (TryHandleStreamedEffectFormulaVariable(_this, spellVariable, a3, spell, a5, a6, a7, a8, a9))
        return a3;

    if (spellVariable < SPELLVARIABLE_hp)
        result = CFormula::GetVariableValue(_this, spellVariable, a3, spell, a5, a6, a7, a8, a9);
    else {
        float value = 0.f;
        CGPlayer* activePlayer = reinterpret_cast<CGPlayer*>(ClntObjMgr::ObjectPtr(ClntObjMgr::GetActivePlayer(), TYPEMASK_PLAYER));

        if (activePlayer) {
            uint32_t effectIndex = 0;
            // Arrays for current and max power fields
            if (TryGetCustomEffectIndex(spellVariable, SPELLVARIABLE_ppl1, SPELLVARIABLE_ppl4, effectIndex)) {
                if (!TryGetEffectRealPointsPerLevel(spell, effectIndex, value))
                    *reinterpret_cast<uint32_t*>(_this) = 1;
            }
            else if (TryGetCustomEffectIndex(spellVariable, SPELLVARIABLE_PPL1, SPELLVARIABLE_PPL4, effectIndex)) {
                if (TryGetEffectRealPointsPerLevel(spell, effectIndex, value))
                    value *= activePlayer->unitBase.unitData->level;
                else
                    *reinterpret_cast<uint32_t*>(_this) = 1;
            }
            else if (TryGetCustomEffectIndex(spellVariable, SPELLVARIABLE_bon1, SPELLVARIABLE_bon4, effectIndex)) {
                if (!TryGetEffectBonusValue(activePlayer, spell, effectIndex, a5, a8, a7, a9, value))
                    *reinterpret_cast<uint32_t*>(_this) = 1;
            }
            else if (spellVariable >= SPELLVARIABLE_power1 && spellVariable <= SPELLVARIABLE_power7) {
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
                    default:
                        *reinterpret_cast<uint32_t*>(_this) = 1;
                        break;
                }
            }
        }

        result = a3;
        PushFormulaValue(a3, value);
    }

    return result;
}

void TooltipExtensions::SetNewVariablePointers() {
    size_t variableCount = 0;
    const char* const* tooltipSpellVariablesExtensions =
        TooltipVariableExtensions::GetExtendedSpellVariableNames(variableCount);

    for (size_t i = 0; i < variableCount; i++)
        spellVariables[SPELLVARIABLE_STOCK_COUNT + i] = reinterpret_cast<uint32_t>(tooltipSpellVariablesExtensions[i]);
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

    bool isCharged = false;
    double recoveryTime = GetSpellTooltipCooldown(spell, isCharged);

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

    CGTooltip::AddLine(_this, dest, src, sColorHexWhite, sColorHexWhite, 0);
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
    uint32_t recoveryTime = GetSpellChargeRemainingCooldown(spell->m_ID, currentCooldown);

    if (recoveryTime) {
        CGTooltip::GetDurationString(dest, 128, recoveryTime, "ITEM_COOLDOWN_TIME", 0, 1, 0);
        CGTooltip::AddLine(_this, dest, 0, sColorHexWhite, sColorHexWhite, 0);
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

int __fastcall TooltipExtensions::SetSpellTooltipHook(void* thisPtr, void* edx, int spellId, int a3, int a4, int a5, int a6, int a7, int a8, uint32_t* a9, int a10, int a11, int a12, int a13, int a14, int a15, int a16)
{
    return SetSpellTooltipImpl(thisPtr, spellId, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16);
}

void TooltipExtensions::PumpShiftTooltipRefresh()
{
    if (!s_ownedSpellTooltipRefresh.active
        || s_ownedSpellTooltipRefresh.refreshing
        || !s_ownedSpellTooltipRefresh.tooltip)
        return;

    bool const shiftDown = IsTooltipFormulaExpanded();
    bool const cacheRefreshPending = s_ownedSpellTooltipRefresh.cacheRefreshPending;
    if (shiftDown == s_ownedSpellTooltipRefresh.shiftDown && !cacheRefreshPending)
        return;

    s_ownedSpellTooltipRefresh.shiftDown = shiftDown;
    s_ownedSpellTooltipRefresh.cacheRefreshPending = false;
    s_ownedSpellTooltipRefresh.awaitedSpellIds.clear();
    s_ownedSpellTooltipRefresh.refreshing = true;
    uint32_t a9Storage = s_ownedSpellTooltipRefresh.a9Value;
    uint32_t* a9 = s_ownedSpellTooltipRefresh.hasA9 ? &a9Storage : nullptr;
#ifdef _MSC_VER
    __try {
#endif
    SetSpellTooltipImpl(
        s_ownedSpellTooltipRefresh.tooltip,
        s_ownedSpellTooltipRefresh.spellId,
        s_ownedSpellTooltipRefresh.a3,
        s_ownedSpellTooltipRefresh.a4,
        s_ownedSpellTooltipRefresh.a5,
        s_ownedSpellTooltipRefresh.a6,
        s_ownedSpellTooltipRefresh.a7,
        s_ownedSpellTooltipRefresh.a8,
        a9,
        s_ownedSpellTooltipRefresh.a10,
        0,
        s_ownedSpellTooltipRefresh.a12,
        s_ownedSpellTooltipRefresh.a13,
        s_ownedSpellTooltipRefresh.a14,
        s_ownedSpellTooltipRefresh.a15,
        s_ownedSpellTooltipRefresh.a16);
#ifdef _MSC_VER
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        s_ownedSpellTooltipRefresh.active = false;
    }
#endif
    s_ownedSpellTooltipRefresh.refreshing = false;
}

void PumpSpellTooltipShiftRefresh()
{
    TooltipExtensions::PumpShiftTooltipRefresh();
}

void NotifySpellTooltipSpellCached(uint32_t spellId)
{
    MarkOwnedSpellTooltipCacheRefresh(spellId);
}

int TooltipExtensions::SetSpellTooltipImpl(void* tooltip, int spellId, int a3, int a4, int a5, int a6, int a7, int a8, uint32_t* a9, int a10, int a11, int a12, int a13, int a14, int a15, int a16)
{
    // Early out if we do not have a tooltip object.
    if (!tooltip)
        return 0;

    uint32_t resolvedSpellId = 0;
    if (spellId > 0
        && spellId <= 1024
        && !SpellCacheStreaming::HasSpell(static_cast<uint32_t>(spellId))
        && SpellCacheStreaming::TryResolveKnownSpellbookSlot(static_cast<uint32_t>(spellId), resolvedSpellId))
    {
        static uint32_t logCount = 0;
        if (logCount < 80)
        {
            LOG_INFO << "Resolved streamed spellbook tooltip slot"
                << "slot" << spellId
                << "spell" << resolvedSpellId
                << "a3" << a3
                << "a4" << a4
                << "a5" << a5
                << "a6" << a6
                << "a11" << a11;
            ++logCount;
        }

        spellId = static_cast<int>(resolvedSpellId);
    }

    // Clear tooltip if this is not an update call.
    if (!a11) {
        CGTooltipInternal::ClearTooltip(tooltip);
        *reinterpret_cast<uint32_t*>(static_cast<char*>(tooltip) + 217) = static_cast<uint32_t>(spellId);
        *reinterpret_cast<uint32_t*>(static_cast<char*>(tooltip) + 242) = 0;
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
        uint64_t targetGuid = *reinterpret_cast<uint64_t*>(kStruC24220);
        CGUnit* targetUnit = reinterpret_cast<CGUnit*>(ClntObjMgr::ObjectPtr(targetGuid, TYPEMASK_UNIT));
        if (targetUnit)
            unit = targetUnit;
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
        if (spellId > 0 && spellId <= 1000000)
        {
            uint32_t const requestedSpellId = static_cast<uint32_t>(spellId);
            RecordPendingSpellTooltipRefreshState(
                tooltip,
                spellId,
                a3,
                a4,
                a5,
                a6,
                a7,
                a8,
                a9,
                a10,
                a12,
                a13,
                a14,
                a15,
                a16);
            SpellCacheAuthority::AttachWaiter(
                requestedSpellId,
                { SpellCacheAuthority::WaiterKind::Tooltip, requestedSpellId, 0 });
            SpellCacheStreaming::RequestSpell(requestedSpellId);

            char loadingLeft[128] = {};
            SStr::Copy(loadingLeft, const_cast<char*>("Loading spell data..."), sizeof(loadingLeft));
            CGTooltip::AddLine(tooltip, loadingLeft, nullptr, sColorHexWhite, sColorHexWhite, 0);
            CSimpleFrame::Show(tooltip);
            LOG_INFO << "Displayed pending streamed spell tooltip"
                << "spell" << requestedSpellId
                << "a3" << a3
                << "a11" << a11;
            return 1;
        }

        if (!a11) {
            CSimpleFrame::Hide(tooltip);
        }
        return 0;
    }
    SpellRow* spell = &spellRow;
    RecordOwnedSpellTooltipRefreshState(tooltip, spellId, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, spell);

    // Basic line buffers.
    char lineLeft[128] = {};
    char lineRight[128] = {};

    // Ability/talent header flag: SPELL_ATTR_ABILITY (0x20) or effect in {53, 24, 157, 59} (disas 200-218).
    // v136: set if any effect is 24, 157, or 59 (used later for embedded item tooltip).
    int v141 = 0;
    int v136 = 0;
    if ((spell->m_attributes & 0x20) != 0) {
        v141 = 1;
        for (unsigned int i = 0; i < 3; ++i) {
            uint32_t eff = spell->m_effect[i];
            if (eff == 24 || eff == 157 || eff == 59) {
                v136 = 1;
                break;
            }
        }
    } else {
        for (unsigned int i = 0; i < 3; ++i) {
            uint32_t eff = spell->m_effect[i];
            if (eff == 53 || eff == 24 || eff == 157 || eff == 59) {
                v141 = 1;
                break;
            }
        }
        for (unsigned int i = 0; i < 3; ++i) {
            uint32_t eff = spell->m_effect[i];
            if (eff == 24 || eff == 157 || eff == 59) {
                v136 = 1;
                break;
            }
        }
    }

    // Top line: talent next rank, or ability (skill line) header, or normal name + subtext (disas 220-266).
    if (a9 && a11) {
        const char* nextRankText = FrameScript::GetText(const_cast<char*>("TOOLTIP_TALENT_NEXT_RANK"), -1, 0);
        if (nextRankText) {
            const char* fmt = reinterpret_cast<const char*>(0xA25978);
            SStr::Printf(lineLeft, sizeof(lineLeft), const_cast<char*>(fmt), nextRankText);
            CGTooltip::AddLine(tooltip, lineLeft, nullptr, sColorHexWhite, sColorHexWhite, 0);
        }
    } else if (v141) {
        bool addedAbilityLine = false;
        uint8_t raceId = unit->unitData->unitBytes0.raceID;
        uint8_t classId = unit->unitData->unitBytes0.classID;
        SkillLineAbilityRow* abilityRow = sub_812410(raceId, classId, static_cast<uint32_t>(spellId));
        if (abilityRow) {
            uint32_t skillLineId = abilityRow->m_skillLine;
            const uintptr_t skillLineDB = 0x00AD45E0;
            const uintptr_t base = skillLineDB + 4;
            uint32_t minID = *reinterpret_cast<uint32_t*>(base + 16);
            uint32_t maxID = *reinterpret_cast<uint32_t*>(base + 12);
            if (skillLineId >= minID && skillLineId <= maxID) {
                SkillLineRow* skillLineRow = reinterpret_cast<SkillLineRow*>(
                    ClientDB::GetRow(reinterpret_cast<void*>(skillLineDB), skillLineId - minID));
                if (skillLineRow && skillLineRow->m_displayName_lang && spell->m_name_lang) {
                    SStr::Printf(lineLeft, sizeof(lineLeft), "%s: %s",
                        skillLineRow->m_displayName_lang, spell->m_name_lang);
                    CGTooltip::AddLine(tooltip, lineLeft, nullptr, sColorHexDarkYellow, sColorHexDarkYellow, 0);
                    addedAbilityLine = true;
                }
            }
        }
        if (!addedAbilityLine && spell->m_name_lang && *spell->m_name_lang) {
            char* subText = (a3 || a6) ? spell->m_nameSubtext_lang : nullptr;
            SStr::Copy(lineLeft, spell->m_name_lang, sizeof(lineLeft));
            if (subText && *subText)
                SStr::Copy(lineRight, subText, sizeof(lineRight));
            CGTooltip::AddLine(tooltip, lineLeft, (subText && *subText) ? lineRight : nullptr, sColorHexWhite, sColorHexGrey0, 0);
        }
    } else {
        char* name = spell->m_name_lang;
        char* subText = (a3 || a6) ? spell->m_nameSubtext_lang : nullptr;
        if (name && *name) {
            SStr::Copy(lineLeft, name, sizeof(lineLeft));
            if (subText && *subText) {
                SStr::Copy(lineRight, subText, sizeof(lineRight));
            } else {
                lineRight[0] = 0;
            }
            CGTooltip::AddLine(tooltip, lineLeft, lineRight[0] ? lineRight : nullptr, sColorHexWhite, sColorHexGrey0, 0);
        }
    }

    // v137 = 1; if (a9 && !a11) { TOOLTIP_TALENT_RANK when a15 >= 0; if (a14 < 0) v137 = LookupEntryById(...); } (pseudo 255-265)
    int v137 = 1;
    if (a9 && !a11) {
        if (a15 >= 0) {
            const char* rankFmt = FrameScript::GetText(const_cast<char*>("TOOLTIP_TALENT_RANK"), -1, 0);
            if (rankFmt) {
                SStr::Printf(lineLeft, sizeof(lineLeft), const_cast<char*>(rankFmt), a14 + 1, a15 + 1);
                CGTooltip::AddLine(tooltip, lineLeft, nullptr, sColorHexWhite, sColorHexWhite, 0);
            }
        }
        if (a14 < 0)
            v137 = LookupEntryById(tooltip, a9, a10, a7, a5, a12);
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
                            CGTooltip::AddLine(tooltip, const_cast<char*>(" "), rangeStr, sColorHexWhite, sColorHexWhite, 0);
                            rangeStr[0] = '\0';
                        }
                    } else if (maxRange >= unlim) {
                        const char* u = FrameScript::GetText(const_cast<char*>("SPELL_RANGE_UNLIMITED"), -1, 0);
                        if (u) {
                            SStr::Copy(rangeStr, const_cast<char*>(u), sizeof(rangeStr));
                            CGTooltip::AddLine(tooltip, const_cast<char*>(" "), rangeStr, sColorHexWhite, sColorHexWhite, 0);
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
        CGTooltip::AddLine(
            tooltip,
            lineLeftFinal,
            lineRightFinal,
            sColorHexWhite,
            sColorHexWhite,
            0);
    }
    int v112 = 0;

    {
        char castBuf[128] = {};
        char cdBuf[128] = {};
        uintptr_t flag = 0;

        SetSpellCooldownTooltip(castBuf, spell, &flag, a5, a7, cdBuf, tooltip, Spell_C::GetPowerCost(spell, unit));
        if (cdBuf[0] != 0)
            v112 = 1;
    }

    // v155: gates min level line, reagents block, dodge/parry/block/crit (disas 476, 492-495).
    int v155 = 0;
    if (!a3 && !v141 && (spell->m_effect[0] == 47 || (spell->m_attributes & 0x40) != 0))
        v155 = 1;

    // Totems line (disas 559-634): only when not pet; show required totems and totem categories.
    const uintptr_t WDB_CACHE_ITEM = 0x00C5D828;
    const char* totemRedPrefix = reinterpret_cast<const char*>(0xAD2A5C);
    if (!a5) {
        char totemBuf[4096] = {};
        int spellIdb = 1;
        int firstEntry = 1;

        for (int i = 0; i < 2; ++i) {
            uint32_t totemId = spell->m_totem0[i];
            if (totemId == 0) continue;
            uint64_t guid = spell->m_ID | 0x1FE0000000000000ULL;
            void* itemBlock = DBItemCache_GetInfoBlockByID(reinterpret_cast<void*>(WDB_CACHE_ITEM), totemId, &guid, CGTooltip_HandleItemLoad, tooltip, 1);
            if (itemBlock) {
                if (firstEntry) {
                    firstEntry = 0;
                    const char* label = FrameScript::GetText(const_cast<char*>("SPELL_TOTEMS"), -1, 0);
                    if (label) SStr::Copy(totemBuf, const_cast<char*>(label), sizeof(totemBuf));
                } else {
                    SStr::Append(totemBuf, const_cast<char*>(", "), sizeof(totemBuf));
                }
                void* bagBase = reinterpret_cast<char*>(unit) + sizeof(CGUnit);
                void* found = CGBag_C__FindItemOfType(bagBase, totemId, 0);
                if (!found) {
                    SStr::Append(totemBuf, const_cast<char*>(totemRedPrefix), sizeof(totemBuf));
                    spellIdb = 0;
                }
                const char* itemName = reinterpret_cast<ItemCacheNameView*>(itemBlock)->namePtr;
                if (itemName) SStr::Append(totemBuf, const_cast<char*>(itemName), sizeof(totemBuf));
                if (!found)
                    SStr::Append(totemBuf, const_cast<char*>("|r"), sizeof(totemBuf));
            } else {
                (*reinterpret_cast<uint32_t*>(static_cast<char*>(tooltip) + 242))++;
            }
        }

        const uintptr_t totemCategoryDB = 0x00AD4C7C;
        const uintptr_t b_base_01 = totemCategoryDB;                          // WowClientDB_Common: m_minID/m_maxID inside here
        const uintptr_t m_recordsById_ptr = totemCategoryDB + 0x18 + 0x08;     // b_base_02.m_recordsById at +0x20
        uint32_t tmin = *reinterpret_cast<uint32_t*>(b_base_01 + 16);
        uint32_t tmax = *reinterpret_cast<uint32_t*>(b_base_01 + 12);
        TotemCategoryRow** tcatRecordsById = *reinterpret_cast<TotemCategoryRow***>(m_recordsById_ptr);
        for (int j = 0; j < 2; ++j) {
            uint32_t catId = spell->m_requiredTotemCategoryID[j];
            const uintptr_t totemCategoryDB = 0x00AD4C7C;

            uint32_t tmax = *reinterpret_cast<uint32_t*>(totemCategoryDB + 0x0C);
            uint32_t tmin = *reinterpret_cast<uint32_t*>(totemCategoryDB + 0x10);
            if (catId < tmin || catId > tmax)
                continue;

            uint32_t tcatIndex = catId - tmin;
            TotemCategoryRow* tcatRow = tcatRecordsById[tcatIndex];
            if (!tcatRow || !tcatRow->m_name) continue;
            
            if (firstEntry) {
                firstEntry = 0;
                const char* label = FrameScript::GetText(const_cast<char*>("SPELL_TOTEMS"), -1, 0);
                if (label) 
                    SStr::Copy(totemBuf, const_cast<char*>(label), sizeof(totemBuf));
            } else {
                SStr::Append(totemBuf, const_cast<char*>(", "), sizeof(totemBuf));
            }
            void* hasTotemThis = reinterpret_cast<char*>(activePlayer) + 0x18F0;
            if (Player_HasTotemCategory(hasTotemThis, catId, 0)) {
                SStr::Append(totemBuf, const_cast<char*>(tcatRow->m_name), sizeof(totemBuf));
            } else {
                SStr::Append(totemBuf, const_cast<char*>("|cffff2020"), sizeof(totemBuf));
                SStr::Append(totemBuf, const_cast<char*>(tcatRow->m_name), sizeof(totemBuf));
                SStr::Append(totemBuf, const_cast<char*>("|r"), sizeof(totemBuf));
                spellIdb = 0;
            }
        }

        if (!firstEntry && (!spellIdb || !a3))
            CGTooltip::AddLine(tooltip, totemBuf, nullptr, sColorHexWhite, sColorHexWhite, 0);
    }

    // Equipped item requirement (pseudo 637-733).
    if ((spell->m_targets & 0x10) == 0 && (int32_t)spell->m_equippedItemClass >= 0 && spell->m_equippedItemSubclass != 0) {
        char equipBuf[512] = {};
        int foundMaskName = 0;
        const uintptr_t itemSubClassMaskDB = 0xAD3F20;
        const uintptr_t itemSubClassDB = 0xAD3F44;
        uint32_t maskNumRecords = *reinterpret_cast<uint32_t*>(itemSubClassMaskDB + 8);
        if (maskNumRecords > 0) {
            ItemSubClassMaskRow* maskRecords = *reinterpret_cast<ItemSubClassMaskRow**>(itemSubClassMaskDB + 0x1C);
            if (maskRecords) {
                for (uint32_t i = 0; i < maskNumRecords; ++i) {
                    ItemSubClassMaskRow* row = &maskRecords[i];
                    if (row->m_classID == spell->m_equippedItemClass && row->m_mask == spell->m_equippedItemSubclass) {
                        if (row->m_name)
                            SStr::Copy(equipBuf, row->m_name, sizeof(equipBuf));
                        foundMaskName = 1;
                        break;
                    }
                }
            }
        }
        int requirementMet = 1;
        int itemSlotMask = (spell->m_attributesExC & 0x400) != 0 ? 0x8000
            : (spell->m_attributesExC & 0x1000000) != 0 ? 0x10000 : -1;
        if (!CGUnit_C__EquippedItemMeetSpellRequirements(unit, spell, itemSlotMask))
            requirementMet = 0;
        if ((spell->m_attributesExC & 0x1000400) == 0x1000400 && !CGUnit_C__EquippedItemMeetSpellRequirements(unit, spell, 0x10000))
            requirementMet = 0;
        uint32_t subNumRecords = *reinterpret_cast<uint32_t*>(itemSubClassDB + 8);
        if (subNumRecords > 0) {
            ItemSubClassRow* subRecords = *reinterpret_cast<ItemSubClassRow**>(itemSubClassDB + 0x1C);
            if (subRecords) {
                int firstSubclass = 1;
                for (uint32_t j = 0; j < subNumRecords; ++j) {
                    ItemSubClassRow* subRow = &subRecords[j];
                    if (subRow) {
                        if (subRow->m_classID != spell->m_equippedItemClass)
                            continue;
                            if (!((1u << subRow->m_subClassID) & spell->m_equippedItemSubclass) || foundMaskName)
                                continue;
                            if (firstSubclass)
                                firstSubclass = 0;
                            else
                                SStr::Append(equipBuf, const_cast<char*>(", "), sizeof(equipBuf));
                            const char* name = (subRow->m_verboseName && *subRow->m_verboseName) ? subRow->m_verboseName : subRow->m_displayName;
                            if (name)
                                SStr::Append(equipBuf, const_cast<char*>(name), sizeof(equipBuf));
                    }
                }
            }
        }
        if (a5)
            requirementMet = 1;
        if (equipBuf[0] != 0) {
            if (!(requirementMet && a3)) {
                const char* key = requirementMet ? "SPELL_EQUIPPED_ITEM" : "SPELL_EQUIPPED_ITEM_NOSPACE";
                const char* fmt = FrameScript::GetText(const_cast<char*>(key), -1, 0);
                if (fmt) {
                    char printedBuf[512] = {};
                    SStr::Printf(printedBuf, sizeof(printedBuf), const_cast<char*>(fmt), equipBuf);
                    void* color = requirementMet ? sColorHexWhite : sColorHexRed0;
                    CGTooltip::AddLine(tooltip, printedBuf, nullptr, color, color, 1);
                }
            }
        }
    }

    // Shapeshift "Required form" block (disas 794-853).
    if (spell->m_shapeshiftMask[0] || spell->m_shapeshiftMask[1]) {
        char formBuf[512] = {};
        int v159 = 1;
        int attrExBFormFlag = (spell->m_attributesExB & 0x80000) != 0;
        if (!attrExBFormFlag) {
            const uintptr_t db = kGSpellShapeshiftFormDB;
            uint32_t numRecords = *reinterpret_cast<uint32_t*>(db + 8);
            if (numRecords > 64)
                numRecords = 64;
            SpellShapeshiftFormRow* records = *reinterpret_cast<SpellShapeshiftFormRow**>(db + 0x1C);
            if (records && numRecords > 0) {
                for (uint32_t idx = 0; idx < numRecords; ++idx) {
                    if (SpellRec__UsableInShapeshift(spell, static_cast<int>(idx))) {
                        SpellShapeshiftFormRow* row = &records[idx];
                        if (row->m_name && *row->m_name) {
                            if (v159) {
                                SStr::Copy(formBuf, const_cast<char*>(row->m_name), sizeof(formBuf));
                                v159 = 0;
                            } else {
                                SStr::Append(formBuf, const_cast<char*>(", "), sizeof(formBuf));
                                SStr::Append(formBuf, const_cast<char*>(row->m_name), sizeof(formBuf));
                            }
                        }
                    }
                }
            }
        }
        int v96 = 0;
        if (unit) {
            int formId = CGUnit_C__GetShapeshiftFormId(unit);
            int formIndex = formId - 1;
            if (SpellRec__UsableInShapeshift(spell, formIndex))
                v96 = 1;
            else if (attrExBFormFlag && !CGUnit_C__IsShapeShifted(unit))
                v96 = 1;
            else if (CGUnit_C__AffectedByAura(unit, 275, spell))
                v96 = 1;
        } else {
            v96 = 1;
        }
        if (!v159) {
            const char* key = v96 ? "SPELL_REQUIRED_FORM" : "SPELL_REQUIRED_FORM_NOSPACE";
            if (!v96 || !a3) {
                const char* fmt = FrameScript::GetText(const_cast<char*>(key), -1, 0);
                if (fmt) {
                    char formLineBuf[128] = {};
                    SStr::Printf(formLineBuf, sizeof(formLineBuf), const_cast<char*>(fmt), formBuf);
                    void* color = v96 ? sColorHexWhite : sColorHexRed0;
                    CGTooltip::AddLine(tooltip, formLineBuf, nullptr, color, color, 0);
                }
            }
        }
    }

    // // Faction / reputation requirement (disas 854-865).
    // if (spell->m_minFactionID != 0) {
    //     int32_t repValue = GetRepListRepValue(spell->m_minFactionID);
    //     int32_t* repThresholds = reinterpret_cast<int32_t*>(kDwordA2D2FC);
    //     int repMet = (spell->m_minReputation < 8 && repValue >= repThresholds[spell->m_minReputation]);
    //     if (!repMet || !a3) {
    //         FactionRec* factionRow = nullptr;
    //         uint32_t minID = *reinterpret_cast<uint32_t*>(kGFactionDB + 16);
    //         uint32_t maxID = *reinterpret_cast<uint32_t*>(kGFactionDB + 12);
    //         if (spell->m_minFactionID >= minID && spell->m_minFactionID <= maxID) {
    //             FactionRec** byId = *reinterpret_cast<FactionRec***>(kGFactionDB + 0x1C);
    //             if (byId)
    //                 factionRow = byId[spell->m_minFactionID - minID];
    //         }
    //         const char* factionName = factionRow && factionRow->m_name ? factionRow->m_name : "UNKNOWN";
    //         char standingKey[64] = {};
    //         SStr::Printf(standingKey, sizeof(standingKey), "FACTION_STANDING_LABEL%d", spell->m_minReputation + 1);
    //         const char* standingText = FrameScript__GetLocalizedText(standingKey, -1);
    //         const char* reqFmt = FrameScript::GetText(const_cast<char*>("ITEM_REQ_REPUTATION"), -1, 0);
    //         if (reqFmt) {
    //             SStr::Printf(lineLeft, sizeof(lineLeft), const_cast<char*>(reqFmt), factionName, standingText ? standingText : "");
    //             void* color = repMet ? sColorHexWhite : sColorHexRed0;
    //             CGTooltip::AddLine(tooltip, lineLeft, nullptr, color, color, 0);
    //         }
    //     }
    // }

    // Item min level (disas 855-872). Show when unit level < baseLevel; original also required v155.
    if (spell->m_baseLevel > 0 && unit && unit->unitData && unit->unitData->level < spell->m_baseLevel) {
        const char* minLevelFmt = FrameScript::GetText(const_cast<char*>("ITEM_MIN_LEVEL"), -1, 0);
        if (minLevelFmt) {
            SStr::Printf(lineLeft, sizeof(lineLeft), const_cast<char*>(minLevelFmt), spell->m_baseLevel);
            CGTooltip::AddLine(tooltip, lineLeft, nullptr, sColorHexRed0, sColorHexRed0, 0);
        }
    }

    // Reagents (disas 873-916).
    if (!a5 && !Player_CanCastSpellInCurrentForm(reinterpret_cast<int>(activePlayer), reinterpret_cast<int>(spell))) {
        char reagentBuf[4096] = {};
        int firstReagent = 1;
        int allMet = 1;
        for (int k = 0; k < 8; ++k) {
            uint32_t reagentId = spell->m_reagent[k];
            if (reagentId == 0) continue;
            uint64_t guid = spell->m_ID | 0x1FE0000000000000ULL;
            void* itemBlock = DBItemCache_GetInfoBlockByID(reinterpret_cast<void*>(WDB_CACHE_ITEM), reagentId, &guid, CGTooltip_HandleItemLoad, tooltip, 1);
            if (!itemBlock) {
                (*reinterpret_cast<uint32_t*>(static_cast<char*>(tooltip) + 242))++;
                continue;
            }
            if (firstReagent) {
                firstReagent = 0;
                const char* label = FrameScript::GetText(const_cast<char*>("SPELL_REAGENTS"), -1, 0);
                if (label) SStr::Copy(reagentBuf, const_cast<char*>(label), sizeof(reagentBuf));
            } else {
                SStr::Append(reagentBuf, const_cast<char*>(", "), sizeof(reagentBuf));
            }
            const char* itemName = reinterpret_cast<ItemCacheNameView*>(itemBlock)->namePtr;
            if (!itemName) itemName = "";
            char partBuf[128] = {};
            if (spell->m_reagentCount[k] <= 1)
                SStr::Copy(partBuf, const_cast<char*>(itemName), sizeof(partBuf));
            else
                SStr::Printf(partBuf, sizeof(partBuf), const_cast<char*>("%s (%d)"), const_cast<char*>(itemName), spell->m_reagentCount[k]);
            void* bagBase = reinterpret_cast<char*>(activePlayer) + sizeof(CGUnit);
            uint32_t haveCount = CGBag_C__GetItemTypeCount(bagBase, reagentId, 0);
            if (haveCount >= spell->m_reagentCount[k]) {
                SStr::Append(reagentBuf, partBuf, sizeof(reagentBuf));
            } else {
                const char* redPrefix = reinterpret_cast<const char*>(0xAD2A5C);
                SStr::Append(reagentBuf, const_cast<char*>(redPrefix), sizeof(reagentBuf));
                SStr::Append(reagentBuf, partBuf, sizeof(reagentBuf));
                SStr::Append(reagentBuf, const_cast<char*>("|r"), sizeof(reagentBuf));
                allMet = 0;
            }
        }
        if (!firstReagent && (!allMet || !a3))
            CGTooltip::AddLine(tooltip, reagentBuf, nullptr, sColorHexWhite, sColorHexWhite, 1);
    }

    // Item / spell remaining cooldown line when a4 (disas 917-922).
    uint32_t remainingCooldown = GetSpellChargeRemainingCooldown(spell->m_ID, static_cast<uint32_t>(a4));
    if (remainingCooldown) {
        CGTooltip::GetDurationString(lineLeft, 128, static_cast<uint64_t>(remainingCooldown), const_cast<char*>("ITEM_COOLDOWN_TIME"), 0, 1, 0);
        CGTooltip::AddLine(tooltip, lineLeft, nullptr, sColorHexWhite, sColorHexWhite, 0);
        v112 = 1;
    }
    if (!a3) {
        // Dodge / Parry / Block / Crit (disas 923-949).
        // Stock selects exactly one localized string/value pair from the first effect:
        //   20 -> dodge, 22 -> parry, 23 -> block, 78 -> crit
        // Effects 20/22/23 are only allowed through when v155 is set.
        const uint32_t effect0 = spell->m_effect[0];
        if (effect0 == 78 || (v155 && (effect0 == 20 || effect0 == 22 || effect0 == 23))) {
            const char* statKey = nullptr;
            float statValue = 0.0f;

            switch (effect0) {
                case 20:
                    statKey = "CHANCE_TO_DODGE";
                    statValue = activePlayer->PlayerData->dodgePct;
                    break;
                case 22:
                    statKey = "CHANCE_TO_PARRY";
                    statValue = activePlayer->PlayerData->parryPct;
                    break;
                case 23:
                    statKey = "CHANCE_TO_BLOCK";
                    statValue = activePlayer->PlayerData->blockPct;
                    break;
                case 78:
                    statKey = "CHANCE_TO_CRIT";
                    statValue = activePlayer->PlayerData->critPct;
                    break;
                default:
                    break;
            }

            if (statKey) {
                const char* lbl = FrameScript::GetText(const_cast<char*>(statKey), -1, 0);
                if (lbl) {
                    SStr::Printf(lineLeft, sizeof(lineLeft), const_cast<char*>(lbl), statValue);
                    CGTooltip::AddLine(tooltip, lineLeft, nullptr, sColorHexWhite, sColorHexWhite, 0);
                    v112 = 1;
                }
            }
        }

        // "Use all power" line (disas 950-955).
        if ((spell->m_attributesEx & 2) != 0) {
            PowerDisplayRow* v134 = nullptr;
            const uintptr_t powerDisplayDB = 0x00AD43A0;
            uint32_t powerMinID = *reinterpret_cast<uint32_t*>(powerDisplayDB + 4 + 16);
            uint32_t powerMaxID = *reinterpret_cast<uint32_t*>(powerDisplayDB + 4 + 12);
            if (spell->m_powerDisplayID >= powerMinID && spell->m_powerDisplayID <= powerMaxID) {
                uint32_t powerIndex = spell->m_powerDisplayID - powerMinID;
                v134 = reinterpret_cast<PowerDisplayRow*>(ClientDB::GetRow(reinterpret_cast<void*>(powerDisplayDB), powerIndex));
            }
            if (v134 && v134->m_globalStringBaseTag) {
                const char* powerStr = FrameScript::GetText(v134->m_globalStringBaseTag, -1, 0);
                if (powerStr) {
                    char buf[256] = {};
                    SStr::Copy(buf, const_cast<char*>(powerStr), sizeof(buf));
                    const char* fmt = FrameScript::GetText(const_cast<char*>("SPELL_USE_ALL_POWER_DISPLAY"), -1, 0);
                    if (fmt) SStr::Printf(lineLeft, sizeof(lineLeft), const_cast<char*>(fmt), buf);
                    else SStr::Copy(lineLeft, buf, sizeof(lineLeft));
                    CGTooltip::AddLine(tooltip, lineLeft, nullptr, sColorHexWhite, sColorHexWhite, 0);
                }
            } else {
                const char* useKey = (spell->m_powerType > 6) ? "SPELL_USE_ALL_HEALTH" : (spell->m_powerType == 0) ? "SPELL_USE_ALL_MANA" : "SPELL_USE_ALL_POWER";
                const char* txt = FrameScript::GetText(const_cast<char*>(useKey), -1, 0);
                if (txt) {
                    SStr::Copy(lineLeft, const_cast<char*>(txt), sizeof(lineLeft));
                    CGTooltip::AddLine(tooltip, lineLeft, nullptr, sColorHexWhite, sColorHexWhite, 0);
                }
            }
        }
    }

    // Description text.
    if (spell->m_description_lang && *spell->m_description_lang) {
        char desc[2048] = {};
        if (!SafeParseSpellTooltipDescription(spell, desc, sizeof(desc), a5, a7)) {
            desc[0] = '\0';
        }

        if (desc[0]) {
            CGTooltip::AddLine(tooltip, desc, nullptr, sColorHexDarkYellow, sColorHexDarkYellow, 1);
        }
    }

    // TalentTooltip_AddActionLines (disas 961-962).
    if (a9 && !a7 && a16)
        TalentTooltip_AddActionLines(a9, a10, v137, a14, a15, 0, a8, a5, a12);

    // Embedded item tooltip (disas 963-971). Use first non-zero effect item (recipe can be in any effect slot).
    if (v136) {
        uint32_t embedItemId = 0;
        for (int e = 0; e < 3; ++e) {
            if (spell->m_effectItemType[e] != 0) {
                embedItemId = spell->m_effectItemType[e];
                break;
            }
        }
        if (embedItemId != 0) {
            // CGTooltipItemData_Reset(static_cast<char*>(tooltip) + 250);
            int v135 = 0;
            uint64_t v139 = 0;
            CGTooltipInternal::SetItem(tooltip, static_cast<int>(embedItemId), reinterpret_cast<unsigned int>(&v135), reinterpret_cast<void*>(&v139), 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1);
        }
    }

    // Tooltip script at +319 (disas 996-997).
    void* scriptPtr = *reinterpret_cast<void**>(static_cast<char*>(tooltip) + 319);
    if (scriptPtr)
        FrameScript_Object__RunScript(static_cast<char*>(tooltip) + 319, 0, 0);

    // Finalize: show and size tooltip.
    CSimpleFrame::Show(tooltip);
    CGTooltipInternal::CalculateSize(tooltip);

    return v112;
}
