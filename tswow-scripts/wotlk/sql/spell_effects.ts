/*
 * Copyright (C) 2024 tswow <https://github.com/tswow/>
 * and Duskhaven <https://github.com/orgs/Duskhaven-Reforged>
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 3.
 */

/* tslint:disable */
import { float, int } from '../../data/primitives'
import { PrimaryKey } from '../../data/table/PrimaryKey'
import { SQLCell, SQLCellReadOnly } from '../../data/sql/SQLCell'
import { SqlRow } from '../../data/sql/SQLRow'
import { SqlTable } from '../../data/sql/SQLTable'

export class spell_effectsRow extends SqlRow<spell_effectsCreator, spell_effectsQuery> {
    @PrimaryKey()
    get SpellID() { return new SQLCellReadOnly<int, this>(this, 'SpellID') }
    @PrimaryKey()
    get EffectIndex() { return new SQLCellReadOnly<int, this>(this, 'EffectIndex') }
    get Effect() { return new SQLCell<int, this>(this, 'Effect') }
    get EffectDieSides() { return new SQLCell<int, this>(this, 'EffectDieSides') }
    get EffectRealPointsPerLevel() { return new SQLCell<float, this>(this, 'EffectRealPointsPerLevel') }
    get EffectBasePoints() { return new SQLCell<int, this>(this, 'EffectBasePoints') }
    get EffectMechanic() { return new SQLCell<int, this>(this, 'EffectMechanic') }
    get EffectImplicitTargetA() { return new SQLCell<int, this>(this, 'EffectImplicitTargetA') }
    get EffectImplicitTargetB() { return new SQLCell<int, this>(this, 'EffectImplicitTargetB') }
    get EffectRadiusIndex() { return new SQLCell<int, this>(this, 'EffectRadiusIndex') }
    get EffectApplyAuraName() { return new SQLCell<int, this>(this, 'EffectApplyAuraName') }
    get EffectAmplitude() { return new SQLCell<int, this>(this, 'EffectAmplitude') }
    get EffectMultipleValue() { return new SQLCell<float, this>(this, 'EffectMultipleValue') }
    get EffectChainTargets() { return new SQLCell<int, this>(this, 'EffectChainTargets') }
    get EffectItemType() { return new SQLCell<int, this>(this, 'EffectItemType') }
    get EffectMiscValue() { return new SQLCell<int, this>(this, 'EffectMiscValue') }
    get EffectMiscValueB() { return new SQLCell<int, this>(this, 'EffectMiscValueB') }
    get EffectTriggerSpell() { return new SQLCell<int, this>(this, 'EffectTriggerSpell') }
    get EffectPointsPerCombo() { return new SQLCell<float, this>(this, 'EffectPointsPerCombo') }
    get EffectSpellClassMaskA() { return new SQLCell<int, this>(this, 'EffectSpellClassMaskA') }
    get EffectSpellClassMaskB() { return new SQLCell<int, this>(this, 'EffectSpellClassMaskB') }
    get EffectSpellClassMaskC() { return new SQLCell<int, this>(this, 'EffectSpellClassMaskC') }
    get EffectChainAmplitude() { return new SQLCell<float, this>(this, 'EffectChainAmplitude') }
    get EffectBonusMultiplier() { return new SQLCell<float, this>(this, 'EffectBonusMultiplier') }

    clone(SpellID: int, EffectIndex: int, c?: spell_effectsCreator): this {
        return this.cloneInternal([SpellID, EffectIndex], c)
    }
}

export type SpellEffectsCreator = {
    Effect?: int
    EffectDieSides?: int
    EffectRealPointsPerLevel?: float
    EffectBasePoints?: int
    EffectMechanic?: int
    EffectImplicitTargetA?: int
    EffectImplicitTargetB?: int
    EffectRadiusIndex?: int
    EffectApplyAuraName?: int
    EffectAmplitude?: int
    EffectMultipleValue?: float
    EffectChainTargets?: int
    EffectItemType?: int
    EffectMiscValue?: int
    EffectMiscValueB?: int
    EffectTriggerSpell?: int
    EffectPointsPerCombo?: float
    EffectSpellClassMaskA?: int
    EffectSpellClassMaskB?: int
    EffectSpellClassMaskC?: int
    EffectChainAmplitude?: float
    EffectBonusMultiplier?: float
}

export type spell_effectsCreator = SpellEffectsCreator & {
    SpellID?: int
    EffectIndex?: int
}

export type spell_effectsQuery = {
    SpellID?: int
    EffectIndex?: int
    Effect?: int
    EffectDieSides?: int
    EffectRealPointsPerLevel?: float
    EffectBasePoints?: int
    EffectMechanic?: int
    EffectImplicitTargetA?: int
    EffectImplicitTargetB?: int
    EffectRadiusIndex?: int
    EffectApplyAuraName?: int
    EffectAmplitude?: int
    EffectMultipleValue?: float
    EffectChainTargets?: int
    EffectItemType?: int
    EffectMiscValue?: int
    EffectMiscValueB?: int
    EffectTriggerSpell?: int
    EffectPointsPerCombo?: float
    EffectSpellClassMaskA?: int
    EffectSpellClassMaskB?: int
    EffectSpellClassMaskC?: int
    EffectChainAmplitude?: float
    EffectBonusMultiplier?: float
}

const SPELL_EFFECTS_DEFAULTS: SpellEffectsCreator = {
    Effect: 0,
    EffectDieSides: 0,
    EffectRealPointsPerLevel: 0,
    EffectBasePoints: 0,
    EffectMechanic: 0,
    EffectImplicitTargetA: 0,
    EffectImplicitTargetB: 0,
    EffectRadiusIndex: 0,
    EffectApplyAuraName: 0,
    EffectAmplitude: 0,
    EffectMultipleValue: 0,
    EffectChainTargets: 0,
    EffectItemType: 0,
    EffectMiscValue: 0,
    EffectMiscValueB: 0,
    EffectTriggerSpell: 0,
    EffectPointsPerCombo: 0,
    EffectSpellClassMaskA: 0,
    EffectSpellClassMaskB: 0,
    EffectSpellClassMaskC: 0,
    EffectChainAmplitude: 1,
    EffectBonusMultiplier: 0,
};

function withDefaults(c?: SpellEffectsCreator): spell_effectsCreator {
    return Object.assign({}, SPELL_EFFECTS_DEFAULTS, c);
}

function setIfChanged<T>(cell: { get(): T, set(value: T): any }, value: T | undefined) {
    if (value !== undefined && cell.get() !== value) {
        cell.set(value);
    }
}

export class spell_effectsTable extends SqlTable<
    spell_effectsCreator,
    spell_effectsQuery,
    spell_effectsRow> {
    private touchedRows = new Set<string>();

    private touch(SpellID: int, EffectIndex: int) {
        this.touchedRows.add(`${SpellID}:${EffectIndex}`);
    }

    findBySpellEffect(SpellID: int, EffectIndex: int) {
        this.touch(SpellID, EffectIndex);
        return this.query({ SpellID, EffectIndex });
    }

    add(SpellID: int, EffectIndex: int, c?: spell_effectsCreator): spell_effectsRow {
        this.touch(SpellID, EffectIndex);
        const first = this.first();
        const values = withDefaults(c);
        if (first) return first.clone(SpellID, EffectIndex, values);
        return this.rowCreator(this, {}).clone(SpellID, EffectIndex, values);
    }

    upsert(SpellID: int, EffectIndex: int, c: SpellEffectsCreator): spell_effectsRow {
        this.touch(SpellID, EffectIndex);
        const row = this.query({ SpellID, EffectIndex });
        if (row) {
            this.apply(row, c);
            return row;
        }
        return this.add(SpellID, EffectIndex, c);
    }

    deleteUntouchedDynamicRows() {
        this.queryAll({}).forEach(row => {
            const effectIndex = row.EffectIndex.get();
            const key = `${row.SpellID.get()}:${effectIndex}`;
            if (!this.touchedRows.has(key)) {
                row.delete();
            }
        });
    }

    validateRows() {
        const seen = new Set<string>();
        this.queryAll({})
            .filter(row => !row.isDeleted())
            .sort((a, b) => {
                const spellCompare = a.SpellID.get() - b.SpellID.get();
                return spellCompare !== 0
                    ? spellCompare
                    : a.EffectIndex.get() - b.EffectIndex.get();
            })
            .forEach(row => {
                const spellId = row.SpellID.get();
                const effectIndex = row.EffectIndex.get();
                const key = `${spellId}:${effectIndex}`;
                if (seen.has(key)) {
                    throw new Error(`Duplicate spell_effects row for SpellID=${spellId}, EffectIndex=${effectIndex}`);
                }
                seen.add(key);
                if (effectIndex < 0 || effectIndex >= 32) {
                    throw new Error(`spell_effects row for SpellID=${spellId} has unsupported EffectIndex=${effectIndex}`);
                }
            });
    }

    apply(row: spell_effectsRow, c: SpellEffectsCreator) {
        setIfChanged(row.Effect, c.Effect);
        setIfChanged(row.EffectDieSides, c.EffectDieSides);
        setIfChanged(row.EffectRealPointsPerLevel, c.EffectRealPointsPerLevel);
        setIfChanged(row.EffectBasePoints, c.EffectBasePoints);
        setIfChanged(row.EffectMechanic, c.EffectMechanic);
        setIfChanged(row.EffectImplicitTargetA, c.EffectImplicitTargetA);
        setIfChanged(row.EffectImplicitTargetB, c.EffectImplicitTargetB);
        setIfChanged(row.EffectRadiusIndex, c.EffectRadiusIndex);
        setIfChanged(row.EffectApplyAuraName, c.EffectApplyAuraName);
        setIfChanged(row.EffectAmplitude, c.EffectAmplitude);
        setIfChanged(row.EffectMultipleValue, c.EffectMultipleValue);
        setIfChanged(row.EffectChainTargets, c.EffectChainTargets);
        setIfChanged(row.EffectItemType, c.EffectItemType);
        setIfChanged(row.EffectMiscValue, c.EffectMiscValue);
        setIfChanged(row.EffectMiscValueB, c.EffectMiscValueB);
        setIfChanged(row.EffectTriggerSpell, c.EffectTriggerSpell);
        setIfChanged(row.EffectPointsPerCombo, c.EffectPointsPerCombo);
        setIfChanged(row.EffectSpellClassMaskA, c.EffectSpellClassMaskA);
        setIfChanged(row.EffectSpellClassMaskB, c.EffectSpellClassMaskB);
        setIfChanged(row.EffectSpellClassMaskC, c.EffectSpellClassMaskC);
        setIfChanged(row.EffectChainAmplitude, c.EffectChainAmplitude);
        setIfChanged(row.EffectBonusMultiplier, c.EffectBonusMultiplier);
    }
}

export const SQL_spell_effects = new spell_effectsTable(
    'spell_effects',
    (table, obj) => new spell_effectsRow(table, obj))
