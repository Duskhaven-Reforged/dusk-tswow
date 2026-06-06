/*
 * Copyright (C) 2024 tswow <https://github.com/tswow/>
 * and Duskhaven <https://github.com/orgs/Duskhaven-Reforged>
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 3.
 */

/* tslint:disable */
import { float, int } from '../../data/primitives'
import { Relation } from '../../data/query/Relations'
import { PrimaryKey } from '../../data/table/PrimaryKey'
import { DBCFloatCell, DBCIntCell, DBCKeyCell } from '../../data/dbc/DBCCell'
import { CDBCFile } from './CDBCFile'
import { DBCRow } from '../../data/dbc/DBCRow'
import { CDBCGenerator } from './CDBCGenerator'
import { DBC } from '../DBCFiles'
import { SQL } from '../SQLFiles'

type SpellEffectCacheValues = {
    Effect: int
    EffectDieSides: int
    EffectRealPointsPerLevel: float
    EffectBasePoints: int
    EffectMechanic: int
    EffectImplicitTargetA: int
    EffectImplicitTargetB: int
    EffectRadiusIndex: int
    EffectApplyAuraName: int
    EffectAmplitude: int
    EffectMultipleValue: float
    EffectChainTargets: int
    EffectItemType: int
    EffectMiscValue: int
    EffectMiscValueB: int
    EffectTriggerSpell: int
    EffectPointsPerCombo: float
    EffectSpellClassMaskA: int
    EffectSpellClassMaskB: int
    EffectSpellClassMaskC: int
    EffectChainAmplitude: float
    EffectBonusMultiplier: float
    EffectSpellPowerBonus: float
    EffectAttackPowerBonus: float
    EffectBlockValueBonus: float
    EffectScalingMode: int
}

const int32Fields: (keyof SpellEffectCacheValues)[] = [
    'Effect',
    'EffectDieSides',
    'EffectBasePoints',
    'EffectMechanic',
    'EffectImplicitTargetA',
    'EffectImplicitTargetB',
    'EffectRadiusIndex',
    'EffectApplyAuraName',
    'EffectAmplitude',
    'EffectChainTargets',
    'EffectItemType',
    'EffectMiscValue',
    'EffectMiscValueB',
    'EffectTriggerSpell',
    'EffectSpellClassMaskA',
    'EffectSpellClassMaskB',
    'EffectSpellClassMaskC',
    'EffectScalingMode',
]

export class SpellEffectCacheRow extends DBCRow<SpellEffectCacheCreator, SpellEffectCacheQuery> {
    @PrimaryKey()
    get SpellID() { return new DBCKeyCell(this, this.buffer, this.offset + 0) }
    @PrimaryKey()
    get EffectIndex() { return new DBCKeyCell(this, this.buffer, this.offset + 4) }
    get Effect() { return new DBCIntCell(this, this.buffer, this.offset + 8) }
    get EffectDieSides() { return new DBCIntCell(this, this.buffer, this.offset + 12) }
    get EffectRealPointsPerLevel() { return new DBCFloatCell(this, this.buffer, this.offset + 16) }
    get EffectBasePoints() { return new DBCIntCell(this, this.buffer, this.offset + 20) }
    get EffectMechanic() { return new DBCIntCell(this, this.buffer, this.offset + 24) }
    get EffectImplicitTargetA() { return new DBCIntCell(this, this.buffer, this.offset + 28) }
    get EffectImplicitTargetB() { return new DBCIntCell(this, this.buffer, this.offset + 32) }
    get EffectRadiusIndex() { return new DBCIntCell(this, this.buffer, this.offset + 36) }
    get EffectApplyAuraName() { return new DBCIntCell(this, this.buffer, this.offset + 40) }
    get EffectAmplitude() { return new DBCIntCell(this, this.buffer, this.offset + 44) }
    get EffectMultipleValue() { return new DBCFloatCell(this, this.buffer, this.offset + 48) }
    get EffectChainTargets() { return new DBCIntCell(this, this.buffer, this.offset + 52) }
    get EffectItemType() { return new DBCIntCell(this, this.buffer, this.offset + 56) }
    get EffectMiscValue() { return new DBCIntCell(this, this.buffer, this.offset + 60) }
    get EffectMiscValueB() { return new DBCIntCell(this, this.buffer, this.offset + 64) }
    get EffectTriggerSpell() { return new DBCIntCell(this, this.buffer, this.offset + 68) }
    get EffectPointsPerCombo() { return new DBCFloatCell(this, this.buffer, this.offset + 72) }
    get EffectSpellClassMaskA() { return new DBCIntCell(this, this.buffer, this.offset + 76) }
    get EffectSpellClassMaskB() { return new DBCIntCell(this, this.buffer, this.offset + 80) }
    get EffectSpellClassMaskC() { return new DBCIntCell(this, this.buffer, this.offset + 84) }
    get EffectChainAmplitude() { return new DBCFloatCell(this, this.buffer, this.offset + 88) }
    get EffectBonusMultiplier() { return new DBCFloatCell(this, this.buffer, this.offset + 92) }
    get EffectSpellPowerBonus() { return new DBCFloatCell(this, this.buffer, this.offset + 96) }
    get EffectAttackPowerBonus() { return new DBCFloatCell(this, this.buffer, this.offset + 100) }
    get EffectBlockValueBonus() { return new DBCFloatCell(this, this.buffer, this.offset + 104) }
    get EffectScalingMode() { return new DBCIntCell(this, this.buffer, this.offset + 108) }

    clone(SpellID: int, EffectIndex: int, c?: SpellEffectCacheCreator): this {
        return this.cloneInternal([SpellID, EffectIndex], c)
    }
}

export type SpellEffectCacheCreator = Partial<SpellEffectCacheValues>

export type SpellEffectCacheQuery = {
    SpellID?: Relation<int>
    EffectIndex?: Relation<int>
}

function legacyEffectValues(spell: any, effectIndex: number): SpellEffectCacheValues {
    const bonusData = SQL.spell_bonus_data.query({ entry: spell.ID.get(), effect: effectIndex });
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
        EffectSpellClassMaskA: toInt32(spell.EffectSpellClassMaskA.getIndex(effectIndex)),
        EffectSpellClassMaskB: toInt32(spell.EffectSpellClassMaskB.getIndex(effectIndex)),
        EffectSpellClassMaskC: toInt32(spell.EffectSpellClassMaskC.getIndex(effectIndex)),
        EffectChainAmplitude: spell.EffectChainAmplitude.getIndex(effectIndex),
        EffectBonusMultiplier: spell.EffectBonusMultiplier.getIndex(effectIndex),
        EffectSpellPowerBonus: bonusData ? bonusData.sp.get() : 0,
        EffectAttackPowerBonus: bonusData ? bonusData.ap.get() : 0,
        EffectBlockValueBonus: bonusData ? bonusData.bv.get() : 0,
        EffectScalingMode: bonusData ? bonusData.scaling_mode.get() : 0,
    }
}

function toInt32(value: number | bigint) {
    return Number(value) | 0
}

function sanitize(values: SpellEffectCacheValues): SpellEffectCacheValues {
    int32Fields.forEach(field => {
        (values as any)[field] = toInt32((values as any)[field])
    })
    return values
}

function isDefaultEffect(values: SpellEffectCacheValues) {
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
        && values.EffectBonusMultiplier === 0
        && values.EffectSpellPowerBonus === 0
        && values.EffectAttackPowerBonus === 0
        && values.EffectBlockValueBonus === 0
        && values.EffectScalingMode === 0
}

export class SpellEffectCacheCDBCFile extends CDBCFile<
    SpellEffectCacheCreator,
    SpellEffectCacheQuery,
    SpellEffectCacheRow> {
    protected defaultRow = [1, 0, 0, 0, 0.0, 0, 0, 0, 0, 0, 0, 0, 0.0, 0, 0, 0, 0, 0, 0.0, 0, 0, 0, 1.0, 0.0, 0.0, 0.0, 0.0, 0]

    constructor() {
        super('SpellEffect', (t, b, o) => new SpellEffectCacheRow(t, b, o))
    }

    static read(path: string): SpellEffectCacheCDBCFile {
        return new SpellEffectCacheCDBCFile().read(path)
    }

    fileWork() {
        new CDBCGenerator(this.defaultRow, 0).generate(this.getPath())
        this.read(this.getPath())
    }

    add(SpellID: int, EffectIndex: int, c?: SpellEffectCacheCreator): SpellEffectCacheRow {
        return this.makeRow(0).clone(SpellID, EffectIndex, c)
    }
}
