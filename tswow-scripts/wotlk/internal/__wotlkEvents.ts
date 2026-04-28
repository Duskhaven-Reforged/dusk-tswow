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

function syncAllSpellDBCRows() {
    DBC.Spell.queryAll({}).forEach(spell => {
        let sqlRow = SQL.spell_dbc.query({ Id: spell.ID.get() });
        if (!sqlRow) {
            sqlRow = SQL.spell_dbc.add(spell.ID.get());
        }

        sqlRow.Dispel.set(spell.DispelType.get())
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
        .CastingTimeIndex.set(spell.CastingTimeIndex.get())
        .AuraInterruptFlags.set(spell.AuraInterruptFlags.get())
        .ProcFlags.set(spell.ProcTypeMask.get())
        .ProcChance.set(spell.ProcChance.get())
        .ProcCharges.set(spell.ProcCharges.get())
        .MaxLevel.set(spell.MaxLevel.get())
        .BaseLevel.set(spell.BaseLevel.get())
        .SpellLevel.set(spell.SpellLevel.get())
        .DurationIndex.set(spell.DurationIndex.get())
        .RangeIndex.set(spell.RangeIndex.get())
        .StackAmount.set(spell.CumulativeAura.get())
        .EquippedItemClass.set(spell.EquippedItemClass.get())
        .EquippedItemSubClassMask.set(spell.EquippedItemSubclass.get())
        .EquippedItemInventoryTypeMask.set(spell.EquippedItemInvTypes.get())
        .Effect1.set(spell.Effect.getIndex(0))
        .Effect2.set(spell.Effect.getIndex(1))
        .Effect3.set(spell.Effect.getIndex(2))
        .EffectDieSides1.set(spell.EffectDieSides.getIndex(0))
        .EffectDieSides2.set(spell.EffectDieSides.getIndex(1))
        .EffectDieSides3.set(spell.EffectDieSides.getIndex(2))
        .EffectRealPointsPerLevel1.set(spell.EffectRealPointsPerLevel.getIndex(0))
        .EffectRealPointsPerLevel2.set(spell.EffectRealPointsPerLevel.getIndex(1))
        .EffectRealPointsPerLevel3.set(spell.EffectRealPointsPerLevel.getIndex(2))
        .EffectBasePoints1.set(spell.EffectBasePoints.getIndex(0))
        .EffectBasePoints2.set(spell.EffectBasePoints.getIndex(1))
        .EffectBasePoints3.set(spell.EffectBasePoints.getIndex(2))
        .EffectMechanic1.set(spell.EffectMechanic.getIndex(0))
        .EffectMechanic2.set(spell.EffectMechanic.getIndex(1))
        .EffectMechanic3.set(spell.EffectMechanic.getIndex(2))
        .EffectImplicitTargetA1.set(spell.ImplicitTargetA.getIndex(0))
        .EffectImplicitTargetA2.set(spell.ImplicitTargetA.getIndex(1))
        .EffectImplicitTargetA3.set(spell.ImplicitTargetA.getIndex(2))
        .EffectImplicitTargetB1.set(spell.ImplicitTargetB.getIndex(0))
        .EffectImplicitTargetB2.set(spell.ImplicitTargetB.getIndex(1))
        .EffectImplicitTargetB3.set(spell.ImplicitTargetB.getIndex(2))
        .EffectRadiusIndex1.set(spell.EffectRadiusIndex.getIndex(0))
        .EffectRadiusIndex2.set(spell.EffectRadiusIndex.getIndex(1))
        .EffectRadiusIndex3.set(spell.EffectRadiusIndex.getIndex(2))
        .EffectApplyAuraName1.set(spell.EffectAura.getIndex(0))
        .EffectApplyAuraName2.set(spell.EffectAura.getIndex(1))
        .EffectApplyAuraName3.set(spell.EffectAura.getIndex(2))
        .EffectAmplitude1.set(spell.EffectAuraPeriod.getIndex(0))
        .EffectAmplitude2.set(spell.EffectAuraPeriod.getIndex(1))
        .EffectAmplitude3.set(spell.EffectAuraPeriod.getIndex(2))
        .EffectMultipleValue1.set(spell.EffectMultipleValue.getIndex(0))
        .EffectMultipleValue2.set(spell.EffectMultipleValue.getIndex(1))
        .EffectMultipleValue3.set(spell.EffectMultipleValue.getIndex(2))
        .EffectItemType1.set(spell.EffectItemType.getIndex(0))
        .EffectItemType2.set(spell.EffectItemType.getIndex(1))
        .EffectItemType3.set(spell.EffectItemType.getIndex(2))
        .EffectMiscValue1.set(spell.EffectMiscValue.getIndex(0))
        .EffectMiscValue2.set(spell.EffectMiscValue.getIndex(1))
        .EffectMiscValue3.set(spell.EffectMiscValue.getIndex(2))
        .EffectMiscValueB1.set(spell.EffectMiscValueB.getIndex(0))
        .EffectMiscValueB2.set(spell.EffectMiscValueB.getIndex(1))
        .EffectMiscValueB3.set(spell.EffectMiscValueB.getIndex(2))
        .EffectTriggerSpell1.set(spell.EffectTriggerSpell.getIndex(0))
        .EffectTriggerSpell2.set(spell.EffectTriggerSpell.getIndex(1))
        .EffectTriggerSpell3.set(spell.EffectTriggerSpell.getIndex(2))
        .EffectSpellClassMaskA1.set(spell.EffectSpellClassMaskA.getIndex(0))
        .EffectSpellClassMaskA2.set(spell.EffectSpellClassMaskA.getIndex(1))
        .EffectSpellClassMaskA3.set(spell.EffectSpellClassMaskA.getIndex(2))
        .EffectSpellClassMaskB1.set(spell.EffectSpellClassMaskB.getIndex(0))
        .EffectSpellClassMaskB2.set(spell.EffectSpellClassMaskB.getIndex(1))
        .EffectSpellClassMaskB3.set(spell.EffectSpellClassMaskB.getIndex(2))
        .EffectSpellClassMaskC1.set(spell.EffectSpellClassMaskC.getIndex(0))
        .EffectSpellClassMaskC2.set(spell.EffectSpellClassMaskC.getIndex(1))
        .EffectSpellClassMaskC3.set(spell.EffectSpellClassMaskC.getIndex(2))
        .SpellName.set(spell.Name.lang('enGB').get())
        .MaxTargetLevel.set(spell.MaxTargetLevel.get())
        .SpellFamilyName.set(spell.SpellClassSet.get())
        .SpellFamilyFlags1.set(spell.SpellClassMask.getIndex(0))
        .SpellFamilyFlags2.set(spell.SpellClassMask.getIndex(1))
        .SpellFamilyFlags3.set(spell.SpellClassMask.getIndex(2))
        .MaxAffectedTargets.set(spell.MaxTargets.get())
        .DmgClass.set(spell.DefenseType.get())
        .PreventionType.set(spell.PreventionType.get())
        .DmgMultiplier1.set(spell.EffectBonusMultiplier.getIndex(0))
        .DmgMultiplier2.set(spell.EffectBonusMultiplier.getIndex(1))
        .DmgMultiplier3.set(spell.EffectBonusMultiplier.getIndex(2))
        .AreaGroupId.set(spell.RequiredAreasID.get())
        .SchoolMask.set(spell.SchoolMask.get())
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
    syncAllSpellDBCRows();
    syncAllMapDBCRows();
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
