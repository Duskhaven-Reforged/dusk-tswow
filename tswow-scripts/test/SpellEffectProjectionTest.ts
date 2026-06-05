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
import * as assert from 'assert';
import { DBC } from '../wotlk/DBCFiles';
import { SQL } from '../wotlk/SQLFiles';
import { SpellRow } from '../wotlk/dbc/Spell';
import { SpellRegistry } from '../wotlk/std/Spell/Spells';

const EFFECT_ARRAYS: [keyof SpellRow, string[]][] = [
    ['Effect', ['Effect1', 'Effect2', 'Effect3']],
    ['EffectDieSides', ['EffectDieSides1', 'EffectDieSides2', 'EffectDieSides3']],
    ['EffectRealPointsPerLevel', ['EffectRealPointsPerLevel1', 'EffectRealPointsPerLevel2', 'EffectRealPointsPerLevel3']],
    ['EffectBasePoints', ['EffectBasePoints1', 'EffectBasePoints2', 'EffectBasePoints3']],
    ['EffectMechanic', ['EffectMechanic1', 'EffectMechanic2', 'EffectMechanic3']],
    ['ImplicitTargetA', ['EffectImplicitTargetA1', 'EffectImplicitTargetA2', 'EffectImplicitTargetA3']],
    ['ImplicitTargetB', ['EffectImplicitTargetB1', 'EffectImplicitTargetB2', 'EffectImplicitTargetB3']],
    ['EffectRadiusIndex', ['EffectRadiusIndex1', 'EffectRadiusIndex2', 'EffectRadiusIndex3']],
    ['EffectAura', ['EffectApplyAuraName1', 'EffectApplyAuraName2', 'EffectApplyAuraName3']],
    ['EffectAuraPeriod', ['EffectAmplitude1', 'EffectAmplitude2', 'EffectAmplitude3']],
    ['EffectMultipleValue', ['EffectMultipleValue1', 'EffectMultipleValue2', 'EffectMultipleValue3']],
    ['EffectItemType', ['EffectItemType1', 'EffectItemType2', 'EffectItemType3']],
    ['EffectMiscValue', ['EffectMiscValue1', 'EffectMiscValue2', 'EffectMiscValue3']],
    ['EffectMiscValueB', ['EffectMiscValueB1', 'EffectMiscValueB2', 'EffectMiscValueB3']],
    ['EffectTriggerSpell', ['EffectTriggerSpell1', 'EffectTriggerSpell2', 'EffectTriggerSpell3']],
    ['EffectSpellClassMaskA', ['EffectSpellClassMaskA1', 'EffectSpellClassMaskA2', 'EffectSpellClassMaskA3']],
    ['EffectSpellClassMaskB', ['EffectSpellClassMaskB1', 'EffectSpellClassMaskB2', 'EffectSpellClassMaskB3']],
    ['EffectSpellClassMaskC', ['EffectSpellClassMaskC1', 'EffectSpellClassMaskC2', 'EffectSpellClassMaskC3']],
    ['EffectBonusMultiplier', ['DmgMultiplier1', 'DmgMultiplier2', 'DmgMultiplier3']],
];

function firstSpellWithLegacyEffects() {
    return DBC.Spell.queryAll({}).find(row =>
        EFFECT_ARRAYS.some(([dbcField]) => {
            const cell = row[dbcField] as any;
            return [0, 1, 2].some(index => cell.getIndex(index) !== 0);
        })
    );
}

describe('Spell effect sidecar projection', function() {
    it('projects legacy Spell.dbc effects into spell_effects sidecar rows', function() {
        const parent = firstSpellWithLegacyEffects();
        assert(parent, 'test fixture error: no Spell.dbc row with legacy effects found');

        const clone = SpellRegistry.create(
            'test',
            `spell-effect-sidecar-parity-${Date.now()}-${Math.random()}`,
            parent.ID.get()
        );
        const sqlRow = SQL.spell_dbc.query({ Id: clone.ID });
        assert(sqlRow, `spell_dbc row was not projected for spell ${clone.ID}`);
        assert.strictEqual((sqlRow as any).Effect1, undefined, 'spell_dbc must not expose legacy effect columns');

        EFFECT_ARRAYS.forEach(([dbcField, sqlFields]) => {
            const cell = parent[dbcField] as any;
            sqlFields.forEach((sqlField, effectIndex) => {
                const effectRow = SQL.spell_effects.findBySpellEffect(clone.ID, effectIndex);
                assert(effectRow, `spell_effects row ${effectIndex} was not projected for spell ${clone.ID}`);
                const sidecarField = sqlField
                    .replace(/^(DmgMultiplier)([1-3])$/, 'EffectBonusMultiplier')
                    .replace(/^(Effect)([1-3])$/, 'Effect')
                    .replace(/[1-3]$/, '');
                assert.strictEqual(
                    (effectRow as any)[sidecarField].get(),
                    cell.getIndex(effectIndex),
                    `${sidecarField} did not preserve ${String(dbcField)}[${effectIndex}]`
                );
            });
        });
    });

    it('writes the fourth effect only to spell_effects beyond legacy Spell.dbc width', function() {
        const spell = SpellRegistry.create(
            'test',
            `spell-effect-sidecar-fourth-effect-${Date.now()}-${Math.random()}`
        );

        [0, 1, 2].forEach(effectIndex => {
            spell.Effects.mod(effectIndex, effect => {
                effect.Type.APPLY_AURA.set()
                    .Aura.DUMMY.set()
                    .ImplicitTargetA.UNIT_CASTER.set()
                    .PointsBase.set(effectIndex + 10)
                    .PointsDieSides.set(1);
            });
        });

        spell.Effects.addMod(effect => {
            effect.Type.APPLY_AURA.set()
                .Aura.MOD_INCREASE_HEALTH.set()
                .ImplicitTargetA.UNIT_CASTER.set()
                .PointsBase.set(3999)
                .PointsDieSides.set(1);
        });

        const dynamicRows = SQL.spell_effects.queryAll({ SpellID: spell.ID })
            .filter(row => row.EffectIndex.get() >= 3);
        assert.strictEqual(dynamicRows.length, 1, 'expected exactly one sidecar row beyond legacy width');

        const fourth = dynamicRows[0];
        assert.strictEqual(fourth.EffectIndex.get(), 3);
        assert.strictEqual(fourth.Effect.get(), spell.Effects.get(3).Type.get());
        assert.strictEqual(fourth.EffectApplyAuraName.get(), spell.Effects.get(3).Aura.get());
        assert.strictEqual(fourth.EffectImplicitTargetA.get(), spell.Effects.get(3).ImplicitTargetA.get());
        assert.strictEqual(fourth.EffectBasePoints.get(), 3998);
        assert.strictEqual(fourth.EffectDieSides.get(), 1);

        const sqlRow = SQL.spell_dbc.query({ Id: spell.ID });
        assert(sqlRow, `spell_dbc row was not projected for spell ${spell.ID}`);
        assert.strictEqual((sqlRow as any).Effect4, undefined, 'legacy spell_dbc must not grow a fourth effect column');
    });
});
