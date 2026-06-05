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
import { Table } from "../../../data/table/Table";
import { SpellQuery, SpellRow } from "../../dbc/Spell";
import { DBC } from "../../DBCFiles";
import { SQL } from "../../SQLFiles";
import { Ids, StaticIDGenerator } from "../Misc/Ids";
import { RegistryStatic } from "../Refs/Registry";
import { Spell } from "./Spell";

function syncSpellDBCRow(spell: Spell, parent: number) {
    let sqlRow = SQL.spell_dbc.query({ Id: spell.ID });
    if (!sqlRow) {
        const parentRow = parent > 0 ? SQL.spell_dbc.query({ Id: parent }) : undefined;
        sqlRow = parentRow ? parentRow.clone(spell.ID) : SQL.spell_dbc.add(spell.ID);
    }

    const row = spell.row;
    sqlRow.Category.set(row.Category.get())
        .Dispel.set(row.DispelType.get())
        .Mechanic.set(row.Mechanic.get())
        .Attributes.set(row.Attributes.get())
        .AttributesEx.set(row.AttributesEx.get())
        .AttributesEx2.set(row.AttributesExB.get())
        .AttributesEx3.set(row.AttributesExC.get())
        .AttributesEx4.set(row.AttributesExD.get())
        .AttributesEx5.set(row.AttributesExE.get())
        .AttributesEx6.set(row.AttributesExF.get())
        .AttributesEx7.set(row.AttributesExG.get())
        .Stances.set(Number(row.ShapeshiftMask.get()))
        .StancesNot.set(Number(row.ShapeshiftExclude.get()))
        .Targets.set(row.Targets.get())
        .TargetCreatureType.set(row.TargetCreatureType.get())
        .RequiresSpellFocus.set(row.RequiresSpellFocus.get())
        .FacingCasterFlags.set(row.FacingCasterFlags.get())
        .CasterAuraState.set(row.CasterAuraState.get())
        .TargetAuraState.set(row.TargetAuraState.get())
        .ExcludeCasterAuraState.set(row.ExcludeCasterAuraState.get())
        .ExcludeTargetAuraState.set(row.ExcludeTargetAuraState.get())
        .CasterAuraSpell.set(row.CasterAuraSpell.get())
        .TargetAuraSpell.set(row.TargetAuraSpell.get())
        .ExcludeCasterAuraSpell.set(row.ExcludeCasterAuraSpell.get())
        .ExcludeTargetAuraSpell.set(row.ExcludeTargetAuraSpell.get())
        .CastingTimeIndex.set(row.CastingTimeIndex.get())
        .RecoveryTime.set(row.RecoveryTime.get())
        .CategoryRecoveryTime.set(row.CategoryRecoveryTime.get())
        .InterruptFlags.set(row.InterruptFlags.get())
        .AuraInterruptFlags.set(row.AuraInterruptFlags.get())
        .ChannelInterruptFlags.set(row.ChannelInterruptFlags.get())
        .ProcFlags.set(row.ProcTypeMask.get())
        .ProcChance.set(row.ProcChance.get())
        .ProcCharges.set(row.ProcCharges.get())
        .MaxLevel.set(row.MaxLevel.get())
        .BaseLevel.set(row.BaseLevel.get())
        .SpellLevel.set(row.SpellLevel.get())
        .DurationIndex.set(row.DurationIndex.get())
        .PowerType.set(row.PowerType.get())
        .ManaCost.set(row.ManaCost.get())
        .ManaCostPerLevel.set(row.ManaCostPerLevel.get())
        .ManaPerSecond.set(row.ManaPerSecond.get())
        .ManaPerSecondPerLevel.set(row.ManaPerSecondPerLevel.get())
        .RangeIndex.set(row.RangeIndex.get())
        .Speed.set(row.Speed.get())
        .ModalNextSpell.set(row.ModalNextSpell.get())
        .StackAmount.set(row.CumulativeAura.get())
        .Totem1.set(row.Totem.getIndex(0))
        .Totem2.set(row.Totem.getIndex(1))
        .Reagent1.set(row.Reagent.getIndex(0))
        .Reagent2.set(row.Reagent.getIndex(1))
        .Reagent3.set(row.Reagent.getIndex(2))
        .Reagent4.set(row.Reagent.getIndex(3))
        .Reagent5.set(row.Reagent.getIndex(4))
        .Reagent6.set(row.Reagent.getIndex(5))
        .Reagent7.set(row.Reagent.getIndex(6))
        .Reagent8.set(row.Reagent.getIndex(7))
        .ReagentCount1.set(row.ReagentCount.getIndex(0))
        .ReagentCount2.set(row.ReagentCount.getIndex(1))
        .ReagentCount3.set(row.ReagentCount.getIndex(2))
        .ReagentCount4.set(row.ReagentCount.getIndex(3))
        .ReagentCount5.set(row.ReagentCount.getIndex(4))
        .ReagentCount6.set(row.ReagentCount.getIndex(5))
        .ReagentCount7.set(row.ReagentCount.getIndex(6))
        .ReagentCount8.set(row.ReagentCount.getIndex(7))
        .EquippedItemClass.set(row.EquippedItemClass.get())
        .EquippedItemSubClassMask.set(row.EquippedItemSubclass.get())
        .EquippedItemInventoryTypeMask.set(row.EquippedItemInvTypes.get())
        .SpellVisualID1.set(row.SpellVisualID.getIndex(0))
        .SpellVisualID2.set(row.SpellVisualID.getIndex(1))
        .SpellIconID.set(row.SpellIconID.get())
        .ActiveIconID.set(row.ActiveIconID.get())
        .SpellPriority.set(row.SpellPriority.get())
        .SpellName.set(spell.Name.enGB.get())
        .Rank.set(spell.Subtext.enGB.get())
        .Description.set(spell.Description.enGB.get())
        .ToolTip.set(spell.AuraDescription.enGB.get())
        .ManaCostPct.set(row.ManaCostPct.get())
        .StartRecoveryCategory.set(row.StartRecoveryCategory.get())
        .StartRecoveryTime.set(row.StartRecoveryTime.get())
        .MaxTargetLevel.set(row.MaxTargetLevel.get())
        .SpellFamilyName.set(row.SpellClassSet.get())
        .SpellFamilyFlags1.set(row.SpellClassMask.getIndex(0))
        .SpellFamilyFlags2.set(row.SpellClassMask.getIndex(1))
        .SpellFamilyFlags3.set(row.SpellClassMask.getIndex(2))
        .MaxAffectedTargets.set(row.MaxTargets.get())
        .DmgClass.set(row.DefenseType.get())
        .PreventionType.set(row.PreventionType.get())
        .StanceBarOrder.set(row.StanceBarOrder.get())
        .MinFactionID.set(row.MinFactionID.get())
        .MinReputation.set(row.MinReputation.get())
        .RequiredAuraVision.set(row.RequiredAuraVision.get())
        .RequiredTotemCategoryID1.set(row.RequiredTotemCategoryID.getIndex(0))
        .RequiredTotemCategoryID2.set(row.RequiredTotemCategoryID.getIndex(1))
        .AreaGroupId.set(row.RequiredAreasID.get())
        .SchoolMask.set(row.SchoolMask.get())
        .RuneCostID.set(row.RuneCostID.get())
        .SpellMissileID.set(row.SpellMissileID.get())
        .PowerDisplayID.set(row.PowerDisplayID.get())
        .DescriptionVariablesID.set(row.SpellDescriptionVariableID.get())
        .Difficulty.set(row.SpellDifficultyID.get())
}

export class SpellRegistryClass extends RegistryStatic<Spell,SpellRow,SpellQuery> {
    create(mod: string, id: string, parent = 0, cloneServerData = true) {
        let v = super.create(mod,id,parent);

        // it's on by default, so be nice
        if(parent > this.nullID() && cloneServerData) {
            let parentEntity = this.load(parent);
            v.Effects.forEach((E, I) => {
                let Effect = parentEntity.Effects.get(I)
                if (Effect) {
                    if (Effect.BonusData.exists())
                        Effect.BonusData.getSQL().clone(v.ID, I, {})
                }
            })

            if(parentEntity.Proc.exists())
            {
                parentEntity.Proc.getSQL().clone(v.ID);
            }

            if(parentEntity.Threat.exists()) {
                parentEntity.Threat.getSQL().clone(v.ID);
            }

            SQL.spell_target_position.queryAll({ID: parent}).forEach(x=>x.clone(v.ID,x.EffectIndex.get()))
            for (let i = 0; i < 3; ++i) {
                parentEntity.Effects.get(i).dynamicRow();
            }
            SQL.spell_effects.queryAll({ SpellID: parent })
                .forEach(x => SQL.spell_effects.upsert(v.ID, x.EffectIndex.get(), {
                    Effect: x.Effect.get(),
                    EffectDieSides: x.EffectDieSides.get(),
                    EffectRealPointsPerLevel: x.EffectRealPointsPerLevel.get(),
                    EffectBasePoints: x.EffectBasePoints.get(),
                    EffectMechanic: x.EffectMechanic.get(),
                    EffectImplicitTargetA: x.EffectImplicitTargetA.get(),
                    EffectImplicitTargetB: x.EffectImplicitTargetB.get(),
                    EffectRadiusIndex: x.EffectRadiusIndex.get(),
                    EffectApplyAuraName: x.EffectApplyAuraName.get(),
                    EffectAmplitude: x.EffectAmplitude.get(),
                    EffectMultipleValue: x.EffectMultipleValue.get(),
                    EffectChainTargets: x.EffectChainTargets.get(),
                    EffectItemType: x.EffectItemType.get(),
                    EffectMiscValue: x.EffectMiscValue.get(),
                    EffectMiscValueB: x.EffectMiscValueB.get(),
                    EffectTriggerSpell: x.EffectTriggerSpell.get(),
                    EffectPointsPerCombo: x.EffectPointsPerCombo.get(),
                    EffectSpellClassMaskA: x.EffectSpellClassMaskA.get(),
                    EffectSpellClassMaskB: x.EffectSpellClassMaskB.get(),
                    EffectSpellClassMaskC: x.EffectSpellClassMaskC.get(),
                    EffectChainAmplitude: x.EffectChainAmplitude.get(),
                    EffectBonusMultiplier: x.EffectBonusMultiplier.get(),
                }));

            if(parentEntity.CustomAttributes.exists()) {
                parentEntity.CustomAttributes.sqlRow().clone(v.ID)
            }
            // note: we're never cloning spell_script_names, spell_scripts or spell_target_position
        }
        syncSpellDBCRow(v, parent)
        v.Tags.add(mod, id)
        return v;
    }

    protected Table(): Table<any, SpellQuery, SpellRow> & { add: (id: number) => SpellRow; } {
        return DBC.Spell;
    }
    protected IDs(): StaticIDGenerator {
        return Ids.Spell
    }
    protected Entity(r: SpellRow): Spell {
        return new Spell(r);
    }
    protected FindByID(id: number): SpellRow {
        return DBC.Spell.findById(id);
    }
    protected EmptyQuery(): SpellQuery {
        return {}
    }
    ID(e: Spell): number {
        return e.ID;
    }
    Clear(r: Spell) {
        r.row
            .ActiveIconID.set(0)
            .Attributes.set(0)
            .AttributesEx.set(0)
            .AttributesExB.set(0)
            .AttributesExC.set(0)
            .AttributesExD.set(0)
            .AttributesExE.set(0)
            .AttributesExF.set(0)
            .AttributesExG.set(0)
            .AuraDescription.clear()
            .AuraInterruptFlags.set(0)
            .BaseLevel.set(0)
            .CasterAuraSpell.set(0)
            .CasterAuraState.set(0)
            .CastingTimeIndex.set(0)
            .Category.set(0)
            .CategoryRecoveryTime.set(0)
            .ChannelInterruptFlags.set(0)
            .CumulativeAura.set(0)
            .DefenseType.set(0)
            .Description.clear()
            .DispelType.set(0)
            .DurationIndex.set(0)
            .Effect.set([0,0,0])
            .EffectAura.set([0,0,0])
            .EffectAuraPeriod.set([0,0,0])
            .EffectBasePoints.set([0,0,0])
            .EffectChainAmplitude.set([1,1,1])
            .EffectChainTargets.set([0,0,0])
            .EffectDieSides.set([0,0,0])
            .EffectItemType.set([0,0,0])
            .EffectMechanic.set([0,0,0])
            .EffectMiscValue.set([0,0,0])
            .EffectMiscValueB.set([0,0,0])
            .EffectMultipleValue.set([0,0,0])
            .EffectPointsPerCombo.set([0,0,0])
            .EffectRadiusIndex.set([0,0,0])
            .EffectRealPointsPerLevel.set([0,0,0])
            .EffectSpellClassMaskA.set([0,0,0])
            .EffectSpellClassMaskB.set([0,0,0])
            .EffectSpellClassMaskC.set([0,0,0])
            .EffectTriggerSpell.set([0,0,0])
            .EquippedItemClass.set(-1)
            .EquippedItemInvTypes.set(0)
            .EquippedItemSubclass.set(0)
            .ExcludeCasterAuraSpell.set(0)
            .ExcludeCasterAuraState.set(0)
            .FacingCasterFlags.set(0)
            .EffectBonusMultiplier.set([0,0,0])
            .ImplicitTargetA.set([0,0,0])
            .ImplicitTargetB.set([0,0,0])
            .InterruptFlags.set(0)
            .ManaCost.set(0)
            .ManaCostPct.set(0)
            .ManaCostPerLevel.set(0)
            .ManaPerSecond.set(0)
            .ManaPerSecondPerLevel.set(0)
            .MaxLevel.set(0)
            .MaxTargetLevel.set(0)
            .MaxTargets.set(0)
            .Mechanic.set(0)
            .MinFactionID.set(0)
            .MinReputation.set(0)
            .ModalNextSpell.set(0)
            .Name.clear()
            .NameSubtext.clear()
            .PowerDisplayID.set(0)
            .PowerType.set(0)
            .PreventionType.set(0)
            .ProcChance.set(101)
            .ProcCharges.set(0)
            .ProcTypeMask.set(0)
            .RangeIndex.set(0)
            .Reagent.set([0,0,0,0,0,0,0,0])
            .ReagentCount.set([0,0,0,0,0,0,0,0])
            .RecoveryTime.set(0)
            .RequiredAreasID.set(0)
            .RequiredAuraVision.set(0)
            .RequiredTotemCategoryID.set([0,0])
            .RequiresSpellFocus.set(0)
            .RuneCostID.set(0)
            .SchoolMask.set(1)
            .ShapeshiftExclude.set(BigInt(0))
            .ShapeshiftMask.set(BigInt(0))
            .Speed.set(0)
            .SpellClassMask.set([0,0,0])
            .SpellClassSet.set(0)
            .SpellDescriptionVariableID.set(0)
            .SpellDifficultyID.set(0)
            .SpellIconID.set(1)
            .SpellLevel.set(0)
            .SpellMissileID.set(0)
            .SpellPriority.set(0)
            .SpellVisualID.set([0,0])
            .StanceBarOrder.set(0)
            .StartRecoveryCategory.set(0)
            .StartRecoveryTime.set(0)
            .TargetAuraSpell.set(0)
            .TargetAuraState.set(0)
            .TargetCreatureType.set(0)
            .Targets.set(0)
            .Totem.set([0,0])
        r.Effects.clearDynamic();
    }
}
export const SpellRegistry = new SpellRegistryClass();
