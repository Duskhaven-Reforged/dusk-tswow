/*
 * Copyright (C) 2024 tswow <https://github.com/tswow/>
 * and Duskhaven <https://github.com/orgs/Duskhaven-Reforged>
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

/* tslint:disable */
import { int, uint } from '../../data/primitives'
import { Relation } from '../../data/query/Relations'
import { PrimaryKey } from '../../data/table/PrimaryKey'
import { DBCKeyCell, DBCStringCell, DBCUIntCell } from '../../data/dbc/DBCCell'
import { CDBCFile } from './CDBCFile'
import { DBCRow } from '../../data/dbc/DBCRow'

/**
 * Main row definition
 * - Add column comments to the commented getters below
 * - Add file comments to DBCFiles.ts
 */
export class OcclusionVolumeRow extends DBCRow<OcclusionVolumeCreator, OcclusionVolumeQuery> {
    /**
     * Primary Key
     *
     * Id
     */
    @PrimaryKey()
    get ID() { return new DBCKeyCell(this, this.buffer, this.offset + 0) }

    /**
     * Name, it's not really used anywhere, just a comment allowing to identify light info easily
     */
    get Name() { return new DBCStringCell(this, this.buffer, this.offset + 4) }

    /**
     * Map ID, from Map.dbc
     */
    get MapID() { return new DBCUIntCell(this, this.buffer, this.offset + 8) }

    /**
     * Flags
     */
    get Flags() { return new DBCUIntCell(this, this.buffer, this.offset + 12) }

    /**
     * Creates a clone of this row with new primary keys.
     *
     * Cloned rows are automatically added at the end of the DBC file.
     */
    clone(ID: int, c?: OcclusionVolumeCreator): this {
        return this.cloneInternal([ID], c);
    }
}

/**
 * Used for object creation (Don't comment these)
 */
export type OcclusionVolumeCreator = {
    Name?: string
    MapID?: uint
    Flags?: uint
}

/**
 * Used for queries (Don't comment these)
 */
export type OcclusionVolumeQuery = {
    ID?: Relation<int>
    Name?: Relation<string>
    MapID?: Relation<uint>
    Flags?: Relation<uint>
}

/**
 * Table definition (specifies arguments to 'add' function)
 * - Add file comments to DBCFiles.ts
 */
export class OcclusionVolumeCDBCFile extends CDBCFile<
    OcclusionVolumeCreator,
    OcclusionVolumeQuery,
    OcclusionVolumeRow> {
    protected defaultRow = [0, "Default", 999, 0];

    constructor() {
        super('OcclusionVolume', (t, b, o) => new OcclusionVolumeRow(t, b, o))
    }
    /** Loads a new SpellAdditionalCostData.dbc from a file. */
    static read(path: string): OcclusionVolumeCDBCFile {
        return new OcclusionVolumeCDBCFile().read(path)
    }
    add(ID: int, c?: OcclusionVolumeCreator): OcclusionVolumeRow {
        return this.makeRow(0).clone(ID, c)
    }
    findByID(id: number) {
        return this.fastSearch(id);
    }

    override fileWork(): void {
        const defaultOcclusionVolumes: Record<int, { Name: string, MapID: uint, Flags: uint }> = {
            1: { Name: "Unnamed", MapID: 0, Flags: 0 },
            2: { Name: "Unnamed", MapID: 571, Flags: 0 },
            3: { Name: "Unnamed", MapID: 571, Flags: 2 },
            4: { Name: "Unnamed", MapID: 571, Flags: 0 },
            5: { Name: "Unnamed", MapID: 571, Flags: 0 },
            6: { Name: "Unnamed", MapID: 571, Flags: 0 },
            7: { Name: "Unnamed", MapID: 571, Flags: 0 },
            8: { Name: "Unnamed", MapID: 571, Flags: 0 },
            9: { Name: "Unnamed", MapID: 571, Flags: 0 },
            10: { Name: "Unnamed", MapID: 571, Flags: 0 },
            11: { Name: "Unnamed", MapID: 571, Flags: 0 },
            12: { Name: "Unnamed", MapID: 571, Flags: 0 },
            13: { Name: "Unnamed", MapID: 571, Flags: 0 },
            14: { Name: "Unnamed", MapID: 571, Flags: 0 },
            15: { Name: "Unnamed", MapID: 571, Flags: 0 },
            16: { Name: "Unnamed", MapID: 571, Flags: 0 },
            17: { Name: "Unnamed", MapID: 571, Flags: 0 },
            18: { Name: "Unnamed", MapID: 571, Flags: 0 },
            19: { Name: "Unnamed", MapID: 571, Flags: 0 },
            20: { Name: "Unnamed", MapID: 571, Flags: 0 },
            21: { Name: "Unnamed", MapID: 609, Flags: 1 },
            22: { Name: "Unnamed", MapID: 609, Flags: 0 },
            23: { Name: "Unnamed", MapID: 609, Flags: 0 },
            24: { Name: "Unnamed", MapID: 609, Flags: 1 },
            25: { Name: "Unnamed", MapID: 571, Flags: 0 },
            26: { Name: "Unnamed", MapID: 571, Flags: 0 },
            27: { Name: "Unnamed", MapID: 571, Flags: 0 },
            28: { Name: "Unnamed", MapID: 571, Flags: 0 },
            29: { Name: "Unnamed", MapID: 571, Flags: 0 },
            30: { Name: "Unnamed", MapID: 571, Flags: 1 },
            31: { Name: "Unnamed", MapID: 571, Flags: 0 },
            32: { Name: "Unnamed", MapID: 571, Flags: 0 },
            33: { Name: "Unnamed", MapID: 571, Flags: 1 },
            34: { Name: "Unnamed", MapID: 571, Flags: 1 },
            35: { Name: "Unnamed", MapID: 571, Flags: 1 },
            36: { Name: "Unnamed", MapID: 571, Flags: 0 },
            37: { Name: "Unnamed", MapID: 571, Flags: 0 },
            38: { Name: "Unnamed", MapID: 571, Flags: 0 },
            39: { Name: "Unnamed", MapID: 571, Flags: 0 },
            40: { Name: "Unnamed", MapID: 571, Flags: 1 },
            41: { Name: "Unnamed", MapID: 571, Flags: 0 },
            42: { Name: "Unnamed", MapID: 571, Flags: 0 },
            43: { Name: "Unnamed", MapID: 571, Flags: 0 },
            44: { Name: "Unnamed", MapID: 600, Flags: 1 },
            45: { Name: "Unnamed", MapID: 600, Flags: 1 },
            46: { Name: "Unnamed", MapID: 600, Flags: 1 },
            47: { Name: "Unnamed", MapID: 600, Flags: 1 },
            48: { Name: "Unnamed", MapID: 600, Flags: 1 },
            49: { Name: "Unnamed", MapID: 600, Flags: 1 },
            50: { Name: "Unnamed", MapID: 571, Flags: 0 },
            51: { Name: "Unnamed", MapID: 571, Flags: 0 },
            52: { Name: "Unnamed", MapID: 571, Flags: 0 },
            53: { Name: "Unnamed", MapID: 571, Flags: 0 },
            54: { Name: "Unnamed", MapID: 575, Flags: 1 },
            55: { Name: "Unnamed", MapID: 575, Flags: 1 },
            56: { Name: "Unnamed", MapID: 603, Flags: 0 },
            57: { Name: "Unnamed", MapID: 603, Flags: 0 },
            58: { Name: "Unnamed", MapID: 603, Flags: 0 },
            59: { Name: "Unnamed", MapID: 631, Flags: 0 },
            60: { Name: "Unnamed", MapID: 631, Flags: 0 },
            61: { Name: "Unnamed", MapID: 631, Flags: 1 },
            62: { Name: "Unnamed", MapID: 631, Flags: 1 },
        };

        for (const [id, val] of Object.entries(defaultOcclusionVolumes)) {
            const idNum = Number(id);
            const row = this.findByID(idNum) ? this.getRow(idNum) : this.add(idNum);
            row.Name.set(val.Name);
            row.MapID.set(val.MapID);
            row.Flags.set(val.Flags);
        }
    }
}
