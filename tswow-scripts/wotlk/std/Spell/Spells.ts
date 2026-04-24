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

    sqlRow.Dispel.set(spell.row.DispelType.get())
    sqlRow.Mechanic.set(spell.row.Mechanic.get())
    sqlRow.Attributes.set(spell.row.Attributes.get())
    sqlRow.AttributesEx.set(spell.row.AttributesEx.get())
    sqlRow.AttributesEx2.set(spell.row.AttributesExB.get())
    sqlRow.AttributesEx3.set(spell.row.AttributesExC.get())
    sqlRow.AttributesEx4.set(spell.row.AttributesExD.get())
    sqlRow.AttributesEx5.set(spell.row.AttributesExE.get())
    sqlRow.AttributesEx6.set(spell.row.AttributesExF.get())
    sqlRow.AttributesEx7.set(spell.row.AttributesExG.get())
    sqlRow.Stances.set(Number(spell.row.ShapeshiftMask.get()))
    sqlRow.StancesNot.set(Number(spell.row.ShapeshiftExclude.get()))
    sqlRow.Targets.set(spell.row.Targets.get())
    sqlRow.CastingTimeIndex.set(spell.row.CastingTimeIndex.get())
    sqlRow.AuraInterruptFlags.set(spell.row.AuraInterruptFlags.get())
    sqlRow.ProcFlags.set(spell.row.ProcTypeMask.get())
    sqlRow.ProcChance.set(spell.row.ProcChance.get())
    sqlRow.ProcCharges.set(spell.row.ProcCharges.get())
    sqlRow.MaxLevel.set(spell.row.MaxLevel.get())
    sqlRow.BaseLevel.set(spell.row.BaseLevel.get())
    sqlRow.SpellLevel.set(spell.row.SpellLevel.get())
    sqlRow.DurationIndex.set(spell.row.DurationIndex.get())
    sqlRow.RangeIndex.set(spell.row.RangeIndex.get())
    sqlRow.StackAmount.set(spell.row.CumulativeAura.get())
    sqlRow.EquippedItemClass.set(spell.row.EquippedItemClass.get())
    sqlRow.EquippedItemSubClassMask.set(spell.row.EquippedItemSubclass.get())
    sqlRow.EquippedItemInventoryTypeMask.set(spell.row.EquippedItemInvTypes.get())
    sqlRow.Effect1.set(spell.row.Effect.getIndex(0))
    sqlRow.Effect2.set(spell.row.Effect.getIndex(1))
    sqlRow.Effect3.set(spell.row.Effect.getIndex(2))
    sqlRow.EffectDieSides1.set(spell.row.EffectDieSides.getIndex(0))
    sqlRow.EffectDieSides2.set(spell.row.EffectDieSides.getIndex(1))
    sqlRow.EffectDieSides3.set(spell.row.EffectDieSides.getIndex(2))
    sqlRow.EffectRealPointsPerLevel1.set(spell.row.EffectRealPointsPerLevel.getIndex(0))
    sqlRow.EffectRealPointsPerLevel2.set(spell.row.EffectRealPointsPerLevel.getIndex(1))
    sqlRow.EffectRealPointsPerLevel3.set(spell.row.EffectRealPointsPerLevel.getIndex(2))
    sqlRow.EffectBasePoints1.set(spell.row.EffectBasePoints.getIndex(0))
    sqlRow.EffectBasePoints2.set(spell.row.EffectBasePoints.getIndex(1))
    sqlRow.EffectBasePoints3.set(spell.row.EffectBasePoints.getIndex(2))
    sqlRow.EffectMechanic1.set(spell.row.EffectMechanic.getIndex(0))
    sqlRow.EffectMechanic2.set(spell.row.EffectMechanic.getIndex(1))
    sqlRow.EffectMechanic3.set(spell.row.EffectMechanic.getIndex(2))
    sqlRow.EffectImplicitTargetA1.set(spell.row.ImplicitTargetA.getIndex(0))
    sqlRow.EffectImplicitTargetA2.set(spell.row.ImplicitTargetA.getIndex(1))
    sqlRow.EffectImplicitTargetA3.set(spell.row.ImplicitTargetA.getIndex(2))
    sqlRow.EffectImplicitTargetB1.set(spell.row.ImplicitTargetB.getIndex(0))
    sqlRow.EffectImplicitTargetB2.set(spell.row.ImplicitTargetB.getIndex(1))
    sqlRow.EffectImplicitTargetB3.set(spell.row.ImplicitTargetB.getIndex(2))
    sqlRow.EffectRadiusIndex1.set(spell.row.EffectRadiusIndex.getIndex(0))
    sqlRow.EffectRadiusIndex2.set(spell.row.EffectRadiusIndex.getIndex(1))
    sqlRow.EffectRadiusIndex3.set(spell.row.EffectRadiusIndex.getIndex(2))
    sqlRow.EffectApplyAuraName1.set(spell.row.EffectAura.getIndex(0))
    sqlRow.EffectApplyAuraName2.set(spell.row.EffectAura.getIndex(1))
    sqlRow.EffectApplyAuraName3.set(spell.row.EffectAura.getIndex(2))
    sqlRow.EffectAmplitude1.set(spell.row.EffectAuraPeriod.getIndex(0))
    sqlRow.EffectAmplitude2.set(spell.row.EffectAuraPeriod.getIndex(1))
    sqlRow.EffectAmplitude3.set(spell.row.EffectAuraPeriod.getIndex(2))
    sqlRow.EffectMultipleValue1.set(spell.row.EffectMultipleValue.getIndex(0))
    sqlRow.EffectMultipleValue2.set(spell.row.EffectMultipleValue.getIndex(1))
    sqlRow.EffectMultipleValue3.set(spell.row.EffectMultipleValue.getIndex(2))
    sqlRow.EffectItemType1.set(spell.row.EffectItemType.getIndex(0))
    sqlRow.EffectItemType2.set(spell.row.EffectItemType.getIndex(1))
    sqlRow.EffectItemType3.set(spell.row.EffectItemType.getIndex(2))
    sqlRow.EffectMiscValue1.set(spell.row.EffectMiscValue.getIndex(0))
    sqlRow.EffectMiscValue2.set(spell.row.EffectMiscValue.getIndex(1))
    sqlRow.EffectMiscValue3.set(spell.row.EffectMiscValue.getIndex(2))
    sqlRow.EffectMiscValueB1.set(spell.row.EffectMiscValueB.getIndex(0))
    sqlRow.EffectMiscValueB2.set(spell.row.EffectMiscValueB.getIndex(1))
    sqlRow.EffectMiscValueB3.set(spell.row.EffectMiscValueB.getIndex(2))
    sqlRow.EffectTriggerSpell1.set(spell.row.EffectTriggerSpell.getIndex(0))
    sqlRow.EffectTriggerSpell2.set(spell.row.EffectTriggerSpell.getIndex(1))
    sqlRow.EffectTriggerSpell3.set(spell.row.EffectTriggerSpell.getIndex(2))
    sqlRow.EffectSpellClassMaskA1.set(spell.row.EffectSpellClassMaskA.getIndex(0))
    sqlRow.EffectSpellClassMaskA2.set(spell.row.EffectSpellClassMaskA.getIndex(1))
    sqlRow.EffectSpellClassMaskA3.set(spell.row.EffectSpellClassMaskA.getIndex(2))
    sqlRow.EffectSpellClassMaskB1.set(spell.row.EffectSpellClassMaskB.getIndex(0))
    sqlRow.EffectSpellClassMaskB2.set(spell.row.EffectSpellClassMaskB.getIndex(1))
    sqlRow.EffectSpellClassMaskB3.set(spell.row.EffectSpellClassMaskB.getIndex(2))
    sqlRow.EffectSpellClassMaskC1.set(spell.row.EffectSpellClassMaskC.getIndex(0))
    sqlRow.EffectSpellClassMaskC2.set(spell.row.EffectSpellClassMaskC.getIndex(1))
    sqlRow.EffectSpellClassMaskC3.set(spell.row.EffectSpellClassMaskC.getIndex(2))
    sqlRow.SpellName.set(spell.Name.enGB.get())
    sqlRow.MaxTargetLevel.set(spell.row.MaxTargetLevel.get())
    sqlRow.SpellFamilyName.set(spell.row.SpellClassSet.get())
    sqlRow.SpellFamilyFlags1.set(spell.row.SpellClassMask.getIndex(0))
    sqlRow.SpellFamilyFlags2.set(spell.row.SpellClassMask.getIndex(1))
    sqlRow.SpellFamilyFlags3.set(spell.row.SpellClassMask.getIndex(2))
    sqlRow.MaxAffectedTargets.set(spell.row.MaxTargets.get())
    sqlRow.DmgClass.set(spell.row.DefenseType.get())
    sqlRow.PreventionType.set(spell.row.PreventionType.get())
    sqlRow.DmgMultiplier1.set(spell.row.EffectBonusMultiplier.getIndex(0))
    sqlRow.DmgMultiplier2.set(spell.row.EffectBonusMultiplier.getIndex(1))
    sqlRow.DmgMultiplier3.set(spell.row.EffectBonusMultiplier.getIndex(2))
    sqlRow.AreaGroupId.set(spell.row.RequiredAreasID.get())
    sqlRow.SchoolMask.set(spell.row.SchoolMask.get())
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
    }
}
export const SpellRegistry = new SpellRegistryClass();
