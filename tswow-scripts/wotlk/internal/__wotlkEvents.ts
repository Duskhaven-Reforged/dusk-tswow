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
import * as fs from 'fs';
import { CDBCGenerator } from '../cdbc/CDBCGenerator';

function syncAllSpellDBCRows() {
    SQL.spell_dbc.queryAll({});
    DBC.Spell.queryAll({}).forEach(spell => {
        let sqlRow = SQL.spell_dbc.query({ Id: spell.ID.get() });
        if (!sqlRow) {
            sqlRow = SQL.spell_dbc.add(spell.ID.get());
        }

        sqlRow.Dispel.set(spell.DispelType.get())
        sqlRow.Mechanic.set(spell.Mechanic.get())
        sqlRow.Attributes.set(spell.Attributes.get())
        sqlRow.AttributesEx.set(spell.AttributesEx.get())
        sqlRow.AttributesEx2.set(spell.AttributesExB.get())
        sqlRow.AttributesEx3.set(spell.AttributesExC.get())
        sqlRow.AttributesEx4.set(spell.AttributesExD.get())
        sqlRow.AttributesEx5.set(spell.AttributesExE.get())
        sqlRow.AttributesEx6.set(spell.AttributesExF.get())
        sqlRow.AttributesEx7.set(spell.AttributesExG.get())
        sqlRow.Stances.set(Number(spell.ShapeshiftMask.get()))
        sqlRow.StancesNot.set(Number(spell.ShapeshiftExclude.get()))
        sqlRow.Targets.set(spell.Targets.get())
        sqlRow.CastingTimeIndex.set(spell.CastingTimeIndex.get())
        sqlRow.AuraInterruptFlags.set(spell.AuraInterruptFlags.get())
        sqlRow.ProcFlags.set(spell.ProcTypeMask.get())
        sqlRow.ProcChance.set(spell.ProcChance.get())
        sqlRow.ProcCharges.set(spell.ProcCharges.get())
        sqlRow.MaxLevel.set(spell.MaxLevel.get())
        sqlRow.BaseLevel.set(spell.BaseLevel.get())
        sqlRow.SpellLevel.set(spell.SpellLevel.get())
        sqlRow.DurationIndex.set(spell.DurationIndex.get())
        sqlRow.RangeIndex.set(spell.RangeIndex.get())
        sqlRow.StackAmount.set(spell.CumulativeAura.get())
        sqlRow.EquippedItemClass.set(spell.EquippedItemClass.get())
        sqlRow.EquippedItemSubClassMask.set(spell.EquippedItemSubclass.get())
        sqlRow.EquippedItemInventoryTypeMask.set(spell.EquippedItemInvTypes.get())
        sqlRow.Effect1.set(spell.Effect.getIndex(0))
        sqlRow.Effect2.set(spell.Effect.getIndex(1))
        sqlRow.Effect3.set(spell.Effect.getIndex(2))
        sqlRow.EffectDieSides1.set(spell.EffectDieSides.getIndex(0))
        sqlRow.EffectDieSides2.set(spell.EffectDieSides.getIndex(1))
        sqlRow.EffectDieSides3.set(spell.EffectDieSides.getIndex(2))
        sqlRow.EffectRealPointsPerLevel1.set(spell.EffectRealPointsPerLevel.getIndex(0))
        sqlRow.EffectRealPointsPerLevel2.set(spell.EffectRealPointsPerLevel.getIndex(1))
        sqlRow.EffectRealPointsPerLevel3.set(spell.EffectRealPointsPerLevel.getIndex(2))
        sqlRow.EffectBasePoints1.set(spell.EffectBasePoints.getIndex(0))
        sqlRow.EffectBasePoints2.set(spell.EffectBasePoints.getIndex(1))
        sqlRow.EffectBasePoints3.set(spell.EffectBasePoints.getIndex(2))
        sqlRow.EffectMechanic1.set(spell.EffectMechanic.getIndex(0))
        sqlRow.EffectMechanic2.set(spell.EffectMechanic.getIndex(1))
        sqlRow.EffectMechanic3.set(spell.EffectMechanic.getIndex(2))
        sqlRow.EffectImplicitTargetA1.set(spell.ImplicitTargetA.getIndex(0))
        sqlRow.EffectImplicitTargetA2.set(spell.ImplicitTargetA.getIndex(1))
        sqlRow.EffectImplicitTargetA3.set(spell.ImplicitTargetA.getIndex(2))
        sqlRow.EffectImplicitTargetB1.set(spell.ImplicitTargetB.getIndex(0))
        sqlRow.EffectImplicitTargetB2.set(spell.ImplicitTargetB.getIndex(1))
        sqlRow.EffectImplicitTargetB3.set(spell.ImplicitTargetB.getIndex(2))
        sqlRow.EffectRadiusIndex1.set(spell.EffectRadiusIndex.getIndex(0))
        sqlRow.EffectRadiusIndex2.set(spell.EffectRadiusIndex.getIndex(1))
        sqlRow.EffectRadiusIndex3.set(spell.EffectRadiusIndex.getIndex(2))
        sqlRow.EffectApplyAuraName1.set(spell.EffectAura.getIndex(0))
        sqlRow.EffectApplyAuraName2.set(spell.EffectAura.getIndex(1))
        sqlRow.EffectApplyAuraName3.set(spell.EffectAura.getIndex(2))
        sqlRow.EffectAmplitude1.set(spell.EffectAuraPeriod.getIndex(0))
        sqlRow.EffectAmplitude2.set(spell.EffectAuraPeriod.getIndex(1))
        sqlRow.EffectAmplitude3.set(spell.EffectAuraPeriod.getIndex(2))
        sqlRow.EffectMultipleValue1.set(spell.EffectMultipleValue.getIndex(0))
        sqlRow.EffectMultipleValue2.set(spell.EffectMultipleValue.getIndex(1))
        sqlRow.EffectMultipleValue3.set(spell.EffectMultipleValue.getIndex(2))
        sqlRow.EffectItemType1.set(spell.EffectItemType.getIndex(0))
        sqlRow.EffectItemType2.set(spell.EffectItemType.getIndex(1))
        sqlRow.EffectItemType3.set(spell.EffectItemType.getIndex(2))
        sqlRow.EffectMiscValue1.set(spell.EffectMiscValue.getIndex(0))
        sqlRow.EffectMiscValue2.set(spell.EffectMiscValue.getIndex(1))
        sqlRow.EffectMiscValue3.set(spell.EffectMiscValue.getIndex(2))
        sqlRow.EffectMiscValueB1.set(spell.EffectMiscValueB.getIndex(0))
        sqlRow.EffectMiscValueB2.set(spell.EffectMiscValueB.getIndex(1))
        sqlRow.EffectMiscValueB3.set(spell.EffectMiscValueB.getIndex(2))
        sqlRow.EffectTriggerSpell1.set(spell.EffectTriggerSpell.getIndex(0))
        sqlRow.EffectTriggerSpell2.set(spell.EffectTriggerSpell.getIndex(1))
        sqlRow.EffectTriggerSpell3.set(spell.EffectTriggerSpell.getIndex(2))
        sqlRow.EffectSpellClassMaskA1.set(spell.EffectSpellClassMaskA.getIndex(0))
        sqlRow.EffectSpellClassMaskA2.set(spell.EffectSpellClassMaskA.getIndex(1))
        sqlRow.EffectSpellClassMaskA3.set(spell.EffectSpellClassMaskA.getIndex(2))
        sqlRow.EffectSpellClassMaskB1.set(spell.EffectSpellClassMaskB.getIndex(0))
        sqlRow.EffectSpellClassMaskB2.set(spell.EffectSpellClassMaskB.getIndex(1))
        sqlRow.EffectSpellClassMaskB3.set(spell.EffectSpellClassMaskB.getIndex(2))
        sqlRow.EffectSpellClassMaskC1.set(spell.EffectSpellClassMaskC.getIndex(0))
        sqlRow.EffectSpellClassMaskC2.set(spell.EffectSpellClassMaskC.getIndex(1))
        sqlRow.EffectSpellClassMaskC3.set(spell.EffectSpellClassMaskC.getIndex(2))
        sqlRow.SpellName.set(spell.Name.lang('enGB').get())
        sqlRow.MaxTargetLevel.set(spell.MaxTargetLevel.get())
        sqlRow.SpellFamilyName.set(spell.SpellClassSet.get())
        sqlRow.SpellFamilyFlags1.set(spell.SpellClassMask.getIndex(0))
        sqlRow.SpellFamilyFlags2.set(spell.SpellClassMask.getIndex(1))
        sqlRow.SpellFamilyFlags3.set(spell.SpellClassMask.getIndex(2))
        sqlRow.MaxAffectedTargets.set(spell.MaxTargets.get())
        sqlRow.DmgClass.set(spell.DefenseType.get())
        sqlRow.PreventionType.set(spell.PreventionType.get())
        sqlRow.DmgMultiplier1.set(spell.EffectBonusMultiplier.getIndex(0))
        sqlRow.DmgMultiplier2.set(spell.EffectBonusMultiplier.getIndex(1))
        sqlRow.DmgMultiplier3.set(spell.EffectBonusMultiplier.getIndex(2))
        sqlRow.AreaGroupId.set(spell.RequiredAreasID.get())
        sqlRow.SchoolMask.set(spell.SchoolMask.get())
    });
}

function saveDbc() {
    for (const file of DBCFiles) {
        saveDBCFile(file, '.dbc')
    }
    for (const file of CDBCFiles) {
        if(!fs.existsSync(file.getPath()))
            new CDBCGenerator(file.getDefaultRow()).generate(file.getPath());
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
    syncAllSpellDBCRows();
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
