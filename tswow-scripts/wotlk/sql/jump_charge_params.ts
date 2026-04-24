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
import { float, int, tinyint } from '../../data/primitives'
import { Relation } from '../../data/query/Relations'
import { PrimaryKey } from '../../data/table/PrimaryKey'
import { SQLCell, SQLCellReadOnly } from '../../data/sql/SQLCell'
import { SqlRow } from '../../data/sql/SQLRow'
import { SqlTable } from '../../data/sql/SQLTable'

export class jump_charge_paramsRow extends SqlRow<jump_charge_paramsCreator, jump_charge_paramsQuery> {
    @PrimaryKey()
    get id() { return new SQLCellReadOnly<int, this>(this, 'id') }

    get speed() { return new SQLCell<float, this>(this, 'speed') }
    get treatSpeedAsMoveTimeSeconds() { return new SQLCell<tinyint, this>(this, 'treatSpeedAsMoveTimeSeconds') }
    get unlimitedSpeed() { return new SQLCell<tinyint, this>(this, 'unlimitedSpeed') }
    get minHeight() { return new SQLCell<float, this>(this, 'minHeight') }
    get maxHeight() { return new SQLCell<float, this>(this, 'maxHeight') }
    get spellVisualId() { return new SQLCell<int, this>(this, 'spellVisualId') }
    get progressCurveId() { return new SQLCell<int, this>(this, 'progressCurveId') }
    get parabolicCurveId() { return new SQLCell<int, this>(this, 'parabolicCurveId') }
    get triggerSpellId() { return new SQLCell<int, this>(this, 'triggerSpellId') }

    clone(id: int, c?: jump_charge_paramsCreator): this {
        return this.cloneInternal([id], c)
    }
}

export type jump_charge_paramsCreator = {
    id?: int,
    speed?: float,
    treatSpeedAsMoveTimeSeconds?: tinyint,
    unlimitedSpeed?: tinyint,
    minHeight?: float,
    maxHeight?: float,
    spellVisualId?: int,
    progressCurveId?: int,
    parabolicCurveId?: int,
    triggerSpellId?: int,
}

export type jump_charge_paramsQuery = {
    id?: Relation<int>,
    speed?: Relation<float>,
    treatSpeedAsMoveTimeSeconds?: Relation<tinyint>,
    unlimitedSpeed?: Relation<tinyint>,
    minHeight?: Relation<float>,
    maxHeight?: Relation<float>,
    spellVisualId?: Relation<int>,
    progressCurveId?: Relation<int>,
    parabolicCurveId?: Relation<int>,
    triggerSpellId?: Relation<int>,
}

export class jump_charge_paramsTable extends SqlTable<
    jump_charge_paramsCreator,
    jump_charge_paramsQuery,
    jump_charge_paramsRow> {
    add(id: int, c?: jump_charge_paramsCreator): jump_charge_paramsRow {
        const first = this.first()
        if (first) return first.clone(id, c)
        return this.rowCreator(this, {}).clone(id, c)
    }
}

export const SQL_jump_charge_params = new jump_charge_paramsTable(
    'jump_charge_params',
    (table, obj) => new jump_charge_paramsRow(table, obj))
