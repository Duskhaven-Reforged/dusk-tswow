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

export class dbc_spellcasttimesRow extends SqlRow<dbc_spellcasttimesCreator,dbc_spellcasttimesQuery> {
    @PrimaryKey()
    get ID() {return new SQLCellReadOnly<int, this>(this, 'ID')}
    get Base() {return new SQLCell<int, this>(this, 'Base')}
    get PerLevel() {return new SQLCell<int, this>(this, 'PerLevel')}
    get Minimum() {return new SQLCell<int, this>(this, 'Minimum')}

    clone(ID : int, c? : dbc_spellcasttimesCreator) : this {
        return this.cloneInternal([ID],c)
    }
}

export type dbc_spellcasttimesCreator = {
    ID? : int,
    Base? : int,
    PerLevel? : int,
    Minimum? : int,
}

export type dbc_spellcasttimesQuery = {
    ID? : Relation<int>,
    Base? : Relation<int>,
    PerLevel? : Relation<int>,
    Minimum? : Relation<int>,
}

export class dbc_spellcasttimesTable extends SqlTable<
    dbc_spellcasttimesCreator,
    dbc_spellcasttimesQuery,
    dbc_spellcasttimesRow> {
    add(ID : int, c? : dbc_spellcasttimesCreator) : dbc_spellcasttimesRow {
        const first = this.first();
        if(first) return first.clone(ID,c)
        else return this.rowCreator(this, {}).clone(ID,c)
    }
}

export const SQL_dbc_spellcasttimes = new dbc_spellcasttimesTable(
    'dbc_spellcasttimes',
    (table, obj)=>new dbc_spellcasttimesRow(table, obj))
