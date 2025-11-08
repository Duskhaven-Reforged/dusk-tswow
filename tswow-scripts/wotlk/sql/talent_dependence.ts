/*
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

/* tslint:disable */
import { mediumint } from '../../data/primitives'
import { PrimaryKey } from '../../data/table/PrimaryKey'
import { SQLCell, SQLCellReadOnly } from '../../data/sql/SQLCell'
import { SqlRow } from '../../data/sql/SQLRow'
import { SqlTable } from '../../data/sql/SQLTable'

 /**
  * Main row definition
  * - Add column comments to the commented getters below
  * - Add file comments to DBCFiles.ts
  */
export class talent_dependenceRow extends SqlRow<talent_dependenceCreator,talent_dependenceQuery> {
    /**
     * Primary Key
     *
     * No comment (yet!)
     */

    @PrimaryKey()
    get spell() {return new SQLCellReadOnly<mediumint, this>(this, 'spell')}

    get dependency() {return new SQLCell<mediumint, this>(this, 'dependency')}

    /**
     * Creates a clone of this row with new primary keys.
     *
     * Cloned rows are automatically added to the SQL table.
     */
    clone(dependency : mediumint, c? : talent_dependenceCreator) : this {
        return this.cloneInternal([dependency],c)
    }
}

/**
 * Used for object creation (Don't comment these)
 */
export type talent_dependenceCreator = {
    dependency? : mediumint,
}

/**
 * Used for object queries (Don't comment these)
 */
export type talent_dependenceQuery = {
    spell?: mediumint,
    dependency? : mediumint,
}

/**
 * Table definition (specifies arguments to 'add' function)
 * - Add file comments to SQLFiles.ts
 */
export class talent_dependenceTable extends SqlTable<
    talent_dependenceCreator,
    talent_dependenceQuery,
    talent_dependenceRow> {
    add(spell : mediumint, c? : talent_dependenceCreator) : talent_dependenceRow {
        const first = this.first();
        if(first) return first.clone(spell, c)
        else return this.rowCreator(this, {}).clone(spell,c)
    }
}

/**
 * Table singleton (Object used by 'SQL' namespace)
 * - Add file comments to SQLFiles.ts
 */
export const SQL_talent_dependence = new talent_dependenceTable(
    'talent_dependence',
    (table, obj)=>new talent_dependenceRow(table, obj))