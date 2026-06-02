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
import { Cell, CPrim } from "../../../data/cell/cells/Cell";
import { CellArray } from "../../../data/cell/cells/CellArray";
import { EnumCellTransform, EnumValueTransform, makeEnumCell } from "../../../data/cell/cells/EnumCell";
import { Objectified, ObjectifyOptions, Objects } from "../../../data/cell/serialization/ObjectIteration";
import { Transient } from "../../../data/cell/serialization/Transient";
import { ArrayEntry, ArraySystem } from "../../../data/cell/systems/ArraySystem";
import { SQL } from "../../SQLFiles";
import { spell_effectsRow } from "../../sql/spell_effects";
import { ShiftedNumberCell } from "../Misc/ShiftedNumberCell";
import { AuraType } from "./AuraType";
import { Spell } from "./Spell";
import { EffectClassSet } from "./SpellClassSet";
import { SpellEffectMechanic } from "./SpellEffectMechanics";
import { SpellEffectType } from "./SpellEffectType";
import { SpellImplicitTarget } from "./SpellImplicitTarget";
import { SpellRadiusRegistry } from "./SpellRadius";
import { SpellRegistry } from "./Spells";
import { SpellTargetPosition } from "./SpellTargetPosition";
import { SpellBonusData } from "./SpellBonusData";

export class SpellEffects extends ArraySystem<SpellEffect,Spell> {
    get length() {
        const effectRows = SQL.spell_effects.queryAll({ SpellID: this.owner.ID })
            .filter(row => !row.isDeleted());
        if (effectRows.length === 0) {
            return 3;
        }

        return Math.max(3, Math.max(...effectRows.map(row => row.EffectIndex.get())) + 1);
    }

    objectifyPlain() {
        return [this.get(0),this.get(1),this.get(2)]
            .filter(x=>!x.isClear())
            .map(x=>x.objectifyPlain())
    }

    swap(index1: number, index2: number) {
        let e1 = this.get(index1);
        let e2 = this.get(index2);
        let r1 = e1.Radius.get()
        let i1 = e1.ItemType.get();
        let a1 = e1.Aura.get();
        let t1 = e1.Type.get();
        let m1 = e1.Mechanic.get();
        let bp1 = e1.PointsBase.get();
        let ds1 = e1.PointsDieSides.get();
        let ppl1 = e1.PointsPerLevel.get();
        let ppc1 = e1.PointsPerCombo.get();
        let ita1 = e1.ImplicitTargetA.get();
        let itb1 = e1.ImplicitTargetB.get();
        let ap1 = e1.AuraPeriod.get();
        let mva1 = e1.MiscValueA.get();
        let mvb1 = e1.MiscValueB.get();
        let ts1 = e1.TriggerSpell.get();
        let ca1 = e1.ChainAmplitude.get();
        let cma1 = e1.ClassMask.A.get();
        let cmb1 = e1.ClassMask.B.get();
        let cmc1 = e1.ClassMask.C.get();
        e1.copyFrom(e2);
        e2.Radius.set(r1);
        e2.ItemType.set(i1);
        e2.Aura.set(a1);
        e2.Type.set(t1);
        e2.Mechanic.set(m1);
        e2.PointsBase.set(bp1);
        e2.PointsDieSides.set(ds1);
        e2.PointsPerLevel.set(ppl1);
        e2.PointsPerCombo.set(ppc1);
        e2.ImplicitTargetA.set(ita1);
        e2.ImplicitTargetB.set(itb1);
        e2.AuraPeriod.set(ap1);
        e2.MiscValueA.set(mva1);
        e2.MiscValueB.set(mvb1);
        e2.TriggerSpell.set(ts1);
        e2.ChainAmplitude.set(ca1)
        e2.ClassMask.A.set(cma1);
        e2.ClassMask.B.set(cmb1);
        e2.ClassMask.C.set(cmc1);
        return this.owner;
    }

    get(index: number) {
        return new SpellEffect(this.owner, index);
    }

    mod(index: number, callback: (eff: SpellEffect)=>void) {
        callback(this.get(index));
        return this.owner;
    }

    addMod(callback: (eff: SpellEffect)=>void) {
        callback(this.addGet());
        return this.owner;
    }

    addLearnSpells(spells: number[]) {
        for(const spell of spells) {
            this.addFreeEffect((eff)=>{
                eff.Type.LEARN_SPELL.set()
                   .LearntSpell.set(spell)
            })
        }
        return this.owner;
    }

    addGet(): SpellEffect {
        for (let i = 0; i < 3; ++i) {
            const cur = this.get(i);
            if (cur.isClear()) {
                cur.clear();
                return cur;
            }
        }

        return this.get(this.length);
    }

    addGetTriggerSpell(mod: string, id: string, parent = 0) {
        let spell = SpellRegistry.create(mod,id,parent);
        this.addGet().Type
            .TRIGGER_SPELL.set()
            .TriggerSpell.set(spell.ID)
        return spell;
    }

    findType<T extends Objectified>(
        callback:
            (eff: SpellEffectType)=>EnumValueTransform<SpellEffect,T>)
            : T
    {
        for(let i=0;i<this.length;++i) {
            let eff = this.get(i);
            let v = callback(eff.Type);
            if(v.is()) return v.as();
        }
        return undefined as any as T;
    }

    findAura<T extends Objectified>(
        callback:
            (eff: AuraType)=>EnumValueTransform<SpellEffect,T>)
            : T
    {
        for(let i=0;i<this.length;++i) {
            let eff = this.get(i);
            let v = callback(eff.Aura);
            if(v.is()) return v.as();
        }
        return undefined as any as T;
    }

    /**
     * @param parent set to 0 for no parent
     */
    addModTriggerSpell(mod: string, id: string, parent: number, callback: (spell: Spell)=>void) {
        callback(this.addGetTriggerSpell(mod,id,parent));
        return this.owner;
    }

    /**
     * Adds an effect via this spell that may override
     * and become a secondary triggered spell
     * @note if this is a combat spell, the dynamic id will break
     *       combat logs over time because the id can vary.
     *
     *       To set up static spell triggers that
     *       work with combat logs, use "add*TriggerSpell"
     *       instead.
     */
    addFreeEffect(callback: (effect: SpellEffect)=>void) {
        callback(this.addGet());
        return this.owner;
    }

    clearDynamic() {
        SQL.spell_effects.queryAll({ SpellID: this.owner.ID })
            .forEach(row => row.delete());
        return this.owner;
    }
}

export class SpellEffect extends ArrayEntry<Spell> {
    isClear(): boolean {
        return this.Type.get() === 0;
    }

    clear(): this {
        this.PointsBase.set(0);
        this.ChainAmplitude.set(1);
        this.ChainTarget.set(0);
        this.PointsDieSides.set(1);
        this.ImplicitTargetA.set(0);
        this.ImplicitTargetB.set(0);
        this.Mechanic.set(0);
        this.MiscValueA.set(0);
        this.MiscValueB.set(0);
        this.MultipleValue.set(0);
        this.PointsPerCombo.set(0);
        this.PointsPerLevel.set(0);
        this.TriggerSpell.set(0);
        this.AuraPeriod.set(0);
        this.Aura.set(0);
        this.Type.set(0);
        this.Mechanic.set(0);
        return this;
    }

    dynamicRow(): spell_effectsRow {
        let row = SQL.spell_effects.findBySpellEffect(this.row.ID.get(), this.index);
        if (!row) {
            row = SQL.spell_effects.add(this.row.ID.get(), this.index, this.legacyEffectValues());
        }
        return row;
    }

    private legacyEffectValues() {
        if (this.index >= 3) {
            return { EffectChainAmplitude: 1 };
        }

        return {
            Effect: this.row.Effect.getIndex(this.index),
            EffectDieSides: this.row.EffectDieSides.getIndex(this.index),
            EffectRealPointsPerLevel: this.row.EffectRealPointsPerLevel.getIndex(this.index),
            EffectBasePoints: this.row.EffectBasePoints.getIndex(this.index),
            EffectMechanic: this.row.EffectMechanic.getIndex(this.index),
            EffectImplicitTargetA: this.row.ImplicitTargetA.getIndex(this.index),
            EffectImplicitTargetB: this.row.ImplicitTargetB.getIndex(this.index),
            EffectRadiusIndex: this.row.EffectRadiusIndex.getIndex(this.index),
            EffectApplyAuraName: this.row.EffectAura.getIndex(this.index),
            EffectAmplitude: this.row.EffectAuraPeriod.getIndex(this.index),
            EffectMultipleValue: this.row.EffectMultipleValue.getIndex(this.index),
            EffectChainTargets: this.row.EffectChainTargets.getIndex(this.index),
            EffectItemType: this.row.EffectItemType.getIndex(this.index),
            EffectMiscValue: this.row.EffectMiscValue.getIndex(this.index),
            EffectMiscValueB: this.row.EffectMiscValueB.getIndex(this.index),
            EffectTriggerSpell: this.row.EffectTriggerSpell.getIndex(this.index),
            EffectPointsPerCombo: this.row.EffectPointsPerCombo.getIndex(this.index),
            EffectSpellClassMaskA: this.row.EffectSpellClassMaskA.getIndex(this.index),
            EffectSpellClassMaskB: this.row.EffectSpellClassMaskB.getIndex(this.index),
            EffectSpellClassMaskC: this.row.EffectSpellClassMaskC.getIndex(this.index),
            EffectChainAmplitude: this.row.EffectChainAmplitude.getIndex(this.index),
            EffectBonusMultiplier: this.row.EffectBonusMultiplier.getIndex(this.index),
        };
    }

    effectCell<T extends CPrim>(_legacy: CellArray<T,any>, dynamic: (row: spell_effectsRow) => Cell<T, any>) {
        return this.wrap(dynamic(this.dynamicRow()));
    }

    @Transient
    get row() { return this.container.row; }

    get Radius() { return SpellRadiusRegistry.ref(this, this.effectCell(this.row.EffectRadiusIndex, row => row.EffectRadiusIndex)); }
    get ItemType() { return this.effectCell(this.row.EffectItemType, row => row.EffectItemType); }
    get Aura(): AuraType { return new AuraType(this, this.index); }
    get Type(): SpellEffectType { return new SpellEffectType(this, this.index); }
    get MechanicRaw() { return this.effectCell(this.row.EffectMechanic, row => row.EffectMechanic); }
    get ImplicitTargetARaw() { return this.effectCell(this.row.ImplicitTargetA, row => row.EffectImplicitTargetA); }
    get ImplicitTargetBRaw() { return this.effectCell(this.row.ImplicitTargetB, row => row.EffectImplicitTargetB); }

    get Mechanic() {
        return makeEnumCell(SpellEffectMechanic, this, this.MechanicRaw);
    }

    get PointsBase() {
        return new ShiftedNumberCell(
            this
          , ()=>this.PointsDieSides.get() > 0
                ? 'STORED_AS_MINUS_ONE'
                : 'NO_CHANGE'
          , this.effectCell(this.row.EffectBasePoints, row => row.EffectBasePoints)
        )
    };
    get PointsDieSides() { return this.effectCell(this.row.EffectDieSides, row => row.EffectDieSides); }
    get PointsPerLevel() { return this.effectCell(this.row.EffectRealPointsPerLevel, row => row.EffectRealPointsPerLevel); }
    get PointsPerCombo() { return this.effectCell(this.row.EffectPointsPerCombo, row => row.EffectPointsPerCombo); }
    get ImplicitTargetA() {
        return makeEnumCell(SpellImplicitTarget, this, this.ImplicitTargetARaw);
    }
    get ImplicitTargetB() {
        return makeEnumCell(SpellImplicitTarget, this, this.ImplicitTargetBRaw);
    }
    get AuraPeriod() { return this.effectCell(this.row.EffectAuraPeriod, row => row.EffectAmplitude); }
    get MultipleValue() { return this.effectCell(this.row.EffectMultipleValue, row => row.EffectMultipleValue); }
    get ChainTarget() { return this.effectCell(this.row.EffectChainTargets, row => row.EffectChainTargets); }
    get MiscValueA() { return this.effectCell(this.row.EffectMiscValue, row => row.EffectMiscValue); }
    get MiscValueB() { return this.effectCell(this.row.EffectMiscValueB, row => row.EffectMiscValueB); }
    get TriggerSpell() { return this.effectCell(this.row.EffectTriggerSpell, row => row.EffectTriggerSpell); }
    get ChainAmplitude() { return this.effectCell(this.row.EffectChainAmplitude, row => row.EffectChainAmplitude); }
    get BonusMultiplier() { return this.effectCell(this.row.EffectBonusMultiplier, row => row.EffectBonusMultiplier); }
    get ClassMask(): EffectClassSet<this> { return new EffectClassSet(this, this); }
    get TargetPosition() {
        return new SpellTargetPosition(this, this.row.ID.get(), this.index);
    }
    /** Custom server core damage bonuses */
    readonly BonusData = new SpellBonusData(this);

    objectifyPlain(){
        return Objects.objectifyObj(this);
    }

    objectify(options?: ObjectifyOptions) {
        if(this.Aura.get() > 0) {
            return {
                Type: this.Type.objectify(options)
              , Aura: this.Aura.objectify(options)
              , ...Objects.objectifyObj(
                      EnumCellTransform.getSelection(this.Aura).cell.as()
                    , options
                )
          }
        }
        if(this.Type.get() > 0) {
            return {
                  Type: this.Type.objectify()
                , Aura: this.Aura.objectify()
                , ...Objects.objectifyObj(
                    EnumCellTransform.getSelection(this.Type).cell.as())
            }
        } else {
            return this.objectifyPlain();
        }
    }

    setPoints(base: number, dieSides: number, pointsPerLevel: number, pointsPerCombo: number) {
        this.PointsBase.set(base);
        this.PointsDieSides.set(dieSides);
        this.PointsPerLevel.set(pointsPerLevel);
        this.PointsPerCombo.set(pointsPerCombo);
        return this.owner;
    }

    copyFrom(source: SpellEffect) {
        this.Radius.set(source.Radius.get());
        this.ItemType.set(source.ItemType.get());
        this.Aura.set(source.Aura.get());
        this.Type.set(source.Type.get());
        this.Mechanic.set(source.Mechanic.get());
        this.PointsBase.set(source.PointsBase.get());
        this.PointsDieSides.set(source.PointsDieSides.get());
        this.PointsPerLevel.set(source.PointsPerLevel.get());
        this.PointsPerCombo.set(source.PointsPerCombo.get());
        this.ImplicitTargetA.set(source.ImplicitTargetA.get());
        this.ImplicitTargetB.set(source.ImplicitTargetB.get());
        this.AuraPeriod.set(source.AuraPeriod.get());
        this.MultipleValue.set(source.MultipleValue.get());
        this.ChainTarget.set(source.ChainTarget.get());
        this.MiscValueA.set(source.MiscValueA.get());
        this.MiscValueB.set(source.MiscValueB.get());
        this.TriggerSpell.set(source.TriggerSpell.get());
        this.ChainAmplitude.set(source.ChainAmplitude.get());
        this.ClassMask.copyFrom(source.ClassMask);
        return this.owner;
    }
}
