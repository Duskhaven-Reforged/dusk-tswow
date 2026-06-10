#pragma once

#include <ClientData/SharedDefines.h>

using namespace ClientData;

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>

class Spells {
  public:
    static inline bool s_castAtCursor = false;
    static inline CVar* g_spell_min_clip_distance_percentage_cvar;
    static char SpellMinClipDistancePercentage_CVarCallback(CVar* cvar, const char*, const char* value, const char*);

  private:
    static void Apply();

    friend class ClientExtensions;
};

const enum SpellMods {
    Damage,
    Duration,
    Threat,
    Effect1,
    ProcCharges,
    Range,
    Radius,
    CritChance,
    AllEffects,
    NotLoseCastingtime,
    CastingTime,
    Cooldown,
    Effect2,
    IgnoreArmor,
    Cost,
    CritDamage,
    ResistMissChance,
    ChainTargets,
    ProcChance,
    Period,
    DamageMult,
    GCD,
    DoT,
    Effect3,
    ScalingRatio,
    HoT,
    PPM,
    ValueMult,
    ResistDispelChance,
    Healing,
    RefundOnFail,
    StackAmount,
    TargetCount
};

const enum SpellEffects {
    SchoolDamage = 2,
    ApplyAura = 6,
    HealthLeech = 9,
    Heal = 10,
    HealMaxHealth = 67,
    HealMechanical = 75,
    HealPct = 136,
};

const enum Auras {
    PeriodicDamage = 3,
    PeriodicHeal = 8,
    PeriodicEnergize = 24,
    PeriodicLeech = 53,
    PeriodicHealthFunnel = 62,
    PeriodicManaLeech = 64,
    PeriodicDamagePct = 89,
    PeriodicDummy = 226,
    PeriodicTriggerSpellWithValue = 227,
};

const enum SpellMechanics {
    Bleed = 15,
};
