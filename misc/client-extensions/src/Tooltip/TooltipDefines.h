#pragma once

#include <CDBCMgr/CDBCDefs/SpellEffect.h>
#include <ClientData/SharedDefines.h>
#include <Spells/SpellCache/SpellCacheStreaming.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

static constexpr uint32_t SPELLVARIABLE_STOCK_COUNT = 140;
static constexpr uint32_t SPELLVARIABLE_OPCODE_BASE = 22;
static constexpr uint32_t SPELLVARIABLE_EFFECT_SLOT_COUNT = 32;
static constexpr uint32_t SPELLVARIABLE_TABLE_CAPACITY = 1024;

static uint32_t spellVariables[SPELLVARIABLE_TABLE_CAPACITY] = { 0 };

enum SpellVariable : uint32_t {
    SPELLVARIABLE_hp        = 162,
    SPELLVARIABLE_HP        = 163,
    SPELLVARIABLE_ppl1      = 164,
    SPELLVARIABLE_ppl2      = 165,
    SPELLVARIABLE_ppl3      = 166,
    SPELLVARIABLE_PPL1      = 167,
    SPELLVARIABLE_PPL2      = 168,
    SPELLVARIABLE_PPL3      = 169,
    SPELLVARIABLE_power1    = 170,
    SPELLVARIABLE_power2    = 171,
    SPELLVARIABLE_power3    = 172,
    SPELLVARIABLE_power4    = 173,
    SPELLVARIABLE_power5    = 174,
    SPELLVARIABLE_power6    = 175,
    SPELLVARIABLE_power7    = 176,
    SPELLVARIABLE_POWER1    = 177,
    SPELLVARIABLE_POWER2    = 178,
    SPELLVARIABLE_POWER3    = 179,
    SPELLVARIABLE_POWER4    = 180,
    SPELLVARIABLE_POWER5    = 181,
    SPELLVARIABLE_POWER6    = 182,
    SPELLVARIABLE_POWER7    = 183,
    SPELLVARIABLE_mastery1  = 184,
    SPELLVARIABLE_mastery2  = 185,
    SPELLVARIABLE_mastery3  = 186,
    SPELLVARIABLE_mastery4  = 187,
    SPELLVARIABLE_MASTERY   = 188,
    SPELLVARIABLE_dpct      = 189,
    SPELLVARIABLE_ppct      = 190,
    SPELLVARIABLE_bon1      = 191,
    SPELLVARIABLE_bon2      = 192,
    SPELLVARIABLE_bon3      = 193,
};

static constexpr uint32_t SPELLVARIABLE_ppl4 = 194;
static constexpr uint32_t SPELLVARIABLE_PPL4 = SPELLVARIABLE_ppl4 + 29;
static constexpr uint32_t SPELLVARIABLE_bon4 = SPELLVARIABLE_PPL4 + 29;
static constexpr uint32_t SPELLVARIABLE_BASELINE_EFFECT_FIRST = SPELLVARIABLE_bon4 + 29;
static constexpr uint32_t SPELLVARIABLE_BASELINE_EFFECT_GROUP_SIZE = SPELLVARIABLE_EFFECT_SLOT_COUNT;
static constexpr uint32_t SPELLVARIABLE_BASELINE_EFFECT_GROUP_COUNT = 18;

namespace TooltipVariableExtensions
{
    inline const char* const* GetExtendedSpellVariableNames(size_t& count)
    {
        static std::vector<std::string> storage;
        static std::vector<const char*> names;

        if (names.empty()) {
            const char* const fixedNames[] = {
                "hp", "HP", "ppl1", "ppl2", "ppl3", "PPL1", "PPL2", "PPL3",
                "power1", "power2", "power3", "power4", "power5", "power6", "power7",
                "POWER1", "POWER2", "POWER3", "POWER4", "POWER5", "POWER6", "POWER7",
                "mastery1", "mastery2", "mastery3", "mastery4", "MASTERY",
                "dpct", "ppct", "bon1", "bon2", "bon3"
            };

            storage.reserve(900);
            for (const char* name : fixedNames)
                storage.emplace_back(name);

            for (uint32_t i = 4; i <= SPELLVARIABLE_EFFECT_SLOT_COUNT; ++i)
                storage.emplace_back(std::string("ppl") + std::to_string(i));
            for (uint32_t i = 4; i <= SPELLVARIABLE_EFFECT_SLOT_COUNT; ++i)
                storage.emplace_back(std::string("PPL") + std::to_string(i));
            for (uint32_t i = 4; i <= SPELLVARIABLE_EFFECT_SLOT_COUNT; ++i)
                storage.emplace_back(std::string("bon") + std::to_string(i));

            const char* const effectPrefixes[] = {
                "m", "M", "a", "A", "x", "X", "t", "T", "b", "B",
                "e", "E", "f", "F", "q", "Q", "bc", "BC"
            };

            for (const char* prefix : effectPrefixes) {
                for (uint32_t i = 1; i <= SPELLVARIABLE_EFFECT_SLOT_COUNT; ++i)
                    storage.emplace_back(std::string(prefix) + std::to_string(i));
            }

            names.reserve(storage.size());
            for (const std::string& name : storage)
                names.push_back(name.c_str());
        }

        count = names.size();
        return names.data();
    }

    inline bool IsVariableBoundary(char value)
    {
        unsigned char ch = static_cast<unsigned char>(value);
        return value == '\0' || (!std::isalnum(ch) && value != '_');
    }

    inline bool TryAppendSpellNameToken(const char* text, size_t& i, std::string& expanded)
    {
        if (text[i] != '$')
            return false;

        const char* idStart = text + i + 1;
        bool bracketed = false;
        if (*idStart == '<') {
            bracketed = true;
            ++idStart;
        }

        if (!std::isdigit(static_cast<unsigned char>(*idStart)))
            return false;

        char* idEnd = nullptr;
        unsigned long spellId = std::strtoul(idStart, &idEnd, 10);
        if (!idEnd || idEnd == idStart || spellId == 0)
            return false;

        if (bracketed) {
            if (*idEnd != '>')
                return false;
            ++idEnd;
        }

        if (std::strncmp(idEnd, "name", 4) != 0 || !IsVariableBoundary(idEnd[4]))
            return false;

        SpellRow spellRow = {};
        if (!ClientDB::GetLocalizedRow(reinterpret_cast<void*>(0xAD49D0), static_cast<uint32_t>(spellId), &spellRow) ||
            !spellRow.m_name_lang ||
            !*spellRow.m_name_lang) {
            return false;
        }

        expanded.append("|cffffffff");
        expanded.append(spellRow.m_name_lang);
        expanded.append("|r");
        i = static_cast<size_t>((idEnd + 4) - text);
        return true;
    }

    inline bool AppendFormattedEffectValue(std::string& expanded, float value, bool integerValue)
    {
        char buffer[64] = {};
        if (integerValue)
            std::snprintf(buffer, sizeof(buffer), "%d", static_cast<int>(value));
        else
            std::snprintf(buffer, sizeof(buffer), "%.1f", value);

        expanded.append(buffer);
        return true;
    }

    inline bool TryGetDirectEffectTokenValue(SpellRow* spell, char token, uint32_t effectIndex, float& value, bool& integerValue)
    {
        if (!spell || effectIndex >= SPELLVARIABLE_EFFECT_SLOT_COUNT)
            return false;

        if (!SpellCacheStreaming::HasSpell(spell->m_ID))
            return false;

        const SpellEffectCacheRow* effect = nullptr;
        if (!SpellCacheStreaming::TryGetSpellEffect(spell->m_ID, effectIndex, effect) || !effect) {
            value = 0.0f;
            integerValue = true;
            return true;
        }

        integerValue = true;
        switch (token) {
            case 'd':
            case 'D':
                value = effect->effectPointsPerCombo;
                return true;
            case 'g':
            case 'G':
                value = effect->effectMultipleValue;
                integerValue = false;
                return true;
            case 'h':
            case 'H':
                value = effect->effectChainAmplitude;
                integerValue = token == 'H';
                return true;
            case 's':
            case 'S':
                value = static_cast<float>(effect->effectMiscValue);
                return true;
            case 'v':
            case 'V':
                value = static_cast<float>((spell->m_procTypeMask & 1) ? 5000 : effect->effectAmplitude);
                return true;
            case 'z':
            case 'Z':
                value = static_cast<float>(effect->effectChainTargets);
                return true;
            default:
                return false;
        }
    }

    inline bool TryParseEffectVariableName(const char* nameStart, const char* nameEnd, std::string& prefix, uint32_t& effectIndex)
    {
        if (!nameStart || !nameEnd || nameStart >= nameEnd)
            return false;

        const char* digitStart = nameEnd;
        while (digitStart > nameStart && std::isdigit(static_cast<unsigned char>(*(digitStart - 1))))
            --digitStart;

        if (digitStart == nameStart || digitStart == nameEnd)
            return false;

        char* parseEnd = nullptr;
        unsigned long ordinal = std::strtoul(digitStart, &parseEnd, 10);
        if (!parseEnd || parseEnd != nameEnd || ordinal < 1 || ordinal > SPELLVARIABLE_EFFECT_SLOT_COUNT)
            return false;

        prefix.assign(nameStart, digitStart);
        effectIndex = static_cast<uint32_t>(ordinal - 1);
        return true;
    }

    inline bool TryGetSimpleEffectFormulaValue(SpellRow* spell, const std::string& prefix, uint32_t effectIndex, float& value, bool& integerValue)
    {
        if (!spell || !SpellCacheStreaming::HasSpell(spell->m_ID))
            return false;

        const SpellEffectCacheRow* effect = nullptr;
        if (!SpellCacheStreaming::TryGetSpellEffect(spell->m_ID, effectIndex, effect) || !effect) {
            value = 0.0f;
            integerValue = true;
            return true;
        }

        integerValue = true;
        if (prefix == "m") {
            value = static_cast<float>(effect->effectBasePoints + 1);
            return true;
        }
        if (prefix == "M") {
            value = static_cast<float>(effect->effectBasePoints + effect->effectDieSides);
            return true;
        }
        if (prefix == "a" || prefix == "A") {
            value = static_cast<float>(effect->effectRadiusIndex);
            return true;
        }
        if (prefix == "x" || prefix == "X") {
            value = static_cast<float>(effect->effectChainTargets);
            return true;
        }
        if (prefix == "t" || prefix == "T") {
            value = static_cast<float>((spell->m_procTypeMask & 1) ? 5000 : effect->effectAmplitude);
            return true;
        }
        if (prefix == "b" || prefix == "B") {
            value = effect->effectPointsPerCombo;
            integerValue = false;
            return true;
        }
        if (prefix == "e" || prefix == "E") {
            value = effect->effectMultipleValue;
            integerValue = false;
            return true;
        }
        if (prefix == "f" || prefix == "F") {
            value = effect->effectChainAmplitude;
            integerValue = false;
            return true;
        }
        if (prefix == "q" || prefix == "Q") {
            value = static_cast<float>(effect->effectMiscValue);
            return true;
        }
        if (prefix == "bc" || prefix == "BC") {
            value = effect->effectBonusMultiplier;
            integerValue = false;
            return true;
        }
        if (prefix == "ppl") {
            value = effect->effectRealPointsPerLevel;
            integerValue = false;
            return true;
        }
        if (prefix == "PPL") {
            CGPlayer* activePlayer = reinterpret_cast<CGPlayer*>(ClntObjMgr::ObjectPtr(ClntObjMgr::GetActivePlayer(), TYPEMASK_PLAYER));
            uint32_t level = activePlayer && activePlayer->unitBase.unitData ? activePlayer->unitBase.unitData->level : 0;
            value = effect->effectRealPointsPerLevel * static_cast<float>(level);
            integerValue = false;
            return true;
        }
        if (prefix == "bon") {
            value = static_cast<float>(effect->effectBasePoints + 1);
            return true;
        }

        return false;
    }

    inline bool TryAppendSimpleEffectFormulaToken(SpellRow* spell, const char* text, size_t& i, std::string& expanded)
    {
        if (text[i] != '$' || text[i + 1] != '{')
            return false;

        const char* nameStart = text + i + 2;
        const char* nameEnd = std::strchr(nameStart, '}');
        if (!nameEnd)
            return false;

        for (const char* p = nameStart; p < nameEnd; ++p) {
            if (!std::isalnum(static_cast<unsigned char>(*p)))
                return false;
        }

        std::string prefix;
        uint32_t effectIndex = 0;
        if (!TryParseEffectVariableName(nameStart, nameEnd, prefix, effectIndex))
            return false;

        float value = 0.0f;
        bool integerValue = true;
        if (!TryGetSimpleEffectFormulaValue(spell, prefix, effectIndex, value, integerValue))
            return false;

        AppendFormattedEffectValue(expanded, value, integerValue);
        i = static_cast<size_t>((nameEnd + 1) - text);
        return true;
    }

    inline bool TryAppendDirectEffectAliasToken(SpellRow* spell, const char* text, size_t& i, std::string& expanded)
    {
        if (text[i] != '$')
            return false;

        char token = text[i + 1];
        switch (token) {
            case 'd': case 'D':
            case 'g': case 'G':
            case 'h': case 'H':
            case 's': case 'S':
            case 'v': case 'V':
            case 'z': case 'Z':
                break;
            default:
                return false;
        }

        if (!spell || !SpellCacheStreaming::HasSpell(spell->m_ID))
            return false;

        const char* indexStart = text + i + 2;
        if (!std::isdigit(static_cast<unsigned char>(*indexStart)))
            return false;

        char* indexEnd = nullptr;
        unsigned long ordinal = std::strtoul(indexStart, &indexEnd, 10);
        if (!indexEnd || indexEnd == indexStart || !IsVariableBoundary(*indexEnd))
            return false;

        if (ordinal < 1 || ordinal > SPELLVARIABLE_EFFECT_SLOT_COUNT) {
            expanded.push_back('0');
            i = static_cast<size_t>(indexEnd - text);
            return true;
        }

        float value = 0.0f;
        bool integerValue = true;
        if (!TryGetDirectEffectTokenValue(spell, token, static_cast<uint32_t>(ordinal - 1), value, integerValue))
            return false;

        AppendFormattedEffectValue(expanded, value, integerValue);
        i = static_cast<size_t>(indexEnd - text);
        return true;
    }

    inline std::string ExpandTooltipText(SpellRow* spell, const char* text)
    {
        if (!text || !*text)
            return text ? std::string(text) : std::string();

        std::string expanded;
        expanded.reserve(std::strlen(text));

        for (size_t i = 0; text[i] != '\0';) {
            if (TryAppendSpellNameToken(text, i, expanded))
                continue;
            if (TryAppendDirectEffectAliasToken(spell, text, i, expanded))
                continue;
            if (TryAppendSimpleEffectFormulaToken(spell, text, i, expanded))
                continue;

            if (text[i] != '$' || text[i + 1] == '{') {
                expanded.push_back(text[i++]);
                continue;
            }

            size_t nameCount = 0;
            const char* const* names = GetExtendedSpellVariableNames(nameCount);
            size_t bestLength = 0;
            const char* bestName = nullptr;

            for (size_t nameIndex = 0; nameIndex < nameCount; ++nameIndex) {
                const char* name = names[nameIndex];
                size_t nameLength = std::strlen(name);
                if (nameLength <= bestLength)
                    continue;

                if (std::strncmp(text + i + 1, name, nameLength) == 0 &&
                    IsVariableBoundary(text[i + 1 + nameLength])) {
                    bestLength = nameLength;
                    bestName = name;
                }
            }

            if (!bestName) {
                expanded.push_back(text[i++]);
                continue;
            }

            expanded.append("${");
            expanded.append(bestName, bestLength);
            expanded.push_back('}');
            i += bestLength + 1;
        }

        return expanded;
    }

    inline std::string ExpandTooltipText(const char* text)
    {
        return ExpandTooltipText(nullptr, text);
    }

    inline void ParseText(SpellRow* spell, char* dest, uint32_t destSize, uint32_t a4, uint32_t a5, uint32_t a6, uint32_t a7, uint32_t a8, uint32_t a9)
    {
        if (!spell) {
            SpellParser::ParseText(spell, dest, destSize, a4, a5, a6, a7, a8, a9);
            return;
        }

        SpellRow spellCopy = *spell;
        std::string description = ExpandTooltipText(spell, spell->m_description_lang);
        std::string auraDescription = ExpandTooltipText(spell, spell->m_auraDescription_lang);

        if (spell->m_description_lang)
            spellCopy.m_description_lang = const_cast<char*>(description.c_str());
        if (spell->m_auraDescription_lang)
            spellCopy.m_auraDescription_lang = const_cast<char*>(auraDescription.c_str());

        SpellParser::ParseText(&spellCopy, dest, destSize, a4, a5, a6, a7, a8, a9);
    }
}

// String helpers used by tooltip formatters
static char* sPluralS = const_cast<char*>("s");
static char* sConnectorPlus = const_cast<char*>(" + ");

// Lightweight view over the item-cache info block returned by
// DBItemCache_GetInfoBlockByID.  Only the name pointer is needed.
struct ItemCacheNameView {
    const char* namePtr;
};
