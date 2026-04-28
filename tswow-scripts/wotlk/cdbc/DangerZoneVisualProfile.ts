/*
 * Copyright (C) 2024 tswow <https://github.com/tswow/>
 * and Duskhaven <https://github.com/orgs/Duskhaven-Reforged>
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License,
 * version 3.
 */

/* tslint:disable */
import { float, int } from '../../data/primitives'
import { Relation } from '../../data/query/Relations'
import { PrimaryKey } from '../../data/table/PrimaryKey'
import { DBCFloatCell, DBCIntCell, DBCKeyCell, DBCStringCell } from '../../data/dbc/DBCCell'
import { CDBCFile } from './CDBCFile'
import { DBCRow } from '../../data/dbc/DBCRow'

export class DangerZoneVisualProfileRow extends DBCRow<DangerZoneVisualProfileCreator,DangerZoneVisualProfileQuery> {
    @PrimaryKey()
    get ID() { return new DBCKeyCell(this,this.buffer,this.offset+0) }

    get SchoolMask() { return new DBCIntCell(this,this.buffer,this.offset+4) }
    get ShapeMask() { return new DBCIntCell(this,this.buffer,this.offset+8) }
    get Flags() { return new DBCIntCell(this,this.buffer,this.offset+12) }
    get ModelPath() { return new DBCStringCell(this,this.buffer,this.offset+16) }
    get ModelRadius() { return new DBCFloatCell(this,this.buffer,this.offset+20) }
    get RadiusScale() { return new DBCFloatCell(this,this.buffer,this.offset+24) }
    get ZOffset() { return new DBCFloatCell(this,this.buffer,this.offset+28) }
    get MinDurationMs() { return new DBCIntCell(this,this.buffer,this.offset+32) }
    get MaxDurationMs() { return new DBCIntCell(this,this.buffer,this.offset+36) }
    get Red() { return new DBCFloatCell(this,this.buffer,this.offset+40) }
    get Green() { return new DBCFloatCell(this,this.buffer,this.offset+44) }
    get Blue() { return new DBCFloatCell(this,this.buffer,this.offset+48) }
    get Alpha() { return new DBCFloatCell(this,this.buffer,this.offset+52) }
    get DebugSpellID() { return new DBCIntCell(this,this.buffer,this.offset+56) }

    clone(ID: int, c?: DangerZoneVisualProfileCreator): this {
        return this.cloneInternal([ID], c);
    }
}

export type DangerZoneVisualProfileCreator = {
    SchoolMask?: int
    ShapeMask?: int
    Flags?: int
    ModelPath?: string
    ModelRadius?: float
    RadiusScale?: float
    ZOffset?: float
    MinDurationMs?: int
    MaxDurationMs?: int
    Red?: float
    Green?: float
    Blue?: float
    Alpha?: float
    DebugSpellID?: int
}

export type DangerZoneVisualProfileQuery = {
    ID?: Relation<int>
    SchoolMask?: Relation<int>
    ShapeMask?: Relation<int>
    Flags?: Relation<int>
    ModelPath?: Relation<string>
    ModelRadius?: Relation<float>
    RadiusScale?: Relation<float>
    ZOffset?: Relation<float>
    MinDurationMs?: Relation<int>
    MaxDurationMs?: Relation<int>
    Red?: Relation<float>
    Green?: Relation<float>
    Blue?: Relation<float>
    Alpha?: Relation<float>
    DebugSpellID?: Relation<int>
}

export class DangerZoneVisualProfileCDBCFile extends CDBCFile<
    DangerZoneVisualProfileCreator,
    DangerZoneVisualProfileQuery,
    DangerZoneVisualProfileRow> {
    protected defaultRow = [1, 0x7F, 1, 0x3D, "Spells\\DeathAndDecay_Area_Runes.mdx", 5.0, 1.0, 0.05, 250, 30000, 1.0, 0.0, 0.0, 0.75, 0];

    constructor() {
        super('DangerZoneVisualProfile', (t, b, o) => new DangerZoneVisualProfileRow(t, b, o))
    }

    static read(path: string): DangerZoneVisualProfileCDBCFile {
        return new DangerZoneVisualProfileCDBCFile().read(path)
    }

    add(ID: int, c?: DangerZoneVisualProfileCreator): DangerZoneVisualProfileRow {
        return this.makeRow(0).clone(ID, c)
    }

    create(ID: int, c?: DangerZoneVisualProfileCreator): DangerZoneVisualProfileRow {
        return this.add(ID, c)
    }

    findByID(id: number) {
        return this.fastSearch(id);
    }
}
