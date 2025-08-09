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
import { float, int, uint } from '../../data/primitives'
import { Relation } from '../../data/query/Relations'
import { PrimaryKey } from '../../data/table/PrimaryKey'
import { DBCFloatCell, DBCKeyCell, DBCUIntCell } from '../../data/dbc/DBCCell'
import { CDBCFile } from './CDBCFile'
import { DBCRow } from '../../data/dbc/DBCRow'

/**
 * Main row definition
 * - Add column comments to the commented getters below
 * - Add file comments to DBCFiles.ts
 */
export class OcclusionVolumePointRow extends DBCRow<OcclusionVolumePointCreator, OcclusionVolumePointQuery> {
    /**
     * Primary Key
     *
     * Id
     */
    @PrimaryKey()
    get ID() { return new DBCKeyCell(this, this.buffer, this.offset + 0) }

    /**
     * Map ID, from OcclusionVolume.cdbc
     */
    get occlusionVolumeID() { return new DBCUIntCell(this, this.buffer, this.offset + 4) }

    /**
     * Position x, client coordinates
     */
    get PositionX() { return new DBCFloatCell(this, this.buffer, this.offset + 8) }

    /**
     * Position y, client coordinates
     */
    get PositionY() { return new DBCFloatCell(this, this.buffer, this.offset + 12) }

    /**
     * Position z, client coordinates
     */
    get PositionZ() { return new DBCFloatCell(this, this.buffer, this.offset + 16) }

    /**
     * Creates a clone of this row with new primary keys.
     *
     * Cloned rows are automatically added at the end of the DBC file.
     */
    clone(ID: int, c?: OcclusionVolumePointCreator): this {
        return this.cloneInternal([ID], c);
    }
}

/**
 * Used for object creation (Don't comment these)
 */
export type OcclusionVolumePointCreator = {
    occlusionVolumeID?: uint
    PositionX?: float
    PositionY?: float
    PositionZ?: float
}

/**
 * Used for queries (Don't comment these)
 */
export type OcclusionVolumePointQuery = {
    ID?: Relation<int>
    occlusionVolumeID?: Relation<uint>
    PositionX?: Relation<float>
    PositionY?: Relation<float>
    PositionZ?: Relation<float>
}

/**
 * Table definition (specifies arguments to 'add' function)
 * - Add file comments to DBCFiles.ts
 */
export class OcclusionVolumePointCDBCFile extends CDBCFile<
    OcclusionVolumePointCreator,
    OcclusionVolumePointQuery,
    OcclusionVolumePointRow> {
    protected defaultRow = [0, 99999, 0.0, 0.0, 0.0];

    constructor() {
        super('OcclusionVolumePoint', (t, b, o) => new OcclusionVolumePointRow(t, b, o))
    }
    /** Loads a new SpellAdditionalCostData.dbc from a file. */
    static read(path: string): OcclusionVolumePointCDBCFile {
        return new OcclusionVolumePointCDBCFile().read(path)
    }
    add(ID: int, c?: OcclusionVolumePointCreator): OcclusionVolumePointRow {
        return this.makeRow(0).clone(ID, c)
    }
    findByID(id: number) {
        return this.fastSearch(id);
    }

    override fileWork(): void {
        const defaultOcclusionVolumePoint: Record<int, { OcclusionVolumeID: uint, PositionX: float, PositionY: float, PositionZ: float }> = {
            1:  { OcclusionVolumeID: 1, PositionX: -8814.79, PositionY: 474.86, PositionZ: 0 },
            2:  { OcclusionVolumeID: 1, PositionX: -8814.79, PositionY: 474.86, PositionZ: 138.87 },
            3:  { OcclusionVolumeID: 1, PositionX: -8986.46, PositionY: 679.74, PositionZ: 138.87 },
            4:  { OcclusionVolumeID: 1, PositionX: -8986.46, PositionY: 679.74, PositionZ: 0 },
            5:  { OcclusionVolumeID: 2, PositionX: 5567.66, PositionY: 714.34, PositionZ: 596.68 },
            6:  { OcclusionVolumeID: 2, PositionX: 5605.32, PositionY: 394.26, PositionZ: 596.68 },
            7:  { OcclusionVolumeID: 2, PositionX: 5888.74, PositionY: 382.14, PositionZ: 596.68 },
            8:  { OcclusionVolumeID: 2, PositionX: 6026.98, PositionY: 494.67, PositionZ: 596.68 },
            9:  { OcclusionVolumeID: 2, PositionX: 6051.12, PositionY: 634.18, PositionZ: 596.68 },
            10: { OcclusionVolumeID: 2, PositionX: 5952.54, PositionY: 849.71, PositionZ: 596.68 },
            11: { OcclusionVolumeID: 2, PositionX: 5793.61, PositionY: 991.54, PositionZ: 596.68 },
            12: { OcclusionVolumeID: 2, PositionX: 5582.06, PositionY: 799.39, PositionZ: 596.68 },
            13: { OcclusionVolumeID: 3, PositionX: 5634.5, PositionY: 419, PositionZ: 630.4 },
            14: { OcclusionVolumeID: 3, PositionX: 5612.57, PositionY: 814, PositionZ: 630.4 },
            15: { OcclusionVolumeID: 3, PositionX: 5810.19, PositionY: 953.83, PositionZ: 630.4 },
            16: { OcclusionVolumeID: 3, PositionX: 5955.67, PositionY: 785.4, PositionZ: 630.4 },
            17: { OcclusionVolumeID: 3, PositionX: 6025.93, PositionY: 545.39, PositionZ: 630.4 },
            18: { OcclusionVolumeID: 3, PositionX: 5817.91, PositionY: 394.35, PositionZ: 630.4 },
            19: { OcclusionVolumeID: 4, PositionX: 5892.49, PositionY: 627.3, PositionZ: 620 },
            20: { OcclusionVolumeID: 4, PositionX: 5892.49, PositionY: 627.3, PositionZ: 664 },
            21: { OcclusionVolumeID: 4, PositionX: 5990.5, PositionY: 557.7, PositionZ: 664 },
            22: { OcclusionVolumeID: 4, PositionX: 5990.5, PositionY: 557.7, PositionZ: 620 },
            23: { OcclusionVolumeID: 5, PositionX: 5847, PositionY: 539, PositionZ: 620 },
            24: { OcclusionVolumeID: 5, PositionX: 5847, PositionY: 539, PositionZ: 685 },
            25: { OcclusionVolumeID: 5, PositionX: 5800, PositionY: 526, PositionZ: 685 },
            26: { OcclusionVolumeID: 5, PositionX: 5800, PositionY: 526, PositionZ: 620 },
            27: { OcclusionVolumeID: 6, PositionX: 5827.39, PositionY: 603.35, PositionZ: 620.34 },
            28: { OcclusionVolumeID: 6, PositionX: 5827.39, PositionY: 603.35, PositionZ: 679.34 },
            29: { OcclusionVolumeID: 6, PositionX: 5786.06, PositionY: 590.07, PositionZ: 678.06 },
            30: { OcclusionVolumeID: 6, PositionX: 5786.06, PositionY: 590.07, PositionZ: 620.06 },
            31: { OcclusionVolumeID: 7, PositionX: 5730.99, PositionY: 679.31, PositionZ: 620.76 },
            32: { OcclusionVolumeID: 7, PositionX: 5730.99, PositionY: 679.31, PositionZ: 670.76 },
            33: { OcclusionVolumeID: 7, PositionX: 5679.87, PositionY: 720.15, PositionZ: 670.76 },
            34: { OcclusionVolumeID: 7, PositionX: 5679.87, PositionY: 720.15, PositionZ: 620.76 },
            35: { OcclusionVolumeID: 8, PositionX: 5774.5, PositionY: 655.43, PositionZ: 620.15 },
            36: { OcclusionVolumeID: 8, PositionX: 5774.5, PositionY: 655.43, PositionZ: 661.15 },
            37: { OcclusionVolumeID: 8, PositionX: 5762.92, PositionY: 611.93, PositionZ: 661.15 },
            38: { OcclusionVolumeID: 8, PositionX: 5762.92, PositionY: 611.93, PositionZ: 620.15 },
            39: { OcclusionVolumeID: 9, PositionX: 5784.65, PositionY: 664.97, PositionZ: 620.02 },
            40: { OcclusionVolumeID: 9, PositionX: 5784.65, PositionY: 664.97, PositionZ: 658.02 },
            41: { OcclusionVolumeID: 9, PositionX: 5823, PositionY: 686.91, PositionZ: 658.02 },
            42: { OcclusionVolumeID: 9, PositionX: 5823, PositionY: 686.91, PositionZ: 620.02 },
            43: { OcclusionVolumeID: 10, PositionX: 5841.56, PositionY: 633.28, PositionZ: 620.66 },
            44: { OcclusionVolumeID: 10, PositionX: 5841.56, PositionY: 633.28, PositionZ: 669.66 },
            45: { OcclusionVolumeID: 10, PositionX: 5845.92, PositionY: 669.59, PositionZ: 664.72 },
            46: { OcclusionVolumeID: 10, PositionX: 5845.92, PositionY: 669.59, PositionZ: 620.72 },
            47: { OcclusionVolumeID: 11, PositionX: 5904.17, PositionY: 618.49, PositionZ: 620.41 },
            48: { OcclusionVolumeID: 11, PositionX: 5904.17, PositionY: 618.49, PositionZ: 672.41 },
            49: { OcclusionVolumeID: 11, PositionX: 5883.92, PositionY: 591.09, PositionZ: 672.36 },
            50: { OcclusionVolumeID: 11, PositionX: 5883.92, PositionY: 591.09, PositionZ: 620.36 },
            51: { OcclusionVolumeID: 12, PositionX: 5895.04, PositionY: 627.82, PositionZ: 620.553 },
            52: { OcclusionVolumeID: 12, PositionX: 5895.04, PositionY: 627.82, PositionZ: 649.553 },
            53: { OcclusionVolumeID: 12, PositionX: 5834.78, PositionY: 553.96, PositionZ: 649.553 },
            54: { OcclusionVolumeID: 12, PositionX: 5834.78, PositionY: 553.96, PositionZ: 620.553 },
            55: { OcclusionVolumeID: 13, PositionX: 5899.41, PositionY: 668.11, PositionZ: 620.54 },
            56: { OcclusionVolumeID: 13, PositionX: 5899.41, PositionY: 668.11, PositionZ: 676.54 },
            57: { OcclusionVolumeID: 13, PositionX: 5941.06, PositionY: 714.99, PositionZ: 661.17 },
            58: { OcclusionVolumeID: 13, PositionX: 5941.06, PositionY: 714.99, PositionZ: 620.17 },
            59: { OcclusionVolumeID: 14, PositionX: 5864.97, PositionY: 751.17, PositionZ: 620.59 },
            60: { OcclusionVolumeID: 14, PositionX: 5864.97, PositionY: 751.17, PositionZ: 669.59 },
            61: { OcclusionVolumeID: 14, PositionX: 5835.75, PositionY: 720.71, PositionZ: 669.59 },
            62: { OcclusionVolumeID: 14, PositionX: 5835.75, PositionY: 720.71, PositionZ: 620.59 },
            63: { OcclusionVolumeID: 15, PositionX: 5636.45, PositionY: 398.96, PositionZ: 630 },
            64: { OcclusionVolumeID: 15, PositionX: 5947.95, PositionY: 827.4, PositionZ: 630 },
            65: { OcclusionVolumeID: 15, PositionX: 5924.75, PositionY: 796.8, PositionZ: 574.11 },
            66: { OcclusionVolumeID: 15, PositionX: 5806.25, PositionY: 636.31, PositionZ: 397.4 },
            67: { OcclusionVolumeID: 15, PositionX: 5636.45, PositionY: 398.96, PositionZ: 574.11 },
            68: { OcclusionVolumeID: 16, PositionX: 5590.91, PositionY: 783.86, PositionZ: 630 },
            69: { OcclusionVolumeID: 16, PositionX: 6006.9, PositionY: 537.08, PositionZ: 630 },
            70: { OcclusionVolumeID: 16, PositionX: 5971.75, PositionY: 552.69, PositionZ: 558.75 },
            71: { OcclusionVolumeID: 16, PositionX: 5806.25, PositionY: 636.31, PositionZ: 397.4 },
            72: { OcclusionVolumeID: 16, PositionX: 5631.61, PositionY: 751.34, PositionZ: 527.85 },
            73: { OcclusionVolumeID: 17, PositionX: 5749.85, PositionY: 865.61, PositionZ: 630 },
            74: { OcclusionVolumeID: 17, PositionX: 5749.85, PositionY: 865.61, PositionZ: 704 },
            75: { OcclusionVolumeID: 17, PositionX: 5864.17, PositionY: 851.47, PositionZ: 704 },
            76: { OcclusionVolumeID: 17, PositionX: 5864.17, PositionY: 851.47, PositionZ: 630 },
            77: { OcclusionVolumeID: 18, PositionX: 5800.51, PositionY: 816.42, PositionZ: 630 },
            78: { OcclusionVolumeID: 18, PositionX: 5800.51, PositionY: 816.42, PositionZ: 704 },
            79: { OcclusionVolumeID: 18, PositionX: 5814.84, PositionY: 906.58, PositionZ: 704 },
            80: { OcclusionVolumeID: 18, PositionX: 5814.84, PositionY: 906.58, PositionZ: 630 },
            81: { OcclusionVolumeID: 19, PositionX: 5868.96, PositionY: 572.64, PositionZ: 623.44 },
            82: { OcclusionVolumeID: 19, PositionX: 5868.96, PositionY: 572.64, PositionZ: 665.44 },
            83: { OcclusionVolumeID: 19, PositionX: 5847.01, PositionY: 539.19, PositionZ: 665.44 },
            84: { OcclusionVolumeID: 19, PositionX: 5847.01, PositionY: 539.19, PositionZ: 623.44 },
            85: { OcclusionVolumeID: 20, PositionX: 5867.43, PositionY: 720.05, PositionZ: 624.9 },
            86: { OcclusionVolumeID: 20, PositionX: 5867.43, PositionY: 720.05, PositionZ: 654.9 },
            87: { OcclusionVolumeID: 20, PositionX: 5893.14, PositionY: 700.98, PositionZ: 654.69 },
            88: { OcclusionVolumeID: 20, PositionX: 5893.14, PositionY: 700.98, PositionZ: 624.69 },
            89: { OcclusionVolumeID: 21, PositionX: 2478.83, PositionY: -5620.13, PositionZ: 366.5 },
            90: { OcclusionVolumeID: 21, PositionX: 2491.95, PositionY: -5599.4, PositionZ: 366.5 },
            91: { OcclusionVolumeID: 21, PositionX: 2487.5, PositionY: -5574.95, PositionZ: 366.5 },
            92: { OcclusionVolumeID: 21, PositionX: 2466.24, PositionY: -5561.56, PositionZ: 366.5 },
            93: { OcclusionVolumeID: 21, PositionX: 2441.82, PositionY: -5566.63, PositionZ: 366.5 },
            94: { OcclusionVolumeID: 21, PositionX: 2433.41, PositionY: -5612.21, PositionZ: 366.5 },
            95: { OcclusionVolumeID: 21, PositionX: 2454.25, PositionY: -5625.66, PositionZ: 366.5 },
            96: { OcclusionVolumeID: 22, PositionX: 2510.17, PositionY: -5702.89, PositionZ: 364 },
            97: { OcclusionVolumeID: 22, PositionX: 2491.45, PositionY: -5658.32, PositionZ: 520 },
            98: { OcclusionVolumeID: 22, PositionX: 2429.59, PositionY: -5523.72, PositionZ: 520 },
            99: { OcclusionVolumeID: 22, PositionX: 2413.68, PositionY: -5483.44, PositionZ: 364 },
            100: { OcclusionVolumeID: 23, PositionX: 2370.95, PositionY: -5680.14, PositionZ: 382 },
            101: { OcclusionVolumeID: 23, PositionX: 2351.14, PositionY: -5635.16, PositionZ: 382 },
            102: { OcclusionVolumeID: 23, PositionX: 2377.8, PositionY: -5472.44, PositionZ: 382 },
            103: { OcclusionVolumeID: 23, PositionX: 2512.15, PositionY: -5448.47, PositionZ: 382 },
            104: { OcclusionVolumeID: 23, PositionX: 2586.21, PositionY: -5551.42, PositionZ: 382 },
            105: { OcclusionVolumeID: 23, PositionX: 2537.38, PositionY: -5704.73, PositionZ: 382 },
            106: { OcclusionVolumeID: 23, PositionX: 2425.22, PositionY: -5744.56, PositionZ: 382 },
            107: { OcclusionVolumeID: 24, PositionX: 2364.06, PositionY: -5692.39, PositionZ: 382.24 },
            108: { OcclusionVolumeID: 24, PositionX: 2347.53, PositionY: -5672.2, PositionZ: 382.24 },
            109: { OcclusionVolumeID: 24, PositionX: 2333.75, PositionY: -5647.47, PositionZ: 382.24 },
            110: { OcclusionVolumeID: 24, PositionX: 2353.04, PositionY: -5636.88, PositionZ: 382.24 },
            111: { OcclusionVolumeID: 24, PositionX: 2382.53, PositionY: -5679.76, PositionZ: 382.24 },
            112: { OcclusionVolumeID: 25, PositionX: 1204.96, PositionY: -4963.32, PositionZ: 0 },
            113: { OcclusionVolumeID: 25, PositionX: 1198.36, PositionY: -4933.69, PositionZ: 346.08 },
            114: { OcclusionVolumeID: 25, PositionX: 1158.87, PositionY: -4823.17, PositionZ: 344.76 },
            115: { OcclusionVolumeID: 25, PositionX: 1149.31, PositionY: -4791.83, PositionZ: 0 },
            116: { OcclusionVolumeID: 26, PositionX: 1219.4, PositionY: -5004.42, PositionZ: 0 },
            117: { OcclusionVolumeID: 26, PositionX: 1219.4, PositionY: -5004.42, PositionZ: 173.61 },
            118: { OcclusionVolumeID: 26, PositionX: 1128.43, PositionY: -4743.66, PositionZ: 173.13 },
            119: { OcclusionVolumeID: 26, PositionX: 1128.43, PositionY: -4743.66, PositionZ: 0 },
            120: { OcclusionVolumeID: 27, PositionX: 1111.31, PositionY: -4819.9, PositionZ: 172.49 },
            121: { OcclusionVolumeID: 27, PositionX: 1090, PositionY: -4809.76, PositionZ: 172.49 },
            122: { OcclusionVolumeID: 27, PositionX: 1073.63, PositionY: -4794.27, PositionZ: 172.49 },
            123: { OcclusionVolumeID: 27, PositionX: 1069.92, PositionY: -4767.41, PositionZ: 172.49 },
            124: { OcclusionVolumeID: 27, PositionX: 1098.59, PositionY: -4757.36, PositionZ: 172.49 },
            125: { OcclusionVolumeID: 27, PositionX: 1163.56, PositionY: -4754.04, PositionZ: 172.49 },
            126: { OcclusionVolumeID: 27, PositionX: 1167.65, PositionY: -4794.54, PositionZ: 172.49 },
            127: { OcclusionVolumeID: 28, PositionX: 1142.32, PositionY: -5006.38, PositionZ: 172.12 },
            128: { OcclusionVolumeID: 28, PositionX: 1153.18, PositionY: -4968.25, PositionZ: 172.12 },
            129: { OcclusionVolumeID: 28, PositionX: 1254.25, PositionY: -4932.14, PositionZ: 172.12 },
            130: { OcclusionVolumeID: 28, PositionX: 1286.37, PositionY: -4988.77, PositionZ: 172.12 },
            131: { OcclusionVolumeID: 28, PositionX: 1156.18, PositionY: -5028.96, PositionZ: 172.12 },
            132: { OcclusionVolumeID: 29, PositionX: 2181.39, PositionY: 5312.53, PositionZ: -50 },
            133: { OcclusionVolumeID: 29, PositionX: 2181.39, PositionY: 5312.53, PositionZ: 44.97 },
            134: { OcclusionVolumeID: 29, PositionX: 2116.82, PositionY: 5332.81, PositionZ: 44.97 },
            135: { OcclusionVolumeID: 29, PositionX: 2116.82, PositionY: 5332.81, PositionZ: -50 },
            136: { OcclusionVolumeID: 30, PositionX: 2116.82, PositionY: 5332.81, PositionZ: -50 },
            137: { OcclusionVolumeID: 30, PositionX: 2116.82, PositionY: 5332.81, PositionZ: 44.97 },
            138: { OcclusionVolumeID: 30, PositionX: 2100.22, PositionY: 5284.27, PositionZ: 44.97 },
            139: { OcclusionVolumeID: 30, PositionX: 2100.22, PositionY: 5284.27, PositionZ: -50 },
            140: { OcclusionVolumeID: 31, PositionX: 2324.46, PositionY: 5188.88, PositionZ: -50 },
            141: { OcclusionVolumeID: 31, PositionX: 2324.46, PositionY: 5188.88, PositionZ: 38.29 },
            142: { OcclusionVolumeID: 31, PositionX: 2273.21, PositionY: 5209.71, PositionZ: 38.29 },
            143: { OcclusionVolumeID: 31, PositionX: 2273.21, PositionY: 5209.71, PositionZ: -50 },
            144: { OcclusionVolumeID: 32, PositionX: 2275.63, PositionY: 5190.17, PositionZ: -50 },
            145: { OcclusionVolumeID: 32, PositionX: 2275.63, PositionY: 5190.17, PositionZ: 39.58 },
            146: { OcclusionVolumeID: 32, PositionX: 2286.96, PositionY: 5210.31, PositionZ: 39.58 },
            147: { OcclusionVolumeID: 32, PositionX: 2286.96, PositionY: 5210.31, PositionZ: -50 },
            148: { OcclusionVolumeID: 33, PositionX: 2878.06, PositionY: 6177.6, PositionZ: 207.71 },
            149: { OcclusionVolumeID: 33, PositionX: 2937.51, PositionY: 6113.32, PositionZ: 207.71 },
            150: { OcclusionVolumeID: 33, PositionX: 2905.17, PositionY: 6082.4, PositionZ: 207.71 },
            151: { OcclusionVolumeID: 33, PositionX: 2845.54, PositionY: 6145.42, PositionZ: 207.71 },
            152: { OcclusionVolumeID: 34, PositionX: 2763.39, PositionY: 6089.57, PositionZ: 207.81 },
            153: { OcclusionVolumeID: 34, PositionX: 2828.63, PositionY: 6147.74, PositionZ: 207.81 },
            154: { OcclusionVolumeID: 34, PositionX: 2797.2, PositionY: 6179.55, PositionZ: 207.81 },
            155: { OcclusionVolumeID: 34, PositionX: 2733.97, PositionY: 6120.93, PositionZ: 207.81 },
            156: { OcclusionVolumeID: 35, PositionX: 2741.27, PositionY: 6261.16, PositionZ: 207.81 },
            157: { OcclusionVolumeID: 35, PositionX: 2799.12, PositionY: 6196.67, PositionZ: 207.81 },
            158: { OcclusionVolumeID: 35, PositionX: 2831.21, PositionY: 6228.19, PositionZ: 207.81 },
            159: { OcclusionVolumeID: 35, PositionX: 2773.36, PositionY: 6292.17, PositionZ: 207.81 },
            160: { OcclusionVolumeID: 36, PositionX: 2902.29, PositionY: 6274.68, PositionZ: 202.81 },
            161: { OcclusionVolumeID: 36, PositionX: 2846.68, PositionY: 6224.76, PositionZ: 202.81 },
            162: { OcclusionVolumeID: 36, PositionX: 2880.4, PositionY: 6195.17, PositionZ: 202.81 },
            163: { OcclusionVolumeID: 36, PositionX: 2931.48, PositionY: 6241.73, PositionZ: 202.81 },
            164: { OcclusionVolumeID: 37, PositionX: 2926.83, PositionY: 6120.47, PositionZ: 79.05 },
            165: { OcclusionVolumeID: 37, PositionX: 2781.04, PositionY: 6278.55, PositionZ: 79.08 },
            166: { OcclusionVolumeID: 37, PositionX: 2772.78, PositionY: 6291.58, PositionZ: 203.9 },
            167: { OcclusionVolumeID: 37, PositionX: 2935.23, PositionY: 6114.39, PositionZ: 192.48 },
            168: { OcclusionVolumeID: 38, PositionX: 2853.66, PositionY: 6199.46, PositionZ: 202.5 },
            169: { OcclusionVolumeID: 38, PositionX: 2910.86, PositionY: 6255.33, PositionZ: 200.34 },
            170: { OcclusionVolumeID: 38, PositionX: 2885.69, PositionY: 6230.23, PositionZ: 74.69 },
            171: { OcclusionVolumeID: 38, PositionX: 2860.12, PositionY: 6206.81, PositionZ: 82.69 },
            172: { OcclusionVolumeID: 39, PositionX: 2826.04, PositionY: 6175.62, PositionZ: 80.37 },
            173: { OcclusionVolumeID: 39, PositionX: 2760.65, PositionY: 6116.42, PositionZ: 78.86 },
            174: { OcclusionVolumeID: 39, PositionX: 2747.8, PositionY: 6104.57, PositionZ: 203.55 },
            175: { OcclusionVolumeID: 39, PositionX: 2826.79, PositionY: 6176.22, PositionZ: 203.27 },
            176: { OcclusionVolumeID: 40, PositionX: 2924.92, PositionY: 6123.46, PositionZ: 191.494 },
            177: { OcclusionVolumeID: 40, PositionX: 2940.1, PositionY: 6198.47, PositionZ: 191.494 },
            178: { OcclusionVolumeID: 40, PositionX: 2914.91, PositionY: 6257.2, PositionZ: 191.494 },
            179: { OcclusionVolumeID: 40, PositionX: 2860.75, PositionY: 6282.54, PositionZ: 191.494 },
            180: { OcclusionVolumeID: 40, PositionX: 2781.79, PositionY: 6276.07, PositionZ: 191.494 },
            181: { OcclusionVolumeID: 41, PositionX: 3968.22, PositionY: -3566.96, PositionZ: 256.79 },
            182: { OcclusionVolumeID: 41, PositionX: 3968.22, PositionY: -3566.96, PositionZ: 377.16 },
            183: { OcclusionVolumeID: 41, PositionX: 3940.76, PositionY: -3451.53, PositionZ: 354.33 },
            184: { OcclusionVolumeID: 41, PositionX: 3940.76, PositionY: -3451.53, PositionZ: 291.48 },
            185: { OcclusionVolumeID: 42, PositionX: 4792.57, PositionY: -1936.71, PositionZ: 160 },
            186: { OcclusionVolumeID: 42, PositionX: 4792.57, PositionY: -1936.71, PositionZ: 295.2 },
            187: { OcclusionVolumeID: 42, PositionX: 4791.64, PositionY: -2034.22, PositionZ: 295.2 },
            188: { OcclusionVolumeID: 42, PositionX: 4791.64, PositionY: -2034.22, PositionZ: 160 },
            189: { OcclusionVolumeID: 43, PositionX: 4790.41, PositionY: -2159.31, PositionZ: 295.2 },
            190: { OcclusionVolumeID: 43, PositionX: 4790.41, PositionY: -2159.31, PositionZ: 160 },
            191: { OcclusionVolumeID: 43, PositionX: 4791.35, PositionY: -2056.96, PositionZ: 160 },
            192: { OcclusionVolumeID: 43, PositionX: 4791.35, PositionY: -2056.96, PositionZ: 295.2 },
            193: { OcclusionVolumeID: 44, PositionX: -208.17, PositionY: -661.93, PositionZ: 120.12 },
            194: { OcclusionVolumeID: 44, PositionX: -232.25, PositionY: -655.88, PositionZ: 120.12 },
            195: { OcclusionVolumeID: 44, PositionX: -262.43, PositionY: -661.54, PositionZ: 120.12 },
            196: { OcclusionVolumeID: 44, PositionX: -261.61, PositionY: -693.07, PositionZ: 120.12 },
            197: { OcclusionVolumeID: 44, PositionX: -220.75, PositionY: -701.13, PositionZ: 120.12 },
            198: { OcclusionVolumeID: 45, PositionX: -336.99, PositionY: -570.87, PositionZ: 46.54 },
            199: { OcclusionVolumeID: 45, PositionX: -327.17, PositionY: -684.76, PositionZ: 46.54 },
            200: { OcclusionVolumeID: 45, PositionX: -510.79, PositionY: -626.06, PositionZ: 46.54 },
            201: { OcclusionVolumeID: 45, PositionX: -467.16, PositionY: -575.2, PositionZ: 46.54 },
            202: { OcclusionVolumeID: 46, PositionX: -336.72, PositionY: -777.35, PositionZ: 0 },
            203: { OcclusionVolumeID: 46, PositionX: -336.72, PositionY: -777.35, PositionZ: 126.6 },
            204: { OcclusionVolumeID: 46, PositionX: -335.71, PositionY: -707.36, PositionZ: 126.6 },
            205: { OcclusionVolumeID: 46, PositionX: -335.71, PositionY: -707.36, PositionZ: 0 },
            206: { OcclusionVolumeID: 47, PositionX: -364.9, PositionY: -637.23, PositionZ: 0 },
            207: { OcclusionVolumeID: 47, PositionX: -364.9, PositionY: -637.23, PositionZ: 85.53 },
            208: { OcclusionVolumeID: 47, PositionX: -365.04, PositionY: -806.45, PositionZ: 87.68 },
            209: { OcclusionVolumeID: 47, PositionX: -365.04, PositionY: -806.45, PositionZ: 0 },
            210: { OcclusionVolumeID: 48, PositionX: -227.26, PositionY: -760.09, PositionZ: 0 },
            211: { OcclusionVolumeID: 48, PositionX: -227.26, PositionY: -760.09, PositionZ: 101.34 },
            212: { OcclusionVolumeID: 48, PositionX: -444.86, PositionY: -740.53, PositionZ: 91.18 },
            213: { OcclusionVolumeID: 48, PositionX: -444.86, PositionY: -740.53, PositionZ: 0 },
            214: { OcclusionVolumeID: 49, PositionX: -287.79, PositionY: -547.27, PositionZ: 6.36 },
            215: { OcclusionVolumeID: 49, PositionX: -227.24, PositionY: -735.28, PositionZ: 6.36 },
            216: { OcclusionVolumeID: 49, PositionX: -393.96, PositionY: -846.24, PositionZ: 6.36 },
            217: { OcclusionVolumeID: 49, PositionX: -499.18, PositionY: -683.28, PositionZ: 6.36 },
            218: { OcclusionVolumeID: 49, PositionX: -441.81, PositionY: -599.64, PositionZ: 6.36 },
            219: { OcclusionVolumeID: 50, PositionX: 1530.73, PositionY: -3570.53, PositionZ: 158.3 },
            220: { OcclusionVolumeID: 50, PositionX: 1418.82, PositionY: -3136.56, PositionZ: 158.3 },
            221: { OcclusionVolumeID: 50, PositionX: 1418.82, PositionY: -3136.56, PositionZ: 0 },
            222: { OcclusionVolumeID: 50, PositionX: 1530.73, PositionY: -3570.53, PositionZ: 0 },
            223: { OcclusionVolumeID: 51, PositionX: 1579.05, PositionY: -3461.45, PositionZ: 147.2 },
            224: { OcclusionVolumeID: 51, PositionX: 1535.65, PositionY: -3238.25, PositionZ: 147.2 },
            225: { OcclusionVolumeID: 51, PositionX: 1447.48, PositionY: -3239.19, PositionZ: 147.2 },
            226: { OcclusionVolumeID: 51, PositionX: 1467.61, PositionY: -3591, PositionZ: 147.2 },
            227: { OcclusionVolumeID: 52, PositionX: 1713.96, PositionY: -3423.56, PositionZ: 133.84 },
            228: { OcclusionVolumeID: 52, PositionX: 1637.58, PositionY: -3417.66, PositionZ: 133.84 },
            229: { OcclusionVolumeID: 52, PositionX: 1608.12, PositionY: -3365.9, PositionZ: 133.84 },
            230: { OcclusionVolumeID: 52, PositionX: 1608.5, PositionY: -3307.29, PositionZ: 133.84 },
            231: { OcclusionVolumeID: 52, PositionX: 1635.45, PositionY: -3263.94, PositionZ: 133.84 },
            232: { OcclusionVolumeID: 52, PositionX: 1687.9, PositionY: -3252.33, PositionZ: 133.84 },
            233: { OcclusionVolumeID: 52, PositionX: 1747.5, PositionY: -3271.53, PositionZ: 133.84 },
            234: { OcclusionVolumeID: 53, PositionX: 1747.58, PositionY: -3461.09, PositionZ: 139 },
            235: { OcclusionVolumeID: 53, PositionX: 1658.71, PositionY: -3446.07, PositionZ: 139 },
            236: { OcclusionVolumeID: 53, PositionX: 1625.67, PositionY: -3548.94, PositionZ: 139 },
            237: { OcclusionVolumeID: 53, PositionX: 1676.45, PositionY: -3612.46, PositionZ: 139 },
            238: { OcclusionVolumeID: 53, PositionX: 1720.96, PositionY: -3582.86, PositionZ: 139 },
            239: { OcclusionVolumeID: 54, PositionX: 301.84, PositionY: -475.68, PositionZ: 104.58 },
            240: { OcclusionVolumeID: 54, PositionX: 293.69, PositionY: -509.9, PositionZ: 104.58 },
            241: { OcclusionVolumeID: 54, PositionX: 307.11, PositionY: -528.01, PositionZ: 104.58 },
            242: { OcclusionVolumeID: 54, PositionX: 485.31, PositionY: -525.93, PositionZ: 104.58 },
            243: { OcclusionVolumeID: 54, PositionX: 496.53, PositionY: -493.97, PositionZ: 104.58 },
            244: { OcclusionVolumeID: 55, PositionX: 481.86, PositionY: -273.24, PositionZ: 103.43 },
            245: { OcclusionVolumeID: 55, PositionX: 499.19, PositionY: -264.26, PositionZ: 103.43 },
            246: { OcclusionVolumeID: 55, PositionX: 490.12, PositionY: -233.92, PositionZ: 103.43 },
            247: { OcclusionVolumeID: 55, PositionX: 304.62, PositionY: -229.97, PositionZ: 103.43 },
            248: { OcclusionVolumeID: 55, PositionX: 302.91, PositionY: -268.57, PositionZ: 103.43 },
            249: { OcclusionVolumeID: 56, PositionX: 2087, PositionY: 300, PositionZ: 503 },
            250: { OcclusionVolumeID: 56, PositionX: 2087, PositionY: -300, PositionZ: 503 },
            251: { OcclusionVolumeID: 56, PositionX: 2087, PositionY: -300, PositionZ: -2000 },
            252: { OcclusionVolumeID: 56, PositionX: 2087, PositionY: 300, PositionZ: -2000 },
            253: { OcclusionVolumeID: 57, PositionX: 2087, PositionY: -300, PositionZ: 500 },
            254: { OcclusionVolumeID: 57, PositionX: 2531, PositionY: -288, PositionZ: 500 },
            255: { OcclusionVolumeID: 57, PositionX: 2531, PositionY: -288, PositionZ: -2000 },
            256: { OcclusionVolumeID: 57, PositionX: 2087, PositionY: -300, PositionZ: -2000 },
            257: { OcclusionVolumeID: 58, PositionX: 2087, PositionY: 300, PositionZ: 500 },
            258: { OcclusionVolumeID: 58, PositionX: 2538, PositionY: 212, PositionZ: 500 },
            259: { OcclusionVolumeID: 58, PositionX: 2538, PositionY: 212, PositionZ: -2000 },
            260: { OcclusionVolumeID: 58, PositionX: 2087, PositionY: 300, PositionZ: -2000 },
            261: { OcclusionVolumeID: 59, PositionX: 9000, PositionY: -1300, PositionZ: 10000 },
            262: { OcclusionVolumeID: 59, PositionX: 9000, PositionY: -1300, PositionZ: -10000 },
            263: { OcclusionVolumeID: 59, PositionX: -9000, PositionY: -1300, PositionZ: -10000 },
            264: { OcclusionVolumeID: 59, PositionX: -9000, PositionY: -1300, PositionZ: 10000 },
            265: { OcclusionVolumeID: 60, PositionX: -274.3, PositionY: 2212.7, PositionZ: 175 },
            266: { OcclusionVolumeID: 60, PositionX: -316.2, PositionY: 2120.5, PositionZ: 175 },
            267: { OcclusionVolumeID: 60, PositionX: -435.9, PositionY: 2033.8, PositionZ: 175 },
            268: { OcclusionVolumeID: 60, PositionX: -549.5, PositionY: 2104.6, PositionZ: 175 },
            269: { OcclusionVolumeID: 60, PositionX: -613.6, PositionY: 2212.3, PositionZ: 175 },
            270: { OcclusionVolumeID: 60, PositionX: -547.4, PositionY: 2320.6, PositionZ: 175 },
            271: { OcclusionVolumeID: 60, PositionX: -438.1, PositionY: 2382.3, PositionZ: 175 },
            272: { OcclusionVolumeID: 60, PositionX: -326.9, PositionY: 2320.5, PositionZ: 175 },
            273: { OcclusionVolumeID: 61, PositionX: -470.6, PositionY: 2243.3, PositionZ: 614 },
            274: { OcclusionVolumeID: 61, PositionX: -520.1, PositionY: 2293.4, PositionZ: -163.9 },
            275: { OcclusionVolumeID: 61, PositionX: -336.1, PositionY: 2116, PositionZ: -163.3 },
            276: { OcclusionVolumeID: 61, PositionX: -405.7, PositionY: 2179.5, PositionZ: 614 },
            277: { OcclusionVolumeID: 62, PositionX: -475.6, PositionY: 2173.8, PositionZ: 614 },
            278: { OcclusionVolumeID: 62, PositionX: -502.2, PositionY: 2148.7, PositionZ: -158 },
            279: { OcclusionVolumeID: 62, PositionX: -344.4, PositionY: 2309, PositionZ: -158 },
            280: { OcclusionVolumeID: 62, PositionX: -403.9, PositionY: 2243.7, PositionZ: 614 },
        }

        for (const [id, val] of Object.entries(defaultOcclusionVolumePoint)) {
            const idNum = Number(id);
            const row = this.findByID(idNum) ? this.getRow(idNum) : this.add(idNum);
            row.occlusionVolumeID.set(val.OcclusionVolumeID);
            row.PositionX.set(val.PositionX);
            row.PositionY.set(val.PositionY);
            row.PositionZ.set(val.PositionZ);
        }
    }
}
