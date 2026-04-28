#pragma once

#include <cstdint>

namespace ClientData
{
    struct ChrClassesRow
    {
        uint32_t m_ID;
        uint32_t m_DisplayPower;
        char* m_petNameToken;
        char* m_name_lang;
        char* m_name_female_lang;
        char* m_name_male_lang;
        char* m_filename;
        uint32_t m_spellClassSet;
        uint32_t m_flags;
        uint32_t m_cinematicSequenceID;
        uint32_t m_required_expansion;
        uint32_t m_attackPowerPerStrength;
        uint32_t m_attackPowerPerAgility;
        uint32_t m_rangedAttackPowerPerAgility;
    };

    struct gtCombatRatingsRow
    {
        uint32_t ID;
        float data;
    };

    struct gtOCTClassCombatRatingScalarRow
    {
        uint32_t ID;
        float data;
    };

    struct MapRow
    {
        uint32_t m_ID;
        char* m_Directory;
        uint32_t m_InstanceType;
        uint32_t m_Flags;
        uint32_t m_PVP;
        char* m_MapName_lang;
        uint32_t m_areaTableID;
        char* m_MapDescription0_lang;
        char* m_MapDescription1_lang;
        uint32_t m_LoadingScreenID;
        float m_minimapIconScale;
        uint32_t m_corpseMapID;
        float m_corpseX;
        float m_corpseY;
        uint32_t m_timeOfDayOverride;
        uint32_t m_expansionID;
        uint32_t m_raidOffset;
        uint32_t m_maxPlayers;
        uint32_t m_parentMapID;
    };

    struct PowerDisplayRow
    {
        uint32_t m_ID;
        uint32_t m_actualType;
        char* m_globalStringBaseTag;
        uint8_t m_red;
        uint8_t m_green;
        uint8_t m_blue;
    };

    struct SkillLineAbilityRow
    {
        uint32_t m_ID;
        uint32_t m_skillLine;
        uint32_t m_spell;
        uint32_t m_raceMask;
        uint32_t m_classMask;
        uint32_t m_excludeRace;
        uint32_t m_excludeClass;
        uint32_t m_minSkillLineRank;
        uint32_t m_supercededBySpell;
        uint32_t m_acquireMethod;
        uint32_t m_trivialSkillLineRankHigh;
        uint32_t m_trivialSkillLineRankLow;
        uint32_t m_characterPoints[2];
        uint32_t m_numSkillUps;
    };

    struct SkillLineRow
    {
        uint32_t m_ID;
        uint32_t m_categoryID;
        uint32_t m_skillCostsID;
        char* m_displayName_lang;
        char* m_description_lang;
        uint32_t m_spellIconID;
        char* m_alternateVerb_lang;
        uint32_t m_canLink;
    };

    struct TotemCategoryRow
    {
        int32_t m_ID;
        char* m_name;
        int32_t m_totemCategoryType;
        int32_t m_totemCategoryMask;
    };

    struct ItemSubClassMaskRow
    {
        uint32_t m_classID;
        uint32_t m_mask;
        char* m_name;
    };

    struct ItemSubClassRow
    {
        int32_t m_classID;
        int32_t m_subClassID;
        int32_t m_prerequisiteProficiency;
        int32_t m_postrequisiteProficiency;
        int32_t m_flags;
        int32_t m_displayFlags;
        int32_t m_weaponParrySeq;
        int32_t m_weaponReadySeq;
        int32_t m_weaponAttackSeq;
        int32_t m_weaponSwingSize;
        const char* m_displayName;
        const char* m_verboseName;
        int32_t m_generatedID;
    };

    struct FactionRec
    {
        int32_t m_ID;
        int32_t m_reputationIndex;
        int32_t m_reputationRaceMask[4];
        int32_t m_reputationClassMask[4];
        int32_t m_reputationBase[4];
        int32_t m_reputationFlags[4];
        int32_t m_parentFactionID;
        float m_parentFactionMod[2];
        int32_t m_parentFactionCap[2];
        const char* m_name;
        const char* m_description;
    };

    struct SpellShapeshiftFormRow
    {
        int32_t m_ID;
        int32_t m_bonusActionBar;
        const char* m_name;
        int32_t m_flags;
        int32_t m_creatureType;
        int32_t m_attackIconID;
        int32_t m_combatRoundTime;
        int32_t m_creatureDisplayID[4];
        int32_t m_presetSpellID[8];
    };

    struct SpellIconRow
    {
        uint32_t m_ID;
        char* m_textureFilename;
    };

    struct SpellRuneCostRow
    {
        int32_t m_ID;
        int32_t m_blood;
        int32_t m_unholy;
        int32_t m_frost;
        int32_t m_runicPower;
    };
}
