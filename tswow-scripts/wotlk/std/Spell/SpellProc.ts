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
import { Cell } from "../../../data/cell/cells/Cell";
import { makeEnumCell } from "../../../data/cell/cells/EnumCell";
import { makeMaskCell32, MaskCell32 } from "../../../data/cell/cells/MaskCell";
import { CellSystem } from "../../../data/cell/systems/CellSystem";
import { any } from "../../../data/query/Relations";
import { spell_procRow } from "../../sql/spell_proc";
import { SQL } from "../../SQLFiles";
import { PercentCell } from "../Misc/PercentCell";
import { SchoolMask } from "../Misc/School";
import { MaybeSQLEntity } from "../Misc/SQLDBCEntity";
import { Spell } from "./Spell";

export enum DisableEffectsMask {
    EFFECT0 = 0x1,
    EFFECT1 = 0x2,
    EFFECT2 = 0x4,
}

export enum SpellAttributesMas {
    /** Requires target to give exp or honor */
    REQUIRE_EXP_OR_HONOR     = 0x1,
    /** Can proc even if this spell is triggered by another spell */
    CAN_PROC_ON_TRIGGERED    = 0x2,
    /** Requires the triggering spell to cost mana */
    REQUIRE_MANA_COST        = 0x4,
    /** Requires triggering spell to be affected by aura of this spell */
    REQUIRE_SPELL_MOD        = 0x8,
    /** Aura has reduced chance to proc if actor is > level 60 */
    REDUCE_PROC60            = 0x80,
    /** Does not allow proc if proc is caused by spell cast by item */
    CANT_PROC_FROM_ITEM_CAST = 0x100,
}

export enum SpellAttributesMask {
    REQUIRE_EXP_OR_HONOR     = 0x1,
    CAN_PROC_ON_TRIGGERED    = 0x2,
    REQUIRE_MANA_COST        = 0x4,
    REQUIRE_SPELL_MOD        = 0x8,
    USE_STACKS_FOR_CHARGES   = 0x10,
    REDUCE_PROC60            = 0x80,
    CANT_PROC_FROM_ITEM_CAST = 0x100,
}

export enum SpellHitMask {
    NORMAL      = 0x1,
    CRITICAL    = 0x2,
    MISS        = 0x4,
    FULL_RESIST = 0x8,
    DODGE       = 0x10,
    PARRY       = 0x20,
    BLOCK       = 0x40,
    EVADE       = 0x80,
    IMMUNE      = 0x100,
    DEFLECT     = 0x200,
    ABSORB      = 0x400,
    REFLECT     = 0x800,
    INTERRUPT   = 0x1000,
    FULL_BLOCK  = 0x2000,
}

export enum SpellPhaseMask {
    CAST   = 0x1,
    HIT    = 0x2,
    FINISH = 0x4,

}

export enum SpellTypeMask {
    DAMAGE = 0x1,
    HEAL   = 0x2,
    OTHER  = 0x4,
}

export enum SpellFamilyName {
    GENERIC      = 0,
    MAGE         = 3,
    WARRIOR      = 4,
    WARLOCK      = 5,
    PRIEST       = 6,
    DRUID        = 7,
    ROGUE        = 8,
    HUNTER       = 9,
    PALADIN      = 10,
    SHAMAN       = 11,
    POTION       = 13,
    DEATH_KNIGHT = 15,
}

export enum SpellProcFlags {
    KILLED                          = 0x00000001,    // 00 Killed by agressor - not sure about this flag
    KILL                            = 0x00000002,    // 01 Kill target (in most cases need XP/Honor reward)
    DONE_MELEE_AUTO_ATTACK          = 0x00000004,    // 02 Done melee auto attack
    TAKEN_MELEE_AUTO_ATTACK         = 0x00000008,    // 03 Taken melee auto attack
    DONE_SPELL_MELEE_DMG_CLASS      = 0x00000010,    // 04 Done attack by Spell that has dmg class melee
    TAKEN_SPELL_MELEE_DMG_CLASS     = 0x00000020,    // 05 Taken attack by Spell that has dmg class melee
    DONE_RANGED_AUTO_ATTACK         = 0x00000040,    // 06 Done ranged auto attack
    TAKEN_RANGED_AUTO_ATTACK        = 0x00000080,    // 07 Taken ranged auto attack
    DONE_SPELL_RANGED_DAMAGE_CLASS     = 0x00000100,    // 08 Done attack by Spell that has dmg class ranged
    TAKEN_SPELL_RANGED_DAMAGE_CLASS    = 0x00000200,    // 09 Taken attack by Spell that has dmg class ranged
    DONE_SPELL_NONE_DAMAGE_CLASS_POSITIVE   = 0x00000400,    // 10 Done positive spell that has dmg class none
    TAKEN_SPELL_NONE_DAMAGE_CLASS_POSITIVE  = 0x00000800,    // 11 Taken positive spell that has dmg class none
    DONE_SPELL_NONE_DAMAGE_CLASS_NEGATIVE   = 0x00001000,    // 12 Done negative spell that has dmg class none
    TAKEN_SPELL_NONE_DAMAGE_CLASS_NEGATIVE  = 0x00002000,    // 13 Taken negative spell that has dmg class none
    DONE_SPELL_MAGIC_DAMAGE_CLASS_POSITIVE  = 0x00004000,    // 14 Done positive spell that has dmg class magic
    TAKEN_SPELL_MAGIC_DAMAGE_CLASS_POSITIVE = 0x00008000,    // 15 Taken positive spell that has dmg class magic
    DONE_SPELL_MAGIC_DAMAGE_CLASS_NEGATIVE  = 0x00010000,    // 16 Done negative spell that has dmg class magic
    TAKEN_SPELL_MAGIC_DAMAGE_CLASS_NEGATIVE = 0x00020000,    // 17 Taken negative spell that has dmg class magic
    DONE_PERIODIC                   = 0x00040000,    // 18 Successful do periodic (damage / healing)
    TAKEN_PERIODIC                  = 0x00080000,    // 19 Taken spell periodic (damage / healing)
    TAKEN_DAMAGE                    = 0x00100000,    // 20 Taken any damage
    DONE_TRAP_ACTIVATION            = 0x00200000,    // 21 On trap activation (possibly needs name change to ON_GAMEOBJECT_CAST or USE)
    DONE_MAINHAND_ATTACK            = 0x00400000,    // 22 Done main-hand melee attacks (spell and autoattack)
    DONE_OFFHAND_ATTACK             = 0x00800000,    // 23 Done off-hand melee attacks (spell and autoattack)
    DEATH                           = 0x01000000,    // 24 Died in any way
    DAMAGE_BLOCKED                  = 0x02000000,    // 25 Damage blocked
    CRITICAL_DAMAGE_DONE            = 0x04000000,    // 26 crit done
    CRITICAL_DAMAGE_TAKEN           = 0x08000000,    // 26 crit taken
    CRITICAL_HEALING_DONE           = 0x10000000,    // 27 Damage blocked
    CRITICAL_HEALING_TAKEN          = 0x20000000,    // 27 Damage blocked
}

export type DoneProcEvent =
    | 'MeleeAutoAttack'
    | 'SpellMeleeDamage'
    | 'RangedAutoAttack'
    | 'SpellRangedDamage'
    | 'SpellNonePositive'
    | 'SpellNoneNegative'
    | 'SpellMagicPositive'
    | 'SpellMagicNegative'
    | 'Periodic'
    | 'TrapActivation'
    | 'MainhandAttack'
    | 'OffhandAttack'
    | 'CriticalDamage'
    | 'CriticalHealing';

export type TakenProcEvent =
    | 'MeleeAutoAttack'
    | 'SpellMeleeDamage'
    | 'RangedAutoAttack'
    | 'SpellRangedDamage'
    | 'SpellNonePositive'
    | 'SpellNoneNegative'
    | 'SpellMagicPositive'
    | 'SpellMagicNegative'
    | 'Periodic'
    | 'Damage'
    | 'CriticalDamage'
    | 'CriticalHealing';

export type ProcEvent =
    | 'Killed'
    | 'Kill'
    | 'Death'
    | 'DamageBlocked';

export type ProcPhase = 'Cast' | 'Hit' | 'Finish';
export type ProcType = 'Damage' | 'Heal' | 'Other';
export type ProcHit =
    | 'Normal'
    | 'Critical'
    | 'Miss'
    | 'FullResist'
    | 'Dodge'
    | 'Parry'
    | 'Block'
    | 'Evade'
    | 'Immune'
    | 'Deflect'
    | 'Absorb'
    | 'Reflect'
    | 'Interrupt'
    | 'FullBlock';

export type ProcAttribute =
    | 'RequireXPOrHonor'
    | 'CanProcOnTriggered'
    | 'RequireManaCost'
    | 'TakesMana'
    | 'RequireSpellMod'
    | 'UseStacksForCharges'
    | 'ReduceProcAbove60'
    | 'CantProcFromItemCast';

export type ProcDisableEffect = 'Effect0' | 'Effect1' | 'Effect2';
export type ProcSchool = 'Physical' | 'Holy' | 'Fire' | 'Nature' | 'Frost' | 'Shadow' | 'Arcane';

export type ProcFamily =
    | 'Generic'
    | 'Mage'
    | 'Warrior'
    | 'Warlock'
    | 'Priest'
    | 'Druid'
    | 'Rogue'
    | 'Hunter'
    | 'Paladin'
    | 'Shaman'
    | 'Potion'
    | 'DeathKnight'
    | 'Tinker'
    | number;

export type ProcSpellOptions = {
    include?: [number, number, number][]
    exclude?: [number, number, number][]
};

const DONE_PROC_EVENTS: Record<DoneProcEvent, SpellProcFlags> = {
    MeleeAutoAttack: SpellProcFlags.DONE_MELEE_AUTO_ATTACK,
    SpellMeleeDamage: SpellProcFlags.DONE_SPELL_MELEE_DMG_CLASS,
    RangedAutoAttack: SpellProcFlags.DONE_RANGED_AUTO_ATTACK,
    SpellRangedDamage: SpellProcFlags.DONE_SPELL_RANGED_DAMAGE_CLASS,
    SpellNonePositive: SpellProcFlags.DONE_SPELL_NONE_DAMAGE_CLASS_POSITIVE,
    SpellNoneNegative: SpellProcFlags.DONE_SPELL_NONE_DAMAGE_CLASS_NEGATIVE,
    SpellMagicPositive: SpellProcFlags.DONE_SPELL_MAGIC_DAMAGE_CLASS_POSITIVE,
    SpellMagicNegative: SpellProcFlags.DONE_SPELL_MAGIC_DAMAGE_CLASS_NEGATIVE,
    Periodic: SpellProcFlags.DONE_PERIODIC,
    TrapActivation: SpellProcFlags.DONE_TRAP_ACTIVATION,
    MainhandAttack: SpellProcFlags.DONE_MAINHAND_ATTACK,
    OffhandAttack: SpellProcFlags.DONE_OFFHAND_ATTACK,
    CriticalDamage: SpellProcFlags.CRITICAL_DAMAGE_DONE,
    CriticalHealing: SpellProcFlags.CRITICAL_HEALING_DONE,
};

const TAKEN_PROC_EVENTS: Record<TakenProcEvent, SpellProcFlags> = {
    MeleeAutoAttack: SpellProcFlags.TAKEN_MELEE_AUTO_ATTACK,
    SpellMeleeDamage: SpellProcFlags.TAKEN_SPELL_MELEE_DMG_CLASS,
    RangedAutoAttack: SpellProcFlags.TAKEN_RANGED_AUTO_ATTACK,
    SpellRangedDamage: SpellProcFlags.TAKEN_SPELL_RANGED_DAMAGE_CLASS,
    SpellNonePositive: SpellProcFlags.TAKEN_SPELL_NONE_DAMAGE_CLASS_POSITIVE,
    SpellNoneNegative: SpellProcFlags.TAKEN_SPELL_NONE_DAMAGE_CLASS_NEGATIVE,
    SpellMagicPositive: SpellProcFlags.TAKEN_SPELL_MAGIC_DAMAGE_CLASS_POSITIVE,
    SpellMagicNegative: SpellProcFlags.TAKEN_SPELL_MAGIC_DAMAGE_CLASS_NEGATIVE,
    Periodic: SpellProcFlags.TAKEN_PERIODIC,
    Damage: SpellProcFlags.TAKEN_DAMAGE,
    CriticalDamage: SpellProcFlags.CRITICAL_DAMAGE_TAKEN,
    CriticalHealing: SpellProcFlags.CRITICAL_HEALING_TAKEN,
};

const PROC_EVENTS: Record<ProcEvent, SpellProcFlags> = {
    Killed: SpellProcFlags.KILLED,
    Kill: SpellProcFlags.KILL,
    Death: SpellProcFlags.DEATH,
    DamageBlocked: SpellProcFlags.DAMAGE_BLOCKED,
};

const PROC_PHASES: Record<ProcPhase, SpellPhaseMask> = {
    Cast: SpellPhaseMask.CAST,
    Hit: SpellPhaseMask.HIT,
    Finish: SpellPhaseMask.FINISH,
};

const PROC_TYPES: Record<ProcType, SpellTypeMask> = {
    Damage: SpellTypeMask.DAMAGE,
    Heal: SpellTypeMask.HEAL,
    Other: SpellTypeMask.OTHER,
};

const PROC_HITS: Record<ProcHit, SpellHitMask> = {
    Normal: SpellHitMask.NORMAL,
    Critical: SpellHitMask.CRITICAL,
    Miss: SpellHitMask.MISS,
    FullResist: SpellHitMask.FULL_RESIST,
    Dodge: SpellHitMask.DODGE,
    Parry: SpellHitMask.PARRY,
    Block: SpellHitMask.BLOCK,
    Evade: SpellHitMask.EVADE,
    Immune: SpellHitMask.IMMUNE,
    Deflect: SpellHitMask.DEFLECT,
    Absorb: SpellHitMask.ABSORB,
    Reflect: SpellHitMask.REFLECT,
    Interrupt: SpellHitMask.INTERRUPT,
    FullBlock: SpellHitMask.FULL_BLOCK,
};

const PROC_ATTRIBUTES: Record<ProcAttribute, SpellAttributesMask> = {
    RequireXPOrHonor: SpellAttributesMask.REQUIRE_EXP_OR_HONOR,
    CanProcOnTriggered: SpellAttributesMask.CAN_PROC_ON_TRIGGERED,
    RequireManaCost: SpellAttributesMask.REQUIRE_MANA_COST,
    TakesMana: SpellAttributesMask.REQUIRE_MANA_COST,
    RequireSpellMod: SpellAttributesMask.REQUIRE_SPELL_MOD,
    UseStacksForCharges: SpellAttributesMask.USE_STACKS_FOR_CHARGES,
    ReduceProcAbove60: SpellAttributesMask.REDUCE_PROC60,
    CantProcFromItemCast: SpellAttributesMask.CANT_PROC_FROM_ITEM_CAST,
};

const PROC_DISABLE_EFFECTS: Record<ProcDisableEffect, DisableEffectsMask> = {
    Effect0: DisableEffectsMask.EFFECT0,
    Effect1: DisableEffectsMask.EFFECT1,
    Effect2: DisableEffectsMask.EFFECT2,
};

const PROC_SCHOOLS: Record<ProcSchool, SchoolMask> = {
    Physical: SchoolMask.PHYSICAL,
    Holy: SchoolMask.HOLY,
    Fire: SchoolMask.FIRE,
    Nature: SchoolMask.NATURE,
    Frost: SchoolMask.FROST,
    Shadow: SchoolMask.SHADOW,
    Arcane: SchoolMask.ARCANE,
};

const PROC_FAMILIES: Record<string, SpellFamilyName | number> = {
    Generic: SpellFamilyName.GENERIC,
    Mage: SpellFamilyName.MAGE,
    Warrior: SpellFamilyName.WARRIOR,
    Warlock: SpellFamilyName.WARLOCK,
    Priest: SpellFamilyName.PRIEST,
    Druid: SpellFamilyName.DRUID,
    Rogue: SpellFamilyName.ROGUE,
    Hunter: SpellFamilyName.HUNTER,
    Paladin: SpellFamilyName.PALADIN,
    Shaman: SpellFamilyName.SHAMAN,
    Potion: SpellFamilyName.POTION,
    DeathKnight: SpellFamilyName.DEATH_KNIGHT,
    Tinker: 19,
};

function maskFrom<T extends string>(values: readonly T[], table: Record<T, number>) {
    return values.reduce((mask, value) => mask | table[value], 0);
}

function familyValue(family: ProcFamily) {
    return typeof family === 'number' ? family : PROC_FAMILIES[family];
}

export class SimpleClassMask<T> extends CellSystem<T>
{
    protected a: Cell<number,any>;
    protected b: Cell<number,any>;
    protected c: Cell<number,any>;

    constructor(owner: T, a: Cell<number, any>, b: Cell<number, any>, c: Cell<number,any>)
    {
        super(owner);
        this.a = a;
        this.b = b;
        this.c = c;
    }

    setSimple([a, b, c]) {
        this.a.set(a);
        this.b.set(b);
        this.c.set(c);

        return this.owner;
    }

    get A() { return new MaskCell32(this.owner, this.a) };
    get B() { return new MaskCell32(this.owner, this.b) };
    get C() { return new MaskCell32(this.owner, this.c) };
}

export class SQLMaybeWriteCell<T> extends Cell<number,T>{
    private proc: SpellProc<T>

    protected dbcCell: Cell<number,any>;
    protected sqlCell: (sql: spell_procRow)=>Cell<number,any>;

    constructor(owner: T, proc: SpellProc<T>, dbcCell: Cell<number,any>, sqlGetter: (sql: spell_procRow)=>Cell<number,any>) {
        super(owner);
        this.proc = proc;
        this.dbcCell = dbcCell;
        this.sqlCell = sqlGetter;
    }

    get() {
        if(this.proc.HasSQL()) {
            return this.sqlCell(MaybeSQLEntity.getSQL(this.proc)).get();
        } else {
            return this.dbcCell.get();
        }
    }

    set(value: number) {
        if(this.proc.HasSQL()) {
            this.sqlCell(MaybeSQLEntity.getSQL(this.proc)).set(value);
        }
        this.dbcCell.set(value);
        return this.owner;
    }
}

export class SpellProc<T> extends MaybeSQLEntity<T, spell_procRow> {
    private realOwner: Spell;
    private fluentEventsStarted = false;

    constructor(owner: T, realOwner: Spell)
    {
        super(owner);
        this.realOwner = realOwner;
    }

    protected createSQL(): spell_procRow {
        return SQL.spell_proc.add(this.realOwner.ID)
            // when we create this in sql, we want
            // the fields currently in dbc to stay the same
            .Chance.set(this.realOwner.row.ProcChance.get())
            .Charges.set(this.realOwner.row.ProcCharges.get())
            .ProcFlags.set(this.realOwner.row.ProcTypeMask.get())
            .AttributesMask.set(0)
            .Cooldown.set(0)
            .DisableEffectsMask.set(0)
            .HitMask.set(0)
            .ProcsPerMinute.set(0)
            .SchoolMask.set(0)
            .SpellFamilyMask0.set(0)
            .SpellFamilyMask1.set(0)
            .SpellFamilyMask2.set(0)
            .SpellFamilyName.set(0)
            .SpellPhaseMask.set(0)
            .SpellTypeMask.set(0)
    }

    HasSQL() {
        return MaybeSQLEntity.hasSQL(this);
    }

    protected findSQL(): spell_procRow {
        return SQL.spell_proc.query({SpellId:any(this.realOwner.ID,-this.realOwner.ID)})
    }
    protected isValidSQL(sql: spell_procRow): boolean {
        return sql.SpellId.get() === this.realOwner.ID
    }

    get TriggerMask() {
        return makeMaskCell32(SpellProcFlags,this, new SQLMaybeWriteCell(
              this
            , this
            , this.realOwner.row.ProcTypeMask
            , sql=>sql.ProcFlags
        ));
    }

    get Chance() {
        return new PercentCell(this.owner,'[1-101]', false, new SQLMaybeWriteCell(
              this.owner
            , this
            , this.realOwner.row.ProcChance
            , sql=>sql.Chance
        ))
    }

    get Charges() {
        return new SQLMaybeWriteCell(
              this.owner
            , this
            , this.realOwner.row.ProcCharges
            , sql=>sql.Charges
        )
    }

    get SchoolMask() {
        return makeMaskCell32(SchoolMask,this.owner, this.wrapSQL(0,sql=>sql.SchoolMask));
    }

    get SpellFamily() {
        return makeEnumCell(SpellFamilyName,this.owner, this.wrapSQL(0,sql=>sql.SpellFamilyName));
    }

    get ClassMask() {
        return new SimpleClassMask(this.owner
            , this.wrapSQL(0,s=>s.SpellFamilyMask0)
            , this.wrapSQL(0,s=>s.SpellFamilyMask1)
            , this.wrapSQL(0,s=>s.SpellFamilyMask2)
        );
    }

    get TypeMask() {
        return makeMaskCell32(
              SpellTypeMask
            , this.owner
            , this.wrapSQL(0,sql=>sql.SpellTypeMask)
        )
    }

    get PhaseMask() {
        return makeMaskCell32(
              SpellPhaseMask
            , this.owner
            , this.wrapSQL(0,sql=>sql.SpellPhaseMask)
        )
    }


    /**
     * - if 0 and TAKEN: will trigger on **normal** + **critical**
     * - if 0 and DONE:  will trigger on **normal** + **critical** + **absorb**
     */
    get HitMask() {
        return makeMaskCell32(
              SpellHitMask
            , this.owner
            , this.wrapSQL(0,sql=>sql.HitMask)
        )
    }

    get AttributesMask() {
        return makeMaskCell32(
              SpellAttributesMask
            , this.owner
            , this.wrapSQL(0,sql=>sql.AttributesMask)
        )
    }

    get DisableEffectsMask() {
        return makeMaskCell32(
            DisableEffectsMask
          , this.owner
          , this.wrapSQL(0,sql=>sql.DisableEffectsMask)
      )
    }

    get ProcsPerMinute() {
        return this.wrapSQL(0,sql=>sql.ProcsPerMinute);
    }

    get Cooldown() {
        return this.wrapSQL(0,sql=>sql.Cooldown);
    }

    private beginFluentEvents() {
        if(!this.fluentEventsStarted) {
            this.TriggerMask.set(0);
            this.fluentEventsStarted = true;
        }
    }

    private addEventMask(mask: number) {
        this.beginFluentEvents();
        this.TriggerMask.set(this.TriggerMask.get() | mask);
        return this;
    }

    private removeEventMask(mask: number) {
        this.TriggerMask.set(this.TriggerMask.get() & ~mask);
        return this;
    }

    /**
     * Adds "done by aura owner" proc events. First fluent event call clears
     * existing event flags, then done/taken/event calls compose one flat mask.
     */
    done(...events: DoneProcEvent[]) {
        return this.addEventMask(maskFrom(events, DONE_PROC_EVENTS));
    }

    /**
     * Adds "taken by aura owner" proc events. First fluent event call clears
     * existing event flags, then done/taken/event calls compose one flat mask.
     */
    taken(...events: TakenProcEvent[]) {
        return this.addEventMask(maskFrom(events, TAKEN_PROC_EVENTS));
    }

    /**
     * Adds proc events that are not naturally done/taken.
     */
    event(...events: ProcEvent[]) {
        return this.addEventMask(maskFrom(events, PROC_EVENTS));
    }

    withoutDone(...events: DoneProcEvent[]) {
        return this.removeEventMask(maskFrom(events, DONE_PROC_EVENTS));
    }

    withoutTaken(...events: TakenProcEvent[]) {
        return this.removeEventMask(maskFrom(events, TAKEN_PROC_EVENTS));
    }

    withoutEvent(...events: ProcEvent[]) {
        return this.removeEventMask(maskFrom(events, PROC_EVENTS));
    }

    /**
     * Sets one flat spell family/class-mask filter from a spell list.
     * This feeds SpellFamilyMask0/1/2; it cannot express mixed families.
     */
    spells(family: ProcFamily, spells: Spell[] = [], options: ProcSpellOptions = {}) {
        const combined: [number, number, number] = [0, 0, 0];
        spells.forEach((spell) => {
            const [a, b, c] = spell.ClassMask.get2();
            combined[0] |= a;
            combined[1] |= b;
            combined[2] |= c;
        });
        options.include?.forEach(([a, b, c]) => {
            combined[0] |= a;
            combined[1] |= b;
            combined[2] |= c;
        });
        options.exclude?.forEach(([a, b, c]) => {
            combined[0] &= ~a;
            combined[1] &= ~b;
            combined[2] &= ~c;
        });

        this.SpellFamily.set(familyValue(family));
        if(spells.length > 0 || options.include || options.exclude) {
            this.ClassMask.setSimple(combined);
        }
        return this;
    }

    phase(...phases: ProcPhase[]) {
        this.PhaseMask.set(maskFrom(phases, PROC_PHASES));
        return this;
    }

    type(...types: ProcType[]) {
        this.TypeMask.set(maskFrom(types, PROC_TYPES));
        return this;
    }

    hit(...hits: ProcHit[]) {
        this.HitMask.set(maskFrom(hits, PROC_HITS));
        return this;
    }

    attributes(...attributes: ProcAttribute[]) {
        this.AttributesMask.set(maskFrom(attributes, PROC_ATTRIBUTES));
        return this;
    }

    withoutAttributes(...attributes: ProcAttribute[]) {
        this.AttributesMask.set(this.AttributesMask.get() & ~maskFrom(attributes, PROC_ATTRIBUTES));
        return this;
    }

    disableEffects(...effects: ProcDisableEffect[]) {
        this.DisableEffectsMask.set(maskFrom(effects, PROC_DISABLE_EFFECTS));
        return this;
    }

    school(...schools: ProcSchool[]) {
        this.SchoolMask.set(maskFrom(schools, PROC_SCHOOLS));
        return this;
    }

    chance(value: number) {
        this.Chance.set(value);
        return this;
    }

    ppm(value: number) {
        this.ProcsPerMinute.set(value);
        return this;
    }

    cooldown(ms: number) {
        this.Cooldown.set(ms);
        return this;
    }

    charges(count: number) {
        this.Charges.set(count);
        return this;
    }

    raw(callback: (proc: this)=>void) {
        callback(this);
        return this;
    }

    mod(callback: (proc: SpellProcCB)=>void)
    {
        callback(new SpellProcCB(this.realOwner));
        return this.owner;
    }
}

export class SpellProcCB extends SpellProc<SpellProcCB>
{
    constructor(spell: Spell)
    {
        super(undefined,spell);
        this.owner = this;
    }
}
