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
import { int } from '../../data/primitives'
import { Relation } from '../../data/query/Relations'
import { PrimaryKey } from '../../data/table/PrimaryKey'
import { SQLCell, SQLCellReadOnly } from '../../data/sql/SQLCell'
import { SqlRow } from '../../data/sql/SQLRow'
import { SqlTable } from '../../data/sql/SQLTable'

export class dbc_spelldurationRow extends SqlRow<dbc_spelldurationCreator,dbc_spelldurationQuery> {
    @PrimaryKey()
    get ID() {return new SQLCellReadOnly<int, this>(this, 'ID')}
    get Duration() {return new SQLCell<int, this>(this, 'Duration')}
    get DurationPerLevel() {return new SQLCell<int, this>(this, 'DurationPerLevel')}
    get MaxDuration() {return new SQLCell<int, this>(this, 'MaxDuration')}

    clone(ID : int, c? : dbc_spelldurationCreator) : this {
        return this.cloneInternal([ID],c)
    }
}

export type dbc_spelldurationCreator = {
    ID? : int,
    Duration? : int,
    DurationPerLevel? : int,
    MaxDuration? : int,
}

export type dbc_spelldurationQuery = {
    ID? : Relation<int>,
    Duration? : Relation<int>,
    DurationPerLevel? : Relation<int>,
    MaxDuration? : Relation<int>,
}

export class dbc_spelldurationTable extends SqlTable<
    dbc_spelldurationCreator,
    dbc_spelldurationQuery,
    dbc_spelldurationRow> {
    add(ID : int, c? : dbc_spelldurationCreator) : dbc_spelldurationRow {
        const first = this.first();
        if(first) return first.clone(ID,c)
        else return this.rowCreator(this, {}).clone(ID,c)
    }
}

export const SQL_dbc_spellduration = new dbc_spelldurationTable(
    'dbc_spellduration',
    (table, obj)=>new dbc_spelldurationRow(table, obj))
