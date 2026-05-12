#pragma once

#include <ClientData/SharedDefines.h>

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

static uint32_t spellVariables[172] = { 0 };

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

namespace TooltipVariableExtensions
{
    inline const char* const* GetExtendedSpellVariableNames(size_t& count)
    {
        static const char* const names[] = {
            "hp", "HP", "ppl1", "ppl2", "ppl3", "PPL1", "PPL2", "PPL3",
            "power1", "power2", "power3", "power4", "power5", "power6", "power7",
            "POWER1", "POWER2", "POWER3", "POWER4", "POWER5", "POWER6", "POWER7",
            "mastery1", "mastery2", "mastery3", "mastery4", "MASTERY",
            "dpct", "ppct", "bon1", "bon2", "bon3"
        };

        count = sizeof(names) / sizeof(names[0]);
        return names;
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

    inline std::string ExpandTooltipText(const char* text)
    {
        if (!text || !*text)
            return text ? std::string(text) : std::string();

        std::string expanded;
        expanded.reserve(std::strlen(text));

        for (size_t i = 0; text[i] != '\0';) {
            if (TryAppendSpellNameToken(text, i, expanded))
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

    inline void ParseText(SpellRow* spell, char* dest, uint32_t destSize, uint32_t a4, uint32_t a5, uint32_t a6, uint32_t a7, uint32_t a8, uint32_t a9)
    {
        if (!spell) {
            SpellParser::ParseText(spell, dest, destSize, a4, a5, a6, a7, a8, a9);
            return;
        }

        SpellRow spellCopy = *spell;
        std::string description = ExpandTooltipText(spell->m_description_lang);
        std::string auraDescription = ExpandTooltipText(spell->m_auraDescription_lang);

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
