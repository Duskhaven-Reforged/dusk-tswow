#pragma once

#include <cstdint>

namespace ClientData
{
    struct PlayerFields
    {
        uint32_t padding0x00[876];
        float blockPct;
        float dodgePct;
        float parryPct;
        uint32_t expertise;
        uint32_t offhandExperise;
        float critPct;
        float rangedCritPct;
        float offhandCritPct;
        float spellCritPct[7];
        float shieldBlock;
        float shieldBlockCritPct;
        uint32_t exploredzones[128];
        uint32_t restedXP;
        uint32_t coinage;
        int32_t SPPos[7];
        int32_t SPNeg[7];
        float SPBonus[7];
        uint32_t healingPower;
        float healingTakenMult;
        float healingDoneMult;
        uint32_t padding0x0DEC[36];
        int32_t crWeaponSkill;
        int32_t crDefenseSkill;
        int32_t crDodge;
        int32_t crParry;
        int32_t crBlock;
        int32_t crSpeed;     // crHitMelee
        int32_t crLifesteal; // crHitRanged
        int32_t crAvoidance; // crHitSpell
        int32_t crCrit;      // crCritMelee
        int32_t crCritRanged;
        int32_t crCritSpell;
        int32_t crHitTakenMelee;
        int32_t crHitTakenRanged;
        int32_t crHitTakenSpell;
        int32_t crCritTakenMelee;
        int32_t crCritTakenRanged;
        int32_t crCritTakenSpell;
        int32_t crHaste; // crHasteMelee
        int32_t crHasteRanged;
        int32_t crHasteSpell;
        int32_t crWeaponSkillMainhand;
        int32_t crWeaponSkillOffhand;
        int32_t crWeaponSkillRanged;
        int32_t crMastery; // crExpertise
        int32_t crThorns;  // crArmorPenetration
        uint32_t padding0x1120[56];
        uint32_t glyphslots[6];
        float speed;
        float leech;
        float avoidance;
        int32_t weaponBonusAP[3];
        uint32_t glyphsEnabled; // can reuse, glyphs are disabled
        int32_t petSpellPower;
        // TODO: add rest when needed
    };
}
