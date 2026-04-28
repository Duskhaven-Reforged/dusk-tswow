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
import { float, int, text, varchar } from '../../data/primitives'
import { Relation } from '../../data/query/Relations'
import { PrimaryKey } from '../../data/table/PrimaryKey'
import { SQLCell, SQLCellReadOnly } from '../../data/sql/SQLCell'
import { SqlRow } from '../../data/sql/SQLRow'
import { SqlTable } from '../../data/sql/SQLTable'

export class map_dbcRow extends SqlRow<map_dbcCreator,map_dbcQuery> {
    @PrimaryKey()
    get ID() {return new SQLCellReadOnly<int, this>(this, 'ID')}
    get Directory() {return new SQLCell<varchar, this>(this, 'Directory')}
    get InstanceType() {return new SQLCell<int, this>(this, 'InstanceType')}
    get Flags() {return new SQLCell<int, this>(this, 'Flags')}
    get PVP() {return new SQLCell<int, this>(this, 'PVP')}
    get MapName_Lang_enUS() {return new SQLCell<varchar, this>(this, 'MapName_Lang_enUS')}
    get MapName_Lang_enGB() {return new SQLCell<varchar, this>(this, 'MapName_Lang_enGB')}
    get MapName_Lang_koKR() {return new SQLCell<varchar, this>(this, 'MapName_Lang_koKR')}
    get MapName_Lang_frFR() {return new SQLCell<varchar, this>(this, 'MapName_Lang_frFR')}
    get MapName_Lang_deDE() {return new SQLCell<varchar, this>(this, 'MapName_Lang_deDE')}
    get MapName_Lang_enCN() {return new SQLCell<varchar, this>(this, 'MapName_Lang_enCN')}
    get MapName_Lang_zhCN() {return new SQLCell<varchar, this>(this, 'MapName_Lang_zhCN')}
    get MapName_Lang_enTW() {return new SQLCell<varchar, this>(this, 'MapName_Lang_enTW')}
    get MapName_Lang_zhTW() {return new SQLCell<varchar, this>(this, 'MapName_Lang_zhTW')}
    get MapName_Lang_esES() {return new SQLCell<varchar, this>(this, 'MapName_Lang_esES')}
    get MapName_Lang_esMX() {return new SQLCell<varchar, this>(this, 'MapName_Lang_esMX')}
    get MapName_Lang_ruRU() {return new SQLCell<varchar, this>(this, 'MapName_Lang_ruRU')}
    get MapName_Lang_ptPT() {return new SQLCell<varchar, this>(this, 'MapName_Lang_ptPT')}
    get MapName_Lang_ptBR() {return new SQLCell<varchar, this>(this, 'MapName_Lang_ptBR')}
    get MapName_Lang_itIT() {return new SQLCell<varchar, this>(this, 'MapName_Lang_itIT')}
    get MapName_Lang_Unk() {return new SQLCell<varchar, this>(this, 'MapName_Lang_Unk')}
    get MapName_Lang_Mask() {return new SQLCell<int, this>(this, 'MapName_Lang_Mask')}
    get AreaTableID() {return new SQLCell<int, this>(this, 'AreaTableID')}
    get MapDescription0_Lang_enUS() {return new SQLCell<text, this>(this, 'MapDescription0_Lang_enUS')}
    get MapDescription0_Lang_enGB() {return new SQLCell<text, this>(this, 'MapDescription0_Lang_enGB')}
    get MapDescription0_Lang_koKR() {return new SQLCell<text, this>(this, 'MapDescription0_Lang_koKR')}
    get MapDescription0_Lang_frFR() {return new SQLCell<text, this>(this, 'MapDescription0_Lang_frFR')}
    get MapDescription0_Lang_deDE() {return new SQLCell<text, this>(this, 'MapDescription0_Lang_deDE')}
    get MapDescription0_Lang_enCN() {return new SQLCell<text, this>(this, 'MapDescription0_Lang_enCN')}
    get MapDescription0_Lang_zhCN() {return new SQLCell<text, this>(this, 'MapDescription0_Lang_zhCN')}
    get MapDescription0_Lang_enTW() {return new SQLCell<text, this>(this, 'MapDescription0_Lang_enTW')}
    get MapDescription0_Lang_zhTW() {return new SQLCell<text, this>(this, 'MapDescription0_Lang_zhTW')}
    get MapDescription0_Lang_esES() {return new SQLCell<text, this>(this, 'MapDescription0_Lang_esES')}
    get MapDescription0_Lang_esMX() {return new SQLCell<text, this>(this, 'MapDescription0_Lang_esMX')}
    get MapDescription0_Lang_ruRU() {return new SQLCell<text, this>(this, 'MapDescription0_Lang_ruRU')}
    get MapDescription0_Lang_ptPT() {return new SQLCell<text, this>(this, 'MapDescription0_Lang_ptPT')}
    get MapDescription0_Lang_ptBR() {return new SQLCell<text, this>(this, 'MapDescription0_Lang_ptBR')}
    get MapDescription0_Lang_itIT() {return new SQLCell<text, this>(this, 'MapDescription0_Lang_itIT')}
    get MapDescription0_Lang_Unk() {return new SQLCell<varchar, this>(this, 'MapDescription0_Lang_Unk')}
    get MapDescription0_Lang_Mask() {return new SQLCell<int, this>(this, 'MapDescription0_Lang_Mask')}
    get MapDescription1_Lang_enUS() {return new SQLCell<text, this>(this, 'MapDescription1_Lang_enUS')}
    get MapDescription1_Lang_enGB() {return new SQLCell<text, this>(this, 'MapDescription1_Lang_enGB')}
    get MapDescription1_Lang_koKR() {return new SQLCell<text, this>(this, 'MapDescription1_Lang_koKR')}
    get MapDescription1_Lang_frFR() {return new SQLCell<text, this>(this, 'MapDescription1_Lang_frFR')}
    get MapDescription1_Lang_deDE() {return new SQLCell<text, this>(this, 'MapDescription1_Lang_deDE')}
    get MapDescription1_Lang_enCN() {return new SQLCell<text, this>(this, 'MapDescription1_Lang_enCN')}
    get MapDescription1_Lang_zhCN() {return new SQLCell<text, this>(this, 'MapDescription1_Lang_zhCN')}
    get MapDescription1_Lang_enTW() {return new SQLCell<text, this>(this, 'MapDescription1_Lang_enTW')}
    get MapDescription1_Lang_zhTW() {return new SQLCell<text, this>(this, 'MapDescription1_Lang_zhTW')}
    get MapDescription1_Lang_esES() {return new SQLCell<text, this>(this, 'MapDescription1_Lang_esES')}
    get MapDescription1_Lang_esMX() {return new SQLCell<text, this>(this, 'MapDescription1_Lang_esMX')}
    get MapDescription1_Lang_ruRU() {return new SQLCell<text, this>(this, 'MapDescription1_Lang_ruRU')}
    get MapDescription1_Lang_ptPT() {return new SQLCell<text, this>(this, 'MapDescription1_Lang_ptPT')}
    get MapDescription1_Lang_ptBR() {return new SQLCell<text, this>(this, 'MapDescription1_Lang_ptBR')}
    get MapDescription1_Lang_itIT() {return new SQLCell<text, this>(this, 'MapDescription1_Lang_itIT')}
    get MapDescription1_Lang_Unk() {return new SQLCell<varchar, this>(this, 'MapDescription1_Lang_Unk')}
    get MapDescription1_Lang_Mask() {return new SQLCell<int, this>(this, 'MapDescription1_Lang_Mask')}
    get LoadingScreenID() {return new SQLCell<int, this>(this, 'LoadingScreenID')}
    get MinimapIconScale() {return new SQLCell<float, this>(this, 'MinimapIconScale')}
    get CorpseMapID() {return new SQLCell<int, this>(this, 'CorpseMapID')}
    get CorpseX() {return new SQLCell<float, this>(this, 'CorpseX')}
    get CorpseY() {return new SQLCell<float, this>(this, 'CorpseY')}
    get TimeOfDayOverride() {return new SQLCell<int, this>(this, 'TimeOfDayOverride')}
    get ExpansionID() {return new SQLCell<int, this>(this, 'ExpansionID')}
    get RaidOffset() {return new SQLCell<int, this>(this, 'RaidOffset')}
    get MaxPlayers() {return new SQLCell<int, this>(this, 'MaxPlayers')}

    clone(ID : int, c? : map_dbcCreator) : this {
        return this.cloneInternal([ID],c)
    }
}

export type map_dbcCreator = {
    ID? : int,
    Directory? : varchar,
    InstanceType? : int,
    Flags? : int,
    PVP? : int,
    MapName_Lang_enUS? : varchar,
    MapName_Lang_enGB? : varchar,
    MapName_Lang_koKR? : varchar,
    MapName_Lang_frFR? : varchar,
    MapName_Lang_deDE? : varchar,
    MapName_Lang_enCN? : varchar,
    MapName_Lang_zhCN? : varchar,
    MapName_Lang_enTW? : varchar,
    MapName_Lang_zhTW? : varchar,
    MapName_Lang_esES? : varchar,
    MapName_Lang_esMX? : varchar,
    MapName_Lang_ruRU? : varchar,
    MapName_Lang_ptPT? : varchar,
    MapName_Lang_ptBR? : varchar,
    MapName_Lang_itIT? : varchar,
    MapName_Lang_Unk? : varchar,
    MapName_Lang_Mask? : int,
    AreaTableID? : int,
    MapDescription0_Lang_enUS? : text,
    MapDescription0_Lang_enGB? : text,
    MapDescription0_Lang_koKR? : text,
    MapDescription0_Lang_frFR? : text,
    MapDescription0_Lang_deDE? : text,
    MapDescription0_Lang_enCN? : text,
    MapDescription0_Lang_zhCN? : text,
    MapDescription0_Lang_enTW? : text,
    MapDescription0_Lang_zhTW? : text,
    MapDescription0_Lang_esES? : text,
    MapDescription0_Lang_esMX? : text,
    MapDescription0_Lang_ruRU? : text,
    MapDescription0_Lang_ptPT? : text,
    MapDescription0_Lang_ptBR? : text,
    MapDescription0_Lang_itIT? : text,
    MapDescription0_Lang_Unk? : varchar,
    MapDescription0_Lang_Mask? : int,
    MapDescription1_Lang_enUS? : text,
    MapDescription1_Lang_enGB? : text,
    MapDescription1_Lang_koKR? : text,
    MapDescription1_Lang_frFR? : text,
    MapDescription1_Lang_deDE? : text,
    MapDescription1_Lang_enCN? : text,
    MapDescription1_Lang_zhCN? : text,
    MapDescription1_Lang_enTW? : text,
    MapDescription1_Lang_zhTW? : text,
    MapDescription1_Lang_esES? : text,
    MapDescription1_Lang_esMX? : text,
    MapDescription1_Lang_ruRU? : text,
    MapDescription1_Lang_ptPT? : text,
    MapDescription1_Lang_ptBR? : text,
    MapDescription1_Lang_itIT? : text,
    MapDescription1_Lang_Unk? : varchar,
    MapDescription1_Lang_Mask? : int,
    LoadingScreenID? : int,
    MinimapIconScale? : float,
    CorpseMapID? : int,
    CorpseX? : float,
    CorpseY? : float,
    TimeOfDayOverride? : int,
    ExpansionID? : int,
    RaidOffset? : int,
    MaxPlayers? : int,
}

export type map_dbcQuery = {
    ID? : Relation<int>,
    Directory? : Relation<varchar>,
    InstanceType? : Relation<int>,
    Flags? : Relation<int>,
    PVP? : Relation<int>,
    MapName_Lang_enUS? : Relation<varchar>,
    MapName_Lang_enGB? : Relation<varchar>,
    MapName_Lang_koKR? : Relation<varchar>,
    MapName_Lang_frFR? : Relation<varchar>,
    MapName_Lang_deDE? : Relation<varchar>,
    MapName_Lang_enCN? : Relation<varchar>,
    MapName_Lang_zhCN? : Relation<varchar>,
    MapName_Lang_enTW? : Relation<varchar>,
    MapName_Lang_zhTW? : Relation<varchar>,
    MapName_Lang_esES? : Relation<varchar>,
    MapName_Lang_esMX? : Relation<varchar>,
    MapName_Lang_ruRU? : Relation<varchar>,
    MapName_Lang_ptPT? : Relation<varchar>,
    MapName_Lang_ptBR? : Relation<varchar>,
    MapName_Lang_itIT? : Relation<varchar>,
    MapName_Lang_Unk? : Relation<varchar>,
    MapName_Lang_Mask? : Relation<int>,
    AreaTableID? : Relation<int>,
    MapDescription0_Lang_enUS? : Relation<text>,
    MapDescription0_Lang_enGB? : Relation<text>,
    MapDescription0_Lang_koKR? : Relation<text>,
    MapDescription0_Lang_frFR? : Relation<text>,
    MapDescription0_Lang_deDE? : Relation<text>,
    MapDescription0_Lang_enCN? : Relation<text>,
    MapDescription0_Lang_zhCN? : Relation<text>,
    MapDescription0_Lang_enTW? : Relation<text>,
    MapDescription0_Lang_zhTW? : Relation<text>,
    MapDescription0_Lang_esES? : Relation<text>,
    MapDescription0_Lang_esMX? : Relation<text>,
    MapDescription0_Lang_ruRU? : Relation<text>,
    MapDescription0_Lang_ptPT? : Relation<text>,
    MapDescription0_Lang_ptBR? : Relation<text>,
    MapDescription0_Lang_itIT? : Relation<text>,
    MapDescription0_Lang_Unk? : Relation<varchar>,
    MapDescription0_Lang_Mask? : Relation<int>,
    MapDescription1_Lang_enUS? : Relation<text>,
    MapDescription1_Lang_enGB? : Relation<text>,
    MapDescription1_Lang_koKR? : Relation<text>,
    MapDescription1_Lang_frFR? : Relation<text>,
    MapDescription1_Lang_deDE? : Relation<text>,
    MapDescription1_Lang_enCN? : Relation<text>,
    MapDescription1_Lang_zhCN? : Relation<text>,
    MapDescription1_Lang_enTW? : Relation<text>,
    MapDescription1_Lang_zhTW? : Relation<text>,
    MapDescription1_Lang_esES? : Relation<text>,
    MapDescription1_Lang_esMX? : Relation<text>,
    MapDescription1_Lang_ruRU? : Relation<text>,
    MapDescription1_Lang_ptPT? : Relation<text>,
    MapDescription1_Lang_ptBR? : Relation<text>,
    MapDescription1_Lang_itIT? : Relation<text>,
    MapDescription1_Lang_Unk? : Relation<varchar>,
    MapDescription1_Lang_Mask? : Relation<int>,
    LoadingScreenID? : Relation<int>,
    MinimapIconScale? : Relation<float>,
    CorpseMapID? : Relation<int>,
    CorpseX? : Relation<float>,
    CorpseY? : Relation<float>,
    TimeOfDayOverride? : Relation<int>,
    ExpansionID? : Relation<int>,
    RaidOffset? : Relation<int>,
    MaxPlayers? : Relation<int>,
}

export class map_dbcTable extends SqlTable<
    map_dbcCreator,
    map_dbcQuery,
    map_dbcRow> {
    add(ID : int, c? : map_dbcCreator) : map_dbcRow {
        const first = this.first();
        if(first) return first.clone(ID,c)
        else return this.rowCreator(this, {}).clone(ID,c)
    }
}

export const SQL_map_dbc = new map_dbcTable(
    'map_dbc',
    (table, obj)=>new map_dbcRow(table, obj))
