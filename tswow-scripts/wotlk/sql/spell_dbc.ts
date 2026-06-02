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
    get Effect1() {return new SQLCell<int, this>(this, 'Effect1')}
    get Effect2() {return new SQLCell<int, this>(this, 'Effect2')}
    get Effect3() {return new SQLCell<int, this>(this, 'Effect3')}
    get EffectDieSides1() {return new SQLCell<int, this>(this, 'EffectDieSides1')}
    get EffectDieSides2() {return new SQLCell<int, this>(this, 'EffectDieSides2')}
    get EffectDieSides3() {return new SQLCell<int, this>(this, 'EffectDieSides3')}
    get EffectRealPointsPerLevel1() {return new SQLCell<float, this>(this, 'EffectRealPointsPerLevel1')}
    get EffectRealPointsPerLevel2() {return new SQLCell<float, this>(this, 'EffectRealPointsPerLevel2')}
    get EffectRealPointsPerLevel3() {return new SQLCell<float, this>(this, 'EffectRealPointsPerLevel3')}
    get EffectBasePoints1() {return new SQLCell<int, this>(this, 'EffectBasePoints1')}
    get EffectBasePoints2() {return new SQLCell<int, this>(this, 'EffectBasePoints2')}
    get EffectBasePoints3() {return new SQLCell<int, this>(this, 'EffectBasePoints3')}
    get EffectMechanic1() {return new SQLCell<int, this>(this, 'EffectMechanic1')}
    get EffectMechanic2() {return new SQLCell<int, this>(this, 'EffectMechanic2')}
    get EffectMechanic3() {return new SQLCell<int, this>(this, 'EffectMechanic3')}
    get EffectImplicitTargetA1() {return new SQLCell<int, this>(this, 'EffectImplicitTargetA1')}
    get EffectImplicitTargetA2() {return new SQLCell<int, this>(this, 'EffectImplicitTargetA2')}
    get EffectImplicitTargetA3() {return new SQLCell<int, this>(this, 'EffectImplicitTargetA3')}
    get EffectImplicitTargetB1() {return new SQLCell<int, this>(this, 'EffectImplicitTargetB1')}
    get EffectImplicitTargetB2() {return new SQLCell<int, this>(this, 'EffectImplicitTargetB2')}
    get EffectImplicitTargetB3() {return new SQLCell<int, this>(this, 'EffectImplicitTargetB3')}
    get EffectRadiusIndex1() {return new SQLCell<int, this>(this, 'EffectRadiusIndex1')}
    get EffectRadiusIndex2() {return new SQLCell<int, this>(this, 'EffectRadiusIndex2')}
    get EffectRadiusIndex3() {return new SQLCell<int, this>(this, 'EffectRadiusIndex3')}
    get EffectApplyAuraName1() {return new SQLCell<int, this>(this, 'EffectApplyAuraName1')}
    get EffectApplyAuraName2() {return new SQLCell<int, this>(this, 'EffectApplyAuraName2')}
    get EffectApplyAuraName3() {return new SQLCell<int, this>(this, 'EffectApplyAuraName3')}
    get EffectAmplitude1() {return new SQLCell<int, this>(this, 'EffectAmplitude1')}
    get EffectAmplitude2() {return new SQLCell<int, this>(this, 'EffectAmplitude2')}
    get EffectAmplitude3() {return new SQLCell<int, this>(this, 'EffectAmplitude3')}
    get EffectMultipleValue1() {return new SQLCell<float, this>(this, 'EffectMultipleValue1')}
    get EffectMultipleValue2() {return new SQLCell<float, this>(this, 'EffectMultipleValue2')}
    get EffectMultipleValue3() {return new SQLCell<float, this>(this, 'EffectMultipleValue3')}
    get EffectItemType1() {return new SQLCell<int, this>(this, 'EffectItemType1')}
    get EffectItemType2() {return new SQLCell<int, this>(this, 'EffectItemType2')}
    get EffectItemType3() {return new SQLCell<int, this>(this, 'EffectItemType3')}
    get EffectMiscValue1() {return new SQLCell<int, this>(this, 'EffectMiscValue1')}
    get EffectMiscValue2() {return new SQLCell<int, this>(this, 'EffectMiscValue2')}
    get EffectMiscValue3() {return new SQLCell<int, this>(this, 'EffectMiscValue3')}
    get EffectMiscValueB1() {return new SQLCell<int, this>(this, 'EffectMiscValueB1')}
    get EffectMiscValueB2() {return new SQLCell<int, this>(this, 'EffectMiscValueB2')}
    get EffectMiscValueB3() {return new SQLCell<int, this>(this, 'EffectMiscValueB3')}
    get EffectTriggerSpell1() {return new SQLCell<int, this>(this, 'EffectTriggerSpell1')}
    get EffectTriggerSpell2() {return new SQLCell<int, this>(this, 'EffectTriggerSpell2')}
    get EffectTriggerSpell3() {return new SQLCell<int, this>(this, 'EffectTriggerSpell3')}
    get EffectSpellClassMaskA1() {return new SQLCell<int, this>(this, 'EffectSpellClassMaskA1')}
    get EffectSpellClassMaskA2() {return new SQLCell<int, this>(this, 'EffectSpellClassMaskA2')}
    get EffectSpellClassMaskA3() {return new SQLCell<int, this>(this, 'EffectSpellClassMaskA3')}
    get EffectSpellClassMaskB1() {return new SQLCell<int, this>(this, 'EffectSpellClassMaskB1')}
    get EffectSpellClassMaskB2() {return new SQLCell<int, this>(this, 'EffectSpellClassMaskB2')}
    get EffectSpellClassMaskB3() {return new SQLCell<int, this>(this, 'EffectSpellClassMaskB3')}
    get EffectSpellClassMaskC1() {return new SQLCell<int, this>(this, 'EffectSpellClassMaskC1')}
    get EffectSpellClassMaskC2() {return new SQLCell<int, this>(this, 'EffectSpellClassMaskC2')}
    get EffectSpellClassMaskC3() {return new SQLCell<int, this>(this, 'EffectSpellClassMaskC3')}
    get DmgMultiplier1() {return new SQLCell<float, this>(this, 'DmgMultiplier1')}
    get DmgMultiplier2() {return new SQLCell<float, this>(this, 'DmgMultiplier2')}
    get DmgMultiplier3() {return new SQLCell<float, this>(this, 'DmgMultiplier3')}
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
    Effect1? : int,
    Effect2? : int,
    Effect3? : int,
    EffectDieSides1? : int,
    EffectDieSides2? : int,
    EffectDieSides3? : int,
    EffectRealPointsPerLevel1? : float,
    EffectRealPointsPerLevel2? : float,
    EffectRealPointsPerLevel3? : float,
    EffectBasePoints1? : int,
    EffectBasePoints2? : int,
    EffectBasePoints3? : int,
    EffectMechanic1? : int,
    EffectMechanic2? : int,
    EffectMechanic3? : int,
    EffectImplicitTargetA1? : int,
    EffectImplicitTargetA2? : int,
    EffectImplicitTargetA3? : int,
    EffectImplicitTargetB1? : int,
    EffectImplicitTargetB2? : int,
    EffectImplicitTargetB3? : int,
    EffectRadiusIndex1? : int,
    EffectRadiusIndex2? : int,
    EffectRadiusIndex3? : int,
    EffectApplyAuraName1? : int,
    EffectApplyAuraName2? : int,
    EffectApplyAuraName3? : int,
    EffectAmplitude1? : int,
    EffectAmplitude2? : int,
    EffectAmplitude3? : int,
    EffectMultipleValue1? : float,
    EffectMultipleValue2? : float,
    EffectMultipleValue3? : float,
    EffectItemType1? : int,
    EffectItemType2? : int,
    EffectItemType3? : int,
    EffectMiscValue1? : int,
    EffectMiscValue2? : int,
    EffectMiscValue3? : int,
    EffectMiscValueB1? : int,
    EffectMiscValueB2? : int,
    EffectMiscValueB3? : int,
    EffectTriggerSpell1? : int,
    EffectTriggerSpell2? : int,
    EffectTriggerSpell3? : int,
    EffectSpellClassMaskA1? : int,
    EffectSpellClassMaskA2? : int,
    EffectSpellClassMaskA3? : int,
    EffectSpellClassMaskB1? : int,
    EffectSpellClassMaskB2? : int,
    EffectSpellClassMaskB3? : int,
    EffectSpellClassMaskC1? : int,
    EffectSpellClassMaskC2? : int,
    EffectSpellClassMaskC3? : int,
    DmgMultiplier1? : float,
    DmgMultiplier2? : float,
    DmgMultiplier3? : float,
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
    Effect1? : Relation<int>,
    Effect2? : Relation<int>,
    Effect3? : Relation<int>,
    EffectDieSides1? : Relation<int>,
    EffectDieSides2? : Relation<int>,
    EffectDieSides3? : Relation<int>,
    EffectRealPointsPerLevel1? : Relation<float>,
    EffectRealPointsPerLevel2? : Relation<float>,
    EffectRealPointsPerLevel3? : Relation<float>,
    EffectBasePoints1? : Relation<int>,
    EffectBasePoints2? : Relation<int>,
    EffectBasePoints3? : Relation<int>,
    EffectMechanic1? : Relation<int>,
    EffectMechanic2? : Relation<int>,
    EffectMechanic3? : Relation<int>,
    EffectImplicitTargetA1? : Relation<int>,
    EffectImplicitTargetA2? : Relation<int>,
    EffectImplicitTargetA3? : Relation<int>,
    EffectImplicitTargetB1? : Relation<int>,
    EffectImplicitTargetB2? : Relation<int>,
    EffectImplicitTargetB3? : Relation<int>,
    EffectRadiusIndex1? : Relation<int>,
    EffectRadiusIndex2? : Relation<int>,
    EffectRadiusIndex3? : Relation<int>,
    EffectApplyAuraName1? : Relation<int>,
    EffectApplyAuraName2? : Relation<int>,
    EffectApplyAuraName3? : Relation<int>,
    EffectAmplitude1? : Relation<int>,
    EffectAmplitude2? : Relation<int>,
    EffectAmplitude3? : Relation<int>,
    EffectMultipleValue1? : Relation<float>,
    EffectMultipleValue2? : Relation<float>,
    EffectMultipleValue3? : Relation<float>,
    EffectItemType1? : Relation<int>,
    EffectItemType2? : Relation<int>,
    EffectItemType3? : Relation<int>,
    EffectMiscValue1? : Relation<int>,
    EffectMiscValue2? : Relation<int>,
    EffectMiscValue3? : Relation<int>,
    EffectMiscValueB1? : Relation<int>,
    EffectMiscValueB2? : Relation<int>,
    EffectMiscValueB3? : Relation<int>,
    EffectTriggerSpell1? : Relation<int>,
    EffectTriggerSpell2? : Relation<int>,
    EffectTriggerSpell3? : Relation<int>,
    EffectSpellClassMaskA1? : Relation<int>,
    EffectSpellClassMaskA2? : Relation<int>,
    EffectSpellClassMaskA3? : Relation<int>,
    EffectSpellClassMaskB1? : Relation<int>,
    EffectSpellClassMaskB2? : Relation<int>,
    EffectSpellClassMaskB3? : Relation<int>,
    EffectSpellClassMaskC1? : Relation<int>,
    EffectSpellClassMaskC2? : Relation<int>,
    EffectSpellClassMaskC3? : Relation<int>,
    DmgMultiplier1? : Relation<float>,
    DmgMultiplier2? : Relation<float>,
    DmgMultiplier3? : Relation<float>,
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
