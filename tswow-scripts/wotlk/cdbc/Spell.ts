/* tslint:disable */
import { float, int } from '../../data/primitives'
import { Relation } from '../../data/query/Relations'
import { PrimaryKey } from '../../data/table/PrimaryKey'
import { DBCFloatCell, DBCIntCell, DBCKeyCell, DBCStringCell } from '../../data/dbc/DBCCell'
import { CDBCFile } from './CDBCFile'
import { DBCRow } from '../../data/dbc/DBCRow'
import { CDBCGenerator } from './CDBCGenerator'

export class SpellCacheRow extends DBCRow<SpellCacheCreator, SpellCacheQuery> {
    @PrimaryKey()
    get Id() { return new DBCKeyCell(this, this.buffer, this.offset + 0) }
    get CacheVersion() { return new DBCIntCell(this, this.buffer, this.offset + 4) }
    get SpellDataHash() { return new DBCIntCell(this, this.buffer, this.offset + 8) }
    get Category() { return new DBCIntCell(this, this.buffer, this.offset + 12) }
    get Dispel() { return new DBCIntCell(this, this.buffer, this.offset + 16) }
    get Mechanic() { return new DBCIntCell(this, this.buffer, this.offset + 20) }
    get Attributes() { return new DBCIntCell(this, this.buffer, this.offset + 24) }
    get AttributesEx() { return new DBCIntCell(this, this.buffer, this.offset + 28) }
    get AttributesEx2() { return new DBCIntCell(this, this.buffer, this.offset + 32) }
    get AttributesEx3() { return new DBCIntCell(this, this.buffer, this.offset + 36) }
    get AttributesEx4() { return new DBCIntCell(this, this.buffer, this.offset + 40) }
    get AttributesEx5() { return new DBCIntCell(this, this.buffer, this.offset + 44) }
    get AttributesEx6() { return new DBCIntCell(this, this.buffer, this.offset + 48) }
    get AttributesEx7() { return new DBCIntCell(this, this.buffer, this.offset + 52) }
    get Stances() { return new DBCIntCell(this, this.buffer, this.offset + 56) }
    get StancesNot() { return new DBCIntCell(this, this.buffer, this.offset + 60) }
    get Targets() { return new DBCIntCell(this, this.buffer, this.offset + 64) }
    get TargetCreatureType() { return new DBCIntCell(this, this.buffer, this.offset + 68) }
    get RequiresSpellFocus() { return new DBCIntCell(this, this.buffer, this.offset + 72) }
    get FacingCasterFlags() { return new DBCIntCell(this, this.buffer, this.offset + 76) }
    get CasterAuraState() { return new DBCIntCell(this, this.buffer, this.offset + 80) }
    get TargetAuraState() { return new DBCIntCell(this, this.buffer, this.offset + 84) }
    get ExcludeCasterAuraState() { return new DBCIntCell(this, this.buffer, this.offset + 88) }
    get ExcludeTargetAuraState() { return new DBCIntCell(this, this.buffer, this.offset + 92) }
    get CasterAuraSpell() { return new DBCIntCell(this, this.buffer, this.offset + 96) }
    get TargetAuraSpell() { return new DBCIntCell(this, this.buffer, this.offset + 100) }
    get ExcludeCasterAuraSpell() { return new DBCIntCell(this, this.buffer, this.offset + 104) }
    get ExcludeTargetAuraSpell() { return new DBCIntCell(this, this.buffer, this.offset + 108) }
    get CastingTimeIndex() { return new DBCIntCell(this, this.buffer, this.offset + 112) }
    get RecoveryTime() { return new DBCIntCell(this, this.buffer, this.offset + 116) }
    get CategoryRecoveryTime() { return new DBCIntCell(this, this.buffer, this.offset + 120) }
    get InterruptFlags() { return new DBCIntCell(this, this.buffer, this.offset + 124) }
    get AuraInterruptFlags() { return new DBCIntCell(this, this.buffer, this.offset + 128) }
    get ChannelInterruptFlags() { return new DBCIntCell(this, this.buffer, this.offset + 132) }
    get ProcFlags() { return new DBCIntCell(this, this.buffer, this.offset + 136) }
    get ProcChance() { return new DBCIntCell(this, this.buffer, this.offset + 140) }
    get ProcCharges() { return new DBCIntCell(this, this.buffer, this.offset + 144) }
    get MaxLevel() { return new DBCIntCell(this, this.buffer, this.offset + 148) }
    get BaseLevel() { return new DBCIntCell(this, this.buffer, this.offset + 152) }
    get SpellLevel() { return new DBCIntCell(this, this.buffer, this.offset + 156) }
    get DurationIndex() { return new DBCIntCell(this, this.buffer, this.offset + 160) }
    get PowerType() { return new DBCIntCell(this, this.buffer, this.offset + 164) }
    get ManaCost() { return new DBCIntCell(this, this.buffer, this.offset + 168) }
    get ManaCostPerLevel() { return new DBCIntCell(this, this.buffer, this.offset + 172) }
    get ManaPerSecond() { return new DBCIntCell(this, this.buffer, this.offset + 176) }
    get ManaPerSecondPerLevel() { return new DBCIntCell(this, this.buffer, this.offset + 180) }
    get RangeIndex() { return new DBCIntCell(this, this.buffer, this.offset + 184) }
    get Speed() { return new DBCFloatCell(this, this.buffer, this.offset + 188) }
    get ModalNextSpell() { return new DBCIntCell(this, this.buffer, this.offset + 192) }
    get StackAmount() { return new DBCIntCell(this, this.buffer, this.offset + 196) }
    get Totem1() { return new DBCIntCell(this, this.buffer, this.offset + 200) }
    get Totem2() { return new DBCIntCell(this, this.buffer, this.offset + 204) }
    get Reagent1() { return new DBCIntCell(this, this.buffer, this.offset + 208) }
    get Reagent2() { return new DBCIntCell(this, this.buffer, this.offset + 212) }
    get Reagent3() { return new DBCIntCell(this, this.buffer, this.offset + 216) }
    get Reagent4() { return new DBCIntCell(this, this.buffer, this.offset + 220) }
    get Reagent5() { return new DBCIntCell(this, this.buffer, this.offset + 224) }
    get Reagent6() { return new DBCIntCell(this, this.buffer, this.offset + 228) }
    get Reagent7() { return new DBCIntCell(this, this.buffer, this.offset + 232) }
    get Reagent8() { return new DBCIntCell(this, this.buffer, this.offset + 236) }
    get ReagentCount1() { return new DBCIntCell(this, this.buffer, this.offset + 240) }
    get ReagentCount2() { return new DBCIntCell(this, this.buffer, this.offset + 244) }
    get ReagentCount3() { return new DBCIntCell(this, this.buffer, this.offset + 248) }
    get ReagentCount4() { return new DBCIntCell(this, this.buffer, this.offset + 252) }
    get ReagentCount5() { return new DBCIntCell(this, this.buffer, this.offset + 256) }
    get ReagentCount6() { return new DBCIntCell(this, this.buffer, this.offset + 260) }
    get ReagentCount7() { return new DBCIntCell(this, this.buffer, this.offset + 264) }
    get ReagentCount8() { return new DBCIntCell(this, this.buffer, this.offset + 268) }
    get EquippedItemClass() { return new DBCIntCell(this, this.buffer, this.offset + 272) }
    get EquippedItemSubClassMask() { return new DBCIntCell(this, this.buffer, this.offset + 276) }
    get EquippedItemInventoryTypeMask() { return new DBCIntCell(this, this.buffer, this.offset + 280) }
    get SpellVisualID1() { return new DBCIntCell(this, this.buffer, this.offset + 284) }
    get SpellVisualID2() { return new DBCIntCell(this, this.buffer, this.offset + 288) }
    get SpellIconID() { return new DBCIntCell(this, this.buffer, this.offset + 292) }
    get ActiveIconID() { return new DBCIntCell(this, this.buffer, this.offset + 296) }
    get SpellPriority() { return new DBCIntCell(this, this.buffer, this.offset + 300) }
    get SpellName() { return new DBCStringCell(this, this.buffer, this.offset + 304) }
    get Rank() { return new DBCStringCell(this, this.buffer, this.offset + 308) }
    get Description() { return new DBCStringCell(this, this.buffer, this.offset + 312) }
    get ToolTip() { return new DBCStringCell(this, this.buffer, this.offset + 316) }
    get ManaCostPct() { return new DBCIntCell(this, this.buffer, this.offset + 320) }
    get StartRecoveryCategory() { return new DBCIntCell(this, this.buffer, this.offset + 324) }
    get StartRecoveryTime() { return new DBCIntCell(this, this.buffer, this.offset + 328) }
    get MaxTargetLevel() { return new DBCIntCell(this, this.buffer, this.offset + 332) }
    get SpellFamilyName() { return new DBCIntCell(this, this.buffer, this.offset + 336) }
    get SpellFamilyFlags1() { return new DBCIntCell(this, this.buffer, this.offset + 340) }
    get SpellFamilyFlags2() { return new DBCIntCell(this, this.buffer, this.offset + 344) }
    get SpellFamilyFlags3() { return new DBCIntCell(this, this.buffer, this.offset + 348) }
    get MaxAffectedTargets() { return new DBCIntCell(this, this.buffer, this.offset + 352) }
    get DmgClass() { return new DBCIntCell(this, this.buffer, this.offset + 356) }
    get PreventionType() { return new DBCIntCell(this, this.buffer, this.offset + 360) }
    get StanceBarOrder() { return new DBCIntCell(this, this.buffer, this.offset + 364) }
    get MinFactionID() { return new DBCIntCell(this, this.buffer, this.offset + 368) }
    get MinReputation() { return new DBCIntCell(this, this.buffer, this.offset + 372) }
    get RequiredAuraVision() { return new DBCIntCell(this, this.buffer, this.offset + 376) }
    get RequiredTotemCategoryID1() { return new DBCIntCell(this, this.buffer, this.offset + 380) }
    get RequiredTotemCategoryID2() { return new DBCIntCell(this, this.buffer, this.offset + 384) }
    get AreaGroupId() { return new DBCIntCell(this, this.buffer, this.offset + 388) }
    get SchoolMask() { return new DBCIntCell(this, this.buffer, this.offset + 392) }
    get RuneCostID() { return new DBCIntCell(this, this.buffer, this.offset + 396) }
    get SpellMissileID() { return new DBCIntCell(this, this.buffer, this.offset + 400) }
    get PowerDisplayID() { return new DBCIntCell(this, this.buffer, this.offset + 404) }
    get DescriptionVariablesID() { return new DBCIntCell(this, this.buffer, this.offset + 408) }
    get Difficulty() { return new DBCIntCell(this, this.buffer, this.offset + 412) }

    clone(Id: int, c?: SpellCacheCreator): this {
        return this.cloneInternal([Id], c)
    }
}

export type SpellCacheCreator = {
    CacheVersion?: int
    SpellDataHash?: int
    Category?: int
    Dispel?: int
    Mechanic?: int
    Attributes?: int
    AttributesEx?: int
    AttributesEx2?: int
    AttributesEx3?: int
    AttributesEx4?: int
    AttributesEx5?: int
    AttributesEx6?: int
    AttributesEx7?: int
    Stances?: int
    StancesNot?: int
    Targets?: int
    TargetCreatureType?: int
    RequiresSpellFocus?: int
    FacingCasterFlags?: int
    CasterAuraState?: int
    TargetAuraState?: int
    ExcludeCasterAuraState?: int
    ExcludeTargetAuraState?: int
    CasterAuraSpell?: int
    TargetAuraSpell?: int
    ExcludeCasterAuraSpell?: int
    ExcludeTargetAuraSpell?: int
    CastingTimeIndex?: int
    RecoveryTime?: int
    CategoryRecoveryTime?: int
    InterruptFlags?: int
    AuraInterruptFlags?: int
    ChannelInterruptFlags?: int
    ProcFlags?: int
    ProcChance?: int
    ProcCharges?: int
    MaxLevel?: int
    BaseLevel?: int
    SpellLevel?: int
    DurationIndex?: int
    PowerType?: int
    ManaCost?: int
    ManaCostPerLevel?: int
    ManaPerSecond?: int
    ManaPerSecondPerLevel?: int
    RangeIndex?: int
    Speed?: float
    ModalNextSpell?: int
    StackAmount?: int
    Totem1?: int
    Totem2?: int
    Reagent1?: int
    Reagent2?: int
    Reagent3?: int
    Reagent4?: int
    Reagent5?: int
    Reagent6?: int
    Reagent7?: int
    Reagent8?: int
    ReagentCount1?: int
    ReagentCount2?: int
    ReagentCount3?: int
    ReagentCount4?: int
    ReagentCount5?: int
    ReagentCount6?: int
    ReagentCount7?: int
    ReagentCount8?: int
    EquippedItemClass?: int
    EquippedItemSubClassMask?: int
    EquippedItemInventoryTypeMask?: int
    SpellVisualID1?: int
    SpellVisualID2?: int
    SpellIconID?: int
    ActiveIconID?: int
    SpellPriority?: int
    SpellName?: string
    Rank?: string
    Description?: string
    ToolTip?: string
    ManaCostPct?: int
    StartRecoveryCategory?: int
    StartRecoveryTime?: int
    MaxTargetLevel?: int
    SpellFamilyName?: int
    SpellFamilyFlags1?: int
    SpellFamilyFlags2?: int
    SpellFamilyFlags3?: int
    MaxAffectedTargets?: int
    DmgClass?: int
    PreventionType?: int
    StanceBarOrder?: int
    MinFactionID?: int
    MinReputation?: int
    RequiredAuraVision?: int
    RequiredTotemCategoryID1?: int
    RequiredTotemCategoryID2?: int
    AreaGroupId?: int
    SchoolMask?: int
    RuneCostID?: int
    SpellMissileID?: int
    PowerDisplayID?: int
    DescriptionVariablesID?: int
    Difficulty?: int
}

export type SpellCacheQuery = {
    Id?: Relation<int>
}

export class SpellCacheCDBCFile extends CDBCFile<
    SpellCacheCreator,
    SpellCacheQuery,
    SpellCacheRow> {
    protected defaultRow = [1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, '', '', '', '', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0]

    constructor() {
        super('Spell', (t, b, o) => new SpellCacheRow(t, b, o))
    }

    static read(path: string): SpellCacheCDBCFile {
        return new SpellCacheCDBCFile().read(path)
    }

    fileWork() {
        new CDBCGenerator(this.defaultRow, 0).generate(this.getPath())
        this.read(this.getPath())
    }

    add(Id: int, c?: SpellCacheCreator): SpellCacheRow {
        return this.makeRow(0).clone(Id, c)
    }

    findByID(id: number) {
        return this.fastSearch(id)
    }
}
