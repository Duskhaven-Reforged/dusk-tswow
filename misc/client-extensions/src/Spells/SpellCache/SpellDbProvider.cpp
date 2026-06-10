#include <Spells/SpellCache/SpellDbProvider.h>

#include <CDBCMgr/CDBCMgr.h>
#include <CDBCMgr/CDBCDefs/Spell.h>
#include <CDBCMgr/CDBCDefs/SpellEffect.h>
#include <Spells/SpellCache/SpellCacheAuthority.h>
#include <Spells/SpellCache/SpellCacheStreaming.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <unordered_map>

namespace
{
    constexpr uint32_t SPELL_CACHE_STREAM_VERSION_PROVIDER = 4;
    constexpr uint32_t SPELL_CACHE_MAX_REASONABLE_SPELL_ID_PROVIDER = 1000000;
    constexpr uint32_t STREAMED_SPELL_ATTR_EX_D_MISSILE_TRAJECTORY = 0x40000;

    struct StableSpellRow
    {
        uint32_t spellDataHash = 0;
        std::unique_ptr<ClientData::SpellRow> row;
    };

    std::unordered_map<uint32_t, StableSpellRow> StableRows;

    bool HasReasonableSpellId(uint32_t spellId)
    {
        return spellId > 0 && spellId <= SPELL_CACHE_MAX_REASONABLE_SPELL_ID_PROVIDER;
    }

    SpellCacheRow* TryGetCacheRow(uint32_t spellId, uint32_t spellDataHash)
    {
        if (!HasReasonableSpellId(spellId))
            return nullptr;

        SpellCacheRow* cached = GlobalCDBCMap.getRow<SpellCacheRow>("Spell", static_cast<int>(spellId));
        if (!cached || cached->cacheVersion != SPELL_CACHE_STREAM_VERSION_PROVIDER)
            return nullptr;

        if (spellDataHash && cached->spellDataHash != spellDataHash)
            return nullptr;

        if (!spellDataHash && cached->spellDataHash == 0)
            return nullptr;

        return cached;
    }

    void FillEffect(ClientData::SpellRow& out, SpellEffectCacheRow const& effect)
    {
        uint32_t const i = effect.effectIndex;
        if (i >= 3)
            return;

        out.m_effect[i] = effect.effect;
        out.m_effectDieSides[i] = static_cast<uint32_t>(effect.effectDieSides);
        out.m_effectRealPointsPerLevel[i] = effect.effectRealPointsPerLevel;
        out.m_effectBasePoints[i] = static_cast<uint32_t>(effect.effectBasePoints);
        out.m_effectMechanic[i] = effect.effectMechanic;
        out.m_implicitTargetA[i] = effect.effectImplicitTargetA;
        out.m_implicitTargetB[i] = effect.effectImplicitTargetB;
        out.m_effectRadiusIndex[i] = effect.effectRadiusIndex;
        out.m_effectAura[i] = effect.effectApplyAuraName;
        out.m_effectAuraPeriod[i] = effect.effectAmplitude;
        out.m_effectAmplitude[i] = effect.effectMultipleValue;
        out.m_effectChainTargets[i] = effect.effectChainTargets;
        out.m_effectItemType[i] = static_cast<uint32_t>(effect.effectItemType);
        out.m_effectMiscValue[i] = static_cast<uint32_t>(effect.effectMiscValue);
        out.m_effectMiscValueB[i] = static_cast<uint32_t>(effect.effectMiscValueB);
        out.m_effectTriggerSpell[i] = effect.effectTriggerSpell;
        out.m_effectPointsPerCombo[i] = effect.effectPointsPerCombo;
        out.m_effectSpellClassMask[i][0] = effect.effectSpellClassMaskA;
        out.m_effectSpellClassMask[i][1] = effect.effectSpellClassMaskB;
        out.m_effectSpellClassMask[i][2] = effect.effectSpellClassMaskC;
        out.m_effectChainAmplitude[i] = effect.effectChainAmplitude;
        out.m_effectBonusCoefficient[i] = effect.effectBonusMultiplier;
    }

    bool BuildSpellRow(SpellCacheRow const& cached, ClientData::SpellRow& out)
    {
        std::memset(&out, 0, sizeof(out));
        out.m_ID = cached.id;
        out.m_category = cached.category;
        out.m_dispelType = cached.dispel;
        out.m_mechanic = cached.mechanic;
        out.m_attributes = cached.attributes;
        out.m_attributesEx = cached.attributesEx;
        out.m_attributesExB = cached.attributesEx2;
        out.m_attributesExC = cached.attributesEx3;
        out.m_attributesExD = cached.attributesEx4;
        if (out.m_ID == 82893)
            out.m_attributesExD &= ~STREAMED_SPELL_ATTR_EX_D_MISSILE_TRAJECTORY;
        out.m_attributesExE = cached.attributesEx5;
        out.m_attributesExF = cached.attributesEx6;
        out.m_attributesExG = cached.attributesEx7;
        out.m_shapeshiftMask[0] = cached.stances;
        out.m_shapeshiftExclude[0] = cached.stancesNot;
        out.m_targets = cached.targets;
        out.m_targetCreatureType = cached.targetCreatureType;
        out.m_requiresSpellFocus = cached.requiresSpellFocus;
        out.m_facingCasterFlags = cached.facingCasterFlags;
        out.m_casterAuraState = cached.casterAuraState;
        out.m_targetAuraState = cached.targetAuraState;
        out.m_excludeCasterAuraState = cached.excludeCasterAuraState;
        out.m_excludeTargetAuraState = cached.excludeTargetAuraState;
        out.m_casterAuraSpell = cached.casterAuraSpell;
        out.m_targetAuraSpell = cached.targetAuraSpell;
        out.m_excludeCasterAuraSpell = cached.excludeCasterAuraSpell;
        out.m_excludeTargetAuraSpell = cached.excludeTargetAuraSpell;
        out.m_castingTimeIndex = cached.castingTimeIndex;
        out.m_recoveryTime = cached.recoveryTime;
        out.m_categoryRecoveryTime = cached.categoryRecoveryTime;
        out.m_interruptFlags = cached.interruptFlags;
        out.m_auraInterruptFlags = cached.auraInterruptFlags;
        out.m_channelInterruptFlags = cached.channelInterruptFlags;
        out.m_procTypeMask = cached.procFlags;
        out.m_procChance = cached.procChance;
        out.m_procCharges = cached.procCharges;
        out.m_maxLevel = cached.maxLevel;
        out.m_baseLevel = cached.baseLevel;
        out.m_spellLevel = cached.spellLevel;
        out.m_durationIndex = cached.durationIndex;
        out.m_powerType = static_cast<uint32_t>(cached.powerType);
        out.m_manaCost = cached.manaCost;
        out.m_manaCostPerLevel = cached.manaCostPerLevel;
        out.m_manaPerSecond = cached.manaPerSecond;
        out.m_manaPerSecondPerLevel = cached.manaPerSecondPerLevel;
        out.m_rangeIndex = cached.rangeIndex;
        out.m_speed = cached.speed;
        out.m_modalNextSpell = cached.modalNextSpell;
        out.m_cumulativeAura = cached.stackAmount;
        out.m_totem0[0] = cached.totem1;
        out.m_totem0[1] = cached.totem2;
        out.m_reagent[0] = static_cast<uint32_t>(cached.reagent1);
        out.m_reagent[1] = static_cast<uint32_t>(cached.reagent2);
        out.m_reagent[2] = static_cast<uint32_t>(cached.reagent3);
        out.m_reagent[3] = static_cast<uint32_t>(cached.reagent4);
        out.m_reagent[4] = static_cast<uint32_t>(cached.reagent5);
        out.m_reagent[5] = static_cast<uint32_t>(cached.reagent6);
        out.m_reagent[6] = static_cast<uint32_t>(cached.reagent7);
        out.m_reagent[7] = static_cast<uint32_t>(cached.reagent8);
        out.m_reagentCount[0] = static_cast<uint32_t>(cached.reagentCount1);
        out.m_reagentCount[1] = static_cast<uint32_t>(cached.reagentCount2);
        out.m_reagentCount[2] = static_cast<uint32_t>(cached.reagentCount3);
        out.m_reagentCount[3] = static_cast<uint32_t>(cached.reagentCount4);
        out.m_reagentCount[4] = static_cast<uint32_t>(cached.reagentCount5);
        out.m_reagentCount[5] = static_cast<uint32_t>(cached.reagentCount6);
        out.m_reagentCount[6] = static_cast<uint32_t>(cached.reagentCount7);
        out.m_reagentCount[7] = static_cast<uint32_t>(cached.reagentCount8);
        out.m_equippedItemClass = static_cast<uint32_t>(cached.equippedItemClass);
        out.m_equippedItemSubclass = static_cast<uint32_t>(cached.equippedItemSubClassMask);
        out.m_equippedItemInvTypes = static_cast<uint32_t>(cached.equippedItemInventoryTypeMask);
        out.m_spellVisualID[0] = cached.spellVisualID1;
        out.m_spellVisualID[1] = cached.spellVisualID2;
        out.m_spellIconID = cached.spellIconID;
        out.m_activeIconID = cached.activeIconID;
        out.m_spellPriority = cached.spellPriority;
        out.m_name_lang = cached.spellName;
        out.m_nameSubtext_lang = cached.spellRank;
        out.m_description_lang = cached.description;
        out.m_auraDescription_lang = cached.auraDescription;
        out.m_manaCostPct = cached.manaCostPct;
        out.m_startRecoveryCategory = cached.startRecoveryCategory;
        out.m_startRecoveryTime = cached.startRecoveryTime;
        out.m_maxTargetLevel = cached.maxTargetLevel;
        out.m_spellClassSet = cached.spellFamilyName;
        out.m_spellClassMask[0] = cached.spellFamilyFlags1;
        out.m_spellClassMask[1] = cached.spellFamilyFlags2;
        out.m_spellClassMask[2] = cached.spellFamilyFlags3;
        out.m_maxTargets = cached.maxAffectedTargets;
        out.m_defenseType = cached.dmgClass;
        out.m_preventionType = cached.preventionType;
        out.m_stanceBarOrder = cached.stanceBarOrder;
        out.m_minFactionID = cached.minFactionID;
        out.m_minReputation = cached.minReputation;
        out.m_requiredAuraVision = cached.requiredAuraVision;
        out.m_requiredTotemCategoryID[0] = cached.requiredTotemCategoryID1;
        out.m_requiredTotemCategoryID[1] = cached.requiredTotemCategoryID2;
        out.m_requiredAreasID = static_cast<uint32_t>(cached.areaGroupId);
        out.m_schoolMask = cached.schoolMask;
        out.m_runeCostID = cached.runeCostID;
        out.m_spellMissileID = cached.spellMissileID;
        out.m_powerDisplayID = static_cast<uint32_t>(cached.powerDisplayID);
        out.m_descriptionVariablesID = cached.descriptionVariablesID;
        out.m_difficulty = cached.difficulty;

        for (uint32_t effectIndex = 0; effectIndex < 32; ++effectIndex)
        {
            SpellEffectCacheRow* effect = GlobalCDBCMap.getRow<SpellEffectCacheRow>(
                "SpellEffect", SpellEffectCacheKey(cached.id, effectIndex));
            if (effect)
                FillEffect(out, *effect);
        }

        return true;
    }
}

namespace SpellDbProvider
{
    bool HasReady(uint32_t spellId, uint32_t spellDataHash)
    {
        if (TryGetCacheRow(spellId, spellDataHash))
        {
            SpellCacheAuthority::MarkReady(spellId, TryGetCacheRow(spellId, spellDataHash)->spellDataHash);
            return true;
        }

        return false;
    }

    bool TryGetRowCopy(uint32_t spellId, ClientData::SpellRow& out)
    {
        SpellCacheRow* cached = TryGetCacheRow(spellId, 0);
        if (!cached)
            return false;

        if (!BuildSpellRow(*cached, out))
            return false;

        SpellCacheAuthority::MarkReady(spellId, cached->spellDataHash);
        return true;
    }

    ClientData::SpellRow* TryGetRowPtr(uint32_t spellId)
    {
        SpellCacheRow* cached = TryGetCacheRow(spellId, 0);
        if (!cached)
            return nullptr;

        auto found = StableRows.find(spellId);
        if (found != StableRows.end() && found->second.spellDataHash == cached->spellDataHash)
            return found->second.row.get();

        ClientData::SpellRow row{};
        if (!BuildSpellRow(*cached, row))
            return nullptr;

        StableSpellRow stable{};
        stable.spellDataHash = cached->spellDataHash;
        stable.row = std::make_unique<ClientData::SpellRow>(row);
        ClientData::SpellRow* raw = stable.row.get();
        StableRows[spellId] = std::move(stable);
        SpellCacheAuthority::MarkReady(spellId, cached->spellDataHash);
        return raw;
    }

    bool RequestOrGet(uint32_t spellId, ClientData::SpellRow& out, RequestPolicy policy)
    {
        if (TryGetRowCopy(spellId, out))
            return true;

        if (policy == RequestPolicy::RequestOnMiss)
            SpellCacheStreaming::RequestSpell(spellId);

        return false;
    }

    uint32_t ForEachEffect(uint32_t spellId, const std::function<bool(const SpellEffectCacheRow&)>& visitor)
    {
        if (!HasReasonableSpellId(spellId))
            return 0;

        uint32_t visited = 0;
        for (uint32_t effectIndex = 0; effectIndex < 32; ++effectIndex)
        {
            SpellEffectCacheRow* effect = GlobalCDBCMap.getRow<SpellEffectCacheRow>(
                "SpellEffect", SpellEffectCacheKey(spellId, effectIndex));
            if (!effect)
                continue;

            ++visited;
            if (visitor && !visitor(*effect))
                break;
        }

        return visited;
    }

    uint32_t GetEffectCount(uint32_t spellId)
    {
        return ForEachEffect(spellId, [](SpellEffectCacheRow const&) {
            return true;
        });
    }

    bool TryGetEffect(uint32_t spellId, uint32_t effectIndex, const SpellEffectCacheRow*& out)
    {
        out = nullptr;
        if (!HasReasonableSpellId(spellId) || effectIndex >= 32)
            return false;

        out = GlobalCDBCMap.getRow<SpellEffectCacheRow>(
            "SpellEffect", SpellEffectCacheKey(spellId, effectIndex));
        return out != nullptr;
    }

    void Invalidate(uint32_t spellId)
    {
        StableRows.erase(spellId);
    }
}
