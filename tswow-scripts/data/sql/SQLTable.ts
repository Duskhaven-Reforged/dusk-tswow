/*
 * This file is part of tswow (https://github.com/tswow)
 *
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
import { inMemory } from '../query/Query';
import { BuildArgs } from '../Settings';
import { Row } from '../table/Row';
import { Table } from '../table/Table';
import { SqlConnection } from './SQLConnection';
import { SqlRow } from './SQLRow';

export type SqlRowCreator<C, Q, R extends SqlRow<C, Q>> = (table: SqlTable<C, Q, R>, obj: {[key: string]: any}) => R;
const EAGER_PRELOAD_TABLES = new Set<string>([
    'trainer_spell',
    'item_set_names',
    'gameobject',
    'item_template',
]);
const AUTO_EAGER_PRELOAD_QUERY_THRESHOLD = 32;
const AUTO_EAGER_PRELOAD_TIME_THRESHOLD_MS = 250;

export class SqlTable<C, Q, R extends SqlRow<C, Q>> extends Table<C, Q, R> {
    private cachedRows: {[key: string]: R} = {};
    private cachedFirst: R | undefined;
    private hasLoadedAllRows = false;
    private autoPreloadDone = false;
    private chunk_size: number;
    protected rowCreator: SqlRowCreator<C, Q, R>;

    private get cachedValues() {
        return Object.values(this.cachedRows);
    }

    static cachedRowCount(table: SqlTable<any, any, any>) {
        return Object.values(table.cachedRows).length;
    }

    static cachedValues(table: SqlTable<any, any, any>)  {
        return table.cachedValues;
    }

    static createRow<C, Q, R extends SqlRow<C, Q>>(table: SqlTable<C, Q, R>, obj: {[key: string]: any}): R {
        return table.rowCreator(table, obj);
    }

    static addRow<C, Q, R extends SqlRow<C, Q>>(table: SqlTable<C, Q, R>, row: R) {
        table.cachedRows[Row.fullKey(row)] = row;
    }

    constructor(name: string, rowCreator: SqlRowCreator<C, Q, R>, chunk_size: number = 2500) {
        super(name);
        this.chunk_size = chunk_size;
        this.rowCreator = rowCreator;
    }

    first(): R {
        if (this.cachedFirst) {
            return this.cachedFirst;
        }
        this.cachedFirst = this.filterInt({} as any, true)[0];
        if (!this.cachedFirst) {
            this.cachedFirst = this.rowCreator(this, {});
        }
        return this.cachedFirst;
    }

    queryAll(where: Q): R[] {
        return this.filterInt(where, false);
    }

    private isPkLookup(where: Q): string {
        let fields: string[] = Row.primaryKeyFields(this.rowCreator(this,{}));
        if(fields.length != Object.entries(where).length) {
            return undefined;
        }
        for(let field of fields) {
            if(where[field] === undefined) {
                return undefined;
            }

            if(typeof(where[field]) == 'object') {
                return undefined;
            }
        }
        return fields.map(x=>where[x]).join('_')
    }

    private isUnfilteredQuery(where: Q, firstOnly: boolean) {
        if(firstOnly || where === undefined || where === null) {
            return false;
        }
        if(typeof(where) !== 'object') {
            return false;
        }
        if((where as any).isAnyQuery || (where as any).isAllQuery) {
            return false;
        }
        return Object.keys(where as any).length === 0;
    }

    private eagerPreload(reason: string) {
        if(this.hasLoadedAllRows) {
            return;
        }

        const start = Date.now();
        const preloadedRows = SqlConnection.getRows(this, {} as any, false)
            .filter(x => !this.cachedRows[Row.fullKey(x)]);
        preloadedRows.forEach(x => this.cachedRows[Row.fullKey(x)] = x);
        this.hasLoadedAllRows = true;
        this.autoPreloadDone = true;

        if(BuildArgs.USE_TIMER) {
            console.log(
                `[timer] Eager SQL preload ${this.name}: `
                + `rows=${preloadedRows.length}, `
                + `reason=${reason}, `
                + `time=${((Date.now()-start)/1000).toFixed(2)}s`
            );
        }
    }

    private maybeEagerPreload() {
        if(!EAGER_PRELOAD_TABLES.has(this.name)) {
            return;
        }
        this.eagerPreload('allowlist');
    }

    private maybeAutoEagerPreload() {
        if(this.autoPreloadDone || this.hasLoadedAllRows) {
            return;
        }
        const stats = SqlConnection.getSourceReadStats(this.name);
        if(stats.count < AUTO_EAGER_PRELOAD_QUERY_THRESHOLD) {
            return;
        }
        if(stats.ms < AUTO_EAGER_PRELOAD_TIME_THRESHOLD_MS) {
            return;
        }
        this.eagerPreload(
              `query-threshold(${AUTO_EAGER_PRELOAD_QUERY_THRESHOLD})`
            + `+time-threshold(${(AUTO_EAGER_PRELOAD_TIME_THRESHOLD_MS/1000).toFixed(2)}s)`
        );
    }

    private filterInt(where: Q, firstOnly = false): R[] {
        this.maybeEagerPreload();
        this.maybeAutoEagerPreload();

        // Try looking up using only primary key
        let pkLookup = this.isPkLookup(where);
        let cacheMatches: R[] = []
        if(pkLookup) {
            let row = this.cachedRows[pkLookup];
            if(row) {
                return [row];
            }
            if(this.hasLoadedAllRows) {
                return [];
            }
        } else {
            for(let key in this.cachedRows) {
                let value = this.cachedRows[key];
                if(inMemory(where, value)) {
                    cacheMatches.push(value);
                    if(firstOnly) {
                        return cacheMatches;
                    }
                }
            }
            if(this.hasLoadedAllRows) {
                return cacheMatches;
            }
        }

        const dbMatches = SqlConnection.getRows(this, where, firstOnly)
            .filter(x => !this.cachedRows[Row.fullKey(x)]);
        dbMatches.forEach(x => this.cachedRows[Row.fullKey(x)] = x);
        if(this.isUnfilteredQuery(where, firstOnly)) {
            this.hasLoadedAllRows = true;
            this.autoPreloadDone = true;
        }
        const matches = cacheMatches.concat(dbMatches);
        return firstOnly ? matches.slice(0,1) : matches;
    }

    addRow(row: R) {
        this.cachedRows[Row.fullKey(row)] = row;
    }

    // TODO/sqltable
    protected writeToFile(sqlfile: string): string {
        const dirtyRows = this.cachedValues.filter(SqlRow.isDirty);
        if (dirtyRows.length === 0) { return sqlfile; }
        return sqlfile + `--${this.name}\n` + dirtyRows.map(x => SqlRow.getSql(x)).join('\n') + '\n';
    }

    static writeSQL(table: SqlTable<any,any,any>) {
        let dummyRow: SqlRow<any, any>;
        for (let row in table.cachedRows) {
            dummyRow = table.cachedRows[row];
            break;
        }
        if (!dummyRow) {
            return;
        }
    
        SqlConnection.world_dst.prepare(table.rowCreator(table, {}));
    
        const values = table.cachedValues.filter(SqlRow.isDirty);
        if (values.length === 0)
            return;
    
        /** Chunking */
        let totalInserts = 0;
        let insertChunks: any[][] = [];
        let currentInsertChunk: any[] = [];

        let totalDeletes = 0;
        let deleteChunks: any[][] = [];
        let currentDeleteChunk: any[] = [];
    
        values.forEach((x: SqlRow<any, any>) => {
            if (x.isDeleted()) {
                totalDeletes++;
                currentDeleteChunk.push(SqlRow.getPreparedDeleteStatement(x));
    
                if (currentDeleteChunk.length >= table.chunk_size) {
                    deleteChunks.push(currentDeleteChunk);
                    currentDeleteChunk = [];
                }
            } else {
                totalInserts++;
                currentInsertChunk.push(SqlRow.getPreparedStatement(x));
    
                if (currentInsertChunk.length >= table.chunk_size) {
                    insertChunks.push(currentInsertChunk);
                    currentInsertChunk = [];
                }
            }
        });
    
        // Push remaining values
        if (currentInsertChunk.length > 0) {
            insertChunks.push(currentInsertChunk);
        }

        if (currentDeleteChunk.length > 0) {
            deleteChunks.push(currentDeleteChunk);
        }
    
        // Execute in chunks
        insertChunks.forEach(chunk => {
            const insertStmt = SqlConnection.world_dst.prepare(SqlRow.generatePreparedStatementBulk(dummyRow, chunk.length));

            insertStmt.writeNormal([].concat.apply([], chunk));
        });

        deleteChunks.forEach(chunk => {
            const deleteStmt = SqlConnection.world_dst.prepare(SqlRow.generatePreparedDeleteStatementBulk(dummyRow, chunk.length));

            deleteStmt.writeNormal([].concat.apply([], chunk));
        });

        // console.log(`\n=== ${table.name} ===`);
        // console.log(`Original Writes: ${totalInserts} - Final Writes: ${insertChunks.length} of chunk size ${table.chunk_size}`);
        // console.log(`Original Deletes: ${totalDeletes} - Final Deletes: ${deleteChunks.length} of chunk size ${table.chunk_size}`);
    
        table.cachedRows = {};
        table.cachedFirst = undefined;
        table.hasLoadedAllRows = false;
        table.autoPreloadDone = false;
    }
}