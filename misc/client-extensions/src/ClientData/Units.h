#pragma once

#include <cstdint>
#include <ClientData/MathTypes.h>

namespace ClientData
{
    enum Field : uint32_t
    {
        CURRENT_HP = 18,
        CURRENT_MANA = 19,
        CURRENT_RAGE = 20,
        CURRENT_FOCUS = 21,
        CURRENT_ENERGY = 22,
        CURRENT_HAPPINESS = 23,
        CURRENT_RUNES = 24,
        CURRENT_RUNIC_POWER = 25,
        MAX_HP = 26,
        MAX_MANA = 27,
        MAX_RAGE = 28,
        MAX_FOCUS = 29,
        MAX_ENERGY = 30,
        MAX_HAPPINESS = 31,
        MAX_RUNES = 32,
        MAX_RUNIC_POWER = 33,
    };

    struct MovementInfo
    {
        uint32_t padding[2];
        uint64_t moverGUID;
        C3Vector position;
        float padding1C;
        float orientation;
        float pitch;
        uint32_t padding28[7];
        uint32_t movementFlags;
        uint32_t movementFlags2;
        uint32_t padding4C[65];
        // TODO: add rest, probably when needed
    };

    struct UnitBytes0
    {
        uint8_t raceID;
        uint8_t classID;
        uint8_t genderID;
        uint8_t powerTypeID;
    };

    struct UnitBytes1
    {
        uint8_t standState;
        uint8_t petTalents;
        uint8_t visFlags;
        uint8_t animTier;
    };

    struct UnitBytes2
    {
        uint8_t sheathState;
        uint8_t pvpFlag;
        uint8_t petFlags;
        uint8_t shapeshift;
    };

    struct UnitFields
    {
        uint64_t padding0x00[8]; // not defining those until we need them
        uint32_t channelSpell;
        UnitBytes0 unitBytes0;
        uint32_t unitCurrHealth;
        uint32_t unitCurrPowers[7];
        uint32_t unitMaxHealth;
        uint32_t unitMaxPowers[7];
        float padding0x88[14];
        uint32_t level;
        uint32_t factionTemplate;
        uint32_t virtualItemSlotID[3];
        uint32_t unitFlags;
        uint32_t unitFlags2;
        uint32_t auraState;
        uint32_t baseAttackTime[3];
        float boundingRadius;
        float combatReach;
        uint32_t displayId;
        uint32_t nativeDisplayId;
        uint32_t mountDisplayId;
        float mainHandMinDamage;
        float mainHandMaxDamage;
        float offHandMinDamage;
        float offHandMaxDamage;
        UnitBytes1 unitBytes1;
        uint32_t petNumber;
        uint32_t padding0x011C[4];
        float modCastSpell;
        uint32_t createdBySpell;
        uint32_t npcFlags;
        uint32_t emoteState;
        int32_t strength;
        int32_t agility;
        int32_t stamina;
        int32_t intellect;
        int32_t spirit;
        float strengthPos;
        float agilityPos;
        float staminaPos;
        float intellectPos;
        float spiritPos;
        float strengthNeg;
        float agilityNeg;
        float staminaNeg;
        float intellectNeg;
        float spiritNeg;
        uint32_t resistances[7];
        uint32_t resistancesPos[7];
        uint32_t resistancesNeg[7];
        uint32_t baseMana;
        uint32_t basehealth;
        UnitBytes2 unitBytes2;
        uint32_t AP;
        uint16_t APMods[2];
        float APMult;
        uint32_t RAP;
        uint16_t RAPMods[2];
        float RAPMult;
        // TODO: add rest at some point, most likely when needed
    };
}
