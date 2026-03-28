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
import { float, int } from '../../data/primitives'
import { Relation } from '../../data/query/Relations'
import { PrimaryKey } from '../../data/table/PrimaryKey'
import { DBCFloatCell, DBCKeyCell, DBCStringCell } from '../../data/dbc/DBCCell'
import { CDBCFile } from './CDBCFile'
import { DBCRow } from '../../data/dbc/DBCRow'

export class ItemDifficultyTextRow extends DBCRow<ItemDifficultyTextCreator,ItemDifficultyTextQuery> {
    @PrimaryKey()
    get ID() { return new DBCKeyCell(this,this.buffer,this.offset+0) }

    get Text() { return new DBCStringCell(this,this.buffer,this.offset+4) }
    get Red() { return new DBCFloatCell(this,this.buffer,this.offset+8) }
    get Green() { return new DBCFloatCell(this,this.buffer,this.offset+12) }
    get Blue() { return new DBCFloatCell(this,this.buffer,this.offset+16) }

    clone(ID: int, c?: ItemDifficultyTextCreator): this {
        return this.cloneInternal([ID], c);
    }
}

export type ItemDifficultyTextCreator = {
    Text?: string
    Red?: float
    Green?: float
    Blue?: float
}

export type ItemDifficultyTextQuery = {
    ID?: Relation<int>
    Text?: Relation<string>
    Red?: Relation<float>
    Green?: Relation<float>
    Blue?: Relation<float>
}

export class ItemDifficultyTextCDBCFile extends CDBCFile<
    ItemDifficultyTextCreator,
    ItemDifficultyTextQuery,
    ItemDifficultyTextRow> {
    protected defaultRow = [1, "Heroic", 0.12, 1.0, 0.0];

    constructor() {
        super('ItemDifficultyText', (t, b, o) => new ItemDifficultyTextRow(t, b, o))
    }

    static read(path: string): ItemDifficultyTextCDBCFile {
        return new ItemDifficultyTextCDBCFile().read(path)
    }

    add(ID: int, c?: ItemDifficultyTextCreator): ItemDifficultyTextRow {
        return this.makeRow(0).clone(ID, c)
    }

    create(ID: int, c?: ItemDifficultyTextCreator): ItemDifficultyTextRow {
        return this.add(ID, c)
    }

    findByID(id: number) {
        return this.fastSearch(id);
    }
}
