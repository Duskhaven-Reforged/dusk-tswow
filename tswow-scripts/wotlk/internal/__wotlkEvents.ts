/*
 * This file is part of tswow (https://github.com/tswow)
 *
 * Copyright (C) 2020 tswow <https://github.com/tswow/>
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
import { DBCFile } from '../../data/dbc/DBCFile';
import { BuildArgs, dataset } from '../../data/Settings';
import { SqlConnection } from '../../data/sql/SQLConnection';
import { SqlTable } from '../../data/sql/SQLTable';
import { WFile } from '../../util/FileTree';
import { DBCFiles } from '../DBCFiles';
import { CDBCFiles } from '../CDBCFiles';
import { DBC } from '../DBCFiles';
import { SQLTables } from '../SQLFiles';
import { SQL } from '../SQLFiles';

function stableHashPart(hash: number, value: any) {
    const text = String(value);
    for (let i = 0; i < text.length; ++i) {
        hash ^= text.charCodeAt(i);
        hash = Math.imul(hash, 16777619) >>> 0;
    }
    hash ^= 124;
    return Math.imul(hash, 16777619) >>> 0;
}

function stableSpellDataHash(parts: any[]) {
    let hash = 2166136261 >>> 0;
    parts.forEach(part => {
        hash = stableHashPart(hash, part);
    });
    return hash & 0x7fffffff;
}

function legacySpellEffectValues(spell: any, effectIndex: number) {
    return {
        Effect: spell.Effect.getIndex(effectIndex),
        EffectDieSides: spell.EffectDieSides.getIndex(effectIndex),
        EffectRealPointsPerLevel: spell.EffectRealPointsPerLevel.getIndex(effectIndex),
        EffectBasePoints: spell.EffectBasePoints.getIndex(effectIndex),
        EffectMechanic: spell.EffectMechanic.getIndex(effectIndex),
        EffectImplicitTargetA: spell.ImplicitTargetA.getIndex(effectIndex),
        EffectImplicitTargetB: spell.ImplicitTargetB.getIndex(effectIndex),
        EffectRadiusIndex: spell.EffectRadiusIndex.getIndex(effectIndex),
        EffectApplyAuraName: spell.EffectAura.getIndex(effectIndex),
        EffectAmplitude: spell.EffectAuraPeriod.getIndex(effectIndex),
        EffectMultipleValue: spell.EffectMultipleValue.getIndex(effectIndex),
        EffectChainTargets: spell.EffectChainTargets.getIndex(effectIndex),
        EffectItemType: spell.EffectItemType.getIndex(effectIndex),
        EffectMiscValue: spell.EffectMiscValue.getIndex(effectIndex),
        EffectMiscValueB: spell.EffectMiscValueB.getIndex(effectIndex),
        EffectTriggerSpell: spell.EffectTriggerSpell.getIndex(effectIndex),
        EffectPointsPerCombo: spell.EffectPointsPerCombo.getIndex(effectIndex),
        EffectSpellClassMaskA: spell.EffectSpellClassMaskA.getIndex(effectIndex),
        EffectSpellClassMaskB: spell.EffectSpellClassMaskB.getIndex(effectIndex),
        EffectSpellClassMaskC: spell.EffectSpellClassMaskC.getIndex(effectIndex),
        EffectChainAmplitude: spell.EffectChainAmplitude.getIndex(effectIndex),
        EffectBonusMultiplier: spell.EffectBonusMultiplier.getIndex(effectIndex),
    };
}

function isDefaultSpellEffect(values: ReturnType<typeof legacySpellEffectValues>) {
    return values.Effect === 0
        && values.EffectDieSides === 0
        && values.EffectRealPointsPerLevel === 0
        && values.EffectBasePoints === 0
        && values.EffectMechanic === 0
        && values.EffectImplicitTargetA === 0
        && values.EffectImplicitTargetB === 0
        && values.EffectRadiusIndex === 0
        && values.EffectApplyAuraName === 0
        && values.EffectAmplitude === 0
        && values.EffectMultipleValue === 0
        && values.EffectChainTargets === 0
        && values.EffectItemType === 0
        && values.EffectMiscValue === 0
        && values.EffectMiscValueB === 0
        && values.EffectTriggerSpell === 0
        && values.EffectPointsPerCombo === 0
        && values.EffectSpellClassMaskA === 0
        && values.EffectSpellClassMaskB === 0
        && values.EffectSpellClassMaskC === 0
        && values.EffectChainAmplitude === 1
        && values.EffectBonusMultiplier === 0;
}

function spellEffectProjection(spell: any, effectIndex: number) {
    const spellId = spell.ID.get();
    const row = SQL.spell_effects.findBySpellEffect(spellId, effectIndex);
    if (row) {
        return {
            Effect: row.Effect.get(),
            EffectDieSides: row.EffectDieSides.get(),
            EffectRealPointsPerLevel: row.EffectRealPointsPerLevel.get(),
            EffectBasePoints: row.EffectBasePoints.get(),
            EffectMechanic: row.EffectMechanic.get(),
            EffectImplicitTargetA: row.EffectImplicitTargetA.get(),
            EffectImplicitTargetB: row.EffectImplicitTargetB.get(),
            EffectRadiusIndex: row.EffectRadiusIndex.get(),
            EffectApplyAuraName: row.EffectApplyAuraName.get(),
            EffectAmplitude: row.EffectAmplitude.get(),
            EffectMultipleValue: row.EffectMultipleValue.get(),
            EffectChainTargets: row.EffectChainTargets.get(),
            EffectItemType: row.EffectItemType.get(),
            EffectMiscValue: row.EffectMiscValue.get(),
            EffectMiscValueB: row.EffectMiscValueB.get(),
            EffectTriggerSpell: row.EffectTriggerSpell.get(),
            EffectPointsPerCombo: row.EffectPointsPerCombo.get(),
            EffectSpellClassMaskA: row.EffectSpellClassMaskA.get(),
            EffectSpellClassMaskB: row.EffectSpellClassMaskB.get(),
            EffectSpellClassMaskC: row.EffectSpellClassMaskC.get(),
            EffectChainAmplitude: row.EffectChainAmplitude.get(),
            EffectBonusMultiplier: row.EffectBonusMultiplier.get(),
        };
    }

    const values = legacySpellEffectValues(spell, effectIndex);
    if (!isDefaultSpellEffect(values)) {
        SQL.spell_effects.upsert(spellId, effectIndex, values);
    }
    return values;
}

function projectLegacySpellEffects() {
    DBC.Spell.queryAll({}).forEach(spell => {
        spellEffectProjection(spell, 0);
        spellEffectProjection(spell, 1);
        spellEffectProjection(spell, 2);
    });
}

function spellEffectRowHashParts(row: any) {
    return [
        row.EffectIndex.get(),
        row.Effect.get(),
        row.EffectDieSides.get(),
        row.EffectRealPointsPerLevel.get(),
        row.EffectBasePoints.get(),
        row.EffectMechanic.get(),
        row.EffectImplicitTargetA.get(),
        row.EffectImplicitTargetB.get(),
        row.EffectRadiusIndex.get(),
        row.EffectApplyAuraName.get(),
        row.EffectAmplitude.get(),
        row.EffectMultipleValue.get(),
        row.EffectChainTargets.get(),
        row.EffectItemType.get(),
        row.EffectMiscValue.get(),
        row.EffectMiscValueB.get(),
        row.EffectTriggerSpell.get(),
        row.EffectPointsPerCombo.get(),
        row.EffectSpellClassMaskA.get(),
        row.EffectSpellClassMaskB.get(),
        row.EffectSpellClassMaskC.get(),
        row.EffectChainAmplitude.get(),
        row.EffectBonusMultiplier.get(),
    ];
}

function buildSpellEffectHashIndex() {
    const bySpellId = new Map<number, any[]>();
    SQL.spell_effects.queryAll({})
        .filter(row => !row.isDeleted())
        .sort((a, b) => {
            const spellCompare = a.SpellID.get() - b.SpellID.get();
            return spellCompare !== 0
                ? spellCompare
                : a.EffectIndex.get() - b.EffectIndex.get();
        })
        .forEach(row => {
            const spellId = row.SpellID.get();
            let parts = bySpellId.get(spellId);
            if (!parts) {
                parts = [];
                bySpellId.set(spellId, parts);
            }
            parts.push(...spellEffectRowHashParts(row));
        });
    return bySpellId;
}

function spellCoreHashParts(spell: any, effect0: ReturnType<typeof spellEffectProjection>, effect1: ReturnType<typeof spellEffectProjection>, effect2: ReturnType<typeof spellEffectProjection>) {
    return [
        spell.ID.get(),
        spell.Category.get(),
        spell.DispelType.get(),
        spell.Mechanic.get(),
        spell.Attributes.get(),
        spell.AttributesEx.get(),
        spell.AttributesExB.get(),
        spell.AttributesExC.get(),
        spell.AttributesExD.get(),
        spell.AttributesExE.get(),
        spell.AttributesExF.get(),
        spell.AttributesExG.get(),
        Number(spell.ShapeshiftMask.get()),
        Number(spell.ShapeshiftExclude.get()),
        spell.Targets.get(),
        spell.TargetCreatureType.get(),
        spell.RequiresSpellFocus.get(),
        spell.FacingCasterFlags.get(),
        spell.CasterAuraState.get(),
        spell.TargetAuraState.get(),
        spell.ExcludeCasterAuraState.get(),
        spell.ExcludeTargetAuraState.get(),
        spell.CasterAuraSpell.get(),
        spell.TargetAuraSpell.get(),
        spell.ExcludeCasterAuraSpell.get(),
        spell.ExcludeTargetAuraSpell.get(),
        spell.CastingTimeIndex.get(),
        spell.RecoveryTime.get(),
        spell.CategoryRecoveryTime.get(),
        spell.InterruptFlags.get(),
        spell.AuraInterruptFlags.get(),
        spell.ChannelInterruptFlags.get(),
        spell.ProcTypeMask.get(),
        spell.ProcChance.get(),
        spell.ProcCharges.get(),
        spell.MaxLevel.get(),
        spell.BaseLevel.get(),
        spell.SpellLevel.get(),
        spell.DurationIndex.get(),
        spell.PowerType.get(),
        spell.ManaCost.get(),
        spell.ManaCostPerLevel.get(),
        spell.ManaPerSecond.get(),
        spell.ManaPerSecondPerLevel.get(),
        spell.RangeIndex.get(),
        spell.Speed.get(),
        spell.ModalNextSpell.get(),
        spell.CumulativeAura.get(),
        spell.Totem.getIndex(0),
        spell.Totem.getIndex(1),
        spell.Reagent.getIndex(0),
        spell.Reagent.getIndex(1),
        spell.Reagent.getIndex(2),
        spell.Reagent.getIndex(3),
        spell.Reagent.getIndex(4),
        spell.Reagent.getIndex(5),
        spell.Reagent.getIndex(6),
        spell.Reagent.getIndex(7),
        spell.ReagentCount.getIndex(0),
        spell.ReagentCount.getIndex(1),
        spell.ReagentCount.getIndex(2),
        spell.ReagentCount.getIndex(3),
        spell.ReagentCount.getIndex(4),
        spell.ReagentCount.getIndex(5),
        spell.ReagentCount.getIndex(6),
        spell.ReagentCount.getIndex(7),
        spell.EquippedItemClass.get(),
        spell.EquippedItemSubclass.get(),
        spell.EquippedItemInvTypes.get(),
        spell.SpellVisualID.getIndex(0),
        spell.SpellVisualID.getIndex(1),
        spell.SpellIconID.get(),
        spell.ActiveIconID.get(),
        spell.SpellPriority.get(),
        spell.Name.lang('enGB').get(),
        spell.NameSubtext.lang('enGB').get(),
        spell.Description.lang('enGB').get(),
        spell.AuraDescription.lang('enGB').get(),
        spell.ManaCostPct.get(),
        spell.StartRecoveryCategory.get(),
        spell.StartRecoveryTime.get(),
        spell.MaxTargetLevel.get(),
        spell.SpellClassSet.get(),
        spell.SpellClassMask.getIndex(0),
        spell.SpellClassMask.getIndex(1),
        spell.SpellClassMask.getIndex(2),
        spell.MaxTargets.get(),
        spell.DefenseType.get(),
        spell.PreventionType.get(),
        spell.StanceBarOrder.get(),
        spell.MinFactionID.get(),
        spell.MinReputation.get(),
        spell.RequiredAuraVision.get(),
        spell.RequiredTotemCategoryID.getIndex(0),
        spell.RequiredTotemCategoryID.getIndex(1),
        spell.RequiredAreasID.get(),
        spell.SchoolMask.get(),
        spell.RuneCostID.get(),
        spell.SpellMissileID.get(),
        spell.PowerDisplayID.get(),
        spell.SpellDescriptionVariableID.get(),
        spell.SpellDifficultyID.get(),
        effect0.Effect,
        effect1.Effect,
        effect2.Effect,
        effect0.EffectDieSides,
        effect1.EffectDieSides,
        effect2.EffectDieSides,
        effect0.EffectRealPointsPerLevel,
        effect1.EffectRealPointsPerLevel,
        effect2.EffectRealPointsPerLevel,
        effect0.EffectBasePoints,
        effect1.EffectBasePoints,
        effect2.EffectBasePoints,
        effect0.EffectMechanic,
        effect1.EffectMechanic,
        effect2.EffectMechanic,
        effect0.EffectImplicitTargetA,
        effect1.EffectImplicitTargetA,
        effect2.EffectImplicitTargetA,
        effect0.EffectImplicitTargetB,
        effect1.EffectImplicitTargetB,
        effect2.EffectImplicitTargetB,
        effect0.EffectRadiusIndex,
        effect1.EffectRadiusIndex,
        effect2.EffectRadiusIndex,
        effect0.EffectApplyAuraName,
        effect1.EffectApplyAuraName,
        effect2.EffectApplyAuraName,
        effect0.EffectAmplitude,
        effect1.EffectAmplitude,
        effect2.EffectAmplitude,
        effect0.EffectMultipleValue,
        effect1.EffectMultipleValue,
        effect2.EffectMultipleValue,
        effect0.EffectItemType,
        effect1.EffectItemType,
        effect2.EffectItemType,
        effect0.EffectMiscValue,
        effect1.EffectMiscValue,
        effect2.EffectMiscValue,
        effect0.EffectMiscValueB,
        effect1.EffectMiscValueB,
        effect2.EffectMiscValueB,
        effect0.EffectTriggerSpell,
        effect1.EffectTriggerSpell,
        effect2.EffectTriggerSpell,
        effect0.EffectSpellClassMaskA,
        effect1.EffectSpellClassMaskA,
        effect2.EffectSpellClassMaskA,
        effect0.EffectSpellClassMaskB,
        effect1.EffectSpellClassMaskB,
        effect2.EffectSpellClassMaskB,
        effect0.EffectSpellClassMaskC,
        effect1.EffectSpellClassMaskC,
        effect2.EffectSpellClassMaskC,
        effect0.EffectBonusMultiplier,
        effect1.EffectBonusMultiplier,
        effect2.EffectBonusMultiplier,
    ];
}

function syncAllSpellDBCRows() {
    const spellEffectHashIndex = buildSpellEffectHashIndex();
    DBC.Spell.queryAll({}).forEach(spell => {
        let sqlRow = SQL.spell_dbc.query({ Id: spell.ID.get() });
        let isNewRow = false;
        if (!sqlRow) {
            sqlRow = SQL.spell_dbc.add(spell.ID.get());
            isNewRow = true;
        }
        const effect0 = spellEffectProjection(spell, 0);
        const effect1 = spellEffectProjection(spell, 1);
        const effect2 = spellEffectProjection(spell, 2);
        const spellDataHash = stableSpellDataHash(spellCoreHashParts(spell, effect0, effect1, effect2).concat(spellEffectHashIndex.get(spell.ID.get()) || []));

        if (!isNewRow && sqlRow.SpellDataHash.get() === spellDataHash) {
            return;
        }

        sqlRow.Category.set(spell.Category.get())
        .Dispel.set(spell.DispelType.get())
        .Mechanic.set(spell.Mechanic.get())
        .Attributes.set(spell.Attributes.get())
        .AttributesEx.set(spell.AttributesEx.get())
        .AttributesEx2.set(spell.AttributesExB.get())
        .AttributesEx3.set(spell.AttributesExC.get())
        .AttributesEx4.set(spell.AttributesExD.get())
        .AttributesEx5.set(spell.AttributesExE.get())
        .AttributesEx6.set(spell.AttributesExF.get())
        .AttributesEx7.set(spell.AttributesExG.get())
        .Stances.set(Number(spell.ShapeshiftMask.get()))
        .StancesNot.set(Number(spell.ShapeshiftExclude.get()))
        .Targets.set(spell.Targets.get())
        .TargetCreatureType.set(spell.TargetCreatureType.get())
        .RequiresSpellFocus.set(spell.RequiresSpellFocus.get())
        .FacingCasterFlags.set(spell.FacingCasterFlags.get())
        .CasterAuraState.set(spell.CasterAuraState.get())
        .TargetAuraState.set(spell.TargetAuraState.get())
        .ExcludeCasterAuraState.set(spell.ExcludeCasterAuraState.get())
        .ExcludeTargetAuraState.set(spell.ExcludeTargetAuraState.get())
        .CasterAuraSpell.set(spell.CasterAuraSpell.get())
        .TargetAuraSpell.set(spell.TargetAuraSpell.get())
        .ExcludeCasterAuraSpell.set(spell.ExcludeCasterAuraSpell.get())
        .ExcludeTargetAuraSpell.set(spell.ExcludeTargetAuraSpell.get())
        .CastingTimeIndex.set(spell.CastingTimeIndex.get())
        .RecoveryTime.set(spell.RecoveryTime.get())
        .CategoryRecoveryTime.set(spell.CategoryRecoveryTime.get())
        .InterruptFlags.set(spell.InterruptFlags.get())
        .AuraInterruptFlags.set(spell.AuraInterruptFlags.get())
        .ChannelInterruptFlags.set(spell.ChannelInterruptFlags.get())
        .ProcFlags.set(spell.ProcTypeMask.get())
        .ProcChance.set(spell.ProcChance.get())
        .ProcCharges.set(spell.ProcCharges.get())
        .MaxLevel.set(spell.MaxLevel.get())
        .BaseLevel.set(spell.BaseLevel.get())
        .SpellLevel.set(spell.SpellLevel.get())
        .DurationIndex.set(spell.DurationIndex.get())
        .PowerType.set(spell.PowerType.get())
        .ManaCost.set(spell.ManaCost.get())
        .ManaCostPerLevel.set(spell.ManaCostPerLevel.get())
        .ManaPerSecond.set(spell.ManaPerSecond.get())
        .ManaPerSecondPerLevel.set(spell.ManaPerSecondPerLevel.get())
        .RangeIndex.set(spell.RangeIndex.get())
        .Speed.set(spell.Speed.get())
        .ModalNextSpell.set(spell.ModalNextSpell.get())
        .StackAmount.set(spell.CumulativeAura.get())
        .Totem1.set(spell.Totem.getIndex(0))
        .Totem2.set(spell.Totem.getIndex(1))
        .Reagent1.set(spell.Reagent.getIndex(0))
        .Reagent2.set(spell.Reagent.getIndex(1))
        .Reagent3.set(spell.Reagent.getIndex(2))
        .Reagent4.set(spell.Reagent.getIndex(3))
        .Reagent5.set(spell.Reagent.getIndex(4))
        .Reagent6.set(spell.Reagent.getIndex(5))
        .Reagent7.set(spell.Reagent.getIndex(6))
        .Reagent8.set(spell.Reagent.getIndex(7))
        .ReagentCount1.set(spell.ReagentCount.getIndex(0))
        .ReagentCount2.set(spell.ReagentCount.getIndex(1))
        .ReagentCount3.set(spell.ReagentCount.getIndex(2))
        .ReagentCount4.set(spell.ReagentCount.getIndex(3))
        .ReagentCount5.set(spell.ReagentCount.getIndex(4))
        .ReagentCount6.set(spell.ReagentCount.getIndex(5))
        .ReagentCount7.set(spell.ReagentCount.getIndex(6))
        .ReagentCount8.set(spell.ReagentCount.getIndex(7))
        .EquippedItemClass.set(spell.EquippedItemClass.get())
        .EquippedItemSubClassMask.set(spell.EquippedItemSubclass.get())
        .EquippedItemInventoryTypeMask.set(spell.EquippedItemInvTypes.get())
        .Effect1.set(effect0.Effect)
        .Effect2.set(effect1.Effect)
        .Effect3.set(effect2.Effect)
        .EffectDieSides1.set(effect0.EffectDieSides)
        .EffectDieSides2.set(effect1.EffectDieSides)
        .EffectDieSides3.set(effect2.EffectDieSides)
        .EffectRealPointsPerLevel1.set(effect0.EffectRealPointsPerLevel)
        .EffectRealPointsPerLevel2.set(effect1.EffectRealPointsPerLevel)
        .EffectRealPointsPerLevel3.set(effect2.EffectRealPointsPerLevel)
        .EffectBasePoints1.set(effect0.EffectBasePoints)
        .EffectBasePoints2.set(effect1.EffectBasePoints)
        .EffectBasePoints3.set(effect2.EffectBasePoints)
        .EffectMechanic1.set(effect0.EffectMechanic)
        .EffectMechanic2.set(effect1.EffectMechanic)
        .EffectMechanic3.set(effect2.EffectMechanic)
        .EffectImplicitTargetA1.set(effect0.EffectImplicitTargetA)
        .EffectImplicitTargetA2.set(effect1.EffectImplicitTargetA)
        .EffectImplicitTargetA3.set(effect2.EffectImplicitTargetA)
        .EffectImplicitTargetB1.set(effect0.EffectImplicitTargetB)
        .EffectImplicitTargetB2.set(effect1.EffectImplicitTargetB)
        .EffectImplicitTargetB3.set(effect2.EffectImplicitTargetB)
        .EffectRadiusIndex1.set(effect0.EffectRadiusIndex)
        .EffectRadiusIndex2.set(effect1.EffectRadiusIndex)
        .EffectRadiusIndex3.set(effect2.EffectRadiusIndex)
        .EffectApplyAuraName1.set(effect0.EffectApplyAuraName)
        .EffectApplyAuraName2.set(effect1.EffectApplyAuraName)
        .EffectApplyAuraName3.set(effect2.EffectApplyAuraName)
        .EffectAmplitude1.set(effect0.EffectAmplitude)
        .EffectAmplitude2.set(effect1.EffectAmplitude)
        .EffectAmplitude3.set(effect2.EffectAmplitude)
        .EffectMultipleValue1.set(effect0.EffectMultipleValue)
        .EffectMultipleValue2.set(effect1.EffectMultipleValue)
        .EffectMultipleValue3.set(effect2.EffectMultipleValue)
        .EffectItemType1.set(effect0.EffectItemType)
        .EffectItemType2.set(effect1.EffectItemType)
        .EffectItemType3.set(effect2.EffectItemType)
        .EffectMiscValue1.set(effect0.EffectMiscValue)
        .EffectMiscValue2.set(effect1.EffectMiscValue)
        .EffectMiscValue3.set(effect2.EffectMiscValue)
        .EffectMiscValueB1.set(effect0.EffectMiscValueB)
        .EffectMiscValueB2.set(effect1.EffectMiscValueB)
        .EffectMiscValueB3.set(effect2.EffectMiscValueB)
        .EffectTriggerSpell1.set(effect0.EffectTriggerSpell)
        .EffectTriggerSpell2.set(effect1.EffectTriggerSpell)
        .EffectTriggerSpell3.set(effect2.EffectTriggerSpell)
        .EffectSpellClassMaskA1.set(effect0.EffectSpellClassMaskA)
        .EffectSpellClassMaskA2.set(effect1.EffectSpellClassMaskA)
        .EffectSpellClassMaskA3.set(effect2.EffectSpellClassMaskA)
        .EffectSpellClassMaskB1.set(effect0.EffectSpellClassMaskB)
        .EffectSpellClassMaskB2.set(effect1.EffectSpellClassMaskB)
        .EffectSpellClassMaskB3.set(effect2.EffectSpellClassMaskB)
        .EffectSpellClassMaskC1.set(effect0.EffectSpellClassMaskC)
        .EffectSpellClassMaskC2.set(effect1.EffectSpellClassMaskC)
        .EffectSpellClassMaskC3.set(effect2.EffectSpellClassMaskC)
        .DmgMultiplier1.set(effect0.EffectBonusMultiplier)
        .DmgMultiplier2.set(effect1.EffectBonusMultiplier)
        .DmgMultiplier3.set(effect2.EffectBonusMultiplier)
        .SpellVisualID1.set(spell.SpellVisualID.getIndex(0))
        .SpellVisualID2.set(spell.SpellVisualID.getIndex(1))
        .SpellIconID.set(spell.SpellIconID.get())
        .ActiveIconID.set(spell.ActiveIconID.get())
        .SpellPriority.set(spell.SpellPriority.get())
        .SpellName.set(spell.Name.lang('enGB').get())
        .Rank.set(spell.NameSubtext.lang('enGB').get())
        .Description.set(spell.Description.lang('enGB').get())
        .ToolTip.set(spell.AuraDescription.lang('enGB').get())
        .ManaCostPct.set(spell.ManaCostPct.get())
        .StartRecoveryCategory.set(spell.StartRecoveryCategory.get())
        .StartRecoveryTime.set(spell.StartRecoveryTime.get())
        .MaxTargetLevel.set(spell.MaxTargetLevel.get())
        .SpellFamilyName.set(spell.SpellClassSet.get())
        .SpellFamilyFlags1.set(spell.SpellClassMask.getIndex(0))
        .SpellFamilyFlags2.set(spell.SpellClassMask.getIndex(1))
        .SpellFamilyFlags3.set(spell.SpellClassMask.getIndex(2))
        .MaxAffectedTargets.set(spell.MaxTargets.get())
        .DmgClass.set(spell.DefenseType.get())
        .PreventionType.set(spell.PreventionType.get())
        .StanceBarOrder.set(spell.StanceBarOrder.get())
        .MinFactionID.set(spell.MinFactionID.get())
        .MinReputation.set(spell.MinReputation.get())
        .RequiredAuraVision.set(spell.RequiredAuraVision.get())
        .RequiredTotemCategoryID1.set(spell.RequiredTotemCategoryID.getIndex(0))
        .RequiredTotemCategoryID2.set(spell.RequiredTotemCategoryID.getIndex(1))
        .AreaGroupId.set(spell.RequiredAreasID.get())
        .SchoolMask.set(spell.SchoolMask.get())
        .RuneCostID.set(spell.RuneCostID.get())
        .SpellMissileID.set(spell.SpellMissileID.get())
        .PowerDisplayID.set(spell.PowerDisplayID.get())
        .DescriptionVariablesID.set(spell.SpellDescriptionVariableID.get())
        .Difficulty.set(spell.SpellDifficultyID.get())
        .SpellDataHash.set(spellDataHash)
    });
}

function syncLoc(sqlRow: any, prefix: string, loc: any) {
    sqlRow[`${prefix}_Lang_enUS`].set(loc.enGB.get())
    sqlRow[`${prefix}_Lang_enGB`].set(loc.enGB.get())
    sqlRow[`${prefix}_Lang_koKR`].set(loc.koKR.get())
    sqlRow[`${prefix}_Lang_frFR`].set(loc.frFR.get())
    sqlRow[`${prefix}_Lang_deDE`].set(loc.deDE.get())
    sqlRow[`${prefix}_Lang_enCN`].set(loc.enCN.get())
    sqlRow[`${prefix}_Lang_zhCN`].set(loc.zhCN.get())
    sqlRow[`${prefix}_Lang_enTW`].set(loc.enTW.get())
    sqlRow[`${prefix}_Lang_zhTW`].set(loc.zhTW.get())
    sqlRow[`${prefix}_Lang_esES`].set(loc.esES.get())
    sqlRow[`${prefix}_Lang_esMX`].set(loc.esMX.get())
    sqlRow[`${prefix}_Lang_ruRU`].set(loc.ruRU.get())
    sqlRow[`${prefix}_Lang_ptPT`].set(loc.ptPT.get())
    sqlRow[`${prefix}_Lang_ptBR`].set(loc.ptBR.get())
    sqlRow[`${prefix}_Lang_itIT`].set(loc.itIT.get())
    sqlRow[`${prefix}_Lang_Unk`].set(loc.Unk.get())
    sqlRow[`${prefix}_Lang_Mask`].set(loc.mask.get())
}

function syncAllMapDBCRows() {
    DBC.Map.queryAll({}).forEach(map => {
        let sqlRow = SQL.map_dbc.query({ ID: map.ID.get() });
        if (!sqlRow) {
            sqlRow = SQL.map_dbc.add(map.ID.get());
        }

        sqlRow.Directory.set(map.Directory.get())
        .InstanceType.set(map.InstanceType.get())
        .Flags.set(map.Flags.get())
        .PVP.set(map.PVP.get())
        .AreaTableID.set(map.AreaTableID.get())
        .LoadingScreenID.set(map.LoadingScreenID.get())
        .MinimapIconScale.set(map.MinimapIconScale.get())
        .CorpseMapID.set(map.CorpseMapID.get())
        .CorpseX.set(map.CorpseX.get())
        .CorpseY.set(map.CorpseY.get())
        .TimeOfDayOverride.set(map.TimeOfDayOverride.get())
        .ExpansionID.set(map.ExpansionID.get())
        .RaidOffset.set(map.RaidOffset.get())
        .MaxPlayers.set(map.MaxPlayers.get())

        syncLoc(sqlRow, 'MapName', map.MapName)
        syncLoc(sqlRow, 'MapDescription0', map.MapDescription0)
        syncLoc(sqlRow, 'MapDescription1', map.MapDescription1)
    });
}

function syncCustomSpellCastTimeDBCRows() {
    const rows = DBC.SpellCastTimes.queryAll({});
    const baseRowCount = DBCFile.getBuffer(DBC.SpellCastTimes).baseRowCount;
    rows
        .filter(row => row.index >= baseRowCount && !row.isDeleted())
        .forEach(row => {
            let sqlRow = SQL.dbc_spellcasttimes.query({ ID: row.ID.get() });
            if (!sqlRow) {
                sqlRow = SQL.dbc_spellcasttimes.add(row.ID.get());
            }

            sqlRow.Base.set(row.Base.get())
                .PerLevel.set(row.PerLevel.get())
                .Minimum.set(row.Minimum.get());
        });
}

function syncCustomSpellDurationDBCRows() {
    const rows = DBC.SpellDuration.queryAll({});
    const baseRowCount = DBCFile.getBuffer(DBC.SpellDuration).baseRowCount;
    rows
        .filter(row => row.index >= baseRowCount && !row.isDeleted())
        .forEach(row => {
            let sqlRow = SQL.dbc_spellduration.query({ ID: row.ID.get() });
            if (!sqlRow) {
                sqlRow = SQL.dbc_spellduration.add(row.ID.get());
            }

            sqlRow.Duration.set(row.Duration.get())
                .DurationPerLevel.set(row.DurationPerLevel.get())
                .MaxDuration.set(row.MaxDuration.get());
        });
}

function saveDbc() {
    for (const file of DBCFiles) {
        saveDBCFile(file, '.dbc')
    }
    for (const file of CDBCFiles) {
        file.ensureSourceFile();
        file.fileWork()
        saveDBCFile(file, '.cdbc')
    }
}

function saveDBCFile(file, ending)
{
    const srcpath = dataset.dbc_source.join(file.name + ending);

        // if we skip the server, we should write dbcs to client directly
        const outPaths: WFile[] = [];
        if(BuildArgs.WRITE_CLIENT) {
            outPaths.push(BuildArgs.CLIENT_PATCH_DIR.join('DBFilesClient',file.name+ending).toFile())
        }

        if(BuildArgs.WRITE_SERVER) {
            outPaths.push(dataset.dbc.join(file.name+ending).toFile())
        }

        if(file.isLoaded()) {
            outPaths[0].writeBuffer(DBCFile.getBuffer(file).write());
        } else {
            srcpath.copy(outPaths[0]);
        }

        if(outPaths.length > 1) {
            outPaths.slice(1).forEach(x=>{
                outPaths[0].copy(outPaths[1])
            })
        }
}

async function saveSQL() {
    projectLegacySpellEffects();
    SQL.spell_effects.deleteUntouchedDynamicRows();
    SQL.spell_effects.validateRows();
    syncAllSpellDBCRows();
    syncAllMapDBCRows();
    syncCustomSpellCastTimeDBCRows();
    syncCustomSpellDurationDBCRows();
    SQLTables.map(x=>{
        SqlTable.writeSQL(x);
    })
    await Promise.all(SqlConnection.allDbs().map(x=>x.apply()));
}

export async function __internal_wotlk_save() {
    if(!BuildArgs.READ_ONLY) {
        saveDbc();
    }

    if(BuildArgs.WRITE_SERVER) {
        await saveSQL();
    }
}

export function __internal_wotlk_applyDeletes() {
    for(const file of DBCFiles) {
        DBCFile.getBuffer(file).applyDeletes();
    }
}
