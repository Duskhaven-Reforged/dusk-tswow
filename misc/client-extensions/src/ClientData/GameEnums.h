#pragma once

#include <cstdint>

namespace ClientData
{
    enum FrameXMLEvent : uint32_t
    {
        EVENT_ACTIONBAR_SLOT_CHANGED = 176,
        EVENT_LFG_ROLE_UPDATE = 506,
        EVENT_PLAYER_TALENT_UPDATE = 625,
    };

    enum GameError : uint32_t
    {
        GERR_LEARN_SPELL_S = 59,
        GERR_LEARN_ABILITY_S = 60,
        GERR_SPELL_UNLEARNED_S = 352,
    };

    enum Powers : int32_t
    {
        POWER_MANA = 0,
        POWER_RAGE = 1,
        POWER_FOCUS = 2,
        POWER_ENERGY = 3,
        POWER_HAPPINESS = 4,
        POWER_RUNES = 5,
        POWER_RUNIC_POWER = 7
    };

    enum SpellCastResult : uint32_t
    {
        SPELL_FAILED_MOVING = 51,
    };

    enum SpellFamilyNames : uint32_t
    {
        SPELLFAMILY_GENERIC = 0,
        SPELLFAMILY_UNK1 = 1,
        SPELLFAMILY_CRAFTING = 2,
        SPELLFAMILY_MAGE = 3,
        SPELLFAMILY_WARRIOR = 4,
        SPELLFAMILY_WARLOCK = 5,
        SPELLFAMILY_PRIEST = 6,
        SPELLFAMILY_DRUID = 7,
        SPELLFAMILY_ROGUE = 8,
        SPELLFAMILY_HUNTER = 9,
        SPELLFAMILY_PALADIN = 10,
        SPELLFAMILY_SHAMAN = 11,
        SPELLFAMILY_UNK2 = 12,
        SPELLFAMILY_POTION = 13,
        SPELLFAMILY_DEATHKNIGHT = 15,
        SPELLFAMILY_PET = 17,
    };

    enum SpellEffect : uint32_t
    {
        SPELL_EFFECT_TRADE_SKILL = 47,
        SPELL_EFFECT_ATTACK = 78,
        SPELL_EFFECT_TITAN_GRIP = 155,
    };

    enum SpellSchools : uint32_t
    {
        SPELL_SCHOOL_NORMAL = 0, // TITLE Physical
        SPELL_SCHOOL_HOLY = 1,   // TITLE Holy
        SPELL_SCHOOL_FIRE = 2,   // TITLE Fire
        SPELL_SCHOOL_NATURE = 3, // TITLE Nature
        SPELL_SCHOOL_FROST = 4,  // TITLE Frost
        SPELL_SCHOOL_SHADOW = 5, // TITLE Shadow
        SPELL_SCHOOL_ARCANE = 6, // TITLE Arcane
        MAX_SPELL_SCHOOL = 7     // SKIP
    };
}
