#pragma once
#include "Util.h"
#include "CDBCMgr/CDBCDefs/SpellAdditionalAttributes.h"

enum SpellAttr0 : uint32_t {
    SPELL_ATTR0_REQ_AMMO = 0x00000002,
    SPELL_ATTR0_ON_NEXT_SWING = 0x00000004,
    SPELL_ATTR0_ABILITY = 0x00000010,
    SPELL_ATTR0_TRADESPELL = 0x00000020,
    SPELL_ATTR0_PASSIVE = 0x00000040,
    SPELL_ATTR0_HIDDEN_CLIENTSIDE = 0x00000080,
    SPELL_ATTR0_ON_NEXT_SWING_2 = 0x00000400,
};

enum SpellAttr1 : uint32_t {
    SPELL_ATTR1_CHANNELED_1 = 0x00000004,
    SPELL_ATTR1_CHANNELED_2 = 0x00000040,
};

enum SpellAttr2 : uint32_t {
    SPELL_ATTR2_AUTOREPEAT_FLAG = 0x00000020,
};

enum SpellAttr3 : uint32_t {
    SPELL_ATTR3_MAIN_HAND = 0x00000400,
    SPELL_ATTR3_REQ_OFFHAND = 0x01000000,
};

enum SpellAttr0Cu : uint32_t
{
    SPELL_ATTR0_CU_ENCHANT_PROC                 = 0x00000001,
    SPELL_ATTR0_CU_CONE_BACK                    = 0x00000002,
    SPELL_ATTR0_CU_CONE_LINE                    = 0x00000004,
    SPELL_ATTR0_CU_SHARE_DAMAGE                 = 0x00000008,
    SPELL_ATTR0_CU_NO_INITIAL_THREAT            = 0x00000010,
    SPELL_ATTR0_CU_AURA_CC                      = 0x00000020,
    SPELL_ATTR0_CU_DONT_BREAK_STEALTH           = 0x00000040,
    SPELL_ATTR0_CU_CAN_CRIT                     = 0x00000080,
    SPELL_ATTR0_CU_DIRECT_DAMAGE                = 0x00000100,
    SPELL_ATTR0_CU_CHARGE                       = 0x00000200,
    SPELL_ATTR0_CU_PICKPOCKET                   = 0x00000400,
    SPELL_ATTR0_CU_ROLLING_PERIODIC             = 0x00000800,
    SPELL_ATTR0_CU_NEGATIVE_EFF0                = 0x00001000,
    SPELL_ATTR0_CU_NEGATIVE_EFF1                = 0x00002000,
    SPELL_ATTR0_CU_NEGATIVE_EFF2                = 0x00004000,
    SPELL_ATTR0_CU_IGNORE_ARMOR                 = 0x00008000,
    SPELL_ATTR0_CU_REQ_TARGET_FACING_CASTER     = 0x00010000,
    SPELL_ATTR0_CU_REQ_CASTER_BEHIND_TARGET     = 0x00020000,
    SPELL_ATTR0_CU_ALLOW_INFLIGHT_TARGET        = 0x00040000,
    SPELL_ATTR0_CU_NEEDS_AMMO_DATA              = 0x00080000,
    SPELL_ATTR0_CU_BINARY_SPELL                 = 0x00100000,
    SPELL_ATTR0_CU_SCHOOLMASK_NORMAL_WITH_MAGIC = 0x00200000,
    SPELL_ATTR0_CU_DEPRECATED_LIQUID_AURA       = 0x00400000, // DO NOT REUSE
    SPELL_ATTR0_CU_IS_TALENT                    = 0x00800000, // reserved for master branch
    SPELL_ATTR0_CU_AURA_CANNOT_BE_SAVED         = 0x01000000,
    SPELL_ATTR0_CU_DONT_RESTART_P_TIMER         = 0x02000000,
    SPELL_ATTR0_CU_USE_RANGED_NO_AMMO           = 0x04000000,
    SPELL_ATTR0_CU_UNUSED27                     = 0x08000000, // Your should add new attributes to unused ones
    SPELL_ATTR0_CU_UNUSED28                     = 0x10000000,
    SPELL_ATTR0_CU_UNUSED29                     = 0x20000000,
    SPELL_ATTR0_CU_UNUSED30                     = 0x40000000,
    SPELL_ATTR0_CU_BYPASS_MECHANIC_IMMUNITY     = 0x80000000,

    SPELL_ATTR0_CU_NEGATIVE = SPELL_ATTR0_CU_NEGATIVE_EFF0 | SPELL_ATTR0_CU_NEGATIVE_EFF1 | SPELL_ATTR0_CU_NEGATIVE_EFF2
};

enum SpellAttr1Cu : uint32_t
{
    SPELL_ATTR1_CU_REQUIRES_COMBAT                      = 0x00000001,
    SPELL_ATTR1_CU_NO_ATTACK_BLOCK                      = 0x00000002,
    SPELL_ATTR1_CU_SCALE_DAMAGE_EFFECTS_ONLY            = 0x00000004,
    SPELL_ATTR1_CU_SCALE_HEALING_EFFECTS_ONLY           = 0x00000008,
    SPELL_ATTR1_CU_USE_TARGETS_LEVEL_FOR_SPELL_SCALING  = 0x00000010,
    SPELL_ATTR1_CU_REMOVE_OUTSIDE_DUNGEONS_AND_RAIDS    = 0x00000020,
    SPELL_ATTR1_CU_NOT_USABLE_IN_INSTANCES              = 0x00000040,
    SPELL_ATTR1_CU_USABLE_IN_INSTANCES_ONLY             = 0x00000080,
    SPELL_ATTR1_CU_PERIODIC_CAN_CRIT                    = 0x00000100,
    SPELL_ATTR1_CU_IGNORES_CASTER_LEVEL                 = 0x00000200,
    SPELL_ATTR1_CU_ONLY_PROC_FROM_CLASS_ABILITIES       = 0x00000400, // TODO
    SPELL_ATTR1_CU_ACTIVATES_REQUIRED_SHAPESHIFT        = 0x00000800, // TODO
    SPELL_ATTR1_CU_REAPPLY_NO_REFRESH_DURATION          = 0x00001000,
    SPELL_ATTR1_CU_SPECIAL_DELAY_CALCULATION            = 0x00002000,
    SPELL_ATTR1_CU_CAST_TIME_UNAFFECTED_BY_HASTE        = 0x00004000,
    SPELL_ATTR1_CU_LOW_CAST_TIME_OK                     = 0x00008000, // sets result to SPELL_CAST_OK if spell cast time is <= 250ms
    SPELL_ATTR1_CU_STACKS_DONT_ADDUP                    = 0x00010000,
    SPELL_ATTR1_CU_PANDEMIC_TIMER                       = 0x00020000,
    SPELL_ATTR1_CU_MISSILE_SPEED_IS_DELAY_IN_SEC        = 0x00040000,
    SPELL_ATTR1_CU_ALLOW_DEFENSE_WHILE_CASTING          = 0x00080000,
    SPELL_ATTR1_CU_DROP_STACK_ON_EXPIRE                 = 0x00100000,
    SPELL_ATTR1_CU_COMBODAMAGE                          = 0x00200000, // Combo points scale damage
    SPELL_ATTR1_CU_COMBODURATION                        = 0x00400000, // Combo points scale duration
    SPELL_ATTR1_CU_UNUSED55                             = 0x00800000,
    SPELL_ATTR1_CU_UNUSED56                             = 0x01000000,
    SPELL_ATTR1_CU_UNUSED57                             = 0x02000000,
    SPELL_ATTR1_CU_UNUSED58                             = 0x04000000,
    SPELL_ATTR1_CU_UNUSED59                             = 0x08000000,
    SPELL_ATTR0_CU_UNUSED60                             = 0x10000000,
    SPELL_ATTR0_CU_UNUSED61                             = 0x20000000,
    SPELL_ATTR0_CU_UNUSED62                             = 0x40000000,
    SPELL_ATTR0_CU_UNUSED63                             = 0x80000000,
};

enum SpellAttr2Cu : uint32_t
{
    SPELL_ATTR2_CU_TREAT_AS_INSTANT             = 0x00000001, // Changes tooltip line responsible for cast time to "Instant"
    SPELL_ATTR2_CU_FORCE_HIDE_CASTBAR           = 0x00000002, // Does not display cast bar
    SPELL_ATTR2_CU_DO_NOT_DISPLAY_POWER_COST    = 0x00000004, // Does not display power cost in tooltip
    SPELL_ATTR2_CU_SUPPRESS_LEARN_MSG           = 0x00000008, // Does not display "You have learned a new spell:" message
    SPELL_ATTR2_CU_SUPPRESS_UNLEARN_MSG         = 0x00000010, // Does not display "You have unlearned" message
    SPELL_ATTR2_CU_INVERT_CASTBAR               = 0x00000020, // NYI; will cost me some sanity it seems
    SPELL_ATTR2_CU_LOW_TIME_TREAT_AS_INSTANT    = 0x00000040, // If cast time <= 250ms, changes tooltip line responsible to "Instant"
    SPELL_ATTR2_CU_LOW_TIME_FORCE_HIDE_CASTBAR  = 0x00000080, // If cast time <= 250ms, does not display cast bar
    SPELL_ATTR2_CU_LOW_CAST_TIME_DONT_INTERRUPT = 0x00000100, // If cast time <= 250ms, does not interrupt
};

inline bool HasAttribute(SpellRow* spell, SpellAttr0 attribute)
{
    return !!(spell->m_attributes & attribute);
}
inline bool HasAttribute(SpellRow* spell, SpellAttr1 attribute)
{
    return !!(spell->m_attributesEx & attribute);
}
inline bool HasAttribute(SpellRow* spell, SpellAttr2 attribute)
{
    return !!(spell->m_attributesExB & attribute);
}
inline bool HasAttribute(SpellRow* spell, SpellAttr3 attribute)
{
    return !!(spell->m_attributesExC & attribute);
}
//inline bool HasAttribute(SpellRow* spell, SpellAttr4 attribute)
//{
//    return !!(spell->m_attributesExD & attribute);
//}
//inline bool HasAttribute(SpellRow* spell, SpellAttr5 attribute)
//{
//    return !!(spell->m_attributesExE & attribute);
//}
//inline bool HasAttribute(SpellRow* spell, SpellAttr6 attribute)
//{
//    return !!(spell->m_attributesExF & attribute);
//}
//inline bool HasAttribute(SpellRow* spell, SpellAttr7 attribute)
//{
//    return !!(spell->m_attributesExG & attribute);
//}

inline bool HasAttribute(SpellRow* spell, SpellAttr0Cu customAttribute)
{
    SpellAdditionalAttributesRow* customAttributesRow =
        GlobalCDBCMap.getRow<SpellAdditionalAttributesRow>("SpellAdditionalAttributes", spell->m_ID);
    if (customAttributesRow)
    {
        return !!(customAttributesRow->customAttr0 & customAttribute);
    }

    return false;
}
inline bool HasAttribute(SpellRow* spell, SpellAttr1Cu customAttribute)
{
    SpellAdditionalAttributesRow* customAttributesRow =
        GlobalCDBCMap.getRow<SpellAdditionalAttributesRow>("SpellAdditionalAttributes", spell->m_ID);
    if (customAttributesRow)
    {
        return !!(customAttributesRow->customAttr1 & customAttribute);
    }

    return false;
}
inline bool HasAttribute(SpellRow* spell, SpellAttr2Cu customAttribute)
{
    SpellAdditionalAttributesRow* customAttributesRow =
        GlobalCDBCMap.getRow<SpellAdditionalAttributesRow>("SpellAdditionalAttributes", spell->m_ID);
    if (customAttributesRow)
    {
        return !!(customAttributesRow->customAttr2 & customAttribute);
    }

    return false;
}
// inline bool HasAttribute(SpellRow* spell, SpellAttr3Cu customAttribute)
//{
//     SpellAdditionalAttributesRow* customAttributesRow =
//     GlobalCDBCMap.getRow<SpellAdditionalAttributesRow>("SpellAdditionalAttributes", spell->m_ID); if
//     (customAttributesRow) {
//         return !!(customAttributesRow->customAttr3 & customAttribute);
//     }
//
//     return false;
// }
