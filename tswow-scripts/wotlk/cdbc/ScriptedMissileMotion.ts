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
import { DBCFloatCell, DBCIntCell, DBCKeyCell, DBCStringCell } from '../../data/dbc/DBCCell'
import { CDBCFile } from './CDBCFile'
import { DBCRow } from '../../data/dbc/DBCRow'

export class ScriptedMissileMotionRow extends DBCRow<ScriptedMissileMotionCreator,ScriptedMissileMotionQuery> {
    @PrimaryKey()
    get ID() { return new DBCKeyCell(this,this.buffer,this.offset+0) }

    get Motion() { return new DBCIntCell(this,this.buffer,this.offset+4) }
    get Count() { return new DBCIntCell(this,this.buffer,this.offset+8) }
    get DurationMs() { return new DBCIntCell(this,this.buffer,this.offset+12) }
    get TickMs() { return new DBCIntCell(this,this.buffer,this.offset+16) }
    get Flags() { return new DBCIntCell(this,this.buffer,this.offset+20) }
    get CollisionRadius() { return new DBCFloatCell(this,this.buffer,this.offset+24) }
    get Radius() { return new DBCFloatCell(this,this.buffer,this.offset+28) }
    get RadiusVelocity() { return new DBCFloatCell(this,this.buffer,this.offset+32) }
    get StartDistance() { return new DBCFloatCell(this,this.buffer,this.offset+36) }
    get Height() { return new DBCFloatCell(this,this.buffer,this.offset+40) }
    get VerticalVelocity() { return new DBCFloatCell(this,this.buffer,this.offset+44) }
    get ForwardSpeed() { return new DBCFloatCell(this,this.buffer,this.offset+48) }
    get AngularSpeed() { return new DBCFloatCell(this,this.buffer,this.offset+52) }
    get SineAmplitude() { return new DBCFloatCell(this,this.buffer,this.offset+56) }
    get SineFrequency() { return new DBCFloatCell(this,this.buffer,this.offset+60) }
    get ModelPath() { return new DBCStringCell(this,this.buffer,this.offset+64) }
    get VisualFlags() { return new DBCIntCell(this,this.buffer,this.offset+68) }
    get AttachMode() { return new DBCIntCell(this,this.buffer,this.offset+72) }
    get AttachPoint() { return new DBCIntCell(this,this.buffer,this.offset+76) }
    get VisualScale() { return new DBCFloatCell(this,this.buffer,this.offset+80) }

    clone(ID: int, c?: ScriptedMissileMotionCreator): this {
        return this.cloneInternal([ID], c);
    }
}

export type ScriptedMissileMotionCreator = {
    Motion?: int
    Count?: int
    DurationMs?: int
    TickMs?: int
    Flags?: int
    CollisionRadius?: float
    Radius?: float
    RadiusVelocity?: float
    StartDistance?: float
    Height?: float
    VerticalVelocity?: float
    ForwardSpeed?: float
    AngularSpeed?: float
    SineAmplitude?: float
    SineFrequency?: float
    ModelPath?: string
    VisualFlags?: int
    AttachMode?: int
    AttachPoint?: int
    VisualScale?: float
}

export type ScriptedMissileMotionQuery = {
    ID?: Relation<int>
    Motion?: Relation<int>
    Count?: Relation<int>
    DurationMs?: Relation<int>
    TickMs?: Relation<int>
    Flags?: Relation<int>
    CollisionRadius?: Relation<float>
    Radius?: Relation<float>
    RadiusVelocity?: Relation<float>
    StartDistance?: Relation<float>
    Height?: Relation<float>
    VerticalVelocity?: Relation<float>
    ForwardSpeed?: Relation<float>
    AngularSpeed?: Relation<float>
    SineAmplitude?: Relation<float>
    SineFrequency?: Relation<float>
    ModelPath?: Relation<string>
    VisualFlags?: Relation<int>
    AttachMode?: Relation<int>
    AttachPoint?: Relation<int>
    VisualScale?: Relation<float>
}

export class ScriptedMissileMotionCDBCFile extends CDBCFile<
    ScriptedMissileMotionCreator,
    ScriptedMissileMotionQuery,
    ScriptedMissileMotionRow> {
    protected defaultRow = [1, 0, 1, 15000, 50, 4, 0.75, 0.0, 0.0, 0.0, 1.25, 0.0, 0.0, 0.0, 0.0, 1.0, "Spells\\Arcane_Missile.mdx", 0, 0, 0, 1.0];

    constructor() {
        super('ScriptedMissileMotion', (t, b, o) => new ScriptedMissileMotionRow(t, b, o))
    }

    static read(path: string): ScriptedMissileMotionCDBCFile {
        return new ScriptedMissileMotionCDBCFile().read(path)
    }

    private nextAvailableID(): int {
        const used = new Set<number>();
        this.queryAll({} as ScriptedMissileMotionQuery).forEach(row => used.add(row.ID.get()));

        let id = 1;
        while (used.has(id)) {
            ++id;
        }
        return id;
    }

    add(c?: ScriptedMissileMotionCreator): ScriptedMissileMotionRow
    add(ID: int, c?: ScriptedMissileMotionCreator): ScriptedMissileMotionRow
    add(IDOrCreator?: int | ScriptedMissileMotionCreator, c?: ScriptedMissileMotionCreator): ScriptedMissileMotionRow {
        const hasExplicitID = typeof IDOrCreator === 'number';
        const ID = hasExplicitID ? (IDOrCreator as int) : this.nextAvailableID();
        const creator = hasExplicitID ? c : (IDOrCreator as ScriptedMissileMotionCreator | undefined);
        return this.makeRow(0).clone(ID, creator)
    }

    create(c?: ScriptedMissileMotionCreator): ScriptedMissileMotionRow
    create(ID: int, c?: ScriptedMissileMotionCreator): ScriptedMissileMotionRow
    create(IDOrCreator?: int | ScriptedMissileMotionCreator, c?: ScriptedMissileMotionCreator): ScriptedMissileMotionRow {
        return typeof IDOrCreator === 'number'
            ? this.add(IDOrCreator, c)
            : this.add(IDOrCreator)
    }

    findByID(id: number) {
        return this.fastSearch(id);
    }
}
