/* tslint:disable */
import { float, int, varchar } from '../../data/primitives'
import { Relation } from '../../data/query/Relations'
import { PrimaryKey } from '../../data/table/PrimaryKey'
import { SQLCell, SQLCellReadOnly } from '../../data/sql/SQLCell'
import { SqlRow } from '../../data/sql/SQLRow'
import { SqlTable } from '../../data/sql/SQLTable'

export class spell_dbcRow extends SqlRow<spell_dbcCreator,spell_dbcQuery> {
    @PrimaryKey()
    get Id() {return new SQLCellReadOnly<int, this>(this, 'Id')}
    get Category() {return new SQLCell<int, this>(this, 'Category')}
    get Dispel() {return new SQLCell<int, this>(this, 'Dispel')}
    get Mechanic() {return new SQLCell<int, this>(this, 'Mechanic')}
    get Attributes() {return new SQLCell<int, this>(this, 'Attributes')}
    get AttributesEx() {return new SQLCell<int, this>(this, 'AttributesEx')}
    get AttributesEx2() {return new SQLCell<int, this>(this, 'AttributesEx2')}
    get AttributesEx3() {return new SQLCell<int, this>(this, 'AttributesEx3')}
    get AttributesEx4() {return new SQLCell<int, this>(this, 'AttributesEx4')}
    get AttributesEx5() {return new SQLCell<int, this>(this, 'AttributesEx5')}
    get AttributesEx6() {return new SQLCell<int, this>(this, 'AttributesEx6')}
    get AttributesEx7() {return new SQLCell<int, this>(this, 'AttributesEx7')}
    get Stances() {return new SQLCell<int, this>(this, 'Stances')}
    get StancesNot() {return new SQLCell<int, this>(this, 'StancesNot')}
    get Targets() {return new SQLCell<int, this>(this, 'Targets')}
    get TargetCreatureType() {return new SQLCell<int, this>(this, 'TargetCreatureType')}
    get RequiresSpellFocus() {return new SQLCell<int, this>(this, 'RequiresSpellFocus')}
    get FacingCasterFlags() {return new SQLCell<int, this>(this, 'FacingCasterFlags')}
    get CasterAuraState() {return new SQLCell<int, this>(this, 'CasterAuraState')}
    get TargetAuraState() {return new SQLCell<int, this>(this, 'TargetAuraState')}
    get ExcludeCasterAuraState() {return new SQLCell<int, this>(this, 'ExcludeCasterAuraState')}
    get ExcludeTargetAuraState() {return new SQLCell<int, this>(this, 'ExcludeTargetAuraState')}
    get CasterAuraSpell() {return new SQLCell<int, this>(this, 'CasterAuraSpell')}
    get TargetAuraSpell() {return new SQLCell<int, this>(this, 'TargetAuraSpell')}
    get ExcludeCasterAuraSpell() {return new SQLCell<int, this>(this, 'ExcludeCasterAuraSpell')}
    get ExcludeTargetAuraSpell() {return new SQLCell<int, this>(this, 'ExcludeTargetAuraSpell')}
    get CastingTimeIndex() {return new SQLCell<int, this>(this, 'CastingTimeIndex')}
    get RecoveryTime() {return new SQLCell<int, this>(this, 'RecoveryTime')}
    get CategoryRecoveryTime() {return new SQLCell<int, this>(this, 'CategoryRecoveryTime')}
    get InterruptFlags() {return new SQLCell<int, this>(this, 'InterruptFlags')}
    get AuraInterruptFlags() {return new SQLCell<int, this>(this, 'AuraInterruptFlags')}
    get ChannelInterruptFlags() {return new SQLCell<int, this>(this, 'ChannelInterruptFlags')}
    get ProcFlags() {return new SQLCell<int, this>(this, 'ProcFlags')}
    get ProcChance() {return new SQLCell<int, this>(this, 'ProcChance')}
    get ProcCharges() {return new SQLCell<int, this>(this, 'ProcCharges')}
    get MaxLevel() {return new SQLCell<int, this>(this, 'MaxLevel')}
    get BaseLevel() {return new SQLCell<int, this>(this, 'BaseLevel')}
    get SpellLevel() {return new SQLCell<int, this>(this, 'SpellLevel')}
    get DurationIndex() {return new SQLCell<int, this>(this, 'DurationIndex')}
    get PowerType() {return new SQLCell<int, this>(this, 'PowerType')}
    get ManaCost() {return new SQLCell<int, this>(this, 'ManaCost')}
    get ManaCostPerLevel() {return new SQLCell<int, this>(this, 'ManaCostPerLevel')}
    get ManaPerSecond() {return new SQLCell<int, this>(this, 'ManaPerSecond')}
    get ManaPerSecondPerLevel() {return new SQLCell<int, this>(this, 'ManaPerSecondPerLevel')}
    get RangeIndex() {return new SQLCell<int, this>(this, 'RangeIndex')}
    get Speed() {return new SQLCell<float, this>(this, 'Speed')}
    get ModalNextSpell() {return new SQLCell<int, this>(this, 'ModalNextSpell')}
    get StackAmount() {return new SQLCell<int, this>(this, 'StackAmount')}
    get Totem1() {return new SQLCell<int, this>(this, 'Totem1')}
    get Totem2() {return new SQLCell<int, this>(this, 'Totem2')}
    get Reagent1() {return new SQLCell<int, this>(this, 'Reagent1')}
    get Reagent2() {return new SQLCell<int, this>(this, 'Reagent2')}
    get Reagent3() {return new SQLCell<int, this>(this, 'Reagent3')}
    get Reagent4() {return new SQLCell<int, this>(this, 'Reagent4')}
    get Reagent5() {return new SQLCell<int, this>(this, 'Reagent5')}
    get Reagent6() {return new SQLCell<int, this>(this, 'Reagent6')}
    get Reagent7() {return new SQLCell<int, this>(this, 'Reagent7')}
    get Reagent8() {return new SQLCell<int, this>(this, 'Reagent8')}
    get ReagentCount1() {return new SQLCell<int, this>(this, 'ReagentCount1')}
    get ReagentCount2() {return new SQLCell<int, this>(this, 'ReagentCount2')}
    get ReagentCount3() {return new SQLCell<int, this>(this, 'ReagentCount3')}
    get ReagentCount4() {return new SQLCell<int, this>(this, 'ReagentCount4')}
    get ReagentCount5() {return new SQLCell<int, this>(this, 'ReagentCount5')}
    get ReagentCount6() {return new SQLCell<int, this>(this, 'ReagentCount6')}
    get ReagentCount7() {return new SQLCell<int, this>(this, 'ReagentCount7')}
    get ReagentCount8() {return new SQLCell<int, this>(this, 'ReagentCount8')}
    get EquippedItemClass() {return new SQLCell<int, this>(this, 'EquippedItemClass')}
    get EquippedItemSubClassMask() {return new SQLCell<int, this>(this, 'EquippedItemSubClassMask')}
    get EquippedItemInventoryTypeMask() {return new SQLCell<int, this>(this, 'EquippedItemInventoryTypeMask')}
    get SpellVisualID1() {return new SQLCell<int, this>(this, 'SpellVisualID1')}
    get SpellVisualID2() {return new SQLCell<int, this>(this, 'SpellVisualID2')}
    get SpellIconID() {return new SQLCell<int, this>(this, 'SpellIconID')}
    get ActiveIconID() {return new SQLCell<int, this>(this, 'ActiveIconID')}
    get SpellPriority() {return new SQLCell<int, this>(this, 'SpellPriority')}
    get SpellName() {return new SQLCell<varchar, this>(this, 'SpellName')}
    get Rank() {return new SQLCell<varchar, this>(this, 'Rank')}
    get Description() {return new SQLCell<varchar, this>(this, 'Description')}
    get ToolTip() {return new SQLCell<varchar, this>(this, 'ToolTip')}
    get ManaCostPct() {return new SQLCell<int, this>(this, 'ManaCostPct')}
    get StartRecoveryCategory() {return new SQLCell<int, this>(this, 'StartRecoveryCategory')}
    get StartRecoveryTime() {return new SQLCell<int, this>(this, 'StartRecoveryTime')}
    get MaxTargetLevel() {return new SQLCell<int, this>(this, 'MaxTargetLevel')}
    get SpellFamilyName() {return new SQLCell<int, this>(this, 'SpellFamilyName')}
    get SpellFamilyFlags1() {return new SQLCell<int, this>(this, 'SpellFamilyFlags1')}
    get SpellFamilyFlags2() {return new SQLCell<int, this>(this, 'SpellFamilyFlags2')}
    get SpellFamilyFlags3() {return new SQLCell<int, this>(this, 'SpellFamilyFlags3')}
    get MaxAffectedTargets() {return new SQLCell<int, this>(this, 'MaxAffectedTargets')}
    get DmgClass() {return new SQLCell<int, this>(this, 'DmgClass')}
    get PreventionType() {return new SQLCell<int, this>(this, 'PreventionType')}
    get StanceBarOrder() {return new SQLCell<int, this>(this, 'StanceBarOrder')}
    get MinFactionID() {return new SQLCell<int, this>(this, 'MinFactionID')}
    get MinReputation() {return new SQLCell<int, this>(this, 'MinReputation')}
    get RequiredAuraVision() {return new SQLCell<int, this>(this, 'RequiredAuraVision')}
    get RequiredTotemCategoryID1() {return new SQLCell<int, this>(this, 'RequiredTotemCategoryID1')}
    get RequiredTotemCategoryID2() {return new SQLCell<int, this>(this, 'RequiredTotemCategoryID2')}
    get AreaGroupId() {return new SQLCell<int, this>(this, 'AreaGroupId')}
    get SchoolMask() {return new SQLCell<int, this>(this, 'SchoolMask')}
    get RuneCostID() {return new SQLCell<int, this>(this, 'RuneCostID')}
    get SpellMissileID() {return new SQLCell<int, this>(this, 'SpellMissileID')}
    get PowerDisplayID() {return new SQLCell<int, this>(this, 'PowerDisplayID')}
    get DescriptionVariablesID() {return new SQLCell<int, this>(this, 'DescriptionVariablesID')}
    get Difficulty() {return new SQLCell<int, this>(this, 'Difficulty')}
    get SpellDataHash() {return new SQLCell<int, this>(this, 'SpellDataHash')}
    clone(Id : int, c? : spell_dbcCreator) : this {
        return this.cloneInternal([Id],c)
    }
}

export type spell_dbcCreator = {
    Id? : int,
    Category? : int,
    Dispel? : int,
    Mechanic? : int,
    Attributes? : int,
    AttributesEx? : int,
    AttributesEx2? : int,
    AttributesEx3? : int,
    AttributesEx4? : int,
    AttributesEx5? : int,
    AttributesEx6? : int,
    AttributesEx7? : int,
    Stances? : int,
    StancesNot? : int,
    Targets? : int,
    TargetCreatureType? : int,
    RequiresSpellFocus? : int,
    FacingCasterFlags? : int,
    CasterAuraState? : int,
    TargetAuraState? : int,
    ExcludeCasterAuraState? : int,
    ExcludeTargetAuraState? : int,
    CasterAuraSpell? : int,
    TargetAuraSpell? : int,
    ExcludeCasterAuraSpell? : int,
    ExcludeTargetAuraSpell? : int,
    CastingTimeIndex? : int,
    RecoveryTime? : int,
    CategoryRecoveryTime? : int,
    InterruptFlags? : int,
    AuraInterruptFlags? : int,
    ChannelInterruptFlags? : int,
    ProcFlags? : int,
    ProcChance? : int,
    ProcCharges? : int,
    MaxLevel? : int,
    BaseLevel? : int,
    SpellLevel? : int,
    DurationIndex? : int,
    PowerType? : int,
    ManaCost? : int,
    ManaCostPerLevel? : int,
    ManaPerSecond? : int,
    ManaPerSecondPerLevel? : int,
    RangeIndex? : int,
    Speed? : float,
    ModalNextSpell? : int,
    StackAmount? : int,
    Totem1? : int,
    Totem2? : int,
    Reagent1? : int,
    Reagent2? : int,
    Reagent3? : int,
    Reagent4? : int,
    Reagent5? : int,
    Reagent6? : int,
    Reagent7? : int,
    Reagent8? : int,
    ReagentCount1? : int,
    ReagentCount2? : int,
    ReagentCount3? : int,
    ReagentCount4? : int,
    ReagentCount5? : int,
    ReagentCount6? : int,
    ReagentCount7? : int,
    ReagentCount8? : int,
    EquippedItemClass? : int,
    EquippedItemSubClassMask? : int,
    EquippedItemInventoryTypeMask? : int,
    SpellVisualID1? : int,
    SpellVisualID2? : int,
    SpellIconID? : int,
    ActiveIconID? : int,
    SpellPriority? : int,
    SpellName? : varchar,
    Rank? : varchar,
    Description? : varchar,
    ToolTip? : varchar,
    ManaCostPct? : int,
    StartRecoveryCategory? : int,
    StartRecoveryTime? : int,
    MaxTargetLevel? : int,
    SpellFamilyName? : int,
    SpellFamilyFlags1? : int,
    SpellFamilyFlags2? : int,
    SpellFamilyFlags3? : int,
    MaxAffectedTargets? : int,
    DmgClass? : int,
    PreventionType? : int,
    StanceBarOrder? : int,
    MinFactionID? : int,
    MinReputation? : int,
    RequiredAuraVision? : int,
    RequiredTotemCategoryID1? : int,
    RequiredTotemCategoryID2? : int,
    AreaGroupId? : int,
    SchoolMask? : int,
    RuneCostID? : int,
    SpellMissileID? : int,
    PowerDisplayID? : int,
    DescriptionVariablesID? : int,
    Difficulty? : int,
    SpellDataHash? : int,
}

export type spell_dbcQuery = {
    Id? : Relation<int>,
    Category? : Relation<int>,
    Dispel? : Relation<int>,
    Mechanic? : Relation<int>,
    Attributes? : Relation<int>,
    AttributesEx? : Relation<int>,
    AttributesEx2? : Relation<int>,
    AttributesEx3? : Relation<int>,
    AttributesEx4? : Relation<int>,
    AttributesEx5? : Relation<int>,
    AttributesEx6? : Relation<int>,
    AttributesEx7? : Relation<int>,
    Stances? : Relation<int>,
    StancesNot? : Relation<int>,
    Targets? : Relation<int>,
    TargetCreatureType? : Relation<int>,
    RequiresSpellFocus? : Relation<int>,
    FacingCasterFlags? : Relation<int>,
    CasterAuraState? : Relation<int>,
    TargetAuraState? : Relation<int>,
    ExcludeCasterAuraState? : Relation<int>,
    ExcludeTargetAuraState? : Relation<int>,
    CasterAuraSpell? : Relation<int>,
    TargetAuraSpell? : Relation<int>,
    ExcludeCasterAuraSpell? : Relation<int>,
    ExcludeTargetAuraSpell? : Relation<int>,
    CastingTimeIndex? : Relation<int>,
    RecoveryTime? : Relation<int>,
    CategoryRecoveryTime? : Relation<int>,
    InterruptFlags? : Relation<int>,
    AuraInterruptFlags? : Relation<int>,
    ChannelInterruptFlags? : Relation<int>,
    ProcFlags? : Relation<int>,
    ProcChance? : Relation<int>,
    ProcCharges? : Relation<int>,
    MaxLevel? : Relation<int>,
    BaseLevel? : Relation<int>,
    SpellLevel? : Relation<int>,
    DurationIndex? : Relation<int>,
    PowerType? : Relation<int>,
    ManaCost? : Relation<int>,
    ManaCostPerLevel? : Relation<int>,
    ManaPerSecond? : Relation<int>,
    ManaPerSecondPerLevel? : Relation<int>,
    RangeIndex? : Relation<int>,
    Speed? : Relation<float>,
    ModalNextSpell? : Relation<int>,
    StackAmount? : Relation<int>,
    Totem1? : Relation<int>,
    Totem2? : Relation<int>,
    Reagent1? : Relation<int>,
    Reagent2? : Relation<int>,
    Reagent3? : Relation<int>,
    Reagent4? : Relation<int>,
    Reagent5? : Relation<int>,
    Reagent6? : Relation<int>,
    Reagent7? : Relation<int>,
    Reagent8? : Relation<int>,
    ReagentCount1? : Relation<int>,
    ReagentCount2? : Relation<int>,
    ReagentCount3? : Relation<int>,
    ReagentCount4? : Relation<int>,
    ReagentCount5? : Relation<int>,
    ReagentCount6? : Relation<int>,
    ReagentCount7? : Relation<int>,
    ReagentCount8? : Relation<int>,
    EquippedItemClass? : Relation<int>,
    EquippedItemSubClassMask? : Relation<int>,
    EquippedItemInventoryTypeMask? : Relation<int>,
    SpellVisualID1? : Relation<int>,
    SpellVisualID2? : Relation<int>,
    SpellIconID? : Relation<int>,
    ActiveIconID? : Relation<int>,
    SpellPriority? : Relation<int>,
    SpellName? : Relation<varchar>,
    Rank? : Relation<varchar>,
    Description? : Relation<varchar>,
    ToolTip? : Relation<varchar>,
    ManaCostPct? : Relation<int>,
    StartRecoveryCategory? : Relation<int>,
    StartRecoveryTime? : Relation<int>,
    MaxTargetLevel? : Relation<int>,
    SpellFamilyName? : Relation<int>,
    SpellFamilyFlags1? : Relation<int>,
    SpellFamilyFlags2? : Relation<int>,
    SpellFamilyFlags3? : Relation<int>,
    MaxAffectedTargets? : Relation<int>,
    DmgClass? : Relation<int>,
    PreventionType? : Relation<int>,
    StanceBarOrder? : Relation<int>,
    MinFactionID? : Relation<int>,
    MinReputation? : Relation<int>,
    RequiredAuraVision? : Relation<int>,
    RequiredTotemCategoryID1? : Relation<int>,
    RequiredTotemCategoryID2? : Relation<int>,
    AreaGroupId? : Relation<int>,
    SchoolMask? : Relation<int>,
    RuneCostID? : Relation<int>,
    SpellMissileID? : Relation<int>,
    PowerDisplayID? : Relation<int>,
    DescriptionVariablesID? : Relation<int>,
    Difficulty? : Relation<int>,
    SpellDataHash? : Relation<int>,
}

export class spell_dbcTable extends SqlTable<
    spell_dbcCreator,
    spell_dbcQuery,
    spell_dbcRow> {
    add(Id : int, c? : spell_dbcCreator) : spell_dbcRow {
        const first = this.first();
        if(first) return first.clone(Id,c)
        else return this.rowCreator(this, {}).clone(Id,c)
    }
}

export const SQL_spell_dbc = new spell_dbcTable(
    'spell_dbc',
    (table, obj)=>new spell_dbcRow(table, obj))
