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
import { int, tinyint } from '../../data/primitives'
import { Relation } from '../../data/query/Relations'
import { PrimaryKey } from '../../data/table/PrimaryKey'
import { SQLCell, SQLCellReadOnly } from '../../data/sql/SQLCell'
import { SqlRow } from '../../data/sql/SQLRow'
import { SqlTable } from '../../data/sql/SQLTable'

export class spell_charge_defRow extends SqlRow<spell_charge_defCreator, spell_charge_defQuery> {
    @PrimaryKey()
    get spell() { return new SQLCellReadOnly<int, this>(this, 'spell') }

    get max() { return new SQLCell<tinyint, this>(this, 'max') }
    get cooldown_ms() { return new SQLCell<int, this>(this, 'cooldown_ms') }

    clone(spell: int, c?: spell_charge_defCreator): this {
        return this.cloneInternal([spell], c)
    }
}

export type spell_charge_defCreator = {
    spell?: int,
    max?: tinyint,
    cooldown_ms?: int,
}

export type spell_charge_defQuery = {
    spell?: Relation<int>,
    max?: Relation<tinyint>,
    cooldown_ms?: Relation<int>,
}

export class spell_charge_defTable extends SqlTable<
    spell_charge_defCreator,
    spell_charge_defQuery,
    spell_charge_defRow> {
    add(spell: int, c?: spell_charge_defCreator): spell_charge_defRow {
        const first = this.first()
        if(first) return first.clone(spell, c)
        return this.rowCreator(this, {}).clone(spell, c)
    }
}

export const SQL_spell_charge_def = new spell_charge_defTable(
    'spell_charge_def',
    (table, obj) => new spell_charge_defRow(table, obj))
