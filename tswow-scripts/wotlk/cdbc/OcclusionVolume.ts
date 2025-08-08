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
   * Name, not used, for debug use only
   */
  get Name() { return new DBCStringCell(this, this.buffer, this.offset + 4) }

  /**
   * Map ID, from Map.dbc
   */
  get MapID() { return new DBCUIntCell(this, this.buffer, this.offset + 8) }

  /**
   * Flags (ukn behavior)
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
  protected defaultRow = [1, "UnusedMap", 2, 1];

  constructor() {
    super('OcclusionVolume', (t, b, o) => new OcclusionVolumeRow(t, b, o))
  }
  /** Loads a new OcclusionVolume.dbc from a file. */
  static read(path: string): OcclusionVolumeCDBCFile {
    return new OcclusionVolumeCDBCFile().read(path)
  }
  add(ID: int, c?: OcclusionVolumeCreator): OcclusionVolumeRow {
    return this.makeRow(0).clone(ID, c)
  }
  findByID(id: number) {
    return this.fastSearch(id);
  }
}
