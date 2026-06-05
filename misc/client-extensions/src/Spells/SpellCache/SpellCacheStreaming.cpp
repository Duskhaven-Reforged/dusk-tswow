#include <Spells/SpellCache/SpellCacheStreaming.h>

#include <ClientDetours.h>
#include <ClientData/GameEnums.h>
#include <ClientData/ObjectFields.h>
#include <ClientData/SharedDefines.h>
#include <ClientData/Spell.h>
#include <ClientData/System.h>
#include <ClientLua.h>
#include <ClientNetwork.h>
#include <CDBCMgr/CDBCMgr.h>
#include <CDBCMgr/CDBCDefs/Spell.h>
#include <CDBCMgr/CDBCDefs/SpellEffect.h>
#include <FrameXMLExtensions.h>
#include <Logger.h>
#include <Spells/ScriptedMissileVisuals.h>
#include <Util.h>

#include <algorithm>
#include <any>
#include <cmath>
#include <cstring>
#include <fstream>
#include <functional>
#include <intrin.h>
#include <memory>
#include <deque>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <Windows.h>

namespace
{
    constexpr opcode_t SPELL_CACHE_QUERY_OPCODE = 0x7A40;
    constexpr opcode_t SPELL_CACHE_RESPONSE_OPCODE = 0x7A41;
    constexpr uint32_t SPELL_CACHE_STREAM_VERSION = 3;
    constexpr uintptr_t SPELL_DB_ADDRESS = 0x00AD49D0;
    constexpr uintptr_t SPELL_ICON_DB_RECORDS_ADDRESS = 0x00AD48A4;
    constexpr uintptr_t SPELL_MISSILE_DB_RECORDS_ADDRESS = 0x00AD4934;
    constexpr uintptr_t SPELL_MISSILE_MOTION_DB_RECORDS_ADDRESS = 0x00AD4958;
    constexpr uintptr_t SPELL_VISUAL_EFFECT_NAME_DB_RECORDS_ADDRESS = 0x00AD4A30;
    constexpr uintptr_t SPELL_VISUAL_KIT_DB_RECORDS_ADDRESS = 0x00AD4A54;
    constexpr uintptr_t SPELL_VISUAL_DB_RECORDS_ADDRESS = 0x00AD4AC0;
    constexpr uintptr_t SKILL_LINE_ABILITY_NUM_ROWS_ADDRESS = 0x00AD45A0;
    constexpr uintptr_t SKILL_LINE_ABILITY_FIRST_ROW_ADDRESS = 0x00AD45B4;
    constexpr uintptr_t SKILL_LINE_DB_ADDRESS = 0x00AD45E0;
    constexpr uintptr_t SKILL_LINE_DB_RECORDS_ADDRESS = 0x00AD45F8;
    constexpr uintptr_t SPELL_DB_LOAD_SLOT_ADDRESS = 0x00634309;
    constexpr size_t SPELL_DB_LOAD_SLOT_SIZE = 17;
    constexpr uintptr_t WOW_CLIENT_DB_GET_ROW_ADDRESS = 0x008B7DA0;
    constexpr uintptr_t PENDING_SPELL_CAST_SPELLREC_LOOKUP_ADDRESS = 0x0080E5C4;
    constexpr uintptr_t PENDING_SPELL_CAST_SPELLREC_LOOKUP_FAIL_CONTINUE_ADDRESS = 0x0080E5CF;
    constexpr uintptr_t PENDING_SPELL_CAST_SPELLREC_LOOKUP_SUCCESS_CONTINUE_ADDRESS = 0x0080E624;
    constexpr size_t PENDING_SPELL_CAST_SPELLREC_LOOKUP_PATCH_SIZE = 11;
    constexpr uintptr_t ADD_KNOWN_SPELL_MAX_ID_GATE_ADDRESS = 0x006E7B19;
    constexpr uintptr_t KNOWN_SPELL_MAX_ID_PLUS_ONE_ADDRESS = 0x00BE8DB8;
    constexpr uintptr_t KNOWN_SPELL_BITSET_COUNT_ADDRESS = 0x00BE8DBC;
    constexpr uintptr_t KNOWN_SPELL_BITSET_CAPACITY_ADDRESS = 0x00BE8DC0;
    constexpr uintptr_t KNOWN_SPELL_BITSET_ADDRESS = 0x00BE8DC4;
    constexpr uintptr_t DEFENSE_SKILL_SPELL_ADDRESS = 0x00C9D33C;
    constexpr uintptr_t WEAPON_SKILL_SPELLS_ADDRESS = 0x00C9EB48;
    constexpr uintptr_t WEAPON_SKILL_SPELLS_ROWS_ADDRESS = 0x00C9EB50;
    constexpr uint32_t STREAMED_SPELL_ATTR0_PASSIVE = 0x40;
    constexpr uint32_t STREAMED_SPELL_ATTR0_HIDDEN_CLIENTSIDE = 0x80;
    constexpr uint32_t STREAMED_SPELL_ATTR0_TRADESPELL = 0x20;
    constexpr uint32_t STREAMED_SPELL_ATTR_EX_D_HIDE_FROM_SPELLBOOK = 0x8000;
    constexpr uint32_t SPELL_EFFECT_SKILL = 26;
    constexpr int32_t ITEM_CLASS_WEAPON = 2;
    constexpr uint32_t SPELL_CACHE_REQUEST_INTERVAL_MS = 250;
    constexpr uint32_t SPELL_CACHE_REQUESTS_PER_PUMP = 10;
    constexpr uint32_t SPELL_CACHE_REQUEST_TIMEOUT_MS = 3000;
    constexpr uint32_t ACTIONBAR_STREAMED_ROW_SCAN_INTERVAL_MS = 1000;
    constexpr bool SPELL_CACHE_ENABLE_UI_INTEGRATION = false;
    constexpr uint32_t SPELL_CACHE_MAX_REASONABLE_SPELL_ID = 1000000;
    constexpr uint32_t SKILL_LINE_ABILITY_MAX_REASONABLE_ROWS = 200000;
    constexpr uint32_t KNOWN_SPELL_UI_WORK_PER_PUMP = 1;
    constexpr uint32_t SPELLBOOK_MAX_SLOTS = 1024;
    constexpr uint32_t SPELLBOOK_MAX_TABS = 64;
    constexpr uint32_t SPELLBOOK_MAX_LANGUAGES = 256;
    constexpr uint32_t SPELLBOOK_MAX_STANCES = 64;
    constexpr uint32_t SPELL_EFFECT_SUMMON = 28;
    constexpr uint32_t SPELL_EFFECT_APPLY_AURA = 6;
    constexpr uint32_t SPELL_EFFECT_LANGUAGE = 39;
    constexpr uint32_t SPELL_EFFECT_TRADE_SKILL = 47;
    constexpr uint32_t SPELL_EFFECT_ATTACK = 78;
    constexpr uint32_t SPELL_AURA_MOUNTED = 78;
    constexpr uint32_t SPELL_AURA_MOD_SHAPESHIFT = 36;
    constexpr uint32_t SPELL_ATTR_EX_STANCE = 0x10;
    constexpr uintptr_t SUMMON_PROPERTIES_MIN_ID_ADDRESS = 0x00AD4B48;
    constexpr uintptr_t SUMMON_PROPERTIES_MAX_ID_ADDRESS = 0x00AD4B44;
    constexpr uintptr_t SUMMON_PROPERTIES_RECORDS_BY_ID_ADDRESS = 0x00AD4B58;
    constexpr size_t SUMMON_PROPERTIES_SLOT_OFFSET = 0x10;
    constexpr uint32_t SUMMON_PROPERTIES_SLOT_CRITTER = 5;
    constexpr uintptr_t PLAYER_SPELL_SLOT_MAP_ADDRESS = 0x00BE6D88;
    constexpr uintptr_t PLAYER_SPELL_SLOT_COUNT_ADDRESS = 0x00BE8D98;
    constexpr uintptr_t SPELLBOOK_VISIBLE_SLOT_MAP_ADDRESS = 0x00BE6D84;
    constexpr uintptr_t SPELLBOOK_VISIBLE_COUNT_ADDRESS = 0x00BE8D9C;
    constexpr uintptr_t SPELLBOOK_TAB_ARRAY_CAPACITY_ADDRESS = 0x00BE8DC8;
    constexpr uintptr_t SPELLBOOK_TAB_ALLOCATED_COUNT_ADDRESS = 0x00BE8DCC;
    constexpr uintptr_t SPELLBOOK_TAB_ARRAY_ADDRESS = 0x00BE8DD0;
    constexpr uintptr_t SPELLBOOK_TAB_ALLOCATION_CHUNK_ADDRESS = 0x00BE8DD4;
    constexpr uintptr_t SPELLBOOK_TAB_COUNT_ADDRESS = 0x00BE8DA0;
    constexpr uintptr_t LANGUAGE_CAPACITY_ADDRESS = 0x00BE8DD8;
    constexpr uintptr_t LANGUAGE_COUNT_ADDRESS = 0x00BE8DDC;
    constexpr uintptr_t LANGUAGE_SPELL_IDS_ADDRESS = 0x00BE8DE0;
    constexpr uintptr_t SHAPESHIFT_CAPACITY_ADDRESS = 0x00BE8E24;
    constexpr uintptr_t SHAPESHIFT_COUNT_ADDRESS = 0x00BE8E28;
    constexpr uintptr_t SHAPESHIFT_ROWS_ADDRESS = 0x00BE8E2C;
    constexpr uintptr_t SHAPESHIFT_ALLOCATION_CHUNK_ADDRESS = 0x00BE8E30;
    constexpr uintptr_t UNIT_MISSILE_TRAJECTORY_GUID_LOW_ADDRESS = 0x00CA0AA8;
    constexpr uintptr_t UNIT_MISSILE_TRAJECTORY_GUID_HIGH_ADDRESS = 0x00CA0AAC;
    constexpr uintptr_t UNIT_MISSILE_TRAJECTORY_LAST_SPELL_ADDRESS = 0x00CA0AB0;
    constexpr uintptr_t UNIT_MISSILE_TRAJECTORY_STATE_ADDRESS = 0x00CA0AC0;
    constexpr uintptr_t UNIT_MISSILE_TRAJECTORY_MODE_ADDRESS = 0x00CA0AC4;
    constexpr size_t UNIT_FIELDS_POINTER_OFFSET = 208;
    constexpr size_t UNIT_FIELD_CRITTER_VALUE_OFFSET = 16;
    constexpr size_t UNIT_AURA_INLINE_OFFSET = 3152;
    constexpr size_t UNIT_AURA_HEAP_POINTER_OFFSET = 3160;
    constexpr size_t UNIT_AURA_LENGTH_OFFSET = 3536;
    constexpr size_t UNIT_AURA_STRIDE = 24;
    constexpr size_t UNIT_AURA_SPELL_ID_OFFSET = 8;
    constexpr size_t UNIT_AURA_FLAGS_OFFSET = 12;
    constexpr size_t UNIT_PENDING_SHAPESHIFT_RESET_OFFSET = 2548;
    constexpr size_t UNIT_MOVEMENT_FLAGS_OFFSET = 0x0A30;
    constexpr size_t UNIT_FIELD_BYTES_2_SHAPESHIFT_FORM_BYTE_OFFSET = 467;
    constexpr uint32_t UNIT_MOVEMENT_FLAG_SWIMMING_OR_UNDERWATER = 0x00200000;
    constexpr uint32_t STREAMED_TYPEMASK_PLAYER = 0x10;

    std::unordered_map<uint32_t, uint32_t> PendingSpellRequests;
    std::deque<uint32_t> QueuedSpellRequests;
    std::unordered_set<uint32_t> QueuedSpellRequestIds;
    std::unordered_map<uint32_t, uint32_t> QueuedSpellRequestHashes;
    std::unordered_set<uint32_t> StreamedSpellRows;
    std::unordered_set<uint32_t> ForcedStreamedSpellRows;
    std::unordered_map<uint32_t, uint32_t> StreamedActionBarEntries;
    std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> StreamedSpellSkillLines;
    std::unordered_set<uint32_t> MissingSpellRows;
    std::unordered_set<uint32_t> KnownSpellRows;
    std::unordered_set<uint32_t> ReadyKnownSpellUiRows;
    std::deque<uint32_t> ReadyKnownSpellUiQueue;
    std::unordered_map<uint32_t, uint32_t> AppliedStreamedLanguageSlots;
    std::unordered_map<uint64_t, std::unique_ptr<char[]>> SpellStringStorage;
    std::unordered_map<uint32_t, std::unique_ptr<ClientData::SpellRow>> NativeSpellDbRows;
    std::unordered_map<uint32_t, uint32_t> StreamedSpellDbLookupHits;
    std::unordered_map<uint32_t, uintptr_t> StreamedSpellDbLookupLastCaller;
    std::unordered_map<uint32_t, uint32_t> StreamedSpellDbVisualLookupHits;
    std::unordered_map<uint32_t, uintptr_t> StreamedSpellDbVisualLookupLastCaller;
    std::unordered_map<uint32_t, uint32_t> StreamedSpellDbActionbarLookupHits;
    std::unordered_map<uint32_t, uint32_t> StreamedSpellDbSpellbookLookupHits;
    std::unordered_map<uint32_t, uint32_t> SpellVisualKitPlayHits;
    std::unordered_map<uint32_t, uint32_t> SpellVisualKitPlayLastCaller;
    std::unordered_map<uint32_t, uint32_t> SpellVisualKitPlayLastKitId;
    std::unordered_map<uint32_t, uint32_t> SpellVisualKitPlayLastKitType;
    std::unordered_map<uint32_t, uint32_t> SpellVisualKitPlayLastEffectMask;
    std::unordered_map<uint32_t, uint32_t> StreamedSpellVisualRowFallbackHits;
    std::unordered_map<uint32_t, uint32_t> StreamedSpellVisualRowFallbackLastVisual;
    std::unordered_map<uint32_t, uintptr_t> StreamedSpellVisualRowFallbackLastCaller;
    std::unordered_map<uint32_t, uint32_t> StreamedUnitAppropriateVisualFallbackHits;
    std::unordered_map<uint32_t, uint32_t> StreamedUnitAppropriateVisualFallbackLastVisual;
    std::unordered_map<uint32_t, uintptr_t> StreamedUnitAppropriateVisualFallbackLastCaller;
    std::unordered_map<uint32_t, uint32_t> StreamedDirectCastKitPlayHits;
    std::unordered_map<uint32_t, uint32_t> StreamedDirectCastKitLastVisual;
    std::unordered_map<uint32_t, uint32_t> StreamedDirectCastKitLastKit;
    std::unordered_map<uint32_t, uint32_t> StreamedDirectImpactKitPlayHits;
    std::unordered_map<uint32_t, uint32_t> StreamedDirectImpactKitLastVisual;
    std::unordered_map<uint32_t, uint32_t> StreamedDirectImpactKitLastKit;
    std::unordered_map<uint32_t, uint32_t> StreamedDirectImpactKitAttempts;
    std::unordered_map<uint32_t, uint64_t> StreamedDirectImpactKitLastTarget;
    std::unordered_map<uint32_t, uint32_t> StreamedDirectImpactKitNoTarget;
    std::unordered_map<uint32_t, uint32_t> StreamedDirectImpactKitNoKit;
    std::unordered_map<uint32_t, uint32_t> NativeMissileCreateHits;
    std::unordered_map<uint32_t, uintptr_t> NativeMissileCreateLastCaller;
    std::unordered_map<uint32_t, uint32_t> NativeMissileCreateLastVisual;
    std::unordered_map<uint32_t, uint32_t> NativeMissileCreateLastVisualModel;
    std::unordered_map<uint32_t, uint32_t> NativeMissileCreateLastMissileMotion;
    std::unordered_map<uint32_t, uint32_t> NativeMissileCreateLastSpeedBits;
    std::unordered_map<uint32_t, uint32_t> StreamedVisualIdToSpellId;
    std::unordered_map<uint32_t, uint32_t> StreamedVisualKitIdToSpellId;
    std::unordered_map<uint32_t, uint32_t> StreamedVisualPipelineHits;
    std::unordered_map<uint32_t, uint32_t> StreamedVisualPipelineLastStage;
    std::unordered_map<uint32_t, uintptr_t> StreamedVisualPipelineLastCaller;
    std::unordered_map<uint32_t, uint32_t> StreamedVisualPipelineLastVisual;
    std::unordered_map<uint32_t, uint32_t> StreamedVisualPipelineLastKit;
    std::unordered_map<uint32_t, uint32_t> StreamedVisualPipelineLastKitType;
    std::unordered_map<uint32_t, uint32_t> StreamedVisualPipelineLastResult;
    std::unordered_map<uint32_t, uint32_t> StreamedVisualPipelineLastMissileModel;
    std::unordered_map<uint32_t, uint32_t> StreamedVisualPipelineLastMissileMotion;
    uint32_t StreamedMissileTrajectoryArmedSpellId = 0;
    uint32_t StreamedMissileTrajectoryGlobalLogCount = 0;
    uint32_t StreamedMissileTrajectoryGlobalReturnLogCount = 0;
    uint32_t NativeVisualChainCheckHits = 0;
    uint32_t NativeVisualChainCheckLastVisual = 0;
    uint32_t NativeVisualChainCheckLastCastKit = 0;
    uint32_t NativeVisualChainCheckLastImpactKit = 0;
    uint32_t NativeVisualChainCheckLastResult = 0;
    uintptr_t NativeVisualChainCheckLastCaller = 0;
    uint32_t NativeVisualDelayHits = 0;
    uint32_t NativeVisualDelayLastArg2 = 0;
    uint32_t NativeVisualDelayLastKitId = 0;
    uint32_t NativeVisualDelayLastDelay = 0;
    uint32_t NativeVisualDelayLastArg4 = 0;
    uint32_t NativeVisualDelayLastArg8 = 0;
    uint32_t NativeVisualDelayLastArg9 = 0;
    uint32_t NativeVisualDelayLastArg10 = 0;
    uint32_t NativeVisualDelayLastArg11 = 0;
    uint32_t NativeVisualDelayLastArg12 = 0;
    uintptr_t NativeVisualDelayLastCaller = 0;
    std::unordered_map<uint64_t, std::unique_ptr<char[]>> StreamedVisualKitNoAnimationRows;
    uint32_t SpellVisualKitPlayTotalHits = 0;
    uint32_t SpellVisualKitPlayNullSpellHits = 0;
    uint32_t SpellVisualKitPlayNullKitHits = 0;
    uint32_t SpellVisualKitPlayLastSpellId = 0;
    uint32_t SpellVisualKitPlayLastRawKitId = 0;
    uint32_t SpellVisualKitPlayLastRawKitType = 0;
    uint32_t SpellVisualKitPlayLastRawCaller = 0;
    std::unique_ptr<ClientData::SpellRow*[]> NativeSpellDbRecordsById;
    std::vector<std::function<void(uint32_t)>> SpellCachedCallbacks;
    uint32_t NextSpellCacheRequestMs = 0;
    uint64_t KnownSpellRowsPlayerGuid = 0;
    uint64_t StreamedActionBarPlayerGuid = 0;
    uint64_t StreamedActionBarPrewarmPlayerGuid = 0;
    uint32_t StreamedActionBarCursorSpellId = 0;
    uint32_t StreamedActionBarCursorSourceSlot = 0xFFFFFFFF;
    uint32_t NextActionBarStreamedRowScanMs = 0;
    bool StreamedActionBarLoadedForPlayer = false;
    bool StreamedActionBarCursorActive = false;
    bool StreamedSpellSkillLinesLoaded = false;
    bool NativeSpellDbLoadPatched = false;
    bool PendingSpellCastSpellRecLookupPatched = false;
    bool KnownSpellMaxIdGatePatched = false;
    bool NativeActionBarRefreshPending = false;
    int32_t ActionBarUpdateCooldownEventId = -2;
    int32_t ActionBarUpdateStateEventId = -2;
    int32_t StopAutorepeatSpellEventId = -2;
    uint32_t SpellCacheRequestDebugLogCount = 0;
    uint32_t KnownSpellNativeFallthroughDebugLogCount = 0;
    uint32_t PendingKnownSpellDebugLogCount = 0;
    uint32_t SkillLineAbilityIndexDebugLogCount = 0;

    struct PendingKnownSpellAdd
    {
        void* player;
        uint32_t spellId;
        int32_t spellCategory;
        int32_t learned;
        int32_t addToSpellbook;
    };

    struct KnownSpellbookEntry
    {
        uint32_t spellId;
        uint32_t tabKey;
        uint32_t spellLevel;
        uint32_t stanceBarOrder;
        int32_t spellCategory;
        int32_t languageIndex;
        bool addToSpellbook;
        bool isLanguage;
        bool isStance;
        bool isSkillGrant;
        bool isHiddenClient;
        bool isTradeskill;
        bool isNativeHiddenSpellbook;
        bool hasName;
        bool visibleInSpellbook;
    };

    struct KnownSpellbookTab
    {
        uint32_t tabKey;
        uint32_t offset;
        uint32_t count;
        std::string name;
        std::string icon;
    };

    struct NativeShapeshiftEntry
    {
        uint32_t spellId;
        uint8_t usable;
        uint8_t lacksPower;
        uint8_t padding[2];
    };

    struct StreamedActionBarNativeSnapshot
    {
        uintptr_t actionButton;
        uintptr_t spellId;
    };

    std::unordered_map<uint32_t, StreamedActionBarNativeSnapshot> StreamedActionBarNativeSnapshots;

    struct SpellVisualDebugRow
    {
        uint32_t id;
        uint32_t precastKit;
        uint32_t castKit;
        uint32_t impactKit;
        uint32_t stateKit;
        uint32_t stateDoneKit;
        uint32_t channelKit;
        uint32_t hasMissile;
        uint32_t missileModel;
        uint32_t missilePathType;
        uint32_t missileDestinationAttachment;
        uint32_t missileSound;
        uint32_t animEventSoundID;
        uint32_t flags;
        uint32_t casterImpactKit;
        uint32_t targetImpactKit;
        uint32_t missileAttachment;
        uint32_t missileFollowGroundHeight;
        uint32_t missileFollowGroundDropSpeed;
        uint32_t missileFollowGroundApproach;
        uint32_t missileFollowGroundFlags;
        uint32_t missileMotion;
        uint32_t missileTargetingKit;
    };

    struct SpellVisualKitDebugRow
    {
        uint32_t id;
        uint32_t startAnimID;
        uint32_t animID;
        uint32_t headEffect;
        uint32_t chestEffect;
        uint32_t baseEffect;
        uint32_t leftHandEffect;
        uint32_t rightHandEffect;
        uint32_t breathEffect;
        uint32_t leftWeaponEffect;
        uint32_t rightWeaponEffect;
        uint32_t specialEffect[3];
        uint32_t worldEffect;
        uint32_t soundID;
        uint32_t shakeID;
    };

    struct SpellVisualEffectNameDebugRow
    {
        uint32_t id;
        uint32_t name;
        char* fileName;
        uint32_t areaEffectSize;
        float scale;
    };

    bool HasReasonableSpellId(uint32_t spellId);
    bool IsPlausibleClientPointer(void const* pointer);
    bool ForceActionBarStreamedSpell(uint32_t spellId, bool requestOnMiss);

    enum StreamedVisualPipelineStage : uint32_t
    {
        STREAMED_VISUAL_STAGE_STACK_ROW = 1,
        STREAMED_VISUAL_STAGE_GET_VISUAL_ROW = 2,
        STREAMED_VISUAL_STAGE_GET_APPROPRIATE_VISUAL = 3,
        STREAMED_VISUAL_STAGE_CHAIN_CHECK = 4,
        STREAMED_VISUAL_STAGE_DELAY_KIT = 5,
        STREAMED_VISUAL_STAGE_PLAY_KIT = 6,
        STREAMED_VISUAL_STAGE_MISSILE_ENTRY = 7,
    };

    void RecordStreamedVisualPipelineStage(
        uint32_t spellId,
        StreamedVisualPipelineStage stage,
        uintptr_t caller,
        uint32_t visualId = 0,
        uint32_t kitId = 0,
        uint32_t kitType = 0,
        uint32_t result = 0,
        uint32_t missileModel = 0,
        uint32_t missileMotion = 0)
    {
        if (!HasReasonableSpellId(spellId))
            return;

        uint32_t& hits = StreamedVisualPipelineHits[spellId];
        if (hits < 0xFFFFFFFF)
            ++hits;

        StreamedVisualPipelineLastStage[spellId] = stage;
        StreamedVisualPipelineLastCaller[spellId] = caller;
        StreamedVisualPipelineLastVisual[spellId] = visualId;
        StreamedVisualPipelineLastKit[spellId] = kitId;
        StreamedVisualPipelineLastKitType[spellId] = kitType;
        StreamedVisualPipelineLastResult[spellId] = result;
        StreamedVisualPipelineLastMissileModel[spellId] = missileModel;
        StreamedVisualPipelineLastMissileMotion[spellId] = missileMotion;

        if (visualId)
        {
            StreamedVisualIdToSpellId[visualId] = spellId;
            auto const* visual = reinterpret_cast<SpellVisualDebugRow const*>(
                ClientDB::GetRow(reinterpret_cast<void*>(SPELL_VISUAL_DB_RECORDS_ADDRESS), visualId));
            if (visual)
            {
                uint32_t const kits[] =
                {
                    visual->precastKit,
                    visual->castKit,
                    visual->impactKit,
                    visual->stateKit,
                    visual->stateDoneKit,
                    visual->channelKit,
                    visual->casterImpactKit,
                    visual->targetImpactKit,
                    visual->missileTargetingKit,
                };
                for (uint32_t knownKit : kits)
                {
                    if (knownKit)
                        StreamedVisualKitIdToSpellId[knownKit] = spellId;
                }
            }
        }
        if (kitId)
            StreamedVisualKitIdToSpellId[kitId] = spellId;

        static std::unordered_set<uint64_t> logged;
        uint64_t const key =
            (static_cast<uint64_t>(spellId) << 32)
            ^ (static_cast<uint64_t>(stage) << 24)
            ^ static_cast<uint64_t>(caller & 0xFFFFFFu);
        if (logged.size() < 160 && logged.insert(key).second)
        {
            LOG_INFO << "Streamed visual pipeline stage"
                << "spell" << spellId
                << "stage" << static_cast<uint32_t>(stage)
                << "caller" << caller
                << "visual" << visualId
                << "kit" << kitId
                << "type" << kitType
                << "result" << result
                << "missileModel" << missileModel
                << "missileMotion" << missileMotion;
        }
    }

    std::vector<PendingKnownSpellAdd> PendingKnownSpellAdds;
    std::deque<PendingKnownSpellAdd> NativeKnownSpellReplayQueue;
    std::unordered_set<uint32_t> NativeSpellbookAppliedRows;
    std::unordered_map<uint32_t, PendingKnownSpellAdd> ReadyKnownSpellUiRecords;
    std::unordered_map<uint32_t, KnownSpellbookEntry> KnownSpellbookEntries;
    std::vector<uint32_t> KnownSpellbookOrder;
    std::vector<KnownSpellbookTab> KnownSpellbookTabs;
    std::vector<KnownSpellbookEntry> CachedStreamedStances;
    std::unordered_map<uint32_t, uint32_t> SkillLineAbilitySpellTabs;
    std::unordered_map<uint32_t, bool> SkillLineCanLinkCache;
    bool KnownSpellbookModelDirty = false;
    uint32_t KnownSpellbookRevision = 1;
    uint32_t PublishedKnownSpellbookRevision = 1;
    uint32_t CachedStreamedStanceRevision = 0;
    bool SkillLineAbilitySpellTabsLoaded = false;
    bool SkillLineAbilitySpellTabsLoadAttempted = false;

    CLIENT_FUNCTION(SMemAlloc_SpellCache, 0x0076E540, __cdecl, void*, (uint32_t, char const*, int32_t, uint32_t))
    CLIENT_FUNCTION(ResizeClientUIntArray_SpellCache, 0x004670D0, __thiscall, void, (void*, uint32_t))
    CLIENT_FUNCTION(GetWeaponSubclassCount_SpellCache, 0x006D3010, __cdecl, int, (uint32_t*))
    CLIENT_FUNCTION(CGUnit_C__GetPredictedHealth_SpellCache, 0x0071C2C0, __thiscall, uint32_t, (void*))
    CLIENT_FUNCTION(CGameUI__Signal_EVENT_LANGUAGE_LIST_CHANGED_SpellCache, 0x004FB530, __cdecl, void, ())
    CLIENT_FUNCTION(CGActionBar__IsSpell_SpellCache, 0x005A7860, __cdecl, bool, (uint32_t))
    CLIENT_FUNCTION(CGActionBar__GetSpell_SpellCache, 0x005A8C30, __cdecl, uint32_t, (uint32_t, uint32_t*))
    CLIENT_FUNCTION(CGActionBar__UpdateUsable_SpellCache, 0x005AA470, __cdecl, void, ())
    CLIENT_FUNCTION(CGActionBar__GetCooldown_SpellCache, 0x005A8E40, __cdecl, void, (uint32_t, uint32_t*, uint32_t*, uint32_t*))
    CLIENT_FUNCTION(CGSpellBook__SetCursorSpell_StreamedActionBar, 0x00520960, __cdecl, void, (uint32_t))
    CLIENT_FUNCTION(CGGameUI__GetCursorSpell_StreamedActionBar, 0x005136C0, __cdecl, uint32_t, ())
    CLIENT_FUNCTION(CGGameUI__ClearCursor_StreamedActionBar, 0x00519280, __cdecl, void, (int32_t, int32_t))
    CLIENT_FUNCTION(CGActionBar__RemoveAction_StreamedActionBar, 0x005AAA90, __cdecl, void, (uint32_t))
    CLIENT_FUNCTION(Spell_C__GetSpellCooldown_SpellCache, 0x00809000, __cdecl, void, (uint32_t, uint32_t, uint32_t*, uint32_t*, uint32_t*))
    CLIENT_FUNCTION(Spell_C_CastActionSpell_StreamedActionBar, 0x0080DA80, __cdecl, void, (uint32_t, uint32_t, uint64_t))
    CLIENT_FUNCTION(Script_CastSpellByID_StreamedActionBar, 0x0053E060, __cdecl, int, (lua_State*))
    CLIENT_FUNCTION(InitializeRawClientPacket_SpellCache, 0x00401050, __thiscall, void, (void*))
    CLIENT_FUNCTION(FinalizeRawClientPacket_SpellCache, 0x00401130, __thiscall, void, (void*))
    CLIENT_FUNCTION(SendRawClientPacket_SpellCache, 0x006B0B50, __cdecl, void, (void*))
    CLIENT_FUNCTION(CDataStore_GetUInt8_SpellCache, 0x0047B340, __thiscall, void, (void*, uint8_t*))
    CLIENT_FUNCTION(CDataStore_GetUInt32_SpellCache, 0x0047B3C0, __thiscall, void, (void*, uint32_t*))
    CLIENT_FUNCTION(CDataStore_GetUInt64_SpellCache, 0x0047B400, __thiscall, void, (void*, uint64_t*))
    CLIENT_FUNCTION(SpellHistory_AddHistory_SpellCache, 0x00805230, __thiscall, void,
        (void*, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint8_t, uint32_t, uint32_t))
    CLIENT_FUNCTION(Spell_C_HasCooldownOnEvent_SpellCache, 0x00801220, __cdecl, bool, (ClientData::SpellRow*, uint64_t))
    CLIENT_FUNCTION(Spell_C_CancelSpellByID_ActionState_SpellCache, 0x00805F30, __cdecl, bool, (uint32_t))
    CLIENT_FUNCTION(SpellRec_GetModifiedStats_SpellCache, 0x007FD970, __cdecl, bool, (ClientData::SpellRow*, uint32_t, int32_t*, int32_t*))
    CLIENT_FUNCTION(AdjustCategoryRecoveryTime_SpellCache, 0x007FEF60, __cdecl, void, (uint32_t*, ClientData::SpellRow*, uint32_t))
    CLIENT_FUNCTION(CGSpellBook_UpdateCooldowns_SpellCache, 0x0053BAC0, __cdecl, void, ())
    CLIENT_FUNCTION(CGSpellBook_AddKnownSpell_SpellCache, 0x00542030, __cdecl, void, (uint32_t, int32_t, uint32_t))
    CLIENT_FUNCTION(CGameUI_SignalEventBagUpdateCooldown_SpellCache, 0x005D6F10, __cdecl, void, ())
    CLIENT_FUNCTION(CGameUI_SignalEventPetBarUpdateCooldown_SpellCache, 0x005D3090, __cdecl, void, ())
    CLIENT_FUNCTION(GetSpellVisualDetailMode_SpellCache, 0x007F3A60, __cdecl, int, ())
    CLIENT_FUNCTION(CGObject_C__PlaySpellVisualKit_Direct_SpellCache, 0x00745230, __thiscall, void*, (void*, uint32_t*))
    CLIENT_FUNCTION(ClntObjMgrGetActivePlayer_ActionState_SpellCache, 0x004D3790, __cdecl, uint64_t, ())
    CLIENT_FUNCTION(ClntObjMgrObjectPtr_ActionState_SpellCache, 0x004D4DB0, __cdecl, void*, (uint64_t, uint32_t))
    CLIENT_FUNCTION(CGPlayer_C_GetCreatureFamilySkillLineBySpellID_ActionState_SpellCache, 0x0071A670, __thiscall, void*, (void*, uint32_t))
    CLIENT_FUNCTION(SkillLineRaceClassInfoLookup_SpellCache, 0x00810ED0, __cdecl, void*, (uint8_t, uint8_t, uint32_t))
    CLIENT_FUNCTION(CGUnit_C_GetAuraCount_ActionState_SpellCache, 0x004F8850, __thiscall, uint32_t, (void*))
    CLIENT_FUNCTION(CGUnit_C__IsSpellKnown_ActionbarStream_SpellCache, 0x007260E0, __thiscall, bool, (void*, uint32_t))
    CLIENT_FUNCTION(IsEquipmentSetKnown_ActionbarStream_SpellCache, 0x005AE4F0, __cdecl, bool, (uint32_t))

    struct RawClientPacket_SpellCache
    {
        uint32_t padding;
        uint8_t* buffer;
        uint32_t base;
        uint32_t alloc;
        uint32_t size;
        uint32_t read;
    };

    uint32_t ReadU32(CustomPacketRead* packet) { return packet->Read<uint32_t>(0); }
    int32_t ReadI32(CustomPacketRead* packet) { return packet->Read<int32_t>(0); }
    float ReadF32(CustomPacketRead* packet) { return packet->Read<float>(0.0f); }

    void RebuildKnownSpellbookOrder();
    std::vector<KnownSpellbookEntry> const& GetCachedStreamedStances();

    void MarkKnownSpellbookModelDirty()
    {
        KnownSpellbookModelDirty = true;
        if (KnownSpellbookRevision < 0xFFFFFFFFu)
            ++KnownSpellbookRevision;
        else
            KnownSpellbookRevision = 1;
    }
    void RecordStreamedSpellDbLookup(uint32_t spellId, ClientData::SpellRow const& row, void* returnAddress);
    void* GetNoAnimationVisualKit(uint32_t spellId, uint32_t kitId, void* kit);

    extern "C" __declspec(naked) void CGActionBar_SetAction_StreamedActionBar(uint32_t slot, uint32_t action, uint32_t notify, uint32_t save)
    {
        __asm
        {
            push ebx
            mov eax, [esp + 8]
            mov ecx, [esp + 0Ch]
            mov edx, [esp + 10h]
            xor ebx, ebx
            push dword ptr [esp + 14h]
            push edx
            push ecx
            push eax
            mov eax, 005AAE80h
            call eax
            add esp, 10h
            pop ebx
            ret
        }
    }

    uint64_t SpellStringKey(uint32_t spellId, uint32_t fieldIndex)
    {
        return (static_cast<uint64_t>(spellId) << 32) | fieldIndex;
    }

    char* ReadStableString(CustomPacketRead* packet, uint32_t spellId, uint32_t fieldIndex)
    {
        std::string value = packet->ReadString("");
        auto stableValue = std::make_unique<char[]>(value.size() + 1);
        std::memcpy(stableValue.get(), value.c_str(), value.size() + 1);
        char* raw = stableValue.get();
        SpellStringStorage[SpellStringKey(spellId, fieldIndex)] = std::move(stableValue);
        return raw;
    }

    bool HasValidPowerType(ClientData::SpellRow const& spell)
    {
        int32_t const powerType = static_cast<int32_t>(spell.m_powerType);
        return powerType < 0 || powerType <= static_cast<int32_t>(ClientData::POWER_RUNIC_POWER);
    }

    bool HasValidPowerType(int32_t powerType)
    {
        return powerType >= -2 && powerType <= static_cast<int32_t>(ClientData::POWER_RUNIC_POWER);
    }

    bool HasReasonableSpellId(uint32_t spellId)
    {
        return spellId != 0 && spellId <= SPELL_CACHE_MAX_REASONABLE_SPELL_ID;
    }

    bool IsPlausibleClientPointer(void const* pointer)
    {
        uintptr_t const value = reinterpret_cast<uintptr_t>(pointer);
        return value >= 0x01000000 && value < 0x80000000;
    }

    bool IsReadableMemory(void const* pointer, size_t minBytes)
    {
        if (!pointer || minBytes == 0)
            return false;

        MEMORY_BASIC_INFORMATION mbi = {};
        if (!VirtualQuery(pointer, &mbi, sizeof(mbi)))
            return false;
        if (mbi.State != MEM_COMMIT)
            return false;
        if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))
            return false;

        auto const* begin = reinterpret_cast<uint8_t const*>(pointer);
        auto const* regionBegin = reinterpret_cast<uint8_t const*>(mbi.BaseAddress);
        auto const* regionEnd = regionBegin + mbi.RegionSize;
        return begin >= regionBegin && begin + minBytes <= regionEnd;
    }

    bool SummonPropertySlotIsCritter(int32_t summonPropertyId)
    {
        auto const* minId = reinterpret_cast<int32_t const*>(SUMMON_PROPERTIES_MIN_ID_ADDRESS);
        auto const* maxId = reinterpret_cast<int32_t const*>(SUMMON_PROPERTIES_MAX_ID_ADDRESS);
        auto const* recordsByIdPointer =
            reinterpret_cast<void const* const* const*>(SUMMON_PROPERTIES_RECORDS_BY_ID_ADDRESS);
        if (!IsReadableMemory(minId, sizeof(*minId))
            || !IsReadableMemory(maxId, sizeof(*maxId))
            || !IsReadableMemory(recordsByIdPointer, sizeof(*recordsByIdPointer)))
            return false;

        if (summonPropertyId < *minId || summonPropertyId > *maxId)
            return false;

        void const* const* recordsById = *recordsByIdPointer;
        uint32_t const offset = static_cast<uint32_t>(summonPropertyId - *minId);
        if (!IsReadableMemory(recordsById + offset, sizeof(void const*)))
            return false;

        void const* row = recordsById[offset];
        auto const* slot = reinterpret_cast<uint32_t const*>(
            reinterpret_cast<uint8_t const*>(row) + SUMMON_PROPERTIES_SLOT_OFFSET);
        return IsReadableMemory(slot, sizeof(*slot)) && *slot == SUMMON_PROPERTIES_SLOT_CRITTER;
    }

    bool StreamedSpellHasEffect(uint32_t spellId, uint32_t effectId)
    {
        return SpellCacheStreaming::HasSpellEffect(spellId, effectId);
    }

    bool StreamedSpellHasAura(uint32_t spellId, uint32_t auraId)
    {
        return SpellCacheStreaming::HasSpellAura(spellId, auraId);
    }

    bool StreamedSpellWillSummonCritter(uint32_t spellId)
    {
        bool found = false;
        SpellCacheStreaming::ForEachSpellEffect(spellId, [&](const SpellEffectCacheRow& effect) {
            if (effect.effectApplyAuraName == SPELL_AURA_MOUNTED)
            {
                found = true;
                return false;
            }

            if (effect.effect == SPELL_EFFECT_SUMMON
                && SummonPropertySlotIsCritter(effect.effectMiscValueB))
            {
                found = true;
                return false;
            }

            return true;
        });

        return found;
    }

    uint32_t GetUnitAuraCountSafe(void* unit)
    {
        if (!unit)
            return 0;

        uint32_t const count = CGUnit_C_GetAuraCount_ActionState_SpellCache(unit);
        return (std::min)(count, 256u);
    }

    bool TryGetUnitAuraEntry(void* unit, uint32_t auraIndex, uint32_t& spellId, uint32_t& flags)
    {
        spellId = 0;
        flags = 0;
        if (!unit)
            return false;

        auto const* length = reinterpret_cast<int32_t const*>(
            reinterpret_cast<uint8_t const*>(unit) + UNIT_AURA_LENGTH_OFFSET);
        if (!IsReadableMemory(length, sizeof(*length)))
            return false;

        uint8_t const* entry = nullptr;
        if (*length == -1)
        {
            auto const* heapAuraBase = reinterpret_cast<uintptr_t const*>(
                reinterpret_cast<uint8_t const*>(unit) + UNIT_AURA_HEAP_POINTER_OFFSET);
            if (!IsReadableMemory(heapAuraBase, sizeof(*heapAuraBase)) || !*heapAuraBase)
                return false;
            entry = reinterpret_cast<uint8_t const*>(*heapAuraBase) + auraIndex * UNIT_AURA_STRIDE;
        }
        else
        {
            entry = reinterpret_cast<uint8_t const*>(unit) + UNIT_AURA_INLINE_OFFSET + auraIndex * UNIT_AURA_STRIDE;
        }

        if (!IsReadableMemory(entry, UNIT_AURA_FLAGS_OFFSET + sizeof(uint32_t)))
            return false;

        spellId = *reinterpret_cast<uint32_t const*>(entry + UNIT_AURA_SPELL_ID_OFFSET);
        flags = *reinterpret_cast<uint32_t const*>(entry + UNIT_AURA_FLAGS_OFFSET);
        return true;
    }

    uint64_t GetUnitCritterGuidSafe(void* unit)
    {
        if (!unit)
            return 0;

        auto const* fieldsPtr = reinterpret_cast<uintptr_t const*>(
            reinterpret_cast<uint8_t const*>(unit) + UNIT_FIELDS_POINTER_OFFSET);
        if (!IsReadableMemory(fieldsPtr, sizeof(*fieldsPtr)) || !*fieldsPtr)
            return 0;

        auto const* critterGuid = reinterpret_cast<uint64_t const*>(
            reinterpret_cast<uint8_t const*>(*fieldsPtr) + UNIT_FIELD_CRITTER_VALUE_OFFSET);
        if (!IsReadableMemory(critterGuid, sizeof(*critterGuid)))
            return 0;

        return *critterGuid;
    }

    bool ReadUnitFieldU32(void* unit, uint32_t fieldIndex, uint32_t& out)
    {
        out = 0;
        if (!unit)
            return false;

        auto const* fieldsPtr = reinterpret_cast<uintptr_t const*>(
            reinterpret_cast<uint8_t const*>(unit) + UNIT_FIELDS_POINTER_OFFSET);
        if (!IsReadableMemory(fieldsPtr, sizeof(*fieldsPtr)) || !*fieldsPtr)
            return false;

        auto const* value = reinterpret_cast<uint32_t const*>(
            reinterpret_cast<uint8_t const*>(*fieldsPtr) + fieldIndex * sizeof(uint32_t));
        if (!IsReadableMemory(value, sizeof(*value)))
            return false;

        out = *value;
        return true;
    }

    uint32_t GetUnitCurrentShapeshiftFormSafe(void* unit)
    {
        if (!unit)
            return 0;

        auto const* pendingReset = reinterpret_cast<uint8_t const*>(
            reinterpret_cast<uint8_t const*>(unit) + UNIT_PENDING_SHAPESHIFT_RESET_OFFSET);
        if (IsReadableMemory(pendingReset, sizeof(*pendingReset)) && *pendingReset)
            return 0;

        auto const* fieldsPtr = reinterpret_cast<uintptr_t const*>(
            reinterpret_cast<uint8_t const*>(unit) + UNIT_FIELDS_POINTER_OFFSET);
        if (!IsReadableMemory(fieldsPtr, sizeof(*fieldsPtr)) || !*fieldsPtr)
            return 0;

        auto const* formByte = reinterpret_cast<uint8_t const*>(
            reinterpret_cast<uint8_t const*>(*fieldsPtr) + UNIT_FIELD_BYTES_2_SHAPESHIFT_FORM_BYTE_OFFSET);
        if (!IsReadableMemory(formByte, sizeof(*formByte)))
            return 0;

        return *formByte;
    }

    bool UnitIsUnderwaterOrSwimmingSafe(void* unit)
    {
        if (!unit)
            return false;

        auto const* movementFlags = reinterpret_cast<uint32_t const*>(
            reinterpret_cast<uint8_t const*>(unit) + UNIT_MOVEMENT_FLAGS_OFFSET);
        return IsReadableMemory(movementFlags, sizeof(*movementFlags))
            && ((*movementFlags & UNIT_MOVEMENT_FLAG_SWIMMING_OR_UNDERWATER) != 0);
    }

    bool SpellMaskHasForm(uint32_t const* mask, uint32_t formId)
    {
        if (!formId || !mask)
            return false;

        uint32_t const formIndex = formId - 1;
        uint32_t const word = formIndex / 32;
        if (word >= 2)
            return false;

        return (mask[word] & (1u << (formIndex & 31))) != 0;
    }

    bool SpellMaskAny(uint32_t const* mask)
    {
        return mask && (mask[0] || mask[1]);
    }

    uint32_t GetStreamedSpellPowerCost(ClientData::SpellRow const& row, void* unit)
    {
        uint32_t cost = row.m_manaCost;
        if (row.m_manaCostPct)
        {
            uint32_t maxPower = 0;
            int32_t const powerType = static_cast<int32_t>(row.m_powerType);
            uint32_t const field = powerType == -2
                ? static_cast<uint32_t>(ClientData::UNIT_FIELD_MAXHEALTH)
                : static_cast<uint32_t>(ClientData::UNIT_FIELD_MAXPOWER1) + static_cast<uint32_t>((std::max)(powerType, 0));
            if (ReadUnitFieldU32(unit, field, maxPower))
                cost += (maxPower * row.m_manaCostPct) / 100u;
        }

        return cost;
    }

    bool GetUnitPowerForSpellSafe(void* unit, ClientData::SpellRow const& row, uint32_t& out)
    {
        out = 0;
        int32_t const powerType = static_cast<int32_t>(row.m_powerType);
        if (powerType < 0)
        {
            if (powerType == -2)
                return ReadUnitFieldU32(unit, static_cast<uint32_t>(ClientData::UNIT_FIELD_HEALTH), out);
            out = UINT32_MAX;
            return true;
        }

        if (powerType == ClientData::POWER_RUNES)
        {
            out = UINT32_MAX;
            return true;
        }

        if (powerType > ClientData::POWER_RUNIC_POWER)
            return false;

        return ReadUnitFieldU32(
            unit,
            static_cast<uint32_t>(ClientData::UNIT_FIELD_POWER1) + static_cast<uint32_t>(powerType),
            out);
    }

    bool StreamedSpellHasEffectAtIndex(uint32_t spellId, uint32_t effectIndex, uint32_t effectId)
    {
        const SpellEffectCacheRow* effect = nullptr;
        return SpellCacheStreaming::TryGetSpellEffect(spellId, effectIndex, effect)
            && effect
            && effect->effect == effectId;
    }

    bool StreamedShapeshiftAuraMatches(uint32_t stanceSpellId, int32_t stanceForm, uint32_t auraSpellId)
    {
        if (auraSpellId == stanceSpellId)
            return true;

        bool matched = false;
        SpellCacheStreaming::ForEachSpellEffect(auraSpellId, [&](const SpellEffectCacheRow& auraEffect) {
            if (auraEffect.effectApplyAuraName == SPELL_AURA_MOD_SHAPESHIFT
                && auraEffect.effectMiscValue == stanceForm)
            {
                matched = true;
                return false;
            }

            return true;
        });

        return matched;
    }

    bool StreamedSpellCurrentOnUnit(ClientData::SpellRow const* spell, void* unit)
    {
        if (!spell || !unit || !SpellCacheStreaming::HasSpell(spell->m_ID))
            return false;

        uint64_t const critterGuid = GetUnitCritterGuidSafe(unit);
        uint32_t const critterEntry = static_cast<uint32_t>((critterGuid >> 24) & 0x0FFFFFFFu);

        bool active = false;
        SpellCacheStreaming::ForEachSpellEffect(spell->m_ID, [&](const SpellEffectCacheRow& effect) {
            if (effect.effect == SPELL_EFFECT_SUMMON
                && critterEntry
                && static_cast<uint32_t>(effect.effectMiscValue) == critterEntry
                && SummonPropertySlotIsCritter(effect.effectMiscValueB))
            {
                active = true;
                return false;
            }

            if (effect.effectApplyAuraName == SPELL_AURA_MOD_SHAPESHIFT)
            {
                uint32_t const currentFormId = GetUnitCurrentShapeshiftFormSafe(unit);
                if (currentFormId && static_cast<uint32_t>(effect.effectMiscValue) == currentFormId)
                {
                    active = true;
                    return false;
                }

                uint32_t const auraCount = GetUnitAuraCountSafe(unit);
                for (uint32_t i = 0; i < auraCount; ++i)
                {
                    uint32_t auraSpellId = 0;
                    uint32_t auraFlags = 0;
                    if (TryGetUnitAuraEntry(unit, i, auraSpellId, auraFlags)
                        && StreamedShapeshiftAuraMatches(spell->m_ID, effect.effectMiscValue, auraSpellId)
                        && (auraFlags & 0x10u) != 0)
                    {
                        active = true;
                        return false;
                    }
                }
            }

            if (effect.effectApplyAuraName == SPELL_AURA_MOUNTED)
            {
                uint32_t const auraCount = GetUnitAuraCountSafe(unit);
                for (uint32_t i = 0; i < auraCount; ++i)
                {
                    uint32_t auraSpellId = 0;
                    uint32_t auraFlags = 0;
                    if (!TryGetUnitAuraEntry(unit, i, auraSpellId, auraFlags))
                        continue;

                    bool matchedMountedAura = false;
                    SpellCacheStreaming::ForEachSpellEffect(auraSpellId, [&](const SpellEffectCacheRow& auraEffect) {
                        if (auraEffect.effectApplyAuraName == SPELL_AURA_MOUNTED
                            && auraEffect.effectMiscValue == effect.effectMiscValue)
                        {
                            matchedMountedAura = true;
                            return false;
                        }
                        return true;
                    });

                    if (matchedMountedAura)
                    {
                        active = true;
                        return false;
                    }
                }
            }

            return true;
        });

        return active;
    }

    bool GetStreamedSpellUsableState(ClientData::SpellRow const& row, void* unit, bool& usable, bool& lacksPower)
    {
        usable = false;
        lacksPower = false;
        if (!unit || !SpellCacheStreaming::HasSpell(row.m_ID))
            return false;

        if ((row.m_attributes & STREAMED_SPELL_ATTR0_PASSIVE) != 0)
            return true;

        if (StreamedSpellCurrentOnUnit(&row, unit))
        {
            usable = true;
            return true;
        }

        uint32_t const currentForm = GetUnitCurrentShapeshiftFormSafe(unit);
        if (SpellMaskHasForm(row.m_shapeshiftExclude, currentForm))
            return true;
        if (SpellMaskAny(row.m_shapeshiftMask) && !SpellMaskHasForm(row.m_shapeshiftMask, currentForm))
            return true;

        bool const underwater = UnitIsUnderwaterOrSwimmingSafe(unit);
        if ((row.m_auraInterruptFlags & 0x100u) != 0 && !underwater)
            return true;
        if ((row.m_auraInterruptFlags & 0x80u) != 0 && underwater)
            return true;

        uint32_t availablePower = 0;
        if (GetUnitPowerForSpellSafe(unit, row, availablePower))
        {
            uint32_t const cost = GetStreamedSpellPowerCost(row, unit);
            if (cost > availablePower)
            {
                lacksPower = true;
                return true;
            }
        }

        usable = true;
        return true;
    }

    bool GetStreamedSpellUsableState(uint32_t spellId, bool& usable, bool& lacksPower)
    {
        usable = false;
        lacksPower = false;

        ClientData::SpellRow row{};
        if (!SpellCacheStreaming::TryBuildSpellRow(spellId, row))
            return false;

        void* localPlayer = ClntObjMgrObjectPtr_ActionState_SpellCache(
            ClntObjMgrGetActivePlayer_ActionState_SpellCache(),
            STREAMED_TYPEMASK_PLAYER);
        return GetStreamedSpellUsableState(row, localPlayer, usable, lacksPower);
    }

    bool GetStreamedSpellStaticBarUsableState(uint32_t spellId, bool& usable, bool& lacksPower)
    {
        usable = false;
        lacksPower = false;

        ClientData::SpellRow row{};
        if (!SpellCacheStreaming::TryBuildSpellRow(spellId, row))
            return false;

        if ((row.m_attributes & STREAMED_SPELL_ATTR0_PASSIVE) != 0)
            return true;

        usable = true;
        return true;
    }

    bool StreamedSpellCanToggleOrCancel(uint32_t spellId)
    {
        ClientData::SpellRow row{};
        if (!SpellCacheStreaming::TryGetSpellRow(spellId, row))
            return false;

        if (Spell_C_CancelSpellByID_ActionState_SpellCache(spellId))
            return true;

        void* localPlayer = ClntObjMgrObjectPtr_ActionState_SpellCache(
            ClntObjMgrGetActivePlayer_ActionState_SpellCache(),
            STREAMED_TYPEMASK_PLAYER);

        if (StreamedSpellHasEffectAtIndex(spellId, 0, SPELL_EFFECT_ATTACK))
        {
            if (!localPlayer)
                return false;

            auto const* attackTargetLow = reinterpret_cast<uint32_t const*>(
                reinterpret_cast<uint8_t const*>(localPlayer) + 0x0A20);
            auto const* attackTargetHigh = reinterpret_cast<uint32_t const*>(
                reinterpret_cast<uint8_t const*>(localPlayer) + 0x0A24);
            return IsReadableMemory(attackTargetLow, sizeof(*attackTargetLow))
                && IsReadableMemory(attackTargetHigh, sizeof(*attackTargetHigh))
                && (*attackTargetLow || *attackTargetHigh);
        }

        if (StreamedSpellHasEffectAtIndex(spellId, 0, SPELL_EFFECT_TRADE_SKILL))
        {
            if (!localPlayer)
                return false;

            void* skillLine = CGPlayer_C_GetCreatureFamilySkillLineBySpellID_ActionState_SpellCache(localPlayer, spellId);
            if (!skillLine)
                return false;

            auto const* skillCategory = reinterpret_cast<uint32_t const*>(
                reinterpret_cast<uint8_t const*>(skillLine) + 0x4);
            auto const* currentCategory = reinterpret_cast<uint32_t const*>(0x00C235A4);
            auto const* categoryBusy = reinterpret_cast<uint32_t const*>(0x00C235E4);
            return IsReadableMemory(skillCategory, sizeof(*skillCategory))
                && IsReadableMemory(currentCategory, sizeof(*currentCategory))
                && IsReadableMemory(categoryBusy, sizeof(*categoryBusy))
                && *categoryBusy == 0
                && *currentCategory == *skillCategory;
        }

        if (!StreamedSpellWillSummonCritter(spellId) && !StreamedSpellHasAura(spellId, SPELL_AURA_MOD_SHAPESHIFT))
            return false;

        return StreamedSpellCurrentOnUnit(&row, localPlayer);
    }

    uint32_t* PlayerSpellSlotMap()
    {
        return reinterpret_cast<uint32_t*>(PLAYER_SPELL_SLOT_MAP_ADDRESS);
    }

    uint32_t& PlayerSpellSlotMapSize()
    {
        return *reinterpret_cast<uint32_t*>(PLAYER_SPELL_SLOT_COUNT_ADDRESS);
    }

    uint32_t* VisibleSpellbookSlots()
    {
        return reinterpret_cast<uint32_t*>(SPELLBOOK_VISIBLE_SLOT_MAP_ADDRESS);
    }

    uint32_t& VisibleSpellbookCount()
    {
        return *reinterpret_cast<uint32_t*>(SPELLBOOK_VISIBLE_COUNT_ADDRESS);
    }

    uint32_t**& SpellbookTabArray()
    {
        return *reinterpret_cast<uint32_t***>(SPELLBOOK_TAB_ARRAY_ADDRESS);
    }

    uint32_t& SpellbookTabArrayCapacity()
    {
        return *reinterpret_cast<uint32_t*>(SPELLBOOK_TAB_ARRAY_CAPACITY_ADDRESS);
    }

    uint32_t& SpellbookTabAllocatedCount()
    {
        return *reinterpret_cast<uint32_t*>(SPELLBOOK_TAB_ALLOCATED_COUNT_ADDRESS);
    }

    uint32_t& SpellbookTabAllocationChunk()
    {
        return *reinterpret_cast<uint32_t*>(SPELLBOOK_TAB_ALLOCATION_CHUNK_ADDRESS);
    }

    uint32_t& SpellbookTabCount()
    {
        return *reinterpret_cast<uint32_t*>(SPELLBOOK_TAB_COUNT_ADDRESS);
    }

    uint32_t*& LanguageSpellIds()
    {
        return *reinterpret_cast<uint32_t**>(LANGUAGE_SPELL_IDS_ADDRESS);
    }

    uint32_t& LanguageCapacity()
    {
        return *reinterpret_cast<uint32_t*>(LANGUAGE_CAPACITY_ADDRESS);
    }

    uint32_t& LanguageCount()
    {
        return *reinterpret_cast<uint32_t*>(LANGUAGE_COUNT_ADDRESS);
    }

    NativeShapeshiftEntry*& ShapeshiftRows()
    {
        return *reinterpret_cast<NativeShapeshiftEntry**>(SHAPESHIFT_ROWS_ADDRESS);
    }

    uint32_t& ShapeshiftCapacity()
    {
        return *reinterpret_cast<uint32_t*>(SHAPESHIFT_CAPACITY_ADDRESS);
    }

    uint32_t& ShapeshiftCount()
    {
        return *reinterpret_cast<uint32_t*>(SHAPESHIFT_COUNT_ADDRESS);
    }

    uint32_t& ShapeshiftAllocationChunk()
    {
        return *reinterpret_cast<uint32_t*>(SHAPESHIFT_ALLOCATION_CHUNK_ADDRESS);
    }

    bool NativeSpellDbCanLookup(uint32_t spellId)
    {
        if (!spellId)
            return false;

        auto const* spellDb = reinterpret_cast<ClientData::WoWClientDB const*>(SPELL_DB_ADDRESS);
        if (!(spellDb
            && spellDb->isLoaded
            && spellDb->Rows
            && spellId >= static_cast<uint32_t>(spellDb->minIndex)
            && spellId <= static_cast<uint32_t>(spellDb->maxIndex)))
        {
            return false;
        }

        auto const* rowsById = reinterpret_cast<ClientData::SpellRow* const*>(spellDb->Rows);
        return rowsById && rowsById[spellId - static_cast<uint32_t>(spellDb->minIndex)] != nullptr;
    }

    ClientData::SpellRow* GetStableStreamedSpellRow(uint32_t spellId)
    {
        if (!HasReasonableSpellId(spellId))
            return nullptr;

        auto found = NativeSpellDbRows.find(spellId);
        if (found != NativeSpellDbRows.end())
            return found->second.get();

        ClientData::SpellRow row{};
        if (!SpellCacheStreaming::TryBuildSpellRow(spellId, row))
            return nullptr;

        auto stored = std::make_unique<ClientData::SpellRow>(row);
        ClientData::SpellRow* raw = stored.get();
        NativeSpellDbRows[spellId] = std::move(stored);
        return raw;
    }

    void EnsureNativeSpellDbRecordMap(uint32_t minimumSpellId)
    {
        (void)minimumSpellId;
        return;

        auto* spellDb = reinterpret_cast<ClientData::WoWClientDB*>(SPELL_DB_ADDRESS);
        if (!spellDb)
            return;

        if (!NativeSpellDbRecordsById)
        {
            NativeSpellDbRecordsById = std::make_unique<ClientData::SpellRow*[]>(SPELL_CACHE_MAX_REASONABLE_SPELL_ID + 1);
            std::memset(
                NativeSpellDbRecordsById.get(),
                0,
                (SPELL_CACHE_MAX_REASONABLE_SPELL_ID + 1) * sizeof(ClientData::SpellRow*));
        }

        spellDb->isLoaded = 1;
        spellDb->minIndex = 0;
        spellDb->maxIndex = static_cast<int32_t>((std::max)(minimumSpellId, static_cast<uint32_t>((std::max)(spellDb->maxIndex, 0))));
        if (spellDb->maxIndex > static_cast<int32_t>(SPELL_CACHE_MAX_REASONABLE_SPELL_ID))
            spellDb->maxIndex = SPELL_CACHE_MAX_REASONABLE_SPELL_ID;
        spellDb->Rows = reinterpret_cast<int32_t*>(NativeSpellDbRecordsById.get());
    }

    bool InstallStreamedSpellIntoNativeDb(uint32_t spellId)
    {
        (void)spellId;
        return false;

        if (!HasReasonableSpellId(spellId))
            return false;

        ClientData::SpellRow row{};
        if (!SpellCacheStreaming::TryBuildSpellRow(spellId, row))
            return false;

        ClientData::SpellRow* raw = nullptr;
        auto found = NativeSpellDbRows.find(spellId);
        if (found != NativeSpellDbRows.end())
        {
            *found->second = row;
            raw = found->second.get();
        }
        else
        {
            auto stored = std::make_unique<ClientData::SpellRow>(row);
            raw = stored.get();
            NativeSpellDbRows[spellId] = std::move(stored);
        }

        EnsureNativeSpellDbRecordMap(spellId);
        if (NativeSpellDbRecordsById)
            NativeSpellDbRecordsById[spellId] = raw;

        static uint32_t logCount = 0;
        if (logCount < 200)
        {
            LOG_INFO << "Installed streamed spell into native Spell.dbc record map" << spellId;
            ++logCount;
        }

        return true;
    }

    int __cdecl PendingSpellCast_CopyStreamedSpellRec(uint32_t spellId, ClientData::SpellRow* out)
    {
        if (!out || !HasReasonableSpellId(spellId))
            return 0;

        ClientData::SpellRow row{};
        if (!SpellCacheStreaming::TryBuildSpellRow(spellId, row))
            return 0;

        std::memcpy(out, &row, sizeof(row));
        RecordStreamedVisualPipelineStage(
            spellId,
            STREAMED_VISUAL_STAGE_STACK_ROW,
            PENDING_SPELL_CAST_SPELLREC_LOOKUP_ADDRESS,
            row.m_spellVisualID[0],
            0,
            0,
            1);
        if (spellId == 82893)
        {
            static bool loggedAvengersShield = false;
            if (!loggedAvengersShield)
            {
                LOG_INFO << "PendingSpellCast streamed SpellRec fields"
                    << "spell" << spellId
                    << "targets" << row.m_targets
                    << "maxTargets" << row.m_maxTargets
                    << "speed" << row.m_speed
                    << "attributes" << row.m_attributes
                    << "attributesExD" << row.m_attributesExD
                    << "attributesExG" << row.m_attributesExG
                    << "visual1" << row.m_spellVisualID[0]
                    << "visual2" << row.m_spellVisualID[1]
                    << "missile" << row.m_spellMissileID
                    << "effect0" << row.m_effect[0]
                    << "effect1" << row.m_effect[1]
                    << "effect2" << row.m_effect[2]
                    << "targetA0" << row.m_implicitTargetA[0]
                    << "targetA1" << row.m_implicitTargetA[1]
                    << "targetA2" << row.m_implicitTargetA[2]
                    << "targetB0" << row.m_implicitTargetB[0]
                    << "targetB1" << row.m_implicitTargetB[1]
                    << "targetB2" << row.m_implicitTargetB[2]
                    << "chain0" << row.m_effectChainTargets[0]
                    << "chain1" << row.m_effectChainTargets[1]
                    << "chain2" << row.m_effectChainTargets[2];
                loggedAvengersShield = true;
            }
        }

        RecordStreamedSpellDbLookup(spellId, row, reinterpret_cast<void*>(PENDING_SPELL_CAST_SPELLREC_LOOKUP_ADDRESS));
        return 1;
    }

    extern "C" __declspec(naked) void PendingSpellCast_SpellRecLookupBridge()
    {
        __asm
        {
            lea ecx, [ebp - 3B0h]
            mov eax, WOW_CLIENT_DB_GET_ROW_ADDRESS
            call eax
            mov eax, [ebp + 8]
            test eax, eax
            jz fail
            mov ecx, [eax + 20h]
            lea edx, [ebp - 3B0h]
            push edx
            push ecx
            call PendingSpellCast_CopyStreamedSpellRec
            add esp, 8
            test eax, eax
            jz fail
            mov ebx, [ebp + 8]
            mov eax, PENDING_SPELL_CAST_SPELLREC_LOOKUP_SUCCESS_CONTINUE_ADDRESS
            jmp eax
        fail:
            mov eax, PENDING_SPELL_CAST_SPELLREC_LOOKUP_FAIL_CONTINUE_ADDRESS
            jmp eax
        }
    }

    void PatchPendingSpellCastSpellRecLookup()
    {
        if (PendingSpellCastSpellRecLookupPatched)
            return;

        auto* const slot = reinterpret_cast<uint8_t*>(PENDING_SPELL_CAST_SPELLREC_LOOKUP_ADDRESS);
        uint8_t const expected[] = { 0x8D, 0x8D, 0x50, 0xFC, 0xFF, 0xFF, 0xE8, 0xD1, 0x97, 0x0A, 0x00 };
        if (std::memcmp(slot, expected, sizeof(expected)) != 0)
        {
            LOG_ERROR << "PendingSpellCast SpellRec direct lookup patch signature mismatch";
            return;
        }

        uint8_t patch[PENDING_SPELL_CAST_SPELLREC_LOOKUP_PATCH_SIZE] = {};
        patch[0] = 0xE9;
        int32_t const rel = static_cast<int32_t>(
            reinterpret_cast<uintptr_t>(&PendingSpellCast_SpellRecLookupBridge)
            - PENDING_SPELL_CAST_SPELLREC_LOOKUP_ADDRESS
            - 5);
        std::memcpy(&patch[1], &rel, sizeof(rel));
        std::memset(&patch[5], 0x90, sizeof(patch) - 5);
        Util::OverwriteBytesAtAddress(static_cast<uint32_t>(PENDING_SPELL_CAST_SPELLREC_LOOKUP_ADDRESS), patch, sizeof(patch));

        PendingSpellCastSpellRecLookupPatched = true;
        LOG_INFO << "Patched PendingSpellCast streamed SpellRec lookup bridge";
    }

    void PatchKnownSpellMaxIdGate()
    {
        if (KnownSpellMaxIdGatePatched)
            return;

        auto* const branch = reinterpret_cast<uint8_t*>(ADD_KNOWN_SPELL_MAX_ID_GATE_ADDRESS);
        uint8_t const expected[] = { 0x7E, 0x1A };
        if (std::memcmp(branch, expected, sizeof(expected)) != 0)
        {
            LOG_ERROR << "AddKnownSpell max-id gate patch signature mismatch"
                << "byte0" << static_cast<uint32_t>(branch[0])
                << "byte1" << static_cast<uint32_t>(branch[1]);
            return;
        }

        uint8_t patch[] = { 0xEB, 0x1A };
        Util::OverwriteBytesAtAddress(static_cast<uint32_t>(ADD_KNOWN_SPELL_MAX_ID_GATE_ADDRESS), patch, sizeof(patch));
        KnownSpellMaxIdGatePatched = true;
        LOG_INFO << "Patched AddKnownSpell max-id gate for streamed spellbook rows";
    }

    void PlayStreamedLocalCastKit(uint32_t spellId)
    {
        if (!HasReasonableSpellId(spellId))
            return;

        ClientData::SpellRow row{};
        if (!SpellCacheStreaming::TryBuildSpellRow(spellId, row))
            return;

        uint32_t visualId = row.m_spellVisualID[0];
        if (GetSpellVisualDetailMode_SpellCache() < 2 && row.m_spellVisualID[1])
            visualId = row.m_spellVisualID[1];
        if (!visualId)
            return;

        auto* visual = reinterpret_cast<SpellVisualDebugRow*>(
            ClientDB::GetRow(reinterpret_cast<void*>(SPELL_VISUAL_DB_RECORDS_ADDRESS), visualId));
        if (!visual || !visual->castKit)
            return;

        void* const kit = ClientDB::GetRow(reinterpret_cast<void*>(SPELL_VISUAL_KIT_DB_RECORDS_ADDRESS), visual->castKit);
        if (!kit)
            return;

        void* const noAnimationKit = GetNoAnimationVisualKit(spellId, visual->castKit, kit);
        if (!noAnimationKit)
            return;

        void* const player = ClntObjMgr::ObjectPtr(ClntObjMgr::GetActivePlayer(), TYPEMASK_PLAYER);
        if (!player)
            return;

        uint32_t args[12] = {};
        args[0] = reinterpret_cast<uint32_t>(&row);
        args[1] = reinterpret_cast<uint32_t>(noAnimationKit);
        args[2] = 0;
        args[3] = 0;
        args[4] = 0;
        args[5] = 0;
        args[6] = 1;
        args[7] = 0;
        args[8] = 0;
        args[9] = 0xFFFFFFFFu;

        CGObject_C__PlaySpellVisualKit_Direct_SpellCache(player, args);

        uint32_t& hits = StreamedDirectCastKitPlayHits[spellId];
        if (hits < 0xFFFFFFFF)
            ++hits;
        StreamedDirectCastKitLastVisual[spellId] = visualId;
        StreamedDirectCastKitLastKit[spellId] = visual->castKit;
    }

    bool ResolveStreamedVisualKit(uint32_t spellId, bool impact, ClientData::SpellRow& row, uint32_t& visualId, uint32_t& kitId, void*& kit)
    {
        if (!HasReasonableSpellId(spellId))
            return false;

        if (!SpellCacheStreaming::TryBuildSpellRow(spellId, row))
            return false;

        visualId = row.m_spellVisualID[0];
        if (GetSpellVisualDetailMode_SpellCache() < 2 && row.m_spellVisualID[1])
            visualId = row.m_spellVisualID[1];
        if (!visualId)
            return false;

        auto* visual = reinterpret_cast<SpellVisualDebugRow*>(
            ClientDB::GetRow(reinterpret_cast<void*>(SPELL_VISUAL_DB_RECORDS_ADDRESS), visualId));
        if (!visual)
            return false;

        kitId = impact ? visual->impactKit : visual->castKit;
        if (!kitId)
            return false;

        kit = ClientDB::GetRow(reinterpret_cast<void*>(SPELL_VISUAL_KIT_DB_RECORDS_ADDRESS), kitId);
        return kit != nullptr;
    }

    void* GetNoAnimationVisualKit(uint32_t spellId, uint32_t kitId, void* kit)
    {
        if (!kit)
            return nullptr;

        uint64_t const key = (static_cast<uint64_t>(spellId) << 32) | kitId;
        auto found = StreamedVisualKitNoAnimationRows.find(key);
        if (found != StreamedVisualKitNoAnimationRows.end())
            return found->second.get();

        auto copy = std::make_unique<char[]>(sizeof(SpellVisualKitDebugRow));
        std::memcpy(copy.get(), kit, sizeof(SpellVisualKitDebugRow));
        auto* copyRow = reinterpret_cast<SpellVisualKitDebugRow*>(copy.get());
        copyRow->startAnimID = 0;
        copyRow->animID = 0;

        void* raw = copy.get();
        StreamedVisualKitNoAnimationRows[key] = std::move(copy);
        return raw;
    }

    uint32_t*& KnownSpellBitset()
    {
        return *reinterpret_cast<uint32_t**>(KNOWN_SPELL_BITSET_ADDRESS);
    }

    uint32_t& KnownSpellBitsetCount()
    {
        return *reinterpret_cast<uint32_t*>(KNOWN_SPELL_BITSET_COUNT_ADDRESS);
    }

    uint32_t& KnownSpellBitsetCapacity()
    {
        return *reinterpret_cast<uint32_t*>(KNOWN_SPELL_BITSET_CAPACITY_ADDRESS);
    }

    bool KnownSpellBitsetCanLookup(uint32_t spellId)
    {
        if (!IsPlausibleClientPointer(KnownSpellBitset()))
            return false;

        uint32_t const wordIndex = spellId >> 5;
        return wordIndex < KnownSpellBitsetCapacity();
    }

    bool KnownSpellBitsetHas(uint32_t spellId)
    {
        if (!KnownSpellBitsetCanLookup(spellId))
            return false;

        return (KnownSpellBitset()[spellId >> 5] & (1u << (spellId & 0x1F))) != 0;
    }

    void EnsureKnownSpellBitset(uint32_t minimumSpellId = 0)
    {
        auto const range = GlobalCDBCMap.getIndexRange("Spell");
        uint32_t maxSpellId = static_cast<uint32_t>((std::max)(range.second, 0));
        maxSpellId = (std::max)(maxSpellId, minimumSpellId);
        uint32_t const wordCount = (std::max)(1u, (maxSpellId >> 5) + 1u);

        uint32_t* const currentBitset = KnownSpellBitset();
        uint32_t const currentWordCount = KnownSpellBitsetCapacity();
        bool const hasValidCurrentBitset = IsPlausibleClientPointer(currentBitset)
            && currentWordCount > 0
            && currentWordCount <= ((SPELL_CACHE_MAX_REASONABLE_SPELL_ID >> 5) + 1);
        if (currentBitset && !hasValidCurrentBitset)
        {
            LOG_ERROR << "Reset invalid native known spell bitset pointer" << currentBitset;
            KnownSpellBitset() = nullptr;
            KnownSpellBitsetCount() = 0;
            KnownSpellBitsetCapacity() = 0;
        }

        if (hasValidCurrentBitset && currentWordCount >= wordCount)
            return;

        void* const bitset = SMemAlloc_SpellCache(
            wordCount * sizeof(uint32_t),
            ".\\SpellCacheStreaming.cpp",
            -2,
            0);
        if (!bitset)
        {
            LOG_ERROR << "Failed to allocate spell known bitset words" << wordCount;
            return;
        }

        std::memset(bitset, 0, wordCount * sizeof(uint32_t));
        if (hasValidCurrentBitset)
        {
            uint32_t const copyWords = (std::min)(currentWordCount, wordCount);
            std::memcpy(bitset, currentBitset, copyWords * sizeof(uint32_t));
        }

        *reinterpret_cast<uint32_t*>(KNOWN_SPELL_MAX_ID_PLUS_ONE_ADDRESS) = maxSpellId + 1;
        KnownSpellBitsetCount() = wordCount;
        KnownSpellBitsetCapacity() = wordCount;
        KnownSpellBitset() = static_cast<uint32_t*>(bitset);

        LOG_INFO << "Allocated spell known bitset words" << wordCount << "maxSpellId" << maxSpellId;
    }

    void MarkKnownSpellBit(uint32_t spellId)
    {
        if (!HasReasonableSpellId(spellId))
            return;

        EnsureKnownSpellBitset(spellId);
        if (KnownSpellBitsetCanLookup(spellId))
            KnownSpellBitset()[spellId >> 5] |= 1u << (spellId & 0x1F);
    }

    bool IsValidActionBarSlot(uint32_t zeroBasedSlot)
    {
        return zeroBasedSlot < 144;
    }

    uintptr_t* NativeActionBarSpellIds()
    {
        return reinterpret_cast<uintptr_t*>(0xC1DED8);
    }

    uintptr_t* NativeActionButtons()
    {
        return reinterpret_cast<uintptr_t*>(0xC1E358);
    }

    uint32_t* ActionButtonPacketActions()
    {
        return reinterpret_cast<uint32_t*>(0xAD9F6C);
    }

    StreamedActionBarNativeSnapshot ReadNativeActionBarSnapshot(uint32_t zeroBasedSlot)
    {
        StreamedActionBarNativeSnapshot snapshot{};
        if (!IsValidActionBarSlot(zeroBasedSlot))
            return snapshot;

        snapshot.actionButton = NativeActionButtons()[zeroBasedSlot];
        snapshot.spellId = NativeActionBarSpellIds()[zeroBasedSlot];
        return snapshot;
    }

    void RecordNativeActionBarSnapshot(uint32_t zeroBasedSlot)
    {
        if (!IsValidActionBarSlot(zeroBasedSlot))
            return;

        StreamedActionBarNativeSnapshots[zeroBasedSlot] = ReadNativeActionBarSnapshot(zeroBasedSlot);
    }

    void SignalActionBarSlotChanged(uint32_t zeroBasedSlot)
    {
        if (!IsValidActionBarSlot(zeroBasedSlot))
            return;

        FrameScript::SignalEvent(
            ClientData::EVENT_ACTIONBAR_SLOT_CHANGED,
            const_cast<char*>("%d"),
            zeroBasedSlot + 1);
    }

    std::string StreamedActionBarPersistencePath(uint64_t playerGuid)
    {
        std::ostringstream path;
        path << "Cache\\SpellCacheActionBar_" << playerGuid << ".txt";
        return path.str();
    }

    void SaveStreamedActionBarEntries()
    {
        if (!StreamedActionBarPlayerGuid)
            return;

        CreateDirectoryA("Cache", nullptr);

        std::ofstream out(StreamedActionBarPersistencePath(StreamedActionBarPlayerGuid), std::ios::trunc);
        if (!out)
        {
            static uint32_t logCount = 0;
            if (logCount < 20)
            {
                LOG_ERROR << "Failed to save streamed actionbar entries"
                    << "player" << StreamedActionBarPlayerGuid
                    << "entries" << static_cast<uint32_t>(StreamedActionBarEntries.size());
                ++logCount;
            }
            return;
        }

        for (auto const& entry : StreamedActionBarEntries)
        {
            if (IsValidActionBarSlot(entry.first) && HasReasonableSpellId(entry.second))
                out << entry.first << ' ' << entry.second << '\n';
        }
    }

    bool TryGetStreamedActionBarEntryRaw(uint32_t zeroBasedSlot, uint32_t& spellId)
    {
        spellId = 0;
        if (!IsValidActionBarSlot(zeroBasedSlot))
            return false;

        auto const itr = StreamedActionBarEntries.find(zeroBasedSlot);
        if (itr == StreamedActionBarEntries.end() || !HasReasonableSpellId(itr->second))
            return false;

        spellId = itr->second;
        return true;
    }

    bool TryGetStreamedActionBarEntry(uint32_t zeroBasedSlot, uint32_t& spellId, bool requestOnMiss)
    {
        if (!TryGetStreamedActionBarEntryRaw(zeroBasedSlot, spellId))
            return false;

        ForcedStreamedSpellRows.insert(spellId);
        if (SpellCacheStreaming::HasSpell(spellId))
            return true;

        if (requestOnMiss)
            SpellCacheStreaming::RequestSpell(spellId);

        return false;
    }

    bool TryGetNativeActionBarRawSpell(uint32_t zeroBasedSlot, uint32_t& spellId)
    {
        spellId = 0;
        if (!IsValidActionBarSlot(zeroBasedSlot))
            return false;

        uint32_t const action = static_cast<uint32_t>(NativeActionButtons()[zeroBasedSlot]);
        if (!action || (action & 0xF0000000u) != 0)
            return false;

        if (!HasReasonableSpellId(action))
            return false;

        spellId = action;
        return true;
    }

    bool TryGetPendingActionBarStreamedSpell(uint32_t zeroBasedSlot, uint32_t& spellId)
    {
        if (!TryGetNativeActionBarRawSpell(zeroBasedSlot, spellId))
            return false;

        ForceActionBarStreamedSpell(spellId, true);
        return !SpellCacheStreaming::HasSpell(spellId);
    }

    bool SetStreamedActionBarEntry(uint32_t zeroBasedSlot, uint32_t spellId, bool requestOnMiss)
    {
        if (!SPELL_CACHE_ENABLE_UI_INTEGRATION)
            return false;

        if (!IsValidActionBarSlot(zeroBasedSlot) || !HasReasonableSpellId(spellId))
            return false;

        ForcedStreamedSpellRows.insert(spellId);
        if (SpellCacheStreaming::HasSpell(spellId))
            CGActionBar_SetAction_StreamedActionBar(zeroBasedSlot, spellId, 1, 0);

        StreamedActionBarEntries[zeroBasedSlot] = spellId;
        RecordNativeActionBarSnapshot(zeroBasedSlot);
        if (requestOnMiss && !SpellCacheStreaming::HasSpell(spellId))
            SpellCacheStreaming::RequestSpell(spellId);

        SaveStreamedActionBarEntries();
        SignalActionBarSlotChanged(zeroBasedSlot);

        static uint32_t logCount = 0;
        if (logCount < 80)
        {
            LOG_INFO << "Set streamed actionbar entry"
                << "slot" << zeroBasedSlot
                << "spell" << spellId
                << "cached" << SpellCacheStreaming::HasSpell(spellId)
                << "entries" << static_cast<uint32_t>(StreamedActionBarEntries.size());
            ++logCount;
        }

        return true;
    }

    bool ForceActionBarStreamedSpell(uint32_t spellId, bool requestOnMiss)
    {
        if (!HasReasonableSpellId(spellId))
            return false;

        bool const newlyForced = ForcedStreamedSpellRows.insert(spellId).second;
        bool const cached = SpellCacheStreaming::HasSpell(spellId);
        if (requestOnMiss && !cached)
            SpellCacheStreaming::RequestSpell(spellId);

        return newlyForced || !cached;
    }

    void ApplyCachedStreamedActionBarSpellToNative(uint32_t spellId)
    {
        if (!SPELL_CACHE_ENABLE_UI_INTEGRATION)
            return;

        if (!HasReasonableSpellId(spellId) || !SpellCacheStreaming::HasSpell(spellId))
            return;

        uint32_t applied = 0;
        uintptr_t* actionButtons = NativeActionButtons();
        for (uint32_t slot = 0; slot < 144; ++slot)
        {
            if (static_cast<uint32_t>(actionButtons[slot]) != spellId)
                continue;

            SignalActionBarSlotChanged(slot);
            ++applied;
        }

        for (auto const& entry : StreamedActionBarEntries)
        {
            if (entry.second != spellId || !IsValidActionBarSlot(entry.first))
                continue;

            CGActionBar_SetAction_StreamedActionBar(entry.first, spellId, 1, 0);
            RecordNativeActionBarSnapshot(entry.first);
            SignalActionBarSlotChanged(entry.first);
            ++applied;
        }

        if (applied)
        {
            LOG_INFO << "Applied cached streamed spell to native actionbar"
                << "spell" << spellId
                << "slots" << applied;
        }
    }

    bool ClearStreamedActionBarEntry(uint32_t zeroBasedSlot)
    {
        if (!SPELL_CACHE_ENABLE_UI_INTEGRATION)
            return false;

        if (!IsValidActionBarSlot(zeroBasedSlot))
            return false;

        auto const erased = StreamedActionBarEntries.erase(zeroBasedSlot);
        if (!erased)
            return false;

        StreamedActionBarNativeSnapshots.erase(zeroBasedSlot);
        SaveStreamedActionBarEntries();
        SignalActionBarSlotChanged(zeroBasedSlot);

        static uint32_t logCount = 0;
        if (logCount < 80)
        {
            LOG_INFO << "Cleared streamed actionbar entry"
                << "slot" << zeroBasedSlot
                << "entries" << static_cast<uint32_t>(StreamedActionBarEntries.size());
            ++logCount;
        }

        return true;
    }

    void ClearAllStreamedActionBarEntries()
    {
        if (!SPELL_CACHE_ENABLE_UI_INTEGRATION)
            return;

        if (StreamedActionBarEntries.empty())
            return;

        std::vector<uint32_t> changedSlots;
        changedSlots.reserve(StreamedActionBarEntries.size());
        for (auto const& entry : StreamedActionBarEntries)
            changedSlots.push_back(entry.first);

        StreamedActionBarEntries.clear();
        StreamedActionBarNativeSnapshots.clear();
        SaveStreamedActionBarEntries();
        for (uint32_t slot : changedSlots)
            SignalActionBarSlotChanged(slot);

        LOG_INFO << "Cleared all streamed actionbar entries";
    }

    void EnsureStreamedActionBarEntriesLoaded()
    {
        uint64_t const playerGuid = ClntObjMgr::GetActivePlayer();
        if (!playerGuid)
            return;

        if (StreamedActionBarLoadedForPlayer && StreamedActionBarPlayerGuid == playerGuid)
            return;

        StreamedActionBarEntries.clear();
        StreamedActionBarPlayerGuid = playerGuid;
        StreamedActionBarLoadedForPlayer = true;

        std::ifstream in(StreamedActionBarPersistencePath(playerGuid));
        if (!in)
            return;

        uint32_t loaded = 0;
        uint32_t requested = 0;
        uint32_t slot = 0;
        uint32_t spellId = 0;
        while (in >> slot >> spellId)
        {
            if (!IsValidActionBarSlot(slot) || !HasReasonableSpellId(spellId))
                continue;

            StreamedActionBarEntries[slot] = spellId;
            RecordNativeActionBarSnapshot(slot);
            if (!SpellCacheStreaming::HasSpell(spellId))
            {
                SpellCacheStreaming::RequestSpell(spellId);
                ++requested;
            }

            SignalActionBarSlotChanged(slot);
            ++loaded;
        }

        LOG_INFO << "Loaded streamed actionbar entries"
            << "player" << playerGuid
            << "loaded" << loaded
            << "requested" << requested;
    }

    void ProcessStreamedActionBarNativeReplacements()
    {
        if (StreamedActionBarEntries.empty())
            return;

        std::vector<uint32_t> replacedSlots;
        for (auto const& entry : StreamedActionBarEntries)
        {
            uint32_t const slot = entry.first;
            auto snapshot = StreamedActionBarNativeSnapshots.find(slot);
            if (snapshot == StreamedActionBarNativeSnapshots.end())
            {
                RecordNativeActionBarSnapshot(slot);
                continue;
            }

            StreamedActionBarNativeSnapshot const current = ReadNativeActionBarSnapshot(slot);
            if (current.actionButton == snapshot->second.actionButton
                && current.spellId == snapshot->second.spellId)
            {
                continue;
            }

            replacedSlots.push_back(slot);
        }

        if (replacedSlots.empty())
            return;

        for (uint32_t slot : replacedSlots)
        {
            uint32_t spellId = 0;
            TryGetStreamedActionBarEntryRaw(slot, spellId);
            StreamedActionBarEntries.erase(slot);
            StreamedActionBarNativeSnapshots.erase(slot);
            SignalActionBarSlotChanged(slot);

            static uint32_t logCount = 0;
            if (logCount < 80)
            {
                StreamedActionBarNativeSnapshot const current = ReadNativeActionBarSnapshot(slot);
                LOG_INFO << "Cleared streamed actionbar entry after native replacement"
                    << "slot" << slot
                    << "spell" << spellId
                    << "nativeAction" << current.actionButton
                    << "nativeSpell" << current.spellId;
                ++logCount;
            }
        }

        SaveStreamedActionBarEntries();
    }

    void PrewarmNativeActionBarSpellCache()
    {
        uint64_t const playerGuid = ClntObjMgr::GetActivePlayer();
        if (!playerGuid || StreamedActionBarPrewarmPlayerGuid == playerGuid)
            return;

        StreamedActionBarPrewarmPlayerGuid = playerGuid;

        uint32_t queued = 0;
        uint32_t sampled = 0;
        uintptr_t* actionButtons = NativeActionButtons();
        for (uint32_t slot = 0; slot < 144; ++slot)
        {
            uint32_t const action = static_cast<uint32_t>(actionButtons[slot]);
            if (!action || (action & 0xF0000000u) != 0)
                continue;

            uint32_t const spellId = action;
            if (!HasReasonableSpellId(spellId))
                continue;

            ++sampled;
            bool const cached = SpellCacheStreaming::HasSpell(spellId);
            ForceActionBarStreamedSpell(spellId, true);
            if (!cached)
            {
                ++queued;
            }
        }

        LOG_INFO << "Prewarmed native actionbar spell cache"
            << "player" << playerGuid
            << "sampled" << sampled
            << "queued" << queued;
    }

    void ScanActionBarForStreamedRows()
    {
        uint64_t const playerGuid = ClntObjMgr::GetActivePlayer();
        if (!playerGuid)
            return;

        uint32_t const nowMs = GetTickCount();
        if (nowMs < NextActionBarStreamedRowScanMs)
            return;
        NextActionBarStreamedRowScanMs = nowMs + ACTIONBAR_STREAMED_ROW_SCAN_INTERVAL_MS;

        uint32_t sampled = 0;
        uint32_t forced = 0;
        uint32_t queued = 0;
        uintptr_t* actionButtons = NativeActionButtons();
        for (uint32_t slot = 0; slot < 144; ++slot)
        {
            uint32_t const action = static_cast<uint32_t>(actionButtons[slot]);
            if (!action || (action & 0xF0000000u) != 0)
                continue;

            uint32_t const spellId = action;
            if (!HasReasonableSpellId(spellId))
                continue;

            ++sampled;
            bool const cached = SpellCacheStreaming::HasSpell(spellId);
            if (ForceActionBarStreamedSpell(spellId, true))
                ++forced;
            if (!cached)
                ++queued;
        }

        static uint32_t logCount = 0;
        if ((forced || queued) && logCount < 80)
        {
            LOG_INFO << "Forced actionbar spell rows to streamed truth"
                << "sampled" << sampled
                << "forcedOrMissing" << forced
                << "queued" << queued;
            ++logCount;
        }
    }

    bool ShouldInstallActionButton(void* player, uint32_t action)
    {
        uint32_t const actionType = action & 0xF0000000u;
        if (actionType == 0)
        {
            if (!HasReasonableSpellId(action))
                return false;

            ForceActionBarStreamedSpell(action, true);
            return true;
        }

        if (actionType == 0x20000000u)
            return IsEquipmentSetKnown_ActionbarStream_SpellCache(action & 0xDFFFFFFFu);

        return true;
    }

    void ForceActionButtonPacketSpellRow(uint32_t slot, uint32_t action, uint32_t& forced, uint32_t& queued)
    {
        if ((action & 0xF0000000u) != 0)
            return;

        uint32_t const spellId = action;
        if (!HasReasonableSpellId(spellId))
            return;

        bool const cached = SpellCacheStreaming::HasSpell(spellId);
        if (ForceActionBarStreamedSpell(spellId, true))
            ++forced;
        if (!cached)
            ++queued;

        static uint32_t logCount = 0;
        if (logCount < 120)
        {
            LOG_INFO << "Action button packet forced streamed spell row"
                << "slot" << slot
                << "spell" << spellId
                << "cached" << cached;
            ++logCount;
        }
    }

    void ProcessReadyKnownSpellUiWork();
    void PumpNativeActionBarRefresh();

    void SendSpellCacheQuery(uint32_t spellId, uint32_t requestedHash)
    {
        PendingSpellRequests[spellId] = GetTickCount();
        LOG_INFO << "Sending spell cache query for" << spellId << "hash" << requestedHash;
        ClientNetwork::SendCustomPacket(SPELL_CACHE_QUERY_OPCODE, sizeof(uint32_t), [spellId](CustomPacketWrite& packet)
        {
            packet.Write<uint32_t>(spellId);
        });
    }

    void PumpSpellCacheRequests()
    {
        if (SpellCacheRequestDebugLogCount < 120)
        {
            LOG_INFO << "Spell cache pump enter"
                << "queued" << static_cast<uint32_t>(QueuedSpellRequests.size())
                << "pendingRequests" << static_cast<uint32_t>(PendingSpellRequests.size())
                << "pendingKnown" << static_cast<uint32_t>(PendingKnownSpellAdds.size())
                << "readyKnown" << static_cast<uint32_t>(ReadyKnownSpellUiQueue.size());
            ++SpellCacheRequestDebugLogCount;
        }

        EnsureStreamedActionBarEntriesLoaded();
        if (SpellCacheRequestDebugLogCount < 120)
        {
            LOG_INFO << "Spell cache pump after actionbar entries load";
            ++SpellCacheRequestDebugLogCount;
        }

        ProcessStreamedActionBarNativeReplacements();
        if (SpellCacheRequestDebugLogCount < 120)
        {
            LOG_INFO << "Spell cache pump after native replacement scan";
            ++SpellCacheRequestDebugLogCount;
        }

        PrewarmNativeActionBarSpellCache();
        if (SpellCacheRequestDebugLogCount < 120)
        {
            LOG_INFO << "Spell cache pump after actionbar prewarm"
                << "queued" << static_cast<uint32_t>(QueuedSpellRequests.size());
            ++SpellCacheRequestDebugLogCount;
        }

        ScanActionBarForStreamedRows();
        if (SpellCacheRequestDebugLogCount < 120)
        {
            LOG_INFO << "Spell cache pump after actionbar scan"
                << "queued" << static_cast<uint32_t>(QueuedSpellRequests.size());
            ++SpellCacheRequestDebugLogCount;
        }

        PumpNativeActionBarRefresh();
        if (SpellCacheRequestDebugLogCount < 120)
        {
            LOG_INFO << "Spell cache pump after native actionbar refresh";
            ++SpellCacheRequestDebugLogCount;
        }

        ProcessReadyKnownSpellUiWork();
        if (SpellCacheRequestDebugLogCount < 120)
        {
            LOG_INFO << "Spell cache pump after known spell UI work"
                << "queued" << static_cast<uint32_t>(QueuedSpellRequests.size())
                << "readyKnown" << static_cast<uint32_t>(ReadyKnownSpellUiQueue.size());
            ++SpellCacheRequestDebugLogCount;
        }

        if (QueuedSpellRequests.empty())
        {
            NextSpellCacheRequestMs = 0;
            return;
        }

        uint32_t const nowMs = GetTickCount();
        if (NextSpellCacheRequestMs && nowMs < NextSpellCacheRequestMs)
            return;

        if (SpellCacheRequestDebugLogCount < 120)
        {
            LOG_INFO << "Spell cache request pump begin"
                << "queued" << static_cast<uint32_t>(QueuedSpellRequests.size())
                << "pendingRequests" << static_cast<uint32_t>(PendingSpellRequests.size())
                << "nowMs" << nowMs;
            ++SpellCacheRequestDebugLogCount;
        }

        uint32_t sent = 0;
        while (sent < SPELL_CACHE_REQUESTS_PER_PUMP && !QueuedSpellRequests.empty())
        {
            uint32_t const spellId = QueuedSpellRequests.front();
            QueuedSpellRequests.pop_front();
            QueuedSpellRequestIds.erase(spellId);

            uint32_t requestedHash = 0;
            auto queuedHash = QueuedSpellRequestHashes.find(spellId);
            if (queuedHash != QueuedSpellRequestHashes.end())
            {
                requestedHash = queuedHash->second;
                QueuedSpellRequestHashes.erase(queuedHash);
            }

            if (!HasReasonableSpellId(spellId)
                || MissingSpellRows.find(spellId) != MissingSpellRows.end()
                || (requestedHash ? SpellCacheStreaming::HasSpell(spellId, requestedHash) : SpellCacheStreaming::HasSpell(spellId)))
                continue;

            auto pending = PendingSpellRequests.find(spellId);
            if (pending != PendingSpellRequests.end())
            {
                if (nowMs - pending->second < SPELL_CACHE_REQUEST_TIMEOUT_MS)
                    continue;

                PendingSpellRequests.erase(pending);
            }

            SendSpellCacheQuery(spellId, requestedHash);
            ++sent;
        }

        if (sent && SpellCacheRequestDebugLogCount < 120)
        {
            LOG_INFO << "Spell cache request pump end"
                << "sent" << sent
                << "queued" << static_cast<uint32_t>(QueuedSpellRequests.size())
                << "pendingRequests" << static_cast<uint32_t>(PendingSpellRequests.size());
            ++SpellCacheRequestDebugLogCount;
        }

        NextSpellCacheRequestMs = QueuedSpellRequests.empty()
            ? 0
            : nowMs + SPELL_CACHE_REQUEST_INTERVAL_MS;
    }

    uint32_t* WeaponSkillSpellRows()
    {
        return *reinterpret_cast<uint32_t**>(WEAPON_SKILL_SPELLS_ROWS_ADDRESS);
    }

    uint32_t LowestSetBit(uint32_t value)
    {
        for (uint32_t bit = 0; bit < 32; ++bit)
            if ((value & (1u << bit)) != 0)
                return bit;

        return 0xFFFFFFFF;
    }

    bool IsSingleBit(uint32_t value)
    {
        return value && (value & (value - 1)) == 0;
    }

    void BuildWeaponSkillSpellMapFromCache()
    {
        uint32_t weaponSubclassCount = 0;
        if (!GetWeaponSubclassCount_SpellCache(&weaponSubclassCount) || !weaponSubclassCount)
            return;

        ResizeClientUIntArray_SpellCache(reinterpret_cast<void*>(WEAPON_SKILL_SPELLS_ADDRESS), weaponSubclassCount);
        uint32_t* rows = WeaponSkillSpellRows();
        if (!rows)
            return;

        std::memset(rows, 0, weaponSubclassCount * sizeof(uint32_t));
        *reinterpret_cast<uint32_t*>(DEFENSE_SKILL_SPELL_ADDRESS) = 0;

        auto spells = GlobalCDBCMap.getCDBC("Spell");
        for (auto const& entry : spells)
        {
            SpellCacheRow const* spell = std::any_cast<SpellCacheRow>(&entry.second);
            if (!spell || (spell->attributes & STREAMED_SPELL_ATTR0_PASSIVE) == 0)
                continue;

            SpellEffectCacheRow* firstEffect = GlobalCDBCMap.getRow<SpellEffectCacheRow>(
                "SpellEffect", SpellEffectCacheKey(spell->id, 0));
            if (firstEffect && firstEffect->effect == SPELL_EFFECT_SKILL && *reinterpret_cast<uint32_t*>(DEFENSE_SKILL_SPELL_ADDRESS) == 0)
                *reinterpret_cast<uint32_t*>(DEFENSE_SKILL_SPELL_ADDRESS) = spell->id;

            uint32_t const subclassMask = static_cast<uint32_t>(spell->equippedItemSubClassMask);
            if (spell->equippedItemClass != ITEM_CLASS_WEAPON || !IsSingleBit(subclassMask))
                continue;

            uint32_t const subclass = LowestSetBit(subclassMask);
            if (subclass < weaponSubclassCount)
                rows[subclass] = spell->id;
        }

        LOG_INFO << "Built weapon skill spell map from Spell.cdbc count" << weaponSubclassCount;
    }

    uint64_t UnitGuid(void* unit)
    {
        if (!unit)
            return 0;

        auto const guidParts = *reinterpret_cast<uint32_t const* const*>(reinterpret_cast<uintptr_t>(unit) + 8);
        if (!guidParts)
            return 0;

        return static_cast<uint64_t>(guidParts[0]) | (static_cast<uint64_t>(guidParts[1]) << 32);
    }

    const char* KnownSpellSource(void* returnAddress)
    {
        uintptr_t const ret = reinterpret_cast<uintptr_t>(returnAddress);
        if (ret >= 0x006E7D60 && ret < 0x006E7E00)
            return "SMSG_LEARNED_SPELL";
        if (ret >= 0x006E7E00 && ret < 0x006E7F50)
            return "SMSG_SUPERCEDED_SPELL";
        if (ret >= 0x006E7F50 && ret < 0x006E8280)
            return "SMSG_INITIAL_SPELLS/PostInit";

        return "unknown";
    }

    void NotifySpellCached(uint32_t spellId)
    {
        for (auto const& callback : SpellCachedCallbacks)
        {
            if (callback)
                callback(spellId);
        }
    }

    void QueueNativeActionBarRefresh(uint32_t spellId, char const* reason)
    {
        if (!SPELL_CACHE_ENABLE_UI_INTEGRATION)
            return;

        NativeActionBarRefreshPending = true;

        static uint32_t logCount = 0;
        if (logCount < 120)
        {
            LOG_INFO << "Queued native actionbar usability refresh"
                << "reason" << reason
                << "spell" << spellId;
            ++logCount;
        }
    }

    void PumpNativeActionBarRefresh()
    {
        if (!SPELL_CACHE_ENABLE_UI_INTEGRATION)
            return;

        if (!NativeActionBarRefreshPending)
            return;

        uint64_t const playerGuid = ClntObjMgr::GetActivePlayer();
        if (!playerGuid)
            return;

        static uint64_t observedPlayerGuid = 0;
        static uint32_t firstPlayerSeenMs = 0;
        uint32_t const nowMs = GetTickCount();
        if (observedPlayerGuid != playerGuid)
        {
            observedPlayerGuid = playerGuid;
            firstPlayerSeenMs = nowMs;
            return;
        }

        if (nowMs - firstPlayerSeenMs < 3000)
            return;

        NativeActionBarRefreshPending = false;

        uint32_t signaled = 0;
        uintptr_t* actionButtons = NativeActionButtons();
        for (uint32_t slot = 0; slot < 144; ++slot)
        {
            uint32_t const action = static_cast<uint32_t>(actionButtons[slot]);
            if (!action || (action & 0xF0000000u) != 0 || !HasReasonableSpellId(action))
                continue;

            ForcedStreamedSpellRows.insert(action);
            if (!SpellCacheStreaming::HasSpell(action))
                continue;

            SignalActionBarSlotChanged(slot);
            ++signaled;
        }

        for (auto const& entry : StreamedActionBarEntries)
        {
            uint32_t const slot = entry.first;
            uint32_t const spellId = entry.second;
            if (!IsValidActionBarSlot(slot) || !HasReasonableSpellId(spellId) || !SpellCacheStreaming::HasSpell(spellId))
                continue;

            CGActionBar_SetAction_StreamedActionBar(slot, spellId, 1, 0);
            RecordNativeActionBarSnapshot(slot);
            SignalActionBarSlotChanged(slot);
            ++signaled;
        }

        static uint32_t logCount = 0;
        if (logCount < 80)
        {
            LOG_INFO << "Applied deferred native actionbar refresh"
                << "signaled" << signaled
                << "entries" << static_cast<uint32_t>(StreamedActionBarEntries.size());
            ++logCount;
        }
    }

    bool DataStoreIsRead(void* packet)
    {
        if (!packet)
            return true;

        void** const vtable = *reinterpret_cast<void***>(packet);
        if (!vtable)
            return true;

        auto const isRead = reinterpret_cast<int(__thiscall*)(void*)>(vtable[5]);
        return !isRead || isRead(packet) != 0;
    }

    void AddStreamedSpellCooldownHistory(uint32_t spellId, uint32_t forcedRecoveryTime, uint32_t packetTimeMs, uint32_t isPet, bool onEventFlag, bool ignoreCooldownOnEvent)
    {
        ClientData::SpellRow row{};
        if (!SpellCacheStreaming::TryGetSpellRow(spellId, row))
        {
            SpellCacheStreaming::RequestSpell(spellId);
            return;
        }

        bool const hasCooldownOnEvent = !ignoreCooldownOnEvent && Spell_C_HasCooldownOnEvent_SpellCache(&row, isPet ? CGPetInfo_C::GetPet(0) : ClntObjMgr::GetActivePlayer());
        uint32_t recoveryTime = forcedRecoveryTime ? forcedRecoveryTime : row.m_recoveryTime;
        if (!forcedRecoveryTime)
        {
            int32_t flatMod = 0;
            int32_t pctMod = 0;
            if (SpellRec_GetModifiedStats_SpellCache(&row, 11, &flatMod, &pctMod))
                recoveryTime = static_cast<uint32_t>(pctMod * (static_cast<int32_t>(recoveryTime) + flatMod) / 100);
        }

        uint32_t categoryRecoveryTime = 0;
        if (!forcedRecoveryTime)
        {
            categoryRecoveryTime = row.m_categoryRecoveryTime;
            AdjustCategoryRecoveryTime_SpellCache(&categoryRecoveryTime, &row, isPet);
        }

        if (!recoveryTime && categoryRecoveryTime)
            recoveryTime = categoryRecoveryTime;

        uint32_t startRecoveryCategory = 0;
        uint32_t startRecoveryTime = 0;
        if (onEventFlag && !hasCooldownOnEvent)
        {
            startRecoveryCategory = row.m_startRecoveryCategory;
            startRecoveryTime = row.m_startRecoveryTime;

            int32_t flatMod = 0;
            int32_t pctMod = 0;
            if (SpellRec_GetModifiedStats_SpellCache(&row, 21, &flatMod, &pctMod))
                startRecoveryTime = static_cast<uint32_t>(pctMod * (static_cast<int32_t>(startRecoveryTime) + flatMod) / 100);
        }

        void* const history = reinterpret_cast<void*>(0x00D3F5AC + 24u * isPet);
        SpellHistory_AddHistory_SpellCache(
            history,
            spellId,
            0,
            packetTimeMs,
            recoveryTime,
            row.m_category,
            packetTimeMs,
            categoryRecoveryTime,
            hasCooldownOnEvent ? 1 : 0,
            startRecoveryCategory,
            startRecoveryTime);

        static uint32_t logCount = 0;
        if (logCount < 120)
        {
            LOG_INFO << "Added streamed spell cooldown history"
                << "spell" << spellId
                << "recovery" << recoveryTime
                << "category" << row.m_category
                << "categoryRecovery" << categoryRecoveryTime
                << "startCategory" << startRecoveryCategory
                << "startRecovery" << startRecoveryTime
                << "isPet" << isPet;
            ++logCount;
        }
    }

    void LogSpellbookDirectReaderDiagnostics(char const* source, bool force = false)
    {
        static uint32_t updateSpellsLogs = 0;
        static uint32_t updateUsableLogs = 0;
        uint32_t& logCount = std::strcmp(source, "UpdateUsable") == 0 ? updateUsableLogs : updateSpellsLogs;
        if (!force && logCount >= 80)
            return;

        RebuildKnownSpellbookOrder();

        uint32_t sampled = 0;
        uint32_t streamedReady = 0;
        uint32_t nativeUnavailable = 0;
        uint32_t nativeAvailable = 0;
        uint32_t firstMissingNative = 0;

        for (uint32_t spellId : KnownSpellbookOrder)
        {
            if (sampled >= 64)
                break;

            ++sampled;
            if (SpellCacheStreaming::HasSpell(spellId))
            {
                ++streamedReady;
                if (NativeSpellDbCanLookup(spellId))
                {
                    ++nativeAvailable;
                }
                else
                {
                    ++nativeUnavailable;
                    if (!firstMissingNative)
                        firstMissingNative = spellId;
                }
            }
        }

        LOG_INFO << "Spellbook direct-reader diagnostics"
            << "source" << source
            << "knownVisible" << static_cast<uint32_t>(KnownSpellbookOrder.size())
            << "sampled" << sampled
            << "streamedReady" << streamedReady
            << "nativeAvailable" << nativeAvailable
            << "nativeUnavailable" << nativeUnavailable
            << "firstMissingNative" << firstMissingNative
            << "nativeSlotCount" << PlayerSpellSlotMapSize()
            << "nativeVisibleCount" << VisibleSpellbookCount()
            << "nativeTabCount" << SpellbookTabCount();

        ++logCount;
    }

    void SyncKnownSpellRowsPlayer(uint64_t playerGuid)
    {
        if (!playerGuid)
            return;

        if (KnownSpellRowsPlayerGuid == playerGuid)
            return;

        if (!KnownSpellRowsPlayerGuid)
        {
            KnownSpellRowsPlayerGuid = playerGuid;
            LOG_INFO << "Bound streamed known spell state to active player" << playerGuid
                << "knownCount" << static_cast<uint32_t>(KnownSpellRows.size());
            return;
        }

        KnownSpellRowsPlayerGuid = playerGuid;
        KnownSpellRows.clear();
        ReadyKnownSpellUiRows.clear();
        ReadyKnownSpellUiQueue.clear();
        ReadyKnownSpellUiRecords.clear();
        NativeKnownSpellReplayQueue.clear();
        NativeSpellbookAppliedRows.clear();
        AppliedStreamedLanguageSlots.clear();
        PendingKnownSpellAdds.clear();
        KnownSpellbookEntries.clear();
        KnownSpellbookOrder.clear();
        KnownSpellbookTabs.clear();
        SkillLineAbilitySpellTabs.clear();
        SkillLineCanLinkCache.clear();
        SkillLineAbilitySpellTabsLoaded = false;
        SkillLineAbilitySpellTabsLoadAttempted = false;
        MarkKnownSpellbookModelDirty();
        PublishedKnownSpellbookRevision = KnownSpellbookRevision;
        LOG_INFO << "Reset streamed known spell state for active player" << playerGuid;
    }

    bool AddKnownSpellRow(uint32_t spellId)
    {
        if (!HasReasonableSpellId(spellId))
            return false;

        return KnownSpellRows.insert(spellId).second;
    }

    bool HasKnownSpellRow(uint32_t spellId)
    {
        return KnownSpellRows.find(spellId) != KnownSpellRows.end();
    }

    bool RemoveKnownSpellRow(uint32_t spellId)
    {
        bool removed = KnownSpellRows.erase(spellId) != 0;
        ReadyKnownSpellUiRows.erase(spellId);
        ReadyKnownSpellUiRecords.erase(spellId);
        ForcedStreamedSpellRows.erase(spellId);
        KnownSpellbookEntries.erase(spellId);

        auto removeQueuedSpell = [spellId](auto& queue)
        {
            queue.erase(
                std::remove_if(queue.begin(), queue.end(), [spellId](auto const& value)
                {
                    if constexpr (std::is_same_v<std::decay_t<decltype(value)>, uint32_t>)
                        return value == spellId;
                    else
                        return value.spellId == spellId;
                }),
                queue.end());
        };

        removeQueuedSpell(ReadyKnownSpellUiQueue);
        removeQueuedSpell(PendingKnownSpellAdds);
        removeQueuedSpell(NativeKnownSpellReplayQueue);

        if (KnownSpellBitsetCanLookup(spellId))
            KnownSpellBitset()[spellId >> 5] &= ~(1u << (spellId & 0x1F));

        if (removed)
            MarkKnownSpellbookModelDirty();

        return removed;
    }

    void RebuildKnownSpellbookOrder();
    char const* GetSpellIconTextureById(uint32_t iconId);

    char const* GetSpellIconTexture(uint32_t spellId)
    {
        ClientData::SpellRow row{};
        if (!SpellCacheStreaming::TryBuildSpellRow(spellId, row))
            return nullptr;

        return GetSpellIconTextureById(row.m_spellIconID);
    }

    char const* GetSpellIconTextureById(uint32_t iconId)
    {
        SpellIconRow* iconRow = reinterpret_cast<SpellIconRow*>(
            ClientDB::GetRow(reinterpret_cast<void*>(SPELL_ICON_DB_RECORDS_ADDRESS), iconId));
        if (!iconRow || !iconRow->m_textureFilename || !*iconRow->m_textureFilename)
            return nullptr;

        return iconRow->m_textureFilename;
    }

    bool NativeDbHasRow(uintptr_t dbAddress, uint32_t rowId)
    {
        return rowId != 0 && ClientDB::GetRow(reinterpret_cast<void*>(dbAddress), rowId) != nullptr;
    }

    void RecordStreamedSpellDbLookup(uint32_t spellId, ClientData::SpellRow const& row, void* returnAddress)
    {
        uint32_t& hits = StreamedSpellDbLookupHits[spellId];
        if (hits < 0xFFFFFFFF)
            ++hits;

        uintptr_t const caller = reinterpret_cast<uintptr_t>(returnAddress);
        StreamedSpellDbLookupLastCaller[spellId] = caller;

        bool const visualCaller =
            (caller >= 0x006FF000 && caller < 0x00705000)
            || (caller >= 0x00802000 && caller < 0x00804000);
        if (visualCaller)
        {
            uint32_t& visualHits = StreamedSpellDbVisualLookupHits[spellId];
            if (visualHits < 0xFFFFFFFF)
                ++visualHits;
            StreamedSpellDbVisualLookupLastCaller[spellId] = caller;
        }

        if (caller >= 0x005A7000 && caller < 0x005AD000)
        {
            uint32_t& actionbarHits = StreamedSpellDbActionbarLookupHits[spellId];
            if (actionbarHits < 0xFFFFFFFF)
                ++actionbarHits;
        }

        if (caller >= 0x0053B000 && caller < 0x00543000)
        {
            uint32_t& spellbookHits = StreamedSpellDbSpellbookLookupHits[spellId];
            if (spellbookHits < 0xFFFFFFFF)
                ++spellbookHits;
        }

        static std::unordered_set<uint64_t> logged;
        uint64_t const key = (static_cast<uint64_t>(spellId) << 32) ^ static_cast<uint64_t>(caller);
        if (logged.size() < 160 && logged.insert(key).second)
        {
            LOG_INFO << "Streamed SpellRec returned to native DB caller"
                << "spell" << spellId
                << "caller" << caller
                << "visual1" << row.m_spellVisualID[0]
                << "visual2" << row.m_spellVisualID[1]
                << "icon" << row.m_spellIconID
                << "missile" << row.m_spellMissileID;
        }
    }

    char const* GetSkillLineName(uint32_t skillLineId)
    {
        if (!skillLineId)
            return "General";

        static std::unordered_map<uint32_t, std::string> skillLineNames;
        auto found = skillLineNames.find(skillLineId);
        if (found != skillLineNames.end())
            return found->second.c_str();

        std::string name = "Skill " + std::to_string(skillLineId);
        SkillLineRow* row = reinterpret_cast<SkillLineRow*>(
            ClientDB::GetRow(reinterpret_cast<void*>(SKILL_LINE_DB_RECORDS_ADDRESS), skillLineId));
        bool const hasName =
            row
            && IsReadableMemory(row, sizeof(*row))
            && row->m_displayName_lang
            && IsReadableMemory(row->m_displayName_lang, 1)
            && row->m_displayName_lang[0];

        static uint32_t logCount = 0;
        if (logCount < 80)
        {
            LOG_INFO << "Streamed spellbook skill line row lookup"
                << "skillLine" << skillLineId
                << "row" << (row != nullptr)
                << "hasName" << hasName;
            ++logCount;
        }

        if (hasName)
            name = row->m_displayName_lang;

        auto inserted = skillLineNames.emplace(skillLineId, std::move(name));
        return inserted.first->second.c_str();
    }

    char const* GetSkillLineIconTexture(uint32_t skillLineId)
    {
        (void)skillLineId;
        return "Interface\\Icons\\Ability_Kick";
    }

    bool TryGetActivePlayerRaceClass(uint8_t& raceId, uint8_t& classId)
    {
        CGPlayer* activePlayer = reinterpret_cast<CGPlayer*>(
            ClntObjMgr::ObjectPtr(ClntObjMgr::GetActivePlayer(), TYPEMASK_PLAYER));
        if (!activePlayer || !activePlayer->unitBase.unitData)
        {
            raceId = 0;
            classId = 0;
            return false;
        }

        raceId = activePlayer->unitBase.unitData->unitBytes0.raceID;
        classId = activePlayer->unitBase.unitData->unitBytes0.classID;
        return raceId != 0 && classId != 0;
    }

    bool SkillLineAbilityMatchesPlayer(SkillLineAbilityRow const& row, uint8_t raceId, uint8_t classId)
    {
        uint32_t const raceMask = raceId > 0 && raceId <= 32 ? (1u << (raceId - 1)) : 0;
        uint32_t const classMask = classId > 0 && classId <= 32 ? (1u << (classId - 1)) : 0;

        if (row.m_raceMask && raceMask && (row.m_raceMask & raceMask) == 0)
            return false;
        if (row.m_classMask && classMask && (row.m_classMask & classMask) == 0)
            return false;
        if (row.m_excludeRace && raceMask && (row.m_excludeRace & raceMask) != 0)
            return false;
        if (row.m_excludeClass && classMask && (row.m_excludeClass & classMask) != 0)
            return false;

        return true;
    }

    bool SkillLineCanLink(uint32_t skillLineId)
    {
        if (!skillLineId)
            return false;

        auto found = SkillLineCanLinkCache.find(skillLineId);
        if (found != SkillLineCanLinkCache.end())
            return found->second;

        SkillLineRow row{};
        bool const canLink = ClientDB::GetLocalizedRow(reinterpret_cast<void*>(SKILL_LINE_DB_ADDRESS), skillLineId, &row)
            && row.m_canLink != 0;
        SkillLineCanLinkCache[skillLineId] = canLink;
        return canLink;
    }

    std::string StreamedSpellSkillLinePersistencePath()
    {
        return "Cache\\SpellCacheSkillLines.txt";
    }

    void EnsureStreamedSpellSkillLinesLoaded()
    {
        if (StreamedSpellSkillLinesLoaded)
            return;

        StreamedSpellSkillLinesLoaded = true;
        std::ifstream in(StreamedSpellSkillLinePersistencePath());
        if (!in)
            return;

        uint32_t spellId = 0;
        uint32_t spellDataHash = 0;
        uint32_t skillLineId = 0;
        uint32_t loaded = 0;
        while (in >> spellId >> spellDataHash >> skillLineId)
        {
            if (!HasReasonableSpellId(spellId) || !spellDataHash)
                continue;

            StreamedSpellSkillLines[spellId] = { spellDataHash, skillLineId };
            ++loaded;
        }

        LOG_INFO << "Loaded streamed spell skill line sidecar entries" << loaded;
    }

    void SaveStreamedSpellSkillLines()
    {
        EnsureStreamedSpellSkillLinesLoaded();
        CreateDirectoryA("Cache", nullptr);

        std::ofstream out(StreamedSpellSkillLinePersistencePath(), std::ios::trunc);
        if (!out)
        {
            static uint32_t logCount = 0;
            if (logCount < 20)
            {
                LOG_ERROR << "Failed to save streamed spell skill line sidecar"
                    << "entries" << static_cast<uint32_t>(StreamedSpellSkillLines.size());
                ++logCount;
            }
            return;
        }

        for (auto const& entry : StreamedSpellSkillLines)
            out << entry.first << ' ' << entry.second.first << ' ' << entry.second.second << '\n';
    }

    void RecordStreamedSpellSkillLine(uint32_t spellId, uint32_t spellDataHash, uint32_t skillLineId)
    {
        if (!HasReasonableSpellId(spellId) || !spellDataHash)
            return;

        EnsureStreamedSpellSkillLinesLoaded();
        StreamedSpellSkillLines[spellId] = { spellDataHash, skillLineId };
        SaveStreamedSpellSkillLines();

        static uint32_t logCount = 0;
        if (logCount < 160)
        {
            LOG_INFO << "Recorded streamed spell skill line"
                << "spell" << spellId
                << "hash" << spellDataHash
                << "skillLine" << skillLineId;
            ++logCount;
        }
    }

    bool BuildSkillLineAbilitySpellTabIndex()
    {
        if (SkillLineAbilitySpellTabsLoaded)
            return true;

        SkillLineAbilitySpellTabsLoaded = true;
        LOG_ERROR << "Runtime SkillLineAbility.dbc scan disabled; raw client DB scan violates login-time DB access contract";
        return true;

#if 0
        if (SkillLineAbilityIndexDebugLogCount < 40)
        {
            LOG_INFO << "Begin streamed spellbook SkillLineAbility tab index build"
                << "loaded" << SkillLineAbilitySpellTabsLoaded
                << "attempted" << SkillLineAbilitySpellTabsLoadAttempted;
            ++SkillLineAbilityIndexDebugLogCount;
        }

        uint8_t raceId = 0;
        uint8_t classId = 0;
        if (!TryGetActivePlayerRaceClass(raceId, classId))
        {
            if (SkillLineAbilityIndexDebugLogCount < 40)
            {
                LOG_INFO << "Deferred streamed spellbook tab index; active player race/class not ready";
                ++SkillLineAbilityIndexDebugLogCount;
            }
            return false;
        }

        auto const* numRows = reinterpret_cast<int32_t const*>(SKILL_LINE_ABILITY_NUM_ROWS_ADDRESS);
        auto const* firstRowPointer = reinterpret_cast<SkillLineAbilityRow const* const*>(SKILL_LINE_ABILITY_FIRST_ROW_ADDRESS);
        bool const numRowsReadable = IsReadableMemory(numRows, sizeof(*numRows));
        bool const firstRowReadable = IsReadableMemory(firstRowPointer, sizeof(*firstRowPointer));
        int32_t const numRowsValue = numRowsReadable ? *numRows : -1;
        SkillLineAbilityRow const* const firstRowValue = firstRowReadable ? *firstRowPointer : nullptr;
        if (SkillLineAbilityIndexDebugLogCount < 40)
        {
            LOG_INFO << "Streamed spellbook SkillLineAbility raw DB state"
                << "numRowsReadable" << numRowsReadable
                << "firstRowReadable" << firstRowReadable
                << "numRows" << numRowsValue
                << "firstRow" << firstRowValue
                << "race" << static_cast<uint32_t>(raceId)
                << "class" << static_cast<uint32_t>(classId);
            ++SkillLineAbilityIndexDebugLogCount;
        }

        if (!IsReadableMemory(numRows, sizeof(*numRows))
            || !IsReadableMemory(firstRowPointer, sizeof(*firstRowPointer))
            || *numRows <= 0
            || static_cast<uint32_t>(*numRows) > SKILL_LINE_ABILITY_MAX_REASONABLE_ROWS
            || !*firstRowPointer)
        {
            if (!SkillLineAbilitySpellTabsLoadAttempted)
            {
                LOG_INFO << "Deferred streamed spellbook tab index; SkillLineAbility.dbc not ready"
                    << "numRowsReadable" << IsReadableMemory(numRows, sizeof(*numRows))
                    << "firstRowReadable" << IsReadableMemory(firstRowPointer, sizeof(*firstRowPointer));
                SkillLineAbilitySpellTabsLoadAttempted = true;
            }
            return false;
        }

        SkillLineAbilityRow const* rows = *firstRowPointer;
        std::unordered_map<uint32_t, uint32_t> index;
        index.reserve(static_cast<size_t>(*numRows));

        if (SkillLineAbilityIndexDebugLogCount < 40)
        {
            LOG_INFO << "Scanning streamed spellbook SkillLineAbility rows"
                << "rows" << *numRows
                << "firstRow" << rows;
            ++SkillLineAbilityIndexDebugLogCount;
        }

        uint32_t scanned = 0;
        uint32_t matched = 0;
        uint32_t linked = 0;
        for (int32_t i = 0; i < *numRows; ++i)
        {
            if ((i == 0 || (i % 10000) == 0) && SkillLineAbilityIndexDebugLogCount < 80)
            {
                LOG_INFO << "Scanning streamed spellbook SkillLineAbility progress"
                    << "index" << i
                    << "rows" << *numRows
                    << "matched" << matched;
                ++SkillLineAbilityIndexDebugLogCount;
            }

            SkillLineAbilityRow const* row = rows + i;
            if (!IsReadableMemory(row, sizeof(*row)))
            {
                LOG_ERROR << "Stopped streamed spellbook SkillLineAbility scan; unreadable row"
                    << "index" << i
                    << "row" << row
                    << "scanned" << scanned;
                break;
            }

            ++scanned;
            if (!HasReasonableSpellId(row->m_spell)
                || !row->m_skillLine
                || !SkillLineAbilityMatchesPlayer(*row, raceId, classId))
            {
                continue;
            }

            auto found = index.find(row->m_spell);
            if (found == index.end())
            {
                index.emplace(row->m_spell, row->m_skillLine);
                ++matched;
                if (SkillLineCanLink(row->m_skillLine))
                    ++linked;
                continue;
            }

            if (!SkillLineCanLink(found->second) && SkillLineCanLink(row->m_skillLine))
                found->second = row->m_skillLine;
        }

        SkillLineAbilitySpellTabs = std::move(index);
        SkillLineAbilitySpellTabsLoaded = true;
        LOG_INFO << "Built streamed spellbook SkillLineAbility tab index"
            << "rows" << scanned
            << "spells" << static_cast<uint32_t>(SkillLineAbilitySpellTabs.size())
            << "matched" << matched
            << "linked" << linked
            << "race" << static_cast<uint32_t>(raceId)
            << "class" << static_cast<uint32_t>(classId);
        return true;
#endif
    }

    uint32_t ResolveStreamedSpellbookSkillLine(uint32_t spellId)
    {
        if (!HasReasonableSpellId(spellId))
            return 0;

        EnsureStreamedSpellSkillLinesLoaded();

        SpellCacheRow* spell = GlobalCDBCMap.getRow<SpellCacheRow>("Spell", int(spellId));
        auto found = StreamedSpellSkillLines.find(spellId);
        if (!spell || found == StreamedSpellSkillLines.end() || found->second.first != spell->spellDataHash)
            return 0;

        return found->second.second;
    }

    uint32_t ResolveNativeSpellbookTabKey(uint32_t spellId)
    {
        if (!HasReasonableSpellId(spellId))
            return 0;

        uint64_t const playerGuid = ClntObjMgr::GetActivePlayer();
        void* player = ClntObjMgr::ObjectPtr(playerGuid, TYPEMASK_PLAYER);
        if (!player)
            return 0;

        void* skillLine = CGPlayer_C_GetCreatureFamilySkillLineBySpellID_ActionState_SpellCache(player, spellId);
        if (!skillLine || !IsReadableMemory(static_cast<uint8_t*>(skillLine) + 4, sizeof(uint32_t)))
            return 0;

        uint32_t const nativeSkillLineId = *reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(skillLine) + 4);
        if (!nativeSkillLineId)
            return 0;

        uint8_t raceId = 0;
        uint8_t classId = 0;
        if (!TryGetActivePlayerRaceClass(raceId, classId))
            return 0;

        void* raceClassInfo = SkillLineRaceClassInfoLookup_SpellCache(raceId, classId, nativeSkillLineId);
        if (!raceClassInfo || !IsReadableMemory(static_cast<uint8_t*>(raceClassInfo) + 0x10, sizeof(uint32_t)))
            return 0;

        uint32_t const flags = *reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(raceClassInfo) + 0x10);
        if ((flags & 0x80) != 0)
            return 0;

        static uint32_t logCount = 0;
        if (logCount < 120)
        {
            uint32_t const sidecarSkillLineId = ResolveStreamedSpellbookSkillLine(spellId);
            LOG_INFO << "Resolved streamed spellbook tab through native classifier"
                << "spell" << spellId
                << "sidecar" << sidecarSkillLineId
                << "native" << nativeSkillLineId
                << "flags" << flags;
            ++logCount;
        }

        return nativeSkillLineId;
    }

    bool TryGetKnownSpellbookSpellBySlot(uint32_t oneBasedSlot, uint32_t& spellId)
    {
        if (!oneBasedSlot)
            return false;

        if (!ReadyKnownSpellUiQueue.empty())
            return false;

        RebuildKnownSpellbookOrder();
        uint32_t const index = oneBasedSlot - 1;
        if (index >= KnownSpellbookOrder.size())
            return false;

        spellId = KnownSpellbookOrder[index];
        return true;
    }

    bool TryResolveSpellLuaArg(lua_State* L, uint32_t& spellId)
    {
        if (!ClientLua::IsNumber(L, 1))
            return false;

        uint32_t const firstArg = static_cast<uint32_t>(ClientLua::GetNumber(L, 1, 0));
        if (ClientLua::GetTop(L) == 1)
        {
            spellId = firstArg;
            return true;
        }

        if (ClientLua::IsString(L, 2))
        {
            std::string const bookType = ClientLua::GetString(L, 2, "");
            if (bookType == "spell" || bookType == "BOOKTYPE_SPELL")
                return TryGetKnownSpellbookSpellBySlot(firstArg, spellId);
        }

        return false;
    }

    bool IsSpellbookSpellLuaArg(lua_State* L)
    {
        if (!ClientLua::IsNumber(L, 1) || ClientLua::GetTop(L) <= 1 || !ClientLua::IsString(L, 2))
            return false;

        std::string const bookType = ClientLua::GetString(L, 2, "");
        return bookType == "spell" || bookType == "BOOKTYPE_SPELL";
    }

    int PushNil(lua_State* L)
    {
        ClientLua::PushNil(L);
        return 1;
    }

    int PushTwoNils(lua_State* L)
    {
        ClientLua::PushNil(L);
        ClientLua::PushNil(L);
        return 2;
    }

    int PushThreeZeros(lua_State* L)
    {
        ClientLua::PushNumber(L, 0.0);
        ClientLua::PushNumber(L, 0.0);
        ClientLua::PushNumber(L, 0.0);
        return 3;
    }

    void SignalStreamedSpellbookRefresh()
    {
        if (!SPELL_CACHE_ENABLE_UI_INTEGRATION)
            return;

        FrameScript::SignalEvent(242, nullptr);
        FrameScript::SignalEvent(448, const_cast<char*>("%d"), 1);
        if (ClientLua::State())
        {
            ClientLua::DoString(
                "if SpellBookFrame and SpellBookFrame:IsShown() and SpellBookFrame_Update then SpellBookFrame_Update() end "
                "if ActionButton_Update then local p={'ActionButton','MultiBarBottomLeftButton','MultiBarBottomRightButton','MultiBarRightButton','MultiBarLeftButton','BonusActionButton'} for _,n in ipairs(p) do for i=1,12 do local b=_G[n..i]; if b then ActionButton_Update(b) end end end end",
                ClientLua::State());
        }
    }

    void ScheduleKnownSpellUiWork(PendingKnownSpellAdd const& pending, char const* reason)
    {
        if (!SPELL_CACHE_ENABLE_UI_INTEGRATION)
            return;

        if (!HasReasonableSpellId(pending.spellId))
            return;

        if (ReadyKnownSpellUiRows.insert(pending.spellId).second)
            ReadyKnownSpellUiQueue.push_back(pending.spellId);
        ReadyKnownSpellUiRecords[pending.spellId] = pending;

        static uint32_t logCount = 0;
        if (logCount < 300)
        {
            LOG_INFO << "Scheduled streamed known spell UI work"
                << "reason" << reason
                << "spell" << pending.spellId
                << "category" << pending.spellCategory
                << "learned" << pending.learned
                << "addToSpellbook" << pending.addToSpellbook
                << "readyQueue" << static_cast<uint32_t>(ReadyKnownSpellUiQueue.size());
            ++logCount;
        }
    }

    bool SpellHasAura(uint32_t spellId, uint32_t auraId)
    {
        for (uint32_t i = 0; i < 32; ++i)
        {
            SpellEffectCacheRow* effect = GlobalCDBCMap.getRow<SpellEffectCacheRow>(
                "SpellEffect", SpellEffectCacheKey(spellId, i));
            if (effect && effect->effectApplyAuraName == auraId)
                return true;
        }

        return false;
    }

    bool SpellHasEffect(uint32_t spellId, uint32_t effectId)
    {
        for (uint32_t i = 0; i < 32; ++i)
        {
            SpellEffectCacheRow* effect = GlobalCDBCMap.getRow<SpellEffectCacheRow>(
                "SpellEffect", SpellEffectCacheKey(spellId, i));
            if (effect && effect->effect == effectId)
                return true;
        }

        return false;
    }

    int32_t SpellLanguageIndex(uint32_t spellId)
    {
        for (uint32_t i = 0; i < 32; ++i)
        {
            SpellEffectCacheRow* effect = GlobalCDBCMap.getRow<SpellEffectCacheRow>(
                "SpellEffect", SpellEffectCacheKey(spellId, i));
            if (effect && effect->effect == SPELL_EFFECT_LANGUAGE)
                return effect->effectMiscValue;
        }

        return -1;
    }

    bool BuildKnownSpellbookEntry(PendingKnownSpellAdd const& pending, KnownSpellbookEntry& out)
    {
        SpellCacheRow* spell = GlobalCDBCMap.getRow<SpellCacheRow>("Spell", int(pending.spellId));
        if (!spell || !SpellCacheStreaming::HasSpell(pending.spellId))
            return false;

        int32_t const languageIndex = SpellLanguageIndex(pending.spellId);
        bool const hasValidStanceBarOrder = spell->stanceBarOrder != 0
            && spell->stanceBarOrder != 0xFFFFFFFFu;
        bool const isStance = hasValidStanceBarOrder
            || (spell->attributesEx & SPELL_ATTR_EX_STANCE) != 0
            || SpellHasAura(pending.spellId, SPELL_AURA_MOD_SHAPESHIFT);
        bool const isSkillGrant = SpellHasEffect(pending.spellId, SPELL_EFFECT_SKILL);
        bool const isHiddenClient = (spell->attributes & STREAMED_SPELL_ATTR0_HIDDEN_CLIENTSIDE) != 0;
        bool const isTradeskill = (spell->attributes & STREAMED_SPELL_ATTR0_TRADESPELL) != 0;
        bool const isNativeHiddenSpellbook = (spell->attributesEx4 & STREAMED_SPELL_ATTR_EX_D_HIDE_FROM_SPELLBOOK) != 0;
        bool const isCompanion = StreamedSpellWillSummonCritter(pending.spellId);
        bool const hasName = spell->spellName && *spell->spellName;

        out = {};
        out.spellId = pending.spellId;
        out.tabKey = ResolveNativeSpellbookTabKey(pending.spellId);
        out.spellLevel = spell->spellLevel;
        out.stanceBarOrder = spell->stanceBarOrder;
        out.spellCategory = pending.spellCategory;
        out.languageIndex = languageIndex;
        out.addToSpellbook = pending.addToSpellbook != 0;
        out.isLanguage = languageIndex >= 0;
        out.isStance = isStance;
        out.isSkillGrant = isSkillGrant;
        out.isHiddenClient = isHiddenClient;
        out.isTradeskill = isTradeskill;
        out.isNativeHiddenSpellbook = isNativeHiddenSpellbook;
        out.hasName = hasName;
        out.visibleInSpellbook = out.addToSpellbook
            && !out.isLanguage
            && !out.isSkillGrant
            && !out.isHiddenClient
            && !out.isTradeskill
            && !out.isNativeHiddenSpellbook
            && !isCompanion
            && out.hasName;

        static uint32_t stanceLogCount = 0;
        if ((isStance || spell->stanceBarOrder || (spell->attributesEx & SPELL_ATTR_EX_STANCE) != 0) && stanceLogCount < 80)
        {
            LOG_INFO << "Classified streamed known spell stance"
                << "spell" << pending.spellId
                << "isStance" << isStance
                << "stanceBarOrder" << spell->stanceBarOrder
                << "validStanceBarOrder" << hasValidStanceBarOrder
                << "attributesEx" << spell->attributesEx
                << "hasShapeshiftAura" << SpellHasAura(pending.spellId, SPELL_AURA_MOD_SHAPESHIFT)
                << "addToSpellbook" << out.addToSpellbook
                << "hidden" << out.isHiddenClient
                << "nativeHidden" << out.isNativeHiddenSpellbook
                << "hasName" << out.hasName;
            ++stanceLogCount;
        }
        return true;
    }

    void RebuildKnownSpellbookOrder()
    {
        if (!KnownSpellbookModelDirty)
            return;

        KnownSpellbookOrder.clear();
        KnownSpellbookTabs.clear();
        KnownSpellbookOrder.reserve(KnownSpellbookEntries.size());
        for (auto const& entry : KnownSpellbookEntries)
            if (entry.second.visibleInSpellbook)
                KnownSpellbookOrder.push_back(entry.first);

        std::sort(KnownSpellbookOrder.begin(), KnownSpellbookOrder.end(), [](uint32_t lhs, uint32_t rhs)
        {
            KnownSpellbookEntry const& left = KnownSpellbookEntries[lhs];
            KnownSpellbookEntry const& right = KnownSpellbookEntries[rhs];
            if (left.tabKey != right.tabKey)
                return left.tabKey < right.tabKey;
            if (left.spellLevel != right.spellLevel)
                return left.spellLevel < right.spellLevel;
            return left.spellId < right.spellId;
        });

        for (uint32_t i = 0; i < KnownSpellbookOrder.size(); ++i)
        {
            uint32_t const spellId = KnownSpellbookOrder[i];
            KnownSpellbookEntry const& entry = KnownSpellbookEntries[spellId];
            if (KnownSpellbookTabs.empty() || KnownSpellbookTabs.back().tabKey != entry.tabKey)
            {
                KnownSpellbookTab tab{};
                tab.tabKey = entry.tabKey;
                tab.offset = i;
                tab.count = 0;
                tab.name = GetSkillLineName(entry.tabKey);
                tab.icon = GetSkillLineIconTexture(entry.tabKey);
                KnownSpellbookTabs.push_back(tab);
            }

            ++KnownSpellbookTabs.back().count;
        }

        KnownSpellbookModelDirty = false;
    }

    bool EnsureNativeSpellbookTabStorage(uint32_t tabCount)
    {
        if (tabCount > SPELLBOOK_MAX_TABS)
            return false;

        uint32_t** tabs = SpellbookTabArray();
        if (!IsPlausibleClientPointer(tabs))
        {
            tabs = static_cast<uint32_t**>(SMemAlloc_SpellCache(
                SPELLBOOK_MAX_TABS * sizeof(uint32_t*),
                ".\\SpellCacheStreaming.cpp",
                -2,
                0));
            if (!tabs)
                return false;

            std::memset(tabs, 0, SPELLBOOK_MAX_TABS * sizeof(uint32_t*));
            SpellbookTabArray() = tabs;
            SpellbookTabArrayCapacity() = SPELLBOOK_MAX_TABS;
            SpellbookTabAllocatedCount() = SPELLBOOK_MAX_TABS;
            SpellbookTabAllocationChunk() = SPELLBOOK_MAX_TABS;
        }

        if (SpellbookTabArrayCapacity() < tabCount || SpellbookTabAllocatedCount() < tabCount)
            return false;

        for (uint32_t i = 0; i < tabCount; ++i)
        {
            if (IsPlausibleClientPointer(tabs[i]))
                continue;

            tabs[i] = static_cast<uint32_t*>(SMemAlloc_SpellCache(
                3 * sizeof(uint32_t),
                ".\\SpellCacheStreaming.cpp",
                -2,
                0));
            if (!tabs[i])
                return false;

            std::memset(tabs[i], 0, 3 * sizeof(uint32_t));
        }

        return true;
    }

    bool EnsureNativeLanguageStorage(uint32_t languageCount)
    {
        if (languageCount > SPELLBOOK_MAX_LANGUAGES)
            return false;

        uint32_t* languages = LanguageSpellIds();
        if (!IsPlausibleClientPointer(languages) || LanguageCapacity() < languageCount)
        {
            languages = static_cast<uint32_t*>(SMemAlloc_SpellCache(
                SPELLBOOK_MAX_LANGUAGES * sizeof(uint32_t),
                ".\\SpellCacheStreaming.cpp",
                -2,
                0));
            if (!languages)
                return false;

            std::memset(languages, 0, SPELLBOOK_MAX_LANGUAGES * sizeof(uint32_t));
            LanguageSpellIds() = languages;
            LanguageCapacity() = SPELLBOOK_MAX_LANGUAGES;
        }

        return LanguageCapacity() >= languageCount;
    }

    void ApplyStreamedLanguageState()
    {
        uint32_t const nativeCount = LanguageCount();
        uint32_t* const nativeLanguages = LanguageSpellIds();
        if (!nativeCount
            || nativeCount > SPELLBOOK_MAX_LANGUAGES
            || !IsReadableMemory(nativeLanguages, nativeCount * sizeof(uint32_t)))
        {
            static uint32_t logCount = 0;
            if (logCount < 40)
            {
                LOG_INFO << "Deferred streamed language slot apply; native storage not ready"
                    << "count" << nativeCount
                    << "capacity" << LanguageCapacity()
                    << "ptr" << nativeLanguages;
                ++logCount;
            }
            return;
        }

        std::unordered_map<uint32_t, uint32_t> desiredSlots;
        for (auto const& entry : KnownSpellbookEntries)
        {
            KnownSpellbookEntry const& language = entry.second;
            if (!language.isLanguage || language.languageIndex < 0)
                continue;

            uint32_t const languageId = static_cast<uint32_t>(language.languageIndex);
            if (languageId >= nativeCount)
                continue;

            auto found = desiredSlots.find(languageId);
            if (found == desiredSlots.end() || language.spellId < found->second)
                desiredSlots[languageId] = language.spellId;
        }

        uint32_t cleared = 0;
        for (auto const& applied : AppliedStreamedLanguageSlots)
        {
            uint32_t const languageId = applied.first;
            if (languageId >= nativeCount || desiredSlots.find(languageId) != desiredSlots.end())
                continue;

            if (nativeLanguages[languageId] == applied.second)
            {
                nativeLanguages[languageId] = 0;
                ++cleared;
            }
        }

        uint32_t written = 0;
        for (auto const& desired : desiredSlots)
        {
            uint32_t const languageId = desired.first;
            uint32_t const spellId = desired.second;
            if (nativeLanguages[languageId] != spellId)
            {
                nativeLanguages[languageId] = spellId;
                ++written;
            }
        }

        AppliedStreamedLanguageSlots = std::move(desiredSlots);
        if (written || cleared)
            CGameUI__Signal_EVENT_LANGUAGE_LIST_CHANGED_SpellCache();

        static uint32_t logCount = 0;
        if ((written || cleared) && logCount < 40)
        {
            LOG_INFO << "Applied streamed language slots"
                << "desired" << static_cast<uint32_t>(AppliedStreamedLanguageSlots.size())
                << "written" << written
                << "cleared" << cleared
                << "nativeCount" << nativeCount
                << "nativeCapacity" << LanguageCapacity();
            ++logCount;
        }
    }

    bool EnsureNativeShapeshiftStorage(uint32_t stanceCount)
    {
        if (stanceCount > SPELLBOOK_MAX_STANCES)
            return false;

        NativeShapeshiftEntry* rows = ShapeshiftRows();
        if (!IsPlausibleClientPointer(rows) || ShapeshiftCapacity() < stanceCount)
        {
            rows = static_cast<NativeShapeshiftEntry*>(SMemAlloc_SpellCache(
                SPELLBOOK_MAX_STANCES * sizeof(NativeShapeshiftEntry),
                ".\\SpellCacheStreaming.cpp",
                -2,
                0));
            if (!rows)
                return false;

            std::memset(rows, 0, SPELLBOOK_MAX_STANCES * sizeof(NativeShapeshiftEntry));
            ShapeshiftRows() = rows;
            ShapeshiftCapacity() = SPELLBOOK_MAX_STANCES;
            ShapeshiftAllocationChunk() = SPELLBOOK_MAX_STANCES;
        }

        return ShapeshiftCapacity() >= stanceCount
            && IsReadableMemory(rows, ShapeshiftCapacity() * sizeof(NativeShapeshiftEntry));
    }

    void SignalFrameEventByName(char const* eventName)
    {
        int32_t const eventId = FrameXMLExtensions::GetEventIdByName(eventName);
        if (eventId >= 0)
            FrameScript::SignalEvent(static_cast<uint32_t>(eventId), nullptr);
    }

    void SendStreamedCastSpellPacket(uint32_t spellId)
    {
        constexpr uint32_t CMSG_CAST_SPELL_OPCODE = 0x12E;
        constexpr uint32_t payloadSize = sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint32_t);
        constexpr uint32_t fullSize = sizeof(uint32_t) + payloadSize;
        static uint8_t castCount = 0;

        uint8_t* bytes = new uint8_t[fullSize];
        uint32_t offset = 0;
        auto writeU8 = [&](uint8_t value)
        {
            bytes[offset++] = value;
        };
        auto writeU32 = [&](uint32_t value)
        {
            std::memcpy(bytes + offset, &value, sizeof(value));
            offset += sizeof(value);
        };

        writeU32(CMSG_CAST_SPELL_OPCODE);
        writeU8(++castCount);
        writeU32(spellId);
        writeU8(0); // cast flags
        writeU32(0); // target mask: none/self-resolved by server spell info

        RawClientPacket_SpellCache* packet = new RawClientPacket_SpellCache{};
        InitializeRawClientPacket_SpellCache(packet);
        packet->buffer = bytes;
        packet->size = fullSize;
        packet->alloc = fullSize;
        FinalizeRawClientPacket_SpellCache(packet);
        SendRawClientPacket_SpellCache(packet);
    }

    void SendStreamedCancelAuraPacket(uint32_t spellId)
    {
        constexpr uint32_t CMSG_CANCEL_AURA_OPCODE = 0x136;
        constexpr uint32_t fullSize = sizeof(uint32_t) + sizeof(uint32_t);

        uint8_t* bytes = new uint8_t[fullSize];
        std::memcpy(bytes, &CMSG_CANCEL_AURA_OPCODE, sizeof(CMSG_CANCEL_AURA_OPCODE));
        std::memcpy(bytes + sizeof(uint32_t), &spellId, sizeof(spellId));

        RawClientPacket_SpellCache* packet = new RawClientPacket_SpellCache{};
        InitializeRawClientPacket_SpellCache(packet);
        packet->buffer = bytes;
        packet->size = fullSize;
        packet->alloc = fullSize;
        FinalizeRawClientPacket_SpellCache(packet);
        SendRawClientPacket_SpellCache(packet);
    }

    std::vector<KnownSpellbookEntry> GetSortedStreamedStances()
    {
        return GetCachedStreamedStances();
    }

    std::vector<KnownSpellbookEntry> const& GetCachedStreamedStances()
    {
        if (CachedStreamedStanceRevision == KnownSpellbookRevision)
            return CachedStreamedStances;

        CachedStreamedStances.clear();
        CachedStreamedStances.reserve(KnownSpellbookEntries.size());
        for (auto const& entry : KnownSpellbookEntries)
            if (entry.second.isStance
                && entry.second.addToSpellbook
                && !entry.second.isHiddenClient
                && !entry.second.isTradeskill
                && !entry.second.isNativeHiddenSpellbook
                && entry.second.hasName)
                CachedStreamedStances.push_back(entry.second);

        std::sort(CachedStreamedStances.begin(), CachedStreamedStances.end(), [](KnownSpellbookEntry const& lhs, KnownSpellbookEntry const& rhs)
        {
            if (lhs.stanceBarOrder != rhs.stanceBarOrder)
                return lhs.stanceBarOrder < rhs.stanceBarOrder;
            return lhs.spellId < rhs.spellId;
        });

        CachedStreamedStanceRevision = KnownSpellbookRevision;
        return CachedStreamedStances;
    }

    bool TryGetStreamedStanceByIndex(uint32_t oneBasedIndex, KnownSpellbookEntry& out)
    {
        if (!oneBasedIndex)
            return false;

        auto const& stances = GetCachedStreamedStances();
        uint32_t const index = oneBasedIndex - 1;
        if (index >= stances.size())
            return false;

        out = stances[index];
        return true;
    }

    bool GetNativeStreamedShapeshiftCastable(uint32_t oneBasedIndex, bool active)
    {
        if (!oneBasedIndex)
            return active;

        uint32_t const index = oneBasedIndex - 1;
        NativeShapeshiftEntry* rows = ShapeshiftRows();
        if (!rows || index >= ShapeshiftCount() || index >= ShapeshiftCapacity())
            return active;

        return active || (rows[index].usable && !rows[index].lacksPower);
    }

    void ApplyStreamedShapeshiftState(bool signalRefresh)
    {
        if (!SPELL_CACHE_ENABLE_UI_INTEGRATION)
            return;

        std::vector<KnownSpellbookEntry> stances = GetSortedStreamedStances();
        uint32_t const stanceCount = (std::min)(static_cast<uint32_t>(stances.size()), SPELLBOOK_MAX_STANCES);
        if (!EnsureNativeShapeshiftStorage(stanceCount))
        {
            static uint32_t logCount = 0;
            if (logCount < 40)
            {
                LOG_INFO << "Deferred streamed shapeshift apply; native storage not ready"
                    << "desired" << stanceCount
                    << "capacity" << ShapeshiftCapacity()
                    << "ptr" << ShapeshiftRows();
                ++logCount;
            }
            return;
        }

        NativeShapeshiftEntry* rows = ShapeshiftRows();
        uint32_t const previousCount = ShapeshiftCount();
        uint32_t const clearCount = (std::min)(previousCount, ShapeshiftCapacity());
        if (clearCount)
            std::memset(rows, 0, clearCount * sizeof(NativeShapeshiftEntry));

        for (uint32_t i = 0; i < stanceCount; ++i)
        {
            bool usable = false;
            bool lacksPower = false;
            GetStreamedSpellStaticBarUsableState(stances[i].spellId, usable, lacksPower);
            rows[i].spellId = stances[i].spellId;
            rows[i].usable = usable ? 1 : 0;
            rows[i].lacksPower = lacksPower ? 1 : 0;
        }

        ShapeshiftCount() = stanceCount;

        if (signalRefresh)
        {
            if (ClientLua::State())
            {
                ClientLua::DoString(
                    "if DHStreamedShapeshiftRefresh then DHStreamedShapeshiftRefresh() end",
                    ClientLua::State());
            }
        }

        static uint32_t logCount = 0;
        if (logCount < 80)
        {
            LOG_INFO << "Applied streamed shapeshift state"
                << "count" << stanceCount
                << "previous" << previousCount
                << "firstSpell" << (stanceCount ? rows[0].spellId : 0)
                << "capacity" << ShapeshiftCapacity()
                << "nativeCount" << ShapeshiftCount();
            ++logCount;
        }
    }

    void ApplyKnownSpellbookModelToNative(bool signalRefresh)
    {
        RebuildKnownSpellbookOrder();
        ApplyStreamedLanguageState();

        static bool loggedDisabledWriter = false;
        if (!loggedDisabledWriter)
        {
            LOG_ERROR << "Streamed spellbook native slot/tab/stance writer disabled after login crash; using Lua spellbook bridge only";
            loggedDisabledWriter = true;
        }

        (void)signalRefresh;
        return;

        uint32_t const visibleCount = (std::min)(
            static_cast<uint32_t>(KnownSpellbookOrder.size()),
            SPELLBOOK_MAX_SLOTS);
        uint32_t const tabCount = (std::min)(
            static_cast<uint32_t>(KnownSpellbookTabs.size()),
            SPELLBOOK_MAX_TABS);

        if (!EnsureNativeSpellbookTabStorage(tabCount))
        {
            static uint32_t logCount = 0;
            if (logCount < 20)
            {
                LOG_ERROR << "Skipped streamed spellbook native write; tab storage guard failed"
                    << "visible" << visibleCount
                    << "capacity" << SpellbookTabArrayCapacity()
                    << "allocated" << SpellbookTabAllocatedCount();
                ++logCount;
            }
            return;
        }

        std::memset(PlayerSpellSlotMap(), 0, SPELLBOOK_MAX_SLOTS * sizeof(uint32_t));
        std::memset(VisibleSpellbookSlots(), 0, SPELLBOOK_MAX_SLOTS * sizeof(uint32_t));
        for (uint32_t i = 0; i < visibleCount; ++i)
        {
            uint32_t const spellId = KnownSpellbookOrder[i];
            PlayerSpellSlotMap()[i] = spellId;
            VisibleSpellbookSlots()[i] = spellId;
        }

        PlayerSpellSlotMapSize() = visibleCount;
        VisibleSpellbookCount() = visibleCount;
        SpellbookTabCount() = tabCount;

        uint32_t** tabs = SpellbookTabArray();
        for (uint32_t i = 0; i < tabCount; ++i)
        {
            KnownSpellbookTab const& tab = KnownSpellbookTabs[i];
            tabs[i][0] = tab.tabKey;
            tabs[i][1] = tab.count;
            tabs[i][2] = tab.count;
        }

        if (signalRefresh)
        {
            FrameScript::SignalEvent(242, nullptr);
            if (tabCount)
                FrameScript::SignalEvent(448, const_cast<char*>("%d"), 1);
        }

        static uint32_t logCount = 0;
        if (logCount < 120)
        {
            LOG_INFO << "Applied streamed spellbook model to native UI state"
                << "visible" << visibleCount
                << "tabs" << tabCount
                << "stances" << ShapeshiftCount();
            ++logCount;
        }
    }

    void ProcessReadyKnownSpellUiWork()
    {
        if (ReadyKnownSpellUiQueue.empty())
            return;

        static uint64_t observedPlayerGuid = 0;
        static uint32_t firstPlayerSeenMs = 0;
        uint64_t const playerGuid = ClntObjMgr::GetActivePlayer();
        if (!playerGuid)
            return;

        uint32_t const nowMs = GetTickCount();
        if (observedPlayerGuid != playerGuid)
        {
            observedPlayerGuid = playerGuid;
            firstPlayerSeenMs = nowMs;
            return;
        }

        if (nowMs - firstPlayerSeenMs < 3000)
            return;

        LOG_INFO << "Streamed known spell UI work pending; checking tab index"
            << "ready" << static_cast<uint32_t>(ReadyKnownSpellUiQueue.size())
            << "pendingKnown" << static_cast<uint32_t>(PendingKnownSpellAdds.size())
            << "model" << static_cast<uint32_t>(KnownSpellbookEntries.size());
        EnsureStreamedSpellSkillLinesLoaded();
        LOG_INFO << "Streamed known spell UI work skill lines loaded";

        uint32_t processed = 0;
        uint32_t skipped = 0;

        while (processed < KNOWN_SPELL_UI_WORK_PER_PUMP && !ReadyKnownSpellUiQueue.empty())
        {
            uint32_t const spellId = ReadyKnownSpellUiQueue.front();
            ReadyKnownSpellUiQueue.pop_front();
            ReadyKnownSpellUiRows.erase(spellId);

            PendingKnownSpellAdd pending{ nullptr, spellId, 0, 0, 1 };
            auto readyRecord = ReadyKnownSpellUiRecords.find(spellId);
            if (readyRecord != ReadyKnownSpellUiRecords.end())
            {
                pending = readyRecord->second;
                ReadyKnownSpellUiRecords.erase(readyRecord);
            }

            KnownSpellbookEntry entry{};
            LOG_INFO << "Streamed known spell UI build begin"
                << "spell" << spellId
                << "cached" << SpellCacheStreaming::HasSpell(spellId)
                << "skillLineSidecar" << ResolveStreamedSpellbookSkillLine(spellId);
            if (BuildKnownSpellbookEntry(pending, entry))
            {
                KnownSpellbookEntries[spellId] = entry;
                MarkKnownSpellbookModelDirty();
                ++processed;
                LOG_INFO << "Streamed known spell UI build ok"
                    << "spell" << spellId
                    << "visible" << entry.visibleInSpellbook
                    << "tab" << entry.tabKey;
            }
            else
            {
                ++skipped;
                LOG_INFO << "Streamed known spell UI build skipped"
                    << "spell" << spellId;
            }
        }

        static uint32_t logCount = 0;
        if ((processed || skipped) && logCount < 120)
        {
            LOG_INFO << "Processed streamed known spell UI model work"
                << "processed" << processed
                << "skipped" << skipped
                << "model" << static_cast<uint32_t>(KnownSpellbookEntries.size())
                << "visible" << static_cast<uint32_t>(KnownSpellbookOrder.size())
                << "pending" << static_cast<uint32_t>(PendingKnownSpellAdds.size())
                << "queued" << static_cast<uint32_t>(ReadyKnownSpellUiQueue.size());
            ++logCount;
        }

        if (processed)
        {
            ApplyStreamedLanguageState();
            if (ReadyKnownSpellUiQueue.empty())
            {
                RebuildKnownSpellbookOrder();
                PublishedKnownSpellbookRevision = KnownSpellbookRevision;
                SignalStreamedSpellbookRefresh();
                if (ClientLua::State())
                {
                    LOG_INFO << "Requesting streamed shapeshift Lua refresh"
                        << "stances" << static_cast<uint32_t>(GetCachedStreamedStances().size());
                    ClientLua::DoString(
                        "if DHStreamedShapeshiftRefresh then DHStreamedShapeshiftRefresh() end",
                        ClientLua::State());
                }

                LOG_INFO << "Flushed streamed known spell UI refresh"
                    << "model" << static_cast<uint32_t>(KnownSpellbookEntries.size())
                    << "visible" << static_cast<uint32_t>(KnownSpellbookOrder.size())
                    << "tabs" << static_cast<uint32_t>(KnownSpellbookTabs.size())
                    << "stances" << static_cast<uint32_t>(GetCachedStreamedStances().size())
                    << "stanceNativeWrite" << 0
                    << "stanceForcedRefresh" << 1;
            }
        }
    }

    void TrackPendingKnownSpellAdd(void* player, uint32_t spellId, int32_t spellCategory, int32_t learned, int32_t addToSpellbook)
    {
        if (!HasReasonableSpellId(spellId))
            return;

        for (PendingKnownSpellAdd& pending : PendingKnownSpellAdds)
        {
            if (pending.spellId == spellId)
            {
                pending.player = player;
                pending.spellCategory = spellCategory;
                pending.learned = learned;
                pending.addToSpellbook = addToSpellbook;
                if (PendingKnownSpellDebugLogCount < 160)
                {
                    LOG_INFO << "Updated pending streamed known spell"
                        << "spell" << spellId
                        << "pending" << static_cast<uint32_t>(PendingKnownSpellAdds.size())
                        << "category" << spellCategory
                        << "learned" << learned
                        << "addToSpellbook" << addToSpellbook;
                    ++PendingKnownSpellDebugLogCount;
                }
                return;
            }
        }

        PendingKnownSpellAdds.push_back({ player, spellId, spellCategory, learned, addToSpellbook });
        if (PendingKnownSpellDebugLogCount < 160)
        {
            LOG_INFO << "Queued pending streamed known spell"
                << "spell" << spellId
                << "pending" << static_cast<uint32_t>(PendingKnownSpellAdds.size())
                << "category" << spellCategory
                << "learned" << learned
                << "addToSpellbook" << addToSpellbook;
            ++PendingKnownSpellDebugLogCount;
        }
    }

    void ApplyDeferredKnownSpellAdds(uint32_t spellId)
    {
        if (!SPELL_CACHE_ENABLE_UI_INTEGRATION)
            return;

        if (!HasReasonableSpellId(spellId))
            return;

        if (!SpellCacheStreaming::HasSpell(spellId))
            return;

        if (PendingKnownSpellDebugLogCount < 220)
        {
            LOG_INFO << "Apply deferred streamed known spell begin"
                << "spell" << spellId
                << "pending" << static_cast<uint32_t>(PendingKnownSpellAdds.size())
                << "ready" << static_cast<uint32_t>(ReadyKnownSpellUiQueue.size());
            ++PendingKnownSpellDebugLogCount;
        }

        std::vector<PendingKnownSpellAdd> apply;
        std::vector<PendingKnownSpellAdd> keep;
        apply.reserve(PendingKnownSpellAdds.size());
        keep.reserve(PendingKnownSpellAdds.size());

        for (PendingKnownSpellAdd const& pending : PendingKnownSpellAdds)
        {
            if (pending.spellId == spellId)
                apply.push_back(pending);
            else
                keep.push_back(pending);
        }

        PendingKnownSpellAdds.swap(keep);

        for (PendingKnownSpellAdd const& pending : apply)
        {
            if (pending.spellId != spellId || !HasReasonableSpellId(pending.spellId))
                continue;

            ScheduleKnownSpellUiWork(pending, "metadata-ready");
        }

        if (PendingKnownSpellDebugLogCount < 220)
        {
            LOG_INFO << "Apply deferred streamed known spell end"
                << "spell" << spellId
                << "applied" << static_cast<uint32_t>(apply.size())
                << "pending" << static_cast<uint32_t>(PendingKnownSpellAdds.size())
                << "ready" << static_cast<uint32_t>(ReadyKnownSpellUiQueue.size());
            ++PendingKnownSpellDebugLogCount;
        }
    }

    void HandleSpellCacheResponse(CustomPacketRead* packet)
    {
        if (!packet)
            return;

        uint32_t const spellId = ReadU32(packet);
        uint8_t const found = packet->Read<uint8_t>(0);
        PendingSpellRequests.erase(spellId);
        MissingSpellRows.erase(spellId);

        if (!HasReasonableSpellId(spellId))
        {
            LOG_ERROR << "Blocked invalid spell cache response spell" << spellId;
            return;
        }

        if (!found)
        {
            MissingSpellRows.insert(spellId);
            LOG_ERROR << "Spell cache server did not find spell" << spellId;
            return;
        }

        uint32_t const cacheVersion = ReadU32(packet);
        bool const legacyV2Packet = cacheVersion == 2;
        if (cacheVersion != SPELL_CACHE_STREAM_VERSION && !legacyV2Packet)
        {
            LOG_ERROR << "Spell cache version mismatch for" << spellId << "server" << cacheVersion << "client" << SPELL_CACHE_STREAM_VERSION;
            return;
        }

        if (legacyV2Packet)
        {
            static uint32_t legacyLogCount = 0;
            if (legacyLogCount < 80)
            {
                LOG_ERROR << "Accepted legacy v2 spell cache packet without skill line metadata"
                    << "spell" << spellId
                    << "client" << SPELL_CACHE_STREAM_VERSION;
                ++legacyLogCount;
            }
        }

        SpellCacheRow spell{};
        spell.id = spellId;
        spell.cacheVersion = SPELL_CACHE_STREAM_VERSION;
        spell.spellDataHash = ReadU32(packet);
        spell.category = ReadU32(packet);
        spell.dispel = ReadU32(packet);
        spell.mechanic = ReadU32(packet);
        spell.attributes = ReadU32(packet);
        spell.attributesEx = ReadU32(packet);
        spell.attributesEx2 = ReadU32(packet);
        spell.attributesEx3 = ReadU32(packet);
        spell.attributesEx4 = ReadU32(packet);
        spell.attributesEx5 = ReadU32(packet);
        spell.attributesEx6 = ReadU32(packet);
        spell.attributesEx7 = ReadU32(packet);
        spell.stances = ReadU32(packet);
        spell.stancesNot = ReadU32(packet);
        spell.targets = ReadU32(packet);
        spell.targetCreatureType = ReadU32(packet);
        spell.requiresSpellFocus = ReadU32(packet);
        spell.facingCasterFlags = ReadU32(packet);
        spell.casterAuraState = ReadU32(packet);
        spell.targetAuraState = ReadU32(packet);
        spell.excludeCasterAuraState = ReadU32(packet);
        spell.excludeTargetAuraState = ReadU32(packet);
        spell.casterAuraSpell = ReadU32(packet);
        spell.targetAuraSpell = ReadU32(packet);
        spell.excludeCasterAuraSpell = ReadU32(packet);
        spell.excludeTargetAuraSpell = ReadU32(packet);
        spell.castingTimeIndex = ReadU32(packet);
        spell.recoveryTime = ReadU32(packet);
        spell.categoryRecoveryTime = ReadU32(packet);
        spell.interruptFlags = ReadU32(packet);
        spell.auraInterruptFlags = ReadU32(packet);
        spell.channelInterruptFlags = ReadU32(packet);
        spell.procFlags = ReadU32(packet);
        spell.procChance = ReadU32(packet);
        spell.procCharges = ReadU32(packet);
        spell.maxLevel = ReadU32(packet);
        spell.baseLevel = ReadU32(packet);
        spell.spellLevel = ReadU32(packet);
        spell.durationIndex = ReadU32(packet);
        spell.powerType = ReadI32(packet);
        if (!HasValidPowerType(spell.powerType))
        {
            LOG_ERROR << "Blocked invalid streamed spell powerType" << spellId << spell.powerType;
            spell.powerType = -1;
        }

        spell.manaCost = ReadU32(packet);
        spell.manaCostPerLevel = ReadU32(packet);
        spell.manaPerSecond = ReadU32(packet);
        spell.manaPerSecondPerLevel = ReadU32(packet);
        spell.rangeIndex = ReadU32(packet);
        spell.speed = ReadF32(packet);
        spell.modalNextSpell = ReadU32(packet);
        spell.stackAmount = ReadU32(packet);
        spell.totem1 = ReadU32(packet);
        spell.totem2 = ReadU32(packet);
        spell.reagent1 = ReadI32(packet);
        spell.reagent2 = ReadI32(packet);
        spell.reagent3 = ReadI32(packet);
        spell.reagent4 = ReadI32(packet);
        spell.reagent5 = ReadI32(packet);
        spell.reagent6 = ReadI32(packet);
        spell.reagent7 = ReadI32(packet);
        spell.reagent8 = ReadI32(packet);
        spell.reagentCount1 = ReadI32(packet);
        spell.reagentCount2 = ReadI32(packet);
        spell.reagentCount3 = ReadI32(packet);
        spell.reagentCount4 = ReadI32(packet);
        spell.reagentCount5 = ReadI32(packet);
        spell.reagentCount6 = ReadI32(packet);
        spell.reagentCount7 = ReadI32(packet);
        spell.reagentCount8 = ReadI32(packet);
        spell.equippedItemClass = ReadI32(packet);
        spell.equippedItemSubClassMask = ReadI32(packet);
        spell.equippedItemInventoryTypeMask = ReadI32(packet);
        spell.spellVisualID1 = ReadU32(packet);
        spell.spellVisualID2 = ReadU32(packet);
        spell.spellIconID = ReadU32(packet);
        spell.activeIconID = ReadU32(packet);
        spell.spellPriority = ReadU32(packet);
        spell.spellName = ReadStableString(packet, spellId, 0);
        spell.spellRank = ReadStableString(packet, spellId, 1);
        spell.description = ReadStableString(packet, spellId, 2);
        spell.auraDescription = ReadStableString(packet, spellId, 3);
        spell.manaCostPct = ReadU32(packet);
        spell.startRecoveryCategory = ReadU32(packet);
        spell.startRecoveryTime = ReadU32(packet);
        spell.maxTargetLevel = ReadU32(packet);
        spell.spellFamilyName = ReadU32(packet);
        spell.spellFamilyFlags1 = ReadU32(packet);
        spell.spellFamilyFlags2 = ReadU32(packet);
        spell.spellFamilyFlags3 = ReadU32(packet);
        spell.maxAffectedTargets = ReadU32(packet);
        spell.dmgClass = ReadU32(packet);
        spell.preventionType = ReadU32(packet);
        spell.stanceBarOrder = ReadU32(packet);
        spell.minFactionID = ReadU32(packet);
        spell.minReputation = ReadU32(packet);
        spell.requiredAuraVision = ReadU32(packet);
        spell.requiredTotemCategoryID1 = ReadU32(packet);
        spell.requiredTotemCategoryID2 = ReadU32(packet);
        spell.areaGroupId = ReadI32(packet);
        spell.schoolMask = ReadU32(packet);
        spell.runeCostID = ReadU32(packet);
        spell.spellMissileID = ReadU32(packet);
        spell.powerDisplayID = ReadI32(packet);
        spell.descriptionVariablesID = ReadU32(packet);
        spell.difficulty = ReadU32(packet);
        uint32_t const skillLineId = legacyV2Packet ? 0 : ReadU32(packet);

        GlobalCDBCMap.addRow("Spell", spell.id, spell);
        auto const spellRange = GlobalCDBCMap.getIndexRange("Spell");
        GlobalCDBCMap.setIndexRange(
            "Spell",
            spellRange.first ? (std::min)(spellRange.first, static_cast<int>(spell.id)) : spell.id,
            (std::max)(spellRange.second, static_cast<int>(spell.id)));
        StreamedSpellRows.insert(spell.id);
        RecordStreamedSpellSkillLine(spell.id, spell.spellDataHash, skillLineId);

        uint32_t const effectCount = (std::min)(ReadU32(packet), 32u);
        for (uint32_t i = 0; i < 32; ++i)
            GlobalCDBCMap.allCDBCs["SpellEffect"].erase(SpellEffectCacheKey(spell.id, i));

        for (uint32_t i = 0; i < effectCount; ++i)
        {
            SpellEffectCacheRow effect{};
            effect.spellID = spellId;
            effect.effectIndex = ReadU32(packet);
            effect.effect = ReadU32(packet);
            effect.effectDieSides = ReadI32(packet);
            effect.effectRealPointsPerLevel = ReadF32(packet);
            effect.effectBasePoints = ReadI32(packet);
            effect.effectMechanic = ReadU32(packet);
            effect.effectImplicitTargetA = ReadU32(packet);
            effect.effectImplicitTargetB = ReadU32(packet);
            effect.effectRadiusIndex = ReadU32(packet);
            effect.effectApplyAuraName = ReadU32(packet);
            effect.effectAmplitude = ReadU32(packet);
            effect.effectMultipleValue = ReadF32(packet);
            effect.effectChainTargets = ReadU32(packet);
            effect.effectItemType = ReadI32(packet);
            effect.effectMiscValue = ReadI32(packet);
            effect.effectMiscValueB = ReadI32(packet);
            effect.effectTriggerSpell = ReadU32(packet);
            effect.effectPointsPerCombo = ReadF32(packet);
            effect.effectSpellClassMaskA = ReadU32(packet);
            effect.effectSpellClassMaskB = ReadU32(packet);
            effect.effectSpellClassMaskC = ReadU32(packet);
            effect.effectChainAmplitude = ReadF32(packet);
            effect.effectBonusMultiplier = ReadF32(packet);

            GlobalCDBCMap.addRow("SpellEffect", SpellEffectCacheKey(effect.spellID, effect.effectIndex), effect);
        }

        LOG_INFO << "Cached streamed spell" << spellId << "hash" << spell.spellDataHash << "skillLine" << skillLineId << "effects" << effectCount;
        NotifySpellCached(spellId);
    }
}

#if 0
DISABLED_DETOUR_THISCALL(ClientDb_GetLocalizedRow_SpellCache, 0x004CFD20, int, (uint32_t rowId, void* rowBuffer))
{
    PumpSpellCacheRequests();

    if (self == reinterpret_cast<void*>(SPELL_DB_ADDRESS) && rowBuffer)
    {
        ClientData::SpellRow cachedSpell{};
        if (SpellCacheStreaming::TryBuildSpellRow(rowId, cachedSpell))
        {
            std::memcpy(rowBuffer, &cachedSpell, sizeof(cachedSpell));
            RecordStreamedSpellDbLookup(rowId, cachedSpell, _ReturnAddress());
            return 1;
        }

        if (!NativeSpellDbCanLookup(rowId))
        {
            if (rowId)
                SpellCacheStreaming::RequestSpell(rowId);
            std::memset(rowBuffer, 0, sizeof(ClientData::SpellRow));
            return 0;
        }

        int const result = ClientDb_GetLocalizedRow_SpellCache(self, rowId, rowBuffer);
        if (result)
        {
            ClientData::SpellRow* spell = static_cast<ClientData::SpellRow*>(rowBuffer);
            if (!HasValidPowerType(*spell))
            {
                LOG_ERROR << "Blocked invalid legacy SpellRec" << rowId << "powerType" << static_cast<int32_t>(spell->m_powerType);
                SpellCacheStreaming::RequestSpell(rowId);
                std::memset(rowBuffer, 0, sizeof(ClientData::SpellRow));
                return 0;
            }
        }
        else if (rowId)
        {
            SpellCacheStreaming::RequestSpell(rowId);
        }

        return result;
    }

    return ClientDb_GetLocalizedRow_SpellCache(self, rowId, rowBuffer);
}

DISABLED_DETOUR(GetSpellVisualRow_SpellCache, 0x007FA290, __cdecl, void*, (ClientData::SpellRow* spell))
{
    void* const nativeVisual = GetSpellVisualRow_SpellCache(spell);
    uintptr_t const caller = reinterpret_cast<uintptr_t>(_ReturnAddress());
    if (spell && HasReasonableSpellId(spell->m_ID) && SpellCacheStreaming::HasSpell(spell->m_ID))
    {
        uint32_t visualId = spell->m_spellVisualID[0];
        if (GetSpellVisualDetailMode_SpellCache() < 2 && spell->m_spellVisualID[1])
            visualId = spell->m_spellVisualID[1];

        if (!visualId)
        {
            ClientData::SpellRow streamed{};
            if (SpellCacheStreaming::TryBuildSpellRow(spell->m_ID, streamed))
            {
                visualId = streamed.m_spellVisualID[0];
                if (GetSpellVisualDetailMode_SpellCache() < 2 && streamed.m_spellVisualID[1])
                    visualId = streamed.m_spellVisualID[1];
            }
        }

        RecordStreamedVisualPipelineStage(
            spell->m_ID,
            STREAMED_VISUAL_STAGE_GET_VISUAL_ROW,
            caller,
            visualId,
            0,
            0,
            nativeVisual ? 1u : 0u);

        if (nativeVisual && caller >= 0x00733000 && caller < 0x00733800)
        {
            auto const* visual = static_cast<SpellVisualDebugRow const*>(nativeVisual);
            StreamedMissileTrajectoryArmedSpellId = spell->m_ID;
            StreamedMissileTrajectoryGlobalLogCount = 0;
            StreamedMissileTrajectoryGlobalReturnLogCount = 0;
            static std::unordered_set<uint64_t> loggedMissileVisual;
            uint64_t const key = (static_cast<uint64_t>(spell->m_ID) << 32) ^ caller;
            if (loggedMissileVisual.size() < 80 && loggedMissileVisual.insert(key).second)
            {
                LOG_INFO << "Native sub_732FF0 SpellVisual fields"
                    << "spell" << spell->m_ID
                    << "caller" << caller
                    << "visual" << visual->id
                    << "hasMissile" << visual->hasMissile
                    << "missileModel" << visual->missileModel
                    << "missileModelExists" << NativeDbHasRow(SPELL_VISUAL_EFFECT_NAME_DB_RECORDS_ADDRESS, visual->missileModel)
                    << "missilePathType" << visual->missilePathType
                    << "missileAttachment" << visual->missileAttachment
                    << "missileFollowGroundHeight" << visual->missileFollowGroundHeight
                    << "missileFollowGroundDropSpeed" << visual->missileFollowGroundDropSpeed
                    << "missileFollowGroundApproach" << visual->missileFollowGroundApproach
                    << "missileFollowGroundFlags" << visual->missileFollowGroundFlags
                    << "missileMotion" << visual->missileMotion
                    << "missileMotionExists" << NativeDbHasRow(SPELL_MISSILE_MOTION_DB_RECORDS_ADDRESS, visual->missileMotion)
                    << "missileTargetingKit" << visual->missileTargetingKit
                    << "missileTargetingKitExists" << NativeDbHasRow(SPELL_VISUAL_KIT_DB_RECORDS_ADDRESS, visual->missileTargetingKit);
            }
        }
    }

    if (nativeVisual || !spell || !HasReasonableSpellId(spell->m_ID))
        return nativeVisual;

    if (caller >= 0x00724800 && caller < 0x00724D00)
        return nullptr;

    ClientData::SpellRow streamed{};
    if (!SpellCacheStreaming::TryBuildSpellRow(spell->m_ID, streamed))
        return nullptr;

    uint32_t visualId = streamed.m_spellVisualID[0];
    if (GetSpellVisualDetailMode_SpellCache() < 2 && streamed.m_spellVisualID[1])
        visualId = streamed.m_spellVisualID[1];

    if (!visualId)
        return nullptr;

    void* const streamedVisual = ClientDB::GetRow(reinterpret_cast<void*>(SPELL_VISUAL_DB_RECORDS_ADDRESS), visualId);
    if (!streamedVisual)
        return nullptr;

    uint32_t& hits = StreamedSpellVisualRowFallbackHits[spell->m_ID];
    if (hits < 0xFFFFFFFF)
        ++hits;
    StreamedSpellVisualRowFallbackLastVisual[spell->m_ID] = visualId;
    StreamedSpellVisualRowFallbackLastCaller[spell->m_ID] = caller;

    static std::unordered_set<uint64_t> logged;
    uint64_t const key = (static_cast<uint64_t>(spell->m_ID) << 32)
        ^ static_cast<uint64_t>(caller);
    if (logged.size() < 80 && logged.insert(key).second)
    {
        LOG_INFO << "Streamed SpellVisual fallback"
            << "spell" << spell->m_ID
            << "visual" << visualId
            << "caller" << caller;
    }

    return streamedVisual;
}

DISABLED_DETOUR_THISCALL(CGUnit_C__GetAppropriateSpellVisual_SpellCache, 0x00720F80, void*, (ClientData::SpellRow* spell, void* dst, uint32_t explicitVisual1, uint32_t explicitVisual2))
{
    void* const nativeVisual = CGUnit_C__GetAppropriateSpellVisual_SpellCache(self, spell, dst, explicitVisual1, explicitVisual2);
    if (spell && HasReasonableSpellId(spell->m_ID) && SpellCacheStreaming::HasSpell(spell->m_ID))
    {
        uint32_t visualId = explicitVisual1 ? explicitVisual1 : spell->m_spellVisualID[0];
        if (!visualId && GetSpellVisualDetailMode_SpellCache() < 2 && spell->m_spellVisualID[1])
            visualId = spell->m_spellVisualID[1];
        if (!visualId && dst)
            visualId = *static_cast<uint32_t*>(dst);

        RecordStreamedVisualPipelineStage(
            spell->m_ID,
            STREAMED_VISUAL_STAGE_GET_APPROPRIATE_VISUAL,
            reinterpret_cast<uintptr_t>(_ReturnAddress()),
            visualId,
            0,
            0,
            nativeVisual ? 1u : 0u);
    }

    if (nativeVisual || !dst || !spell || !HasReasonableSpellId(spell->m_ID))
        return nativeVisual;

    ClientData::SpellRow streamed{};
    if (!SpellCacheStreaming::TryBuildSpellRow(spell->m_ID, streamed))
        return nullptr;

    uint32_t visualId = streamed.m_spellVisualID[0];
    if (GetSpellVisualDetailMode_SpellCache() < 2 && streamed.m_spellVisualID[1])
        visualId = streamed.m_spellVisualID[1];

    if (!visualId)
        return nullptr;

    void* const streamedVisual = ClientDB::GetRow(reinterpret_cast<void*>(SPELL_VISUAL_DB_RECORDS_ADDRESS), visualId);
    if (!streamedVisual)
        return nullptr;

    std::memcpy(dst, streamedVisual, 0x80);

    uint32_t& hits = StreamedUnitAppropriateVisualFallbackHits[spell->m_ID];
    if (hits < 0xFFFFFFFF)
        ++hits;
    StreamedUnitAppropriateVisualFallbackLastVisual[spell->m_ID] = visualId;
    StreamedUnitAppropriateVisualFallbackLastCaller[spell->m_ID] = reinterpret_cast<uintptr_t>(_ReturnAddress());

    static std::unordered_set<uint64_t> logged;
    uint64_t const key = (static_cast<uint64_t>(spell->m_ID) << 32)
        ^ static_cast<uint64_t>(StreamedUnitAppropriateVisualFallbackLastCaller[spell->m_ID]);
    if (logged.size() < 80 && logged.insert(key).second)
    {
        LOG_INFO << "Streamed unit appropriate visual fallback"
            << "spell" << spell->m_ID
            << "visual" << visualId
            << "explicit1" << explicitVisual1
            << "explicit2" << explicitVisual2
            << "caller" << StreamedUnitAppropriateVisualFallbackLastCaller[spell->m_ID];
    }

    return dst;
}

DISABLED_DETOUR_THISCALL(Unit_CreateMissile_SpellCache, 0x00733E20, void, (void* a0, void* a4, uint32_t a8, uint32_t aC, uint32_t* visualArg, uint32_t spellId, uint32_t a18))
{
    ClientData::SpellRow row{};
    if (HasReasonableSpellId(spellId) && SpellCacheStreaming::TryGetSpellRow(spellId, row, false))
    {
        uint32_t& hits = NativeMissileCreateHits[spellId];
        if (hits < 0xFFFFFFFF)
            ++hits;

        NativeMissileCreateLastCaller[spellId] = reinterpret_cast<uintptr_t>(_ReturnAddress());
        NativeMissileCreateLastVisual[spellId] = row.m_spellVisualID[0];
        NativeMissileCreateLastSpeedBits[spellId] = *reinterpret_cast<uint32_t*>(&row.m_speed);

        if (visualArg)
        {
            NativeMissileCreateLastVisualModel[spellId] = visualArg[8];
            NativeMissileCreateLastMissileMotion[spellId] = visualArg[21];
        }
        else
        {
            NativeMissileCreateLastVisualModel[spellId] = 0;
            NativeMissileCreateLastMissileMotion[spellId] = 0;
        }

        RecordStreamedVisualPipelineStage(
            spellId,
            STREAMED_VISUAL_STAGE_MISSILE_ENTRY,
            NativeMissileCreateLastCaller[spellId],
            NativeMissileCreateLastVisual[spellId],
            0,
            0,
            1,
            NativeMissileCreateLastVisualModel[spellId],
            NativeMissileCreateLastMissileMotion[spellId]);

        static std::unordered_set<uint32_t> logged;
        if (logged.size() < 80 && logged.insert(spellId).second)
        {
            LOG_INFO << "Native Unit::CreateMissile for streamed spell"
                << "spell" << spellId
                << "speed" << row.m_speed
                << "spellVisual1" << row.m_spellVisualID[0]
                << "spellVisual2" << row.m_spellVisualID[1]
                << "argVisual" << (visualArg ? visualArg[0] : 0)
                << "argMissileModel" << (visualArg ? visualArg[8] : 0)
                << "argMissileMotion" << (visualArg ? visualArg[21] : 0)
                << "caller" << NativeMissileCreateLastCaller[spellId];
        }
    }

    Unit_CreateMissile_SpellCache(self, a0, a4, a8, aC, visualArg, spellId, a18);
}

DISABLED_DETOUR(ClntObjMgrObjectPtr_MissileTrace_SpellCache, 0x004D4DB0, __cdecl, void*, (uint64_t guid, uint32_t typeMask))
{
    void* const result = ClntObjMgrObjectPtr_MissileTrace_SpellCache(guid, typeMask);
    uintptr_t const caller = reinterpret_cast<uintptr_t>(_ReturnAddress());
    if (caller >= 0x00733000 && caller < 0x00733800)
    {
        static uint32_t logCount = 0;
        if (logCount < 80)
        {
            LOG_INFO << "Native sub_732FF0 object lookup"
                << "caller" << caller
                << "guidLow" << static_cast<uint32_t>(guid)
                << "guidHigh" << static_cast<uint32_t>(guid >> 32)
                << "typeMask" << typeMask
                << "result" << reinterpret_cast<uintptr_t>(result);
            ++logCount;
        }
    }

    return result;
}

DISABLED_DETOUR(NativeMissileAllocate_SpellCache, 0x00702B10, __cdecl, int, ())
{
    uintptr_t const caller = reinterpret_cast<uintptr_t>(_ReturnAddress());
    int const result = NativeMissileAllocate_SpellCache();
    if (caller >= 0x00733000 && caller < 0x00733800)
    {
        static uint32_t logCount = 0;
        if (logCount < 80)
        {
            LOG_INFO << "Native sub_732FF0 missile allocate"
                << "caller" << caller
                << "result" << result;
            ++logCount;
        }
    }

    return result;
}

DISABLED_DETOUR(NativeMissileListInsert_SpellCache, 0x006FF6C0, __stdcall, int, (uint32_t listPtr))
{
    uintptr_t const caller = reinterpret_cast<uintptr_t>(_ReturnAddress());
    int const result = NativeMissileListInsert_SpellCache(listPtr);
    if (caller >= 0x00733000 && caller < 0x00733800)
    {
        static uint32_t logCount = 0;
        if (logCount < 80)
        {
            LOG_INFO << "Native sub_732FF0 missile list insert"
                << "caller" << caller
                << "list" << listPtr
                << "result" << result
                << "armedSpell" << StreamedMissileTrajectoryArmedSpellId;
            ++logCount;
        }
    }

    return result;
}

bool TrySpawnStreamedMissileFallback(uint32_t spellId, void* unit, ClientData::SpellRow const* spell, uint32_t* targetList, uint32_t visualPtr)
{
    (void)spellId;
    (void)unit;
    (void)spell;
    (void)targetList;
    (void)visualPtr;

    static bool loggedDisabled = false;
    if (!loggedDisabled)
    {
        LOG_ERROR << "Streamed scripted missile fallback disabled; local child missile enqueue crashed in native world update";
        loggedDisabled = true;
    }

    return false;

    if (!spell || !targetList || !IsReadableMemory(targetList, sizeof(uint32_t) * 2))
        return false;

    uint32_t const targetCount = targetList[0];
    auto* const targetData = reinterpret_cast<uint64_t*>(static_cast<uintptr_t>(targetList[1]));
    if (targetCount == 0 || !IsReadableMemory(targetData, sizeof(uint64_t)))
        return false;

    auto const* visual = reinterpret_cast<SpellVisualDebugRow const*>(static_cast<uintptr_t>(visualPtr));
    if (!IsReadableMemory(visual, sizeof(SpellVisualDebugRow)) || !visual->hasMissile || !visual->missileModel)
        return false;

    auto const* effectName = static_cast<SpellVisualEffectNameDebugRow const*>(
        ClientDB::GetRow(reinterpret_cast<void*>(SPELL_VISUAL_EFFECT_NAME_DB_RECORDS_ADDRESS), visual->missileModel));
    if (!effectName || !IsReadableMemory(effectName, sizeof(SpellVisualEffectNameDebugRow)) || !effectName->fileName || !IsReadableMemory(effectName->fileName, 1) || !effectName->fileName[0])
        return false;

    uint64_t casterGuid = 0;
    if (unit && IsReadableMemory(reinterpret_cast<uint8_t*>(unit) + 8, sizeof(uint32_t)))
    {
        auto* const casterGuidPtr = *reinterpret_cast<uint64_t**>(reinterpret_cast<uint8_t*>(unit) + 8);
        if (IsReadableMemory(casterGuidPtr, sizeof(uint64_t)))
            casterGuid = *casterGuidPtr;
    }

    uint64_t const targetGuid = targetData[0];
    float const scale = std::isfinite(effectName->scale) && effectName->scale > 0.0f ? effectName->scale : 1.0f;
    uint32_t const instanceId = (spellId << 8) ^ (OsGetAsyncTimeMs() & 0x00FFFFFFu);
    ClientData::C3Vector origin{};
    ClientData::C3Vector dest{};
    ScriptedMissileVisuals::SpawnLocalMissile(instanceId, casterGuid, targetGuid, origin, dest, spell->m_speed > 0.0f ? spell->m_speed : 24.0f, scale, effectName->fileName);

    static std::unordered_set<uint32_t> logged;
    if (logged.size() < 80 && logged.insert(spellId).second)
    {
        LOG_INFO << "Spawned streamed scripted missile fallback"
            << "spell" << spellId
            << "visual" << visual->id
            << "missileModel" << visual->missileModel
            << "casterLow" << static_cast<uint32_t>(casterGuid)
            << "targetLow" << static_cast<uint32_t>(targetGuid)
            << "speed" << spell->m_speed
            << "scale" << scale
            << "model" << effectName->fileName;
    }

    return true;
}

DISABLED_DETOUR(Spell_C_PlayTargetedMissileVisuals_SpellCache, 0x007FFFB0, __cdecl, void, (void* unit, ClientData::SpellRow* spell, uint32_t visual, uint32_t a5, uint32_t a6, uint32_t a7, void* areaTargetCount, void* areaTargetValue, uint32_t a10))
{
    uint32_t* targetList = nullptr;
    __asm
    {
        mov targetList, esi
    }

    uint32_t const spellId = spell ? spell->m_ID : 0;
    if (HasReasonableSpellId(spellId) && SpellCacheStreaming::HasSpell(spellId))
    {
        static std::unordered_set<uint64_t> logged;
        uint64_t const key = (static_cast<uint64_t>(spellId) << 32)
            ^ static_cast<uint64_t>(reinterpret_cast<uintptr_t>(_ReturnAddress()));
        if (logged.size() < 80 && logged.insert(key).second)
        {
            LOG_INFO << "Skipped unsafe streamed targeted missile visual helper"
                << "spell" << spellId
                << "caller" << reinterpret_cast<uintptr_t>(_ReturnAddress())
                << "targetCount" << (IsReadableMemory(targetList, sizeof(uint32_t) * 2) ? targetList[0] : 0)
                << "targetData" << (IsReadableMemory(targetList, sizeof(uint32_t) * 2) ? targetList[1] : 0)
                << "visual" << visual
                << "attributesExD" << spell->m_attributesExD
                << "speed" << spell->m_speed;
        }
        if (!TrySpawnStreamedMissileFallback(spellId, unit, spell, targetList, visual))
        {
            LOG_INFO << "Streamed scripted missile fallback unavailable"
                << "spell" << spellId
                << "visual" << visual
                << "targetList" << reinterpret_cast<uintptr_t>(targetList);
        }
        return;
    }

    __asm
    {
        mov esi, targetList
    }
    Spell_C_PlayTargetedMissileVisuals_SpellCache(unit, spell, visual, a5, a6, a7, areaTargetCount, areaTargetValue, a10);
}

DISABLED_DETOUR(Spell_C_PlayMissileTrajectoryCastVisuals_SpellCache, 0x00800DD0, __cdecl, void, (uint32_t* unit, uint32_t pendingCast, ClientData::SpellRow* spell, uint32_t* visual, uint32_t* kit, uint32_t* targets, float delay))
{
    uint32_t const spellId = spell ? spell->m_ID : 0;
    uint32_t const targetCount = targets ? targets[0] : 0;
    uintptr_t const caller = reinterpret_cast<uintptr_t>(_ReturnAddress());
    bool const streamedSpell = HasReasonableSpellId(spellId) && SpellCacheStreaming::HasSpell(spellId);

    if (streamedSpell)
    {
        static std::unordered_set<uint64_t> logged;
        uint64_t const key = (static_cast<uint64_t>(spellId) << 32) ^ caller;
        if (logged.size() < 80 && logged.insert(key).second)
        {
            LOG_INFO << "Native Spell_C_PlayMissileTrajectoryCastVisuals"
                << "spell" << spellId
                << "caller" << caller
                << "pendingCast" << pendingCast
                << "visual" << (visual ? visual[0] : 0)
                << "kit" << (kit ? kit[0] : 0)
                << "targetCount" << targetCount
                << "attributesExD" << spell->m_attributesExD
                << "delay" << delay;
        }

        if (targetCount > 64)
        {
            LOG_ERROR << "Blocked unreasonable streamed missile trajectory target count"
                << "spell" << spellId
                << "caller" << caller
                << "targetCount" << targetCount;
            return;
        }
    }

    Spell_C_PlayMissileTrajectoryCastVisuals_SpellCache(unit, pendingCast, spell, visual, kit, targets, delay);
}

DISABLED_DETOUR_THISCALL_NOARGS(CGUnit_C__UpdateMissileTrajectory_SpellCache, 0x006FE980, void)
{
    uint32_t const spellId = *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(self) + 0xF68);
    if (HasReasonableSpellId(spellId) && SpellCacheStreaming::HasSpell(spellId))
    {
        static uint32_t logCount = 0;
        if (logCount < 80)
        {
            LOG_INFO << "Native CGUnit_C__UpdateMissileTrajectory"
                << "spell" << spellId
                << "self" << reinterpret_cast<uintptr_t>(self)
                << "remaining" << *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(self) + 0xF6C)
                << "active" << *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(self) + 0xF70)
                << "started" << *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(self) + 0xF74)
                << "finalSent" << static_cast<uint32_t>(*reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(self) + 0xF78))
                << "caller" << reinterpret_cast<uintptr_t>(_ReturnAddress());
            ++logCount;
        }
    }

    CGUnit_C__UpdateMissileTrajectory_SpellCache(self);
}

DISABLED_DETOUR_THISCALL_NOARGS(CGUnit_C__SendFinalMissileTrajectoryUpdate_SpellCache, 0x006FD6B0, void)
{
    uint32_t const spellId = *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(self) + 0xF68);
    if (HasReasonableSpellId(spellId) && SpellCacheStreaming::HasSpell(spellId))
    {
        static uint32_t logCount = 0;
        if (logCount < 80)
        {
            LOG_INFO << "Native CGUnit_C__SendFinalMissileTrajectoryUpdate"
                << "spell" << spellId
                << "self" << reinterpret_cast<uintptr_t>(self)
                << "remaining" << *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(self) + 0xF6C)
                << "active" << *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(self) + 0xF70)
                << "started" << *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(self) + 0xF74)
                << "finalSent" << static_cast<uint32_t>(*reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(self) + 0xF78))
                << "caller" << reinterpret_cast<uintptr_t>(_ReturnAddress());
            ++logCount;
        }
    }

    CGUnit_C__SendFinalMissileTrajectoryUpdate_SpellCache(self);
}

DISABLED_DETOUR(UnitMissileTrajectoryUpdateGlobal_SpellCache, 0x006FDA20, __cdecl, int, ())
{
    uint32_t const armedSpell = StreamedMissileTrajectoryArmedSpellId;
    if (StreamedMissileTrajectoryArmedSpellId && StreamedMissileTrajectoryGlobalLogCount < 80)
    {
        LOG_INFO << "Native UnitMissileTrajectory global update enter"
            << "armedSpell" << armedSpell
            << "caller" << reinterpret_cast<uintptr_t>(_ReturnAddress());
        ++StreamedMissileTrajectoryGlobalLogCount;
    }

    int const result = UnitMissileTrajectoryUpdateGlobal_SpellCache();

    if (armedSpell && StreamedMissileTrajectoryGlobalReturnLogCount < 80)
    {
        LOG_INFO << "Native UnitMissileTrajectory global update return"
            << "armedSpell" << armedSpell
            << "result" << result
            << "currentArmedSpell" << StreamedMissileTrajectoryArmedSpellId;
        ++StreamedMissileTrajectoryGlobalReturnLogCount;
    }

    if (armedSpell && result == 0 && StreamedMissileTrajectoryGlobalReturnLogCount >= 1)
    {
        LOG_INFO << "Cleared stuck streamed UnitMissileTrajectory global state"
            << "spell" << armedSpell
            << "guidLow" << *reinterpret_cast<uint32_t*>(UNIT_MISSILE_TRAJECTORY_GUID_LOW_ADDRESS)
            << "guidHigh" << *reinterpret_cast<uint32_t*>(UNIT_MISSILE_TRAJECTORY_GUID_HIGH_ADDRESS)
            << "lastSpell" << *reinterpret_cast<uint32_t*>(UNIT_MISSILE_TRAJECTORY_LAST_SPELL_ADDRESS)
            << "state" << *reinterpret_cast<uint32_t*>(UNIT_MISSILE_TRAJECTORY_STATE_ADDRESS)
            << "mode" << *reinterpret_cast<uint32_t*>(UNIT_MISSILE_TRAJECTORY_MODE_ADDRESS);
        *reinterpret_cast<uint32_t*>(UNIT_MISSILE_TRAJECTORY_GUID_LOW_ADDRESS) = 0;
        *reinterpret_cast<uint32_t*>(UNIT_MISSILE_TRAJECTORY_GUID_HIGH_ADDRESS) = 0;
        *reinterpret_cast<uint32_t*>(UNIT_MISSILE_TRAJECTORY_LAST_SPELL_ADDRESS) = 0;
        *reinterpret_cast<uint32_t*>(UNIT_MISSILE_TRAJECTORY_STATE_ADDRESS) = 0;
        *reinterpret_cast<uint32_t*>(UNIT_MISSILE_TRAJECTORY_MODE_ADDRESS) = 0;
        StreamedMissileTrajectoryArmedSpellId = 0;
    }

    return result;
}

DISABLED_DETOUR(Spell_C_SpellVisualHasChainEffect_SpellCache, 0x00800BF0, __cdecl, bool, (uint32_t* visual))
{
    bool const result = Spell_C_SpellVisualHasChainEffect_SpellCache(visual);

    if (NativeVisualChainCheckHits < 0xFFFFFFFF)
        ++NativeVisualChainCheckHits;

    NativeVisualChainCheckLastVisual = visual ? visual[0] : 0;
    NativeVisualChainCheckLastCastKit = visual ? visual[2] : 0;
    NativeVisualChainCheckLastImpactKit = visual ? visual[3] : 0;
    NativeVisualChainCheckLastResult = result ? 1u : 0u;
    NativeVisualChainCheckLastCaller = reinterpret_cast<uintptr_t>(_ReturnAddress());

    if (visual)
    {
        auto spell = StreamedVisualIdToSpellId.find(visual[0]);
        if (spell != StreamedVisualIdToSpellId.end())
        {
            RecordStreamedVisualPipelineStage(
                spell->second,
                STREAMED_VISUAL_STAGE_CHAIN_CHECK,
                NativeVisualChainCheckLastCaller,
                visual[0],
                0,
                0,
                NativeVisualChainCheckLastResult);
        }
    }

    static uint32_t logCount = 0;
    if (logCount < 40)
    {
        LOG_INFO << "Native SpellVisualHasChainEffect"
            << "visual" << NativeVisualChainCheckLastVisual
            << "castKit" << NativeVisualChainCheckLastCastKit
            << "impactKit" << NativeVisualChainCheckLastImpactKit
            << "result" << NativeVisualChainCheckLastResult
            << "caller" << NativeVisualChainCheckLastCaller;
        ++logCount;
    }

    return result;
}

DISABLED_DETOUR_THISCALL(CGUnit_C__DelaySpellVisualKit_SpellCache, 0x00728050, float*, (uint32_t a2, uint32_t a3, uint32_t a4, float* a5, uint32_t a6, uint32_t a7, uint32_t a8, uint32_t a9, uint32_t a10, uint32_t a11, uint32_t a12))
{
    if (NativeVisualDelayHits < 0xFFFFFFFF)
        ++NativeVisualDelayHits;

    NativeVisualDelayLastArg2 = a2;
    NativeVisualDelayLastKitId = a3;
    NativeVisualDelayLastArg4 = a4;
    NativeVisualDelayLastArg8 = a8;
    NativeVisualDelayLastArg9 = a9;
    NativeVisualDelayLastArg10 = a10;
    NativeVisualDelayLastArg11 = a11;
    NativeVisualDelayLastArg12 = a12;
    NativeVisualDelayLastCaller = reinterpret_cast<uintptr_t>(_ReturnAddress());
    std::memcpy(&NativeVisualDelayLastDelay, &a12, sizeof(NativeVisualDelayLastDelay));

    auto delayedSpell = StreamedVisualKitIdToSpellId.find(a3);
    if (delayedSpell != StreamedVisualKitIdToSpellId.end())
    {
        RecordStreamedVisualPipelineStage(
            delayedSpell->second,
            STREAMED_VISUAL_STAGE_DELAY_KIT,
            NativeVisualDelayLastCaller,
            0,
            a3,
            0,
            1);
    }

    static uint32_t logCount = 0;
    if (logCount < 40)
    {
        float delay = 0.0f;
        std::memcpy(&delay, &a12, sizeof(delay));

        LOG_INFO << "Native CGUnit_C__DelaySpellVisualKit"
            << "arg2" << a2
            << "kit" << a3
            << "arg4" << a4
            << "hasPos" << (a5 ? 1 : 0)
            << "arg8" << a8
            << "arg9" << a9
            << "arg10" << a10
            << "arg11" << a11
            << "delay" << delay
            << "caller" << NativeVisualDelayLastCaller;
        ++logCount;
    }

    return CGUnit_C__DelaySpellVisualKit_SpellCache(self, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12);
}

DISABLED_DETOUR_THISCALL(CGObject_C__PlaySpellVisualKit_SpellCache, 0x00745230, void*, (uint32_t* args))
{
    if (SpellVisualKitPlayTotalHits < 0xFFFFFFFF)
        ++SpellVisualKitPlayTotalHits;

    SpellVisualKitPlayLastRawCaller = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(_ReturnAddress()));
    if (!args || !args[0])
    {
        if (SpellVisualKitPlayNullSpellHits < 0xFFFFFFFF)
            ++SpellVisualKitPlayNullSpellHits;
    }
    if (!args || !args[1])
    {
        if (SpellVisualKitPlayNullKitHits < 0xFFFFFFFF)
            ++SpellVisualKitPlayNullKitHits;
    }

    if (args && args[0] && args[1])
    {
        auto const* spell = reinterpret_cast<ClientData::SpellRow const*>(args[0]);
        uint32_t const spellId = spell->m_ID;
        uint32_t const kitType = args[2];
        auto const* kit = reinterpret_cast<SpellVisualKitDebugRow const*>(args[1]);
        SpellVisualKitPlayLastSpellId = spellId;
        SpellVisualKitPlayLastRawKitId = kit->id;
        SpellVisualKitPlayLastRawKitType = kitType;
        uint32_t const effectMask =
            (kit->headEffect ? 0x0001u : 0u)
            | (kit->chestEffect ? 0x0002u : 0u)
            | (kit->baseEffect ? 0x0004u : 0u)
            | (kit->leftHandEffect ? 0x0008u : 0u)
            | (kit->rightHandEffect ? 0x0010u : 0u)
            | (kit->breathEffect ? 0x0020u : 0u)
            | (kit->leftWeaponEffect ? 0x0040u : 0u)
            | (kit->rightWeaponEffect ? 0x0080u : 0u)
            | (kit->specialEffect[0] ? 0x0100u : 0u)
            | (kit->specialEffect[1] ? 0x0200u : 0u)
            | (kit->specialEffect[2] ? 0x0400u : 0u)
            | (kit->worldEffect ? 0x0800u : 0u);

        uint32_t& hits = SpellVisualKitPlayHits[spellId];
        if (hits < 0xFFFFFFFF)
            ++hits;

        SpellVisualKitPlayLastCaller[spellId] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(_ReturnAddress()));
        SpellVisualKitPlayLastKitId[spellId] = kit->id;
        SpellVisualKitPlayLastKitType[spellId] = kitType;
        SpellVisualKitPlayLastEffectMask[spellId] = effectMask;
        if (SpellCacheStreaming::HasSpell(spellId))
        {
            RecordStreamedVisualPipelineStage(
                spellId,
                STREAMED_VISUAL_STAGE_PLAY_KIT,
                SpellVisualKitPlayLastCaller[spellId],
                spell->m_spellVisualID[0],
                kit->id,
                kitType,
                1);
        }

        static std::unordered_set<uint64_t> logged;
        uint64_t const key =
            (static_cast<uint64_t>(spellId) << 32)
            ^ (static_cast<uint64_t>(kit->id) << 8)
            ^ static_cast<uint64_t>(kitType);
        if (logged.size() < 160 && logged.insert(key).second)
        {
            LOG_INFO << "Native PlaySpellVisualKit"
                << "spell" << spellId
                << "kit" << kit->id
                << "type" << kitType
                << "effects" << effectMask
                << "head" << kit->headEffect
                << "chest" << kit->chestEffect
                << "base" << kit->baseEffect
                << "leftHand" << kit->leftHandEffect
                << "rightHand" << kit->rightHandEffect
                << "world" << kit->worldEffect;
        }

    }

    return CGObject_C__PlaySpellVisualKit_SpellCache(self, args);
}

DISABLED_DETOUR(Unit_GetPowerDivisor_SpellCache, 0x007FDE00, __cdecl, uint32_t, (uint32_t powerType))
{
    int32_t const signedPowerType = static_cast<int32_t>(powerType);
    if (signedPowerType >= 0 && signedPowerType > static_cast<int32_t>(ClientData::POWER_RUNIC_POWER))
    {
        LOG_ERROR << "Blocked invalid power divisor lookup" << signedPowerType;
        return 1;
    }

    return Unit_GetPowerDivisor_SpellCache(powerType);
}

DISABLED_DETOUR(SpellRec_HasEffect_SpellCache, 0x007FDE20, __cdecl, char, (ClientData::SpellRow* spell, uint32_t effectId))
{
    if (!IsReadableMemory(spell, sizeof(ClientData::SpellRow)))
        return 0;

    if (SpellCacheStreaming::HasSpell(spell->m_ID))
        return SpellCacheStreaming::HasSpellEffect(spell->m_ID, effectId) ? 1 : 0;

    return SpellRec_HasEffect_SpellCache(spell, effectId);
}

DISABLED_DETOUR(SpellRec_HasAura_SpellCache, 0x007FDE50, __cdecl, char, (ClientData::SpellRow* spell, uint32_t auraId))
{
    if (!IsReadableMemory(spell, sizeof(ClientData::SpellRow)))
        return 0;

    if (SpellCacheStreaming::HasSpell(spell->m_ID))
        return SpellCacheStreaming::HasSpellAura(spell->m_ID, auraId) ? 1 : 0;

    return SpellRec_HasAura_SpellCache(spell, auraId);
}

DISABLED_DETOUR(SpellRec_HasAnyApplyAuraEffect_SpellCache, 0x007FDF20, __cdecl, char, (ClientData::SpellRow* spell))
{
    if (!IsReadableMemory(spell, sizeof(ClientData::SpellRow)))
        return 0;

    if (SpellCacheStreaming::HasSpell(spell->m_ID))
        return SpellCacheStreaming::HasSpellEffect(spell->m_ID, SPELL_EFFECT_APPLY_AURA) ? 1 : 0;

    return SpellRec_HasAnyApplyAuraEffect_SpellCache(spell);
}

DISABLED_DETOUR(SpellRec_HasTrackingAura_SpellCache, 0x007FDF60, __cdecl, char, (ClientData::SpellRow* spell))
{
    if (!IsReadableMemory(spell, sizeof(ClientData::SpellRow)))
        return 0;

    if (SpellCacheStreaming::HasSpell(spell->m_ID))
    {
        static constexpr uint32_t TRACKING_AURAS[] = { 44, 45, 151 };
        return SpellCacheStreaming::HasAnySpellAura(spell->m_ID, TRACKING_AURAS, 3) ? 1 : 0;
    }

    return SpellRec_HasTrackingAura_SpellCache(spell);
}

DISABLED_DETOUR(SpellRec_WillSummonCritter_SpellCache, 0x008009B0, __cdecl, char, (ClientData::SpellRow* spell))
{
    if (!IsReadableMemory(spell, sizeof(ClientData::SpellRow)))
        return 0;

    if (SpellCacheStreaming::HasSpell(spell->m_ID))
        return StreamedSpellWillSummonCritter(spell->m_ID) ? 1 : 0;

    return SpellRec_WillSummonCritter_SpellCache(spell);
}

DISABLED_DETOUR(SpellRec_GetCompanionType_SpellCache, 0x0053B410, __cdecl, uint32_t, (ClientData::SpellRow* spell))
{
    if (!IsReadableMemory(spell, sizeof(ClientData::SpellRow)))
        return 2;

    if (SpellCacheStreaming::HasSpell(spell->m_ID))
    {
        if (StreamedSpellHasEffect(spell->m_ID, SPELL_EFFECT_SUMMON))
            return 0;

        return StreamedSpellHasAura(spell->m_ID, SPELL_AURA_MOUNTED) ? 1 : 2;
    }

    return SpellRec_GetCompanionType_SpellCache(spell);
}

DISABLED_DETOUR(SpellRec_IsCurrentOrActive_SpellCache, 0x00802CB0, __cdecl, char, (ClientData::SpellRow* spell, void* unit))
{
    char const nativeResult = SpellRec_IsCurrentOrActive_SpellCache(spell, unit);
    if (nativeResult || !IsReadableMemory(spell, sizeof(ClientData::SpellRow)) || !SpellCacheStreaming::HasSpell(spell->m_ID))
        return nativeResult;

    return StreamedSpellCurrentOnUnit(spell, unit) ? 1 : 0;
}

DISABLED_DETOUR(Spell_C_IsCurrentOrCancelable_SpellCache, 0x00806030, __cdecl, char, (uint32_t spellId))
{
    char const nativeResult = Spell_C_IsCurrentOrCancelable_SpellCache(spellId);
    if (nativeResult || !SpellCacheStreaming::HasSpell(spellId))
        return nativeResult;

    return StreamedSpellCanToggleOrCancel(spellId) ? 1 : 0;
}

DISABLED_DETOUR_THISCALL(CGUnit_C__GetPredictedPower_SpellCache, 0x0071C2E0, uint32_t, (int32_t powerIndex))
{
    if (powerIndex == -2)
        return CGUnit_C__GetPredictedHealth_SpellCache(self);

    if (powerIndex < 0 || powerIndex > static_cast<int32_t>(ClientData::POWER_RUNIC_POWER))
    {
        static uint32_t logCount = 0;
        if (logCount < 20)
        {
            LOG_ERROR << "Blocked invalid predicted power lookup" << powerIndex;
            ++logCount;
        }

        return 0;
    }

    return CGUnit_C__GetPredictedPower_SpellCache(self, powerIndex);
}

CLIENT_DETOUR_THISCALL(CGUnit_C__IsSpellKnown_SpellCache, 0x007260E0, bool, (uint32_t spellId))
{
    if (!SPELL_CACHE_ENABLE_UI_INTEGRATION)
        return CGUnit_C__IsSpellKnown_SpellCache(self, spellId);

    PumpSpellCacheRequests();

    uint64_t const activePlayerGuid = ClntObjMgr::GetActivePlayer();
    if (UnitGuid(self) == activePlayerGuid)
    {
        SyncKnownSpellRowsPlayer(activePlayerGuid);
        return HasReasonableSpellId(spellId) && HasKnownSpellRow(spellId);
    }

    return CGUnit_C__IsSpellKnown_SpellCache(self, spellId);
}

DISABLED_DETOUR(BuildWeaponSkillSpellMap_SpellCache, 0x006E0340, __cdecl, void, ())
{
    BuildWeaponSkillSpellMapFromCache();
}

DISABLED_DETOUR_THISCALL(CGPlayer_C__AddKnownSpell_SpellCache, 0x006E7B00, void, (int32_t spellId, int32_t spellCategory, int32_t learned, int32_t addToSpellbook))
{
    static uint32_t logCount = 0;
    void* const returnAddress = _ReturnAddress();
    const char* source = KnownSpellSource(returnAddress);
    uint64_t const activePlayerGuid = ClntObjMgr::GetActivePlayer();
    SyncKnownSpellRowsPlayer(activePlayerGuid);

    if (spellId <= 0 || !HasReasonableSpellId(static_cast<uint32_t>(spellId)))
    {
        if (logCount < 300)
        {
            LOG_ERROR << "Blocked invalid AddKnownSpell source" << source
                << "spell" << spellId
                << "category" << spellCategory
                << "learned" << learned
                << "addToSpellbook" << addToSpellbook
                << "caller" << returnAddress;
            ++logCount;
        }

        return;
    }

    uint32_t const knownSpellId = static_cast<uint32_t>(spellId);
    bool const insertedKnownSpell = AddKnownSpellRow(knownSpellId);
    if (insertedKnownSpell && logCount < 300)
    {
        LOG_INFO << "Inserted streamed known spell row source" << source
            << "spell" << knownSpellId
            << "knownCount" << static_cast<uint32_t>(KnownSpellRows.size());
        ++logCount;
    }

    ClientData::SpellRow row{};
    if (!SpellCacheStreaming::TryBuildSpellRow(knownSpellId, row))
    {
        if (logCount < 300)
        {
            LOG_INFO << "AddKnownSpell cache miss source" << source
                << "spell" << spellId
                << "category" << spellCategory
                << "learned" << learned
                << "addToSpellbook" << addToSpellbook
                << "caller" << returnAddress;
            ++logCount;
        }

        TrackPendingKnownSpellAdd(self, knownSpellId, spellCategory, learned, addToSpellbook);
        SpellCacheStreaming::RequestSpell(knownSpellId);
        return;
    }

    if (logCount < 300)
    {
        LOG_INFO << "AddKnownSpell cache hit scheduled streamed UI source" << source
            << "spell" << spellId
            << "hash" << GlobalCDBCMap.getRow<SpellCacheRow>("Spell", spellId)->spellDataHash
            << "caller" << returnAddress;
        ++logCount;
    }

    ScheduleKnownSpellUiWork({ self, knownSpellId, spellCategory, learned, addToSpellbook }, "cache-hit");
}

DISABLED_DETOUR(Script_GetNumSpellTabs_SpellCache, 0x0053B5C0, __cdecl, int, (lua_State* L))
{
    if (SpellbookTabCount() > 0)
        return Script_GetNumSpellTabs_SpellCache(L);

    RebuildKnownSpellbookOrder();
    if (!KnownSpellbookTabs.empty())
    {
        ClientLua::PushNumber(L, static_cast<double>(KnownSpellbookTabs.size()));
        return 1;
    }

    return Script_GetNumSpellTabs_SpellCache(L);
}

DISABLED_DETOUR(Script_GetSpellTabInfo_SpellCache, 0x0053BE70, __cdecl, int, (lua_State* L))
{
    if (SpellbookTabCount() > 0)
        return Script_GetSpellTabInfo_SpellCache(L);

    RebuildKnownSpellbookOrder();
    if (!KnownSpellbookTabs.empty() && ClientLua::IsNumber(L, 1))
    {
        uint32_t const oneBasedTab = static_cast<uint32_t>(ClientLua::GetNumber(L, 1, 0));
        if (oneBasedTab && oneBasedTab <= KnownSpellbookTabs.size())
        {
            KnownSpellbookTab const& tab = KnownSpellbookTabs[oneBasedTab - 1];
            ClientLua::PushString(L, tab.name.c_str());
            ClientLua::PushString(L, tab.icon.c_str());
            ClientLua::PushNumber(L, static_cast<double>(tab.offset));
            ClientLua::PushNumber(L, static_cast<double>(tab.count));
            ClientLua::PushNumber(L, static_cast<double>(tab.offset));
            ClientLua::PushNumber(L, static_cast<double>(tab.count));
            return 6;
        }
    }

    return Script_GetSpellTabInfo_SpellCache(L);
}

DISABLED_DETOUR(Script_GetSpellInfo_SpellCache, 0x00540A30, __cdecl, int, (lua_State* L))
{
    if (PlayerSpellSlotMapSize() > 0 && ClientLua::GetTop(L) > 1)
        return Script_GetSpellInfo_SpellCache(L);

    uint32_t spellId = 0;
    if (!TryResolveSpellLuaArg(L, spellId))
        return Script_GetSpellInfo_SpellCache(L);

    ClientData::SpellRow row{};
    if (!SpellCacheStreaming::TryGetSpellRow(spellId, row))
        return Script_GetSpellInfo_SpellCache(L);

    ClientLua::PushString(L, row.m_name_lang ? row.m_name_lang : "");
    ClientLua::PushString(L, row.m_nameSubtext_lang ? row.m_nameSubtext_lang : "");
    ClientLua::PushString(L, GetSpellIconTexture(spellId) ? GetSpellIconTexture(spellId) : "");
    ClientLua::PushNumber(L, 0);
    ClientLua::PushBoolean(L, false);
    ClientLua::PushNumber(L, static_cast<double>(row.m_powerType));
    ClientLua::PushNumber(L, (row.m_attributes & 0x40) ? 0.0 : static_cast<double>(SpellRec_C::GetCastTime(&row, 0, 0, 1)));
    ClientLua::PushNumber(L, 0);
    ClientLua::PushNumber(L, 0);
    return 9;
}

DISABLED_DETOUR(Script_GetSpellName_SpellCache, 0x005407F0, __cdecl, int, (lua_State* L))
{
    if (PlayerSpellSlotMapSize() > 0 && ClientLua::GetTop(L) > 1)
        return Script_GetSpellName_SpellCache(L);

    uint32_t spellId = 0;
    if (!TryResolveSpellLuaArg(L, spellId))
        return Script_GetSpellName_SpellCache(L);

    ClientData::SpellRow row{};
    if (!SpellCacheStreaming::TryGetSpellRow(spellId, row))
        return Script_GetSpellName_SpellCache(L);

    ClientLua::PushString(L, row.m_name_lang ? row.m_name_lang : "");
    ClientLua::PushString(L, row.m_nameSubtext_lang ? row.m_nameSubtext_lang : "");
    return 2;
}

DISABLED_DETOUR(Script_GetSpellLink_SpellCache, 0x005408E0, __cdecl, int, (lua_State* L))
{
    if (PlayerSpellSlotMapSize() > 0 && ClientLua::GetTop(L) > 1)
        return Script_GetSpellLink_SpellCache(L);

    uint32_t spellId = 0;
    if (!TryResolveSpellLuaArg(L, spellId))
        return Script_GetSpellLink_SpellCache(L);

    ClientData::SpellRow row{};
    if (!SpellCacheStreaming::TryGetSpellRow(spellId, row))
        return Script_GetSpellLink_SpellCache(L);

    char const* name = row.m_name_lang && *row.m_name_lang ? row.m_name_lang : "Unknown";
    std::string link = "|cff71d5ff|Hspell:";
    link += std::to_string(spellId);
    link += "|h[";
    link += name;
    link += "]|h|r";
    ClientLua::PushString(L, link.c_str());
    ClientLua::PushNil(L);
    return 2;
}

DISABLED_DETOUR(Script_GetSpellTexture_SpellCache, 0x00540D70, __cdecl, int, (lua_State* L))
{
    if (PlayerSpellSlotMapSize() > 0 && ClientLua::GetTop(L) > 1)
        return Script_GetSpellTexture_SpellCache(L);

    uint32_t spellId = 0;
    if (TryResolveSpellLuaArg(L, spellId))
    {
        if (char const* texture = GetSpellIconTexture(spellId))
            ClientLua::PushString(L, texture);
        else
            ClientLua::PushNil(L);
        return 1;
    }

    return Script_GetSpellTexture_SpellCache(L);
}

DISABLED_DETOUR(Script_GetSpellCooldown_SpellCache, 0x00540E80, __cdecl, int, (lua_State* L))
{
    if (PlayerSpellSlotMapSize() > 0 && ClientLua::GetTop(L) > 1)
        return Script_GetSpellCooldown_SpellCache(L);

    uint32_t spellId = 0;
    if (!TryResolveSpellLuaArg(L, spellId))
        return Script_GetSpellCooldown_SpellCache(L);

    uint32_t durationMs = 0;
    uint32_t startMs = 0;
    uint32_t enable = 0;
    Spell_C__GetSpellCooldown_SpellCache(spellId, 0, &durationMs, &startMs, &enable);

    ClientLua::PushNumber(L, static_cast<double>(startMs) * 0.001);
    ClientLua::PushNumber(L, static_cast<double>(durationMs) * 0.001);
    ClientLua::PushNumber(L, static_cast<double>(enable));
    return 3;
}

DISABLED_DETOUR(Script_IsPassiveSpell_SpellCache, 0x00541340, __cdecl, int, (lua_State* L))
{
    if (PlayerSpellSlotMapSize() > 0 && ClientLua::GetTop(L) > 1)
        return Script_IsPassiveSpell_SpellCache(L);

    uint32_t spellId = 0;
    if (!TryResolveSpellLuaArg(L, spellId))
        return Script_IsPassiveSpell_SpellCache(L);

    ClientData::SpellRow row{};
    if (!SpellCacheStreaming::TryGetSpellRow(spellId, row))
        return Script_IsPassiveSpell_SpellCache(L);

    if ((row.m_attributes & STREAMED_SPELL_ATTR0_PASSIVE) != 0)
        ClientLua::PushNumber(L, 1.0);
    else
        ClientLua::PushNil(L);
    return 1;
}

DISABLED_DETOUR(CGActionBar__GetTexture_SpellCache, 0x005A97F0, __cdecl, char*, (uint32_t slot))
{
    uint32_t isPet = 0;
    uint32_t const spellId = CGActionBar__GetSpell_SpellCache(slot, &isPet);
    if (spellId)
    {
        if (char const* texture = GetSpellIconTexture(spellId))
            return const_cast<char*>(texture);
    }

    return CGActionBar__GetTexture_SpellCache(slot);
}

DISABLED_DETOUR(Script_GetActionTexture_SpellCache, 0x005A9B30, __cdecl, int, (lua_State* L))
{
    if (!ClientLua::IsNumber(L, 1))
        return Script_GetActionTexture_SpellCache(L);

    uint32_t const oneBasedSlot = static_cast<uint32_t>(ClientLua::GetNumber(L, 1, 0));
    if (oneBasedSlot && oneBasedSlot <= 144)
    {
        uint32_t isPet = 0;
        uint32_t const spellId = CGActionBar__GetSpell_SpellCache(oneBasedSlot - 1, &isPet);
        if (spellId)
        {
            if (char const* texture = GetSpellIconTexture(spellId))
            {
                ClientLua::PushString(L, texture);
                return 1;
            }
        }
    }

    return Script_GetActionTexture_SpellCache(L);
}

DISABLED_DETOUR(CGSpellBook__UpdateSpells_SpellCache, 0x0053CA70, __cdecl, void, (int sortPlayer, int sortPet, int updateInterruptMasks))
{
    LogSpellbookDirectReaderDiagnostics("UpdateSpells");
    CGSpellBook__UpdateSpells_SpellCache(sortPlayer, sortPet, updateInterruptMasks);
}

DISABLED_DETOUR(CGSpellBook__UpdateUsable_SpellCache, 0x0053CF10, __cdecl, void, ())
{
    LogSpellbookDirectReaderDiagnostics("UpdateUsable");
    CGSpellBook__UpdateUsable_SpellCache();
}

DISABLED_DETOUR(PH_SMSG_SPELL_COOLDOWN_SpellCache, 0x00806DD0, __cdecl, int, (int a1, int opcode, int packetTimeMs, void* packet))
{
    (void)a1;
    (void)opcode;

    if (!packet)
        return 1;

    uint64_t casterGuid = 0;
    uint8_t flags = 0;
    CDataStore_GetUInt64_SpellCache(packet, &casterGuid);
    CDataStore_GetUInt8_SpellCache(packet, &flags);

    uint32_t isPet = 0;
    if (casterGuid == ClntObjMgr::GetActivePlayer())
    {
        isPet = 0;
    }
    else if (casterGuid == CGPetInfo_C::GetPet(0))
    {
        isPet = 1;
    }
    else
    {
        return 1;
    }

    bool const onEventFlag = (flags & 1) != 0;
    bool const ignoreCooldownOnEvent = (flags & 2) != 0;

    while (!DataStoreIsRead(packet))
    {
        uint32_t spellId = 0;
        uint32_t forcedRecoveryTime = 0;
        CDataStore_GetUInt32_SpellCache(packet, &spellId);
        CDataStore_GetUInt32_SpellCache(packet, &forcedRecoveryTime);

        if (!HasReasonableSpellId(spellId))
            continue;

        AddStreamedSpellCooldownHistory(spellId, forcedRecoveryTime, static_cast<uint32_t>(packetTimeMs), isPet, onEventFlag, ignoreCooldownOnEvent);
    }

    CGSpellBook_UpdateCooldowns_SpellCache();
    if (isPet)
        CGameUI_SignalEventPetBarUpdateCooldown_SpellCache();
    else
        CGameUI_SignalEventBagUpdateCooldown_SpellCache();

    QueueNativeActionBarRefresh(0, "spell-cooldown-packet");
    return 1;
}

DISABLED_DETOUR(Spell_C_CancelSpell_SpellCache, 0x00806200, __cdecl, void, (void* spellCast, char a2, char a3, int spellCastResult))
{
    if (!spellCast)
    {
        static uint32_t logCount = 0;
        if (logCount < 20)
        {
            LOG_ERROR << "Blocked Spell_C_CancelSpell null spellCast";
            ++logCount;
        }
        return;
    }

    Spell_C_CancelSpell_SpellCache(spellCast, a2, a3, spellCastResult);
}

DISABLED_LUA_FN(StreamedSpellBook_GetKnownCount, (lua_State* L))
{
    RebuildKnownSpellbookOrder();
    ClientLua::PushNumber(L, static_cast<double>(KnownSpellbookOrder.size()));
    return 1;
}

DISABLED_LUA_FN(StreamedSpellBook_GetNumSpellTabs, (lua_State* L))
{
    RebuildKnownSpellbookOrder();
    ClientLua::PushNumber(L, static_cast<double>(KnownSpellbookTabs.size()));
    return 1;
}

DISABLED_LUA_FN(StreamedSpellBook_GetSpellId, (lua_State* L))
{
    uint32_t spellId = 0;
    if (ClientLua::IsNumber(L, 1) && TryGetKnownSpellbookSpellBySlot(static_cast<uint32_t>(ClientLua::GetNumber(L, 1, 0)), spellId))
    {
        ClientLua::PushNumber(L, static_cast<double>(spellId));
        return 1;
    }

    ClientLua::PushNil(L);
    return 1;
}

DISABLED_LUA_FN(StreamedSpellBook_GetSpellTabInfo, (lua_State* L))
{
    RebuildKnownSpellbookOrder();
    if (KnownSpellbookTabs.empty() || !ClientLua::IsNumber(L, 1))
    {
        return 0;
    }

    uint32_t const oneBasedTab = static_cast<uint32_t>(ClientLua::GetNumber(L, 1, 0));
    if (!oneBasedTab || oneBasedTab > KnownSpellbookTabs.size())
        return 0;

    KnownSpellbookTab const& tab = KnownSpellbookTabs[oneBasedTab - 1];
    ClientLua::PushString(L, tab.name.c_str());
    ClientLua::PushString(L, tab.icon.c_str());
    ClientLua::PushNumber(L, static_cast<double>(tab.offset));
    ClientLua::PushNumber(L, static_cast<double>(tab.count));
    ClientLua::PushNumber(L, static_cast<double>(tab.offset));
    ClientLua::PushNumber(L, static_cast<double>(tab.count));
    return 6;
}

DISABLED_LUA_FN(StreamedSpellBook_DebugSlot, (lua_State* L))
{
    if (!ClientLua::IsNumber(L, 1))
        return 0;

    uint32_t spellId = 0;
    if (!TryGetKnownSpellbookSpellBySlot(static_cast<uint32_t>(ClientLua::GetNumber(L, 1, 0)), spellId))
        return 0;

    auto found = KnownSpellbookEntries.find(spellId);
    if (found == KnownSpellbookEntries.end())
        return 0;

    KnownSpellbookEntry const& entry = found->second;
    ClientData::SpellRow row{};
    SpellCacheStreaming::TryGetSpellRow(spellId, row);
    ClientLua::PushNumber(L, static_cast<double>(spellId));
    ClientLua::PushNumber(L, static_cast<double>(entry.tabKey));
    ClientLua::PushNumber(L, static_cast<double>(entry.spellCategory));
    ClientLua::PushNumber(L, entry.addToSpellbook ? 1.0 : 0.0);
    ClientLua::PushNumber(L, entry.visibleInSpellbook ? 1.0 : 0.0);
    ClientLua::PushNumber(L, static_cast<double>(row.m_attributes));
    ClientLua::PushNumber(L, static_cast<double>(row.m_attributesEx));
    ClientLua::PushNumber(L, static_cast<double>(row.m_effect[0]));
    ClientLua::PushNumber(L, static_cast<double>(row.m_effect[1]));
    ClientLua::PushNumber(L, static_cast<double>(row.m_effect[2]));
    ClientLua::PushString(L, row.m_name_lang ? row.m_name_lang : "");
    return 11;
}

DISABLED_LUA_FN(StreamedSpellCache_GetSpellInfoById, (lua_State* L))
{
    if (!ClientLua::IsNumber(L, 1))
        return 0;

    uint32_t const spellId = static_cast<uint32_t>(ClientLua::GetNumber(L, 1, 0));
    ClientData::SpellRow row{};
    if (!SpellCacheStreaming::TryGetSpellRow(spellId, row))
        return 0;

    char const* texture = GetSpellIconTexture(spellId);
    ClientLua::PushString(L, row.m_name_lang ? row.m_name_lang : "");
    ClientLua::PushString(L, row.m_nameSubtext_lang ? row.m_nameSubtext_lang : "");
    ClientLua::PushString(L, texture ? texture : "");
    ClientLua::PushNumber(L, 0);
    ClientLua::PushBoolean(L, false);
    ClientLua::PushNumber(L, static_cast<double>(row.m_powerType));
    ClientLua::PushNumber(L, (row.m_attributes & 0x40) ? 0.0 : static_cast<double>(SpellRec_C::GetCastTime(&row, 0, 0, 1)));
    ClientLua::PushNumber(L, 0);
    ClientLua::PushNumber(L, 0);
    return 9;
}

DISABLED_LUA_FN(StreamedSpellCache_GetSpellTextureById, (lua_State* L))
{
    if (!ClientLua::IsNumber(L, 1))
    {
        ClientLua::PushNil(L);
        return 1;
    }

    char const* texture = GetSpellIconTexture(static_cast<uint32_t>(ClientLua::GetNumber(L, 1, 0)));
    if (texture)
        ClientLua::PushString(L, texture);
    else
        ClientLua::PushNil(L);
    return 1;
}

DISABLED_LUA_FN(StreamedSpellCache_DebugActionSlot, (lua_State* L))
{
    if (!ClientLua::IsNumber(L, 1))
        return 0;

    uint32_t const oneBasedSlot = static_cast<uint32_t>(ClientLua::GetNumber(L, 1, 0));
    if (!oneBasedSlot || oneBasedSlot > 144)
        return 0;

    uint32_t const zeroBasedSlot = oneBasedSlot - 1;
    uint32_t isPet = 0;
    uint32_t const spellId = CGActionBar__GetSpell_SpellCache(zeroBasedSlot, &isPet);

    uint32_t actionDuration = 0;
    uint32_t actionStart = 0;
    uint32_t actionEnable = 0;
    CGActionBar__GetCooldown_SpellCache(zeroBasedSlot, &actionStart, &actionDuration, &actionEnable);

    uint32_t spellDuration = 0;
    uint32_t spellStart = 0;
    uint32_t spellEnable = 0;
    if (spellId)
        Spell_C__GetSpellCooldown_SpellCache(spellId, isPet, &spellDuration, &spellStart, &spellEnable);

    ClientLua::PushNumber(L, static_cast<double>(spellId));
    ClientLua::PushNumber(L, static_cast<double>(isPet));
    ClientLua::PushNumber(L, static_cast<double>(actionStart) * 0.001);
    ClientLua::PushNumber(L, static_cast<double>(actionDuration) * 0.001);
    ClientLua::PushNumber(L, static_cast<double>(actionEnable));
    ClientLua::PushNumber(L, static_cast<double>(spellStart) * 0.001);
    ClientLua::PushNumber(L, static_cast<double>(spellDuration) * 0.001);
    ClientLua::PushNumber(L, static_cast<double>(spellEnable));
    return 8;
}

DISABLED_LUA_FN(StreamedSpellCache_DebugSpellRow, (lua_State* L))
{
    if (!ClientLua::IsNumber(L, 1))
        return 0;

    uint32_t const spellId = static_cast<uint32_t>(ClientLua::GetNumber(L, 1, 0));
    ClientData::SpellRow row{};
    if (!SpellCacheStreaming::TryGetSpellRow(spellId, row))
        return 0;

    ClientLua::PushNumber(L, static_cast<double>(row.m_ID));
    ClientLua::PushNumber(L, static_cast<double>(row.m_category));
    ClientLua::PushNumber(L, static_cast<double>(row.m_recoveryTime));
    ClientLua::PushNumber(L, static_cast<double>(row.m_categoryRecoveryTime));
    ClientLua::PushNumber(L, static_cast<double>(row.m_startRecoveryCategory));
    ClientLua::PushNumber(L, static_cast<double>(row.m_startRecoveryTime));
    ClientLua::PushNumber(L, static_cast<double>(row.m_attributes));
    ClientLua::PushNumber(L, static_cast<double>(row.m_attributesEx));
    ClientLua::PushNumber(L, static_cast<double>(row.m_effect[0]));
    ClientLua::PushNumber(L, static_cast<double>(row.m_effect[1]));
    ClientLua::PushNumber(L, static_cast<double>(row.m_effect[2]));
    ClientLua::PushNumber(L, static_cast<double>(row.m_targets));
    ClientLua::PushNumber(L, static_cast<double>(row.m_maxTargets));
    ClientLua::PushNumber(L, static_cast<double>(row.m_speed));
    ClientLua::PushNumber(L, static_cast<double>(row.m_attributesExD));
    ClientLua::PushNumber(L, static_cast<double>(row.m_attributesExG));
    ClientLua::PushNumber(L, static_cast<double>(row.m_spellVisualID[0]));
    ClientLua::PushNumber(L, static_cast<double>(row.m_spellVisualID[1]));
    ClientLua::PushNumber(L, static_cast<double>(row.m_spellMissileID));
    for (uint32_t i = 0; i < 3; ++i)
    {
        ClientLua::PushNumber(L, static_cast<double>(row.m_implicitTargetA[i]));
        ClientLua::PushNumber(L, static_cast<double>(row.m_implicitTargetB[i]));
        ClientLua::PushNumber(L, static_cast<double>(row.m_effectChainTargets[i]));
    }
    return 28;
}

DISABLED_LUA_FN(StreamedShapeshift_GetNumForms, (lua_State* L))
{
    RebuildKnownSpellbookOrder();
    ClientLua::PushNumber(L, static_cast<double>(GetSortedStreamedStances().size()));
    return 1;
}

DISABLED_LUA_FN(StreamedShapeshift_GetSpellId, (lua_State* L))
{
    KnownSpellbookEntry stance{};
    if (!ClientLua::IsNumber(L, 1) || !TryGetStreamedStanceByIndex(static_cast<uint32_t>(ClientLua::GetNumber(L, 1, 0)), stance))
    {
        ClientLua::PushNil(L);
        return 1;
    }

    ClientLua::PushNumber(L, static_cast<double>(stance.spellId));
    return 1;
}

DISABLED_LUA_FN(StreamedShapeshift_GetFormInfo, (lua_State* L))
{
    KnownSpellbookEntry stance{};
    if (!ClientLua::IsNumber(L, 1) || !TryGetStreamedStanceByIndex(static_cast<uint32_t>(ClientLua::GetNumber(L, 1, 0)), stance))
        return 0;

    ClientData::SpellRow row{};
    if (!SpellCacheStreaming::TryGetSpellRow(stance.spellId, row))
        return 0;

    char const* texture = GetSpellIconTexture(stance.spellId);
    ClientLua::PushString(L, texture ? texture : "");
    ClientLua::PushString(L, (row.m_name_lang && *row.m_name_lang) ? row.m_name_lang : "");
    void* localPlayer = ClntObjMgrObjectPtr_ActionState_SpellCache(
        ClntObjMgrGetActivePlayer_ActionState_SpellCache(),
        STREAMED_TYPEMASK_PLAYER);
    ClientLua::PushBoolean(L, StreamedSpellCurrentOnUnit(&row, localPlayer) ? 1 : 0);
    ClientLua::PushBoolean(L, true);
    return 4;
}

DISABLED_LUA_FN(StreamedShapeshift_GetFormCooldown, (lua_State* L))
{
    KnownSpellbookEntry stance{};
    if (!ClientLua::IsNumber(L, 1) || !TryGetStreamedStanceByIndex(static_cast<uint32_t>(ClientLua::GetNumber(L, 1, 0)), stance))
    {
        ClientLua::PushNumber(L, 0.0);
        ClientLua::PushNumber(L, 0.0);
        ClientLua::PushNumber(L, 1.0);
        return 3;
    }

    uint32_t start = 0;
    uint32_t duration = 0;
    uint32_t enable = 1;
    Spell_C__GetSpellCooldown_SpellCache(stance.spellId, 0, &start, &duration, &enable);
    ClientLua::PushNumber(L, static_cast<double>(start) * 0.001);
    ClientLua::PushNumber(L, static_cast<double>(duration) * 0.001);
    ClientLua::PushNumber(L, static_cast<double>(enable));
    return 3;
}

DISABLED_LUA_FN(StreamedSpellCache_DebugDirectReaders, (lua_State* L))
{
    RebuildKnownSpellbookOrder();

    uint32_t streamedReady = 0;
    uint32_t nativeAvailable = 0;
    uint32_t nativeUnavailable = 0;
    uint32_t firstMissingNative = 0;
    uint32_t hiddenSkill = 0;
    uint32_t hiddenLanguage = 0;
    uint32_t hiddenStance = 0;
    uint32_t hiddenClient = 0;
    uint32_t hiddenNoName = 0;

    for (uint32_t spellId : KnownSpellbookOrder)
    {
        if (!SpellCacheStreaming::HasSpell(spellId))
            continue;

        ++streamedReady;
        if (NativeSpellDbCanLookup(spellId))
        {
            ++nativeAvailable;
        }
        else
        {
            ++nativeUnavailable;
            if (!firstMissingNative)
                firstMissingNative = spellId;
        }
    }

    for (auto const& entry : KnownSpellbookEntries)
    {
        KnownSpellbookEntry const& spell = entry.second;
        if (spell.visibleInSpellbook)
            continue;

        if (spell.isSkillGrant)
            ++hiddenSkill;
        if (spell.isLanguage)
            ++hiddenLanguage;
        if (spell.isStance)
            ++hiddenStance;
        if (spell.isHiddenClient)
            ++hiddenClient;
        if (!spell.hasName)
            ++hiddenNoName;
    }

    LogSpellbookDirectReaderDiagnostics("LuaDebug", true);

    ClientLua::PushNumber(L, static_cast<double>(KnownSpellbookOrder.size()));
    ClientLua::PushNumber(L, static_cast<double>(streamedReady));
    ClientLua::PushNumber(L, static_cast<double>(nativeAvailable));
    ClientLua::PushNumber(L, static_cast<double>(nativeUnavailable));
    ClientLua::PushNumber(L, static_cast<double>(firstMissingNative));
    ClientLua::PushNumber(L, static_cast<double>(PlayerSpellSlotMapSize()));
    ClientLua::PushNumber(L, static_cast<double>(VisibleSpellbookCount()));
    ClientLua::PushNumber(L, static_cast<double>(SpellbookTabCount()));
    ClientLua::PushNumber(L, static_cast<double>(KnownSpellbookEntries.size()));
    ClientLua::PushNumber(L, static_cast<double>(hiddenSkill));
    ClientLua::PushNumber(L, static_cast<double>(hiddenLanguage));
    ClientLua::PushNumber(L, static_cast<double>(hiddenStance));
    ClientLua::PushNumber(L, static_cast<double>(hiddenClient));
    ClientLua::PushNumber(L, static_cast<double>(hiddenNoName));
    return 14;
}

DISABLED_LUA_FN(StreamedSpellCache_DebugNativeSpellDb, (lua_State* L))
{
    auto const* spellDb = reinterpret_cast<ClientData::WoWClientDB const*>(SPELL_DB_ADDRESS);
    if (!spellDb)
        return 0;

    ClientLua::PushNumber(L, static_cast<double>(reinterpret_cast<uintptr_t>(spellDb->funcTable)));
    ClientLua::PushNumber(L, static_cast<double>(spellDb->isLoaded));
    ClientLua::PushNumber(L, static_cast<double>(spellDb->numRows));
    ClientLua::PushNumber(L, static_cast<double>(spellDb->minIndex));
    ClientLua::PushNumber(L, static_cast<double>(spellDb->maxIndex));
    ClientLua::PushNumber(L, static_cast<double>(spellDb->stringTable));
    ClientLua::PushNumber(L, static_cast<double>(reinterpret_cast<uintptr_t>(spellDb->funcTable2)));
    ClientLua::PushNumber(L, static_cast<double>(reinterpret_cast<uintptr_t>(spellDb->FirstRow)));
    ClientLua::PushNumber(L, static_cast<double>(reinterpret_cast<uintptr_t>(spellDb->Rows)));
    ClientLua::PushNumber(L, static_cast<double>(NativeSpellDbRows.size()));
    ClientLua::PushNumber(L, NativeSpellDbRecordsById ? 1.0 : 0.0);
    ClientLua::PushNumber(L, static_cast<double>(PlayerSpellSlotMapSize()));
    ClientLua::PushNumber(L, static_cast<double>(VisibleSpellbookCount()));
    ClientLua::PushNumber(L, static_cast<double>(SpellbookTabCount()));
    return 14;
}

DISABLED_LUA_FN(StreamedSpellCache_DebugVisualChain, (lua_State* L))
{
    if (!ClientLua::IsNumber(L, 1))
        return 0;

    uint32_t const spellId = static_cast<uint32_t>(ClientLua::GetNumber(L, 1, 0));
    ClientData::SpellRow row{};
    if (!SpellCacheStreaming::TryGetSpellRow(spellId, row))
        return 0;

    bool const iconExists = NativeDbHasRow(SPELL_ICON_DB_RECORDS_ADDRESS, row.m_spellIconID);
    bool const activeIconExists = NativeDbHasRow(SPELL_ICON_DB_RECORDS_ADDRESS, row.m_activeIconID);
    bool const spellMissileExists = NativeDbHasRow(SPELL_MISSILE_DB_RECORDS_ADDRESS, row.m_spellMissileID);
    auto* visual = reinterpret_cast<SpellVisualDebugRow*>(
        ClientDB::GetRow(reinterpret_cast<void*>(SPELL_VISUAL_DB_RECORDS_ADDRESS), row.m_spellVisualID[0]));
    bool const visualExists = visual != nullptr;
    uint32_t const castKit = visual ? visual->castKit : 0;
    uint32_t const impactKit = visual ? visual->impactKit : 0;
    uint32_t const channelKit = visual ? visual->channelKit : 0;
    uint32_t const missileModel = visual ? visual->missileModel : 0;
    uint32_t const missileMotion = visual ? visual->missileMotion : 0;
    uint32_t const missileTargetingKit = visual ? visual->missileTargetingKit : 0;
    bool const castKitExists = NativeDbHasRow(SPELL_VISUAL_KIT_DB_RECORDS_ADDRESS, castKit);
    bool const impactKitExists = NativeDbHasRow(SPELL_VISUAL_KIT_DB_RECORDS_ADDRESS, impactKit);
    bool const channelKitExists = NativeDbHasRow(SPELL_VISUAL_KIT_DB_RECORDS_ADDRESS, channelKit);
    bool const missileModelExists = NativeDbHasRow(SPELL_VISUAL_EFFECT_NAME_DB_RECORDS_ADDRESS, missileModel);
    bool const missileMotionExists = NativeDbHasRow(SPELL_MISSILE_MOTION_DB_RECORDS_ADDRESS, missileMotion);
    bool const missileTargetingKitExists = NativeDbHasRow(SPELL_VISUAL_KIT_DB_RECORDS_ADDRESS, missileTargetingKit);

    LOG_INFO << "Streamed spell visual chain"
        << "spell" << spellId
        << "visual1" << row.m_spellVisualID[0]
        << "visual2" << row.m_spellVisualID[1]
        << "visual1Exists" << visualExists
        << "icon" << row.m_spellIconID
        << "iconExists" << iconExists
        << "activeIcon" << row.m_activeIconID
        << "activeIconExists" << activeIconExists
        << "missile" << row.m_spellMissileID
        << "spellMissileExists" << spellMissileExists
        << "castKit" << castKit
        << "castKitExists" << castKitExists
        << "impactKit" << impactKit
        << "impactKitExists" << impactKitExists
        << "channelKit" << channelKit
        << "channelKitExists" << channelKitExists
        << "missileModel" << missileModel
        << "missileModelExists" << missileModelExists
        << "missileMotion" << missileMotion
        << "missileMotionExists" << missileMotionExists
        << "missileTargetingKit" << missileTargetingKit
        << "missileTargetingKitExists" << missileTargetingKitExists;

    ClientLua::PushNumber(L, static_cast<double>(row.m_ID));
    ClientLua::PushNumber(L, static_cast<double>(row.m_spellVisualID[0]));
    ClientLua::PushNumber(L, static_cast<double>(row.m_spellVisualID[1]));
    ClientLua::PushNumber(L, visualExists ? 1.0 : 0.0);
    ClientLua::PushNumber(L, static_cast<double>(row.m_spellIconID));
    ClientLua::PushNumber(L, iconExists ? 1.0 : 0.0);
    ClientLua::PushNumber(L, static_cast<double>(row.m_activeIconID));
    ClientLua::PushNumber(L, activeIconExists ? 1.0 : 0.0);
    ClientLua::PushNumber(L, static_cast<double>(row.m_spellMissileID));
    ClientLua::PushNumber(L, spellMissileExists ? 1.0 : 0.0);
    ClientLua::PushNumber(L, static_cast<double>(castKit));
    ClientLua::PushNumber(L, castKitExists ? 1.0 : 0.0);
    ClientLua::PushNumber(L, static_cast<double>(impactKit));
    ClientLua::PushNumber(L, impactKitExists ? 1.0 : 0.0);
    ClientLua::PushNumber(L, static_cast<double>(channelKit));
    ClientLua::PushNumber(L, channelKitExists ? 1.0 : 0.0);
    ClientLua::PushNumber(L, static_cast<double>(missileModel));
    ClientLua::PushNumber(L, missileModelExists ? 1.0 : 0.0);
    ClientLua::PushNumber(L, static_cast<double>(missileMotion));
    ClientLua::PushNumber(L, missileMotionExists ? 1.0 : 0.0);
    ClientLua::PushNumber(L, static_cast<double>(missileTargetingKit));
    ClientLua::PushNumber(L, missileTargetingKitExists ? 1.0 : 0.0);
    return 22;
}

DISABLED_LUA_FN(StreamedSpellCache_DebugSpellDbLookup, (lua_State* L))
{
    if (!ClientLua::IsNumber(L, 1))
        return 0;

    uint32_t const spellId = static_cast<uint32_t>(ClientLua::GetNumber(L, 1, 0));
    ClientData::SpellRow row{};
    if (!SpellCacheStreaming::TryGetSpellRow(spellId, row))
        return 0;

    auto hit = StreamedSpellDbLookupHits.find(spellId);
    auto caller = StreamedSpellDbLookupLastCaller.find(spellId);
    auto visualHit = StreamedSpellDbVisualLookupHits.find(spellId);
    auto visualCaller = StreamedSpellDbVisualLookupLastCaller.find(spellId);
    auto actionbarHit = StreamedSpellDbActionbarLookupHits.find(spellId);
    auto spellbookHit = StreamedSpellDbSpellbookLookupHits.find(spellId);
    ClientLua::PushNumber(L, static_cast<double>(spellId));
    ClientLua::PushNumber(L, hit != StreamedSpellDbLookupHits.end() ? static_cast<double>(hit->second) : 0.0);
    ClientLua::PushNumber(L, caller != StreamedSpellDbLookupLastCaller.end() ? static_cast<double>(caller->second) : 0.0);
    ClientLua::PushNumber(L, visualHit != StreamedSpellDbVisualLookupHits.end() ? static_cast<double>(visualHit->second) : 0.0);
    ClientLua::PushNumber(L, visualCaller != StreamedSpellDbVisualLookupLastCaller.end() ? static_cast<double>(visualCaller->second) : 0.0);
    ClientLua::PushNumber(L, actionbarHit != StreamedSpellDbActionbarLookupHits.end() ? static_cast<double>(actionbarHit->second) : 0.0);
    ClientLua::PushNumber(L, spellbookHit != StreamedSpellDbSpellbookLookupHits.end() ? static_cast<double>(spellbookHit->second) : 0.0);
    ClientLua::PushNumber(L, static_cast<double>(row.m_spellVisualID[0]));
    ClientLua::PushNumber(L, static_cast<double>(row.m_spellVisualID[1]));
    ClientLua::PushNumber(L, static_cast<double>(row.m_spellIconID));
    ClientLua::PushNumber(L, static_cast<double>(row.m_spellMissileID));
    return 11;
}

DISABLED_LUA_FN(StreamedSpellCache_DebugSpellVisualFallback, (lua_State* L))
{
    if (!ClientLua::IsNumber(L, 1))
        return 0;

    uint32_t const spellId = static_cast<uint32_t>(ClientLua::GetNumber(L, 1, 0));
    auto hits = StreamedSpellVisualRowFallbackHits.find(spellId);
    auto visual = StreamedSpellVisualRowFallbackLastVisual.find(spellId);
    auto caller = StreamedSpellVisualRowFallbackLastCaller.find(spellId);
    auto unitHits = StreamedUnitAppropriateVisualFallbackHits.find(spellId);
    auto unitVisual = StreamedUnitAppropriateVisualFallbackLastVisual.find(spellId);
    auto unitCaller = StreamedUnitAppropriateVisualFallbackLastCaller.find(spellId);
    auto directHits = StreamedDirectCastKitPlayHits.find(spellId);
    auto directVisual = StreamedDirectCastKitLastVisual.find(spellId);
    auto directKit = StreamedDirectCastKitLastKit.find(spellId);
    auto impactHits = StreamedDirectImpactKitPlayHits.find(spellId);
    auto impactVisual = StreamedDirectImpactKitLastVisual.find(spellId);
    auto impactKit = StreamedDirectImpactKitLastKit.find(spellId);
    auto impactAttempts = StreamedDirectImpactKitAttempts.find(spellId);
    auto impactTarget = StreamedDirectImpactKitLastTarget.find(spellId);
    auto impactNoTarget = StreamedDirectImpactKitNoTarget.find(spellId);
    auto impactNoKit = StreamedDirectImpactKitNoKit.find(spellId);

    ClientLua::PushNumber(L, static_cast<double>(spellId));
    ClientLua::PushNumber(L, hits != StreamedSpellVisualRowFallbackHits.end() ? static_cast<double>(hits->second) : 0.0);
    ClientLua::PushNumber(L, visual != StreamedSpellVisualRowFallbackLastVisual.end() ? static_cast<double>(visual->second) : 0.0);
    ClientLua::PushNumber(L, caller != StreamedSpellVisualRowFallbackLastCaller.end() ? static_cast<double>(caller->second) : 0.0);
    ClientLua::PushNumber(L, unitHits != StreamedUnitAppropriateVisualFallbackHits.end() ? static_cast<double>(unitHits->second) : 0.0);
    ClientLua::PushNumber(L, unitVisual != StreamedUnitAppropriateVisualFallbackLastVisual.end() ? static_cast<double>(unitVisual->second) : 0.0);
    ClientLua::PushNumber(L, unitCaller != StreamedUnitAppropriateVisualFallbackLastCaller.end() ? static_cast<double>(unitCaller->second) : 0.0);
    ClientLua::PushNumber(L, directHits != StreamedDirectCastKitPlayHits.end() ? static_cast<double>(directHits->second) : 0.0);
    ClientLua::PushNumber(L, directVisual != StreamedDirectCastKitLastVisual.end() ? static_cast<double>(directVisual->second) : 0.0);
    ClientLua::PushNumber(L, directKit != StreamedDirectCastKitLastKit.end() ? static_cast<double>(directKit->second) : 0.0);
    ClientLua::PushNumber(L, impactHits != StreamedDirectImpactKitPlayHits.end() ? static_cast<double>(impactHits->second) : 0.0);
    ClientLua::PushNumber(L, impactVisual != StreamedDirectImpactKitLastVisual.end() ? static_cast<double>(impactVisual->second) : 0.0);
    ClientLua::PushNumber(L, impactKit != StreamedDirectImpactKitLastKit.end() ? static_cast<double>(impactKit->second) : 0.0);
    ClientLua::PushNumber(L, impactAttempts != StreamedDirectImpactKitAttempts.end() ? static_cast<double>(impactAttempts->second) : 0.0);
    ClientLua::PushNumber(L, impactTarget != StreamedDirectImpactKitLastTarget.end() ? static_cast<double>(impactTarget->second) : 0.0);
    ClientLua::PushNumber(L, impactNoTarget != StreamedDirectImpactKitNoTarget.end() ? static_cast<double>(impactNoTarget->second) : 0.0);
    ClientLua::PushNumber(L, impactNoKit != StreamedDirectImpactKitNoKit.end() ? static_cast<double>(impactNoKit->second) : 0.0);
    return 17;
}

DISABLED_LUA_FN(StreamedSpellCache_DebugVisualKitPlay, (lua_State* L))
{
    if (!ClientLua::IsNumber(L, 1))
        return 0;

    uint32_t const spellId = static_cast<uint32_t>(ClientLua::GetNumber(L, 1, 0));
    auto hit = SpellVisualKitPlayHits.find(spellId);
    auto caller = SpellVisualKitPlayLastCaller.find(spellId);
    auto kit = SpellVisualKitPlayLastKitId.find(spellId);
    auto type = SpellVisualKitPlayLastKitType.find(spellId);
    auto effects = SpellVisualKitPlayLastEffectMask.find(spellId);
    ClientLua::PushNumber(L, static_cast<double>(spellId));
    ClientLua::PushNumber(L, hit != SpellVisualKitPlayHits.end() ? static_cast<double>(hit->second) : 0.0);
    ClientLua::PushNumber(L, caller != SpellVisualKitPlayLastCaller.end() ? static_cast<double>(caller->second) : 0.0);
    ClientLua::PushNumber(L, kit != SpellVisualKitPlayLastKitId.end() ? static_cast<double>(kit->second) : 0.0);
    ClientLua::PushNumber(L, type != SpellVisualKitPlayLastKitType.end() ? static_cast<double>(type->second) : 0.0);
    ClientLua::PushNumber(L, effects != SpellVisualKitPlayLastEffectMask.end() ? static_cast<double>(effects->second) : 0.0);
    return 6;
}

DISABLED_LUA_FN(StreamedSpellCache_DebugVisualKitPlayGlobal, (lua_State* L))
{
    ClientLua::PushNumber(L, static_cast<double>(SpellVisualKitPlayTotalHits));
    ClientLua::PushNumber(L, static_cast<double>(SpellVisualKitPlayNullSpellHits));
    ClientLua::PushNumber(L, static_cast<double>(SpellVisualKitPlayNullKitHits));
    ClientLua::PushNumber(L, static_cast<double>(SpellVisualKitPlayLastSpellId));
    ClientLua::PushNumber(L, static_cast<double>(SpellVisualKitPlayLastRawKitId));
    ClientLua::PushNumber(L, static_cast<double>(SpellVisualKitPlayLastRawKitType));
    ClientLua::PushNumber(L, static_cast<double>(SpellVisualKitPlayLastRawCaller));
    return 7;
}

DISABLED_LUA_FN(StreamedSpellCache_DebugNativeMissile, (lua_State* L))
{
    if (!ClientLua::IsNumber(L, 1))
        return 0;

    uint32_t const spellId = static_cast<uint32_t>(ClientLua::GetNumber(L, 1, 0));
    auto hits = NativeMissileCreateHits.find(spellId);
    auto caller = NativeMissileCreateLastCaller.find(spellId);
    auto visual = NativeMissileCreateLastVisual.find(spellId);
    auto visualModel = NativeMissileCreateLastVisualModel.find(spellId);
    auto missileMotion = NativeMissileCreateLastMissileMotion.find(spellId);
    auto speedBits = NativeMissileCreateLastSpeedBits.find(spellId);

    float speed = 0.0f;
    if (speedBits != NativeMissileCreateLastSpeedBits.end())
        std::memcpy(&speed, &speedBits->second, sizeof(speed));

    ClientLua::PushNumber(L, static_cast<double>(spellId));
    ClientLua::PushNumber(L, hits != NativeMissileCreateHits.end() ? static_cast<double>(hits->second) : 0.0);
    ClientLua::PushNumber(L, caller != NativeMissileCreateLastCaller.end() ? static_cast<double>(caller->second) : 0.0);
    ClientLua::PushNumber(L, visual != NativeMissileCreateLastVisual.end() ? static_cast<double>(visual->second) : 0.0);
    ClientLua::PushNumber(L, visualModel != NativeMissileCreateLastVisualModel.end() ? static_cast<double>(visualModel->second) : 0.0);
    ClientLua::PushNumber(L, missileMotion != NativeMissileCreateLastMissileMotion.end() ? static_cast<double>(missileMotion->second) : 0.0);
    ClientLua::PushNumber(L, static_cast<double>(speed));
    return 7;
}

DISABLED_LUA_FN(StreamedSpellCache_DebugNativeVisualStages, (lua_State* L))
{
    float delay = 0.0f;
    std::memcpy(&delay, &NativeVisualDelayLastDelay, sizeof(delay));

    ClientLua::PushNumber(L, static_cast<double>(NativeVisualChainCheckHits));
    ClientLua::PushNumber(L, static_cast<double>(NativeVisualChainCheckLastVisual));
    ClientLua::PushNumber(L, static_cast<double>(NativeVisualChainCheckLastCastKit));
    ClientLua::PushNumber(L, static_cast<double>(NativeVisualChainCheckLastImpactKit));
    ClientLua::PushNumber(L, static_cast<double>(NativeVisualChainCheckLastResult));
    ClientLua::PushNumber(L, static_cast<double>(NativeVisualChainCheckLastCaller));
    ClientLua::PushNumber(L, static_cast<double>(NativeVisualDelayHits));
    ClientLua::PushNumber(L, static_cast<double>(NativeVisualDelayLastArg2));
    ClientLua::PushNumber(L, static_cast<double>(NativeVisualDelayLastKitId));
    ClientLua::PushNumber(L, static_cast<double>(NativeVisualDelayLastArg4));
    ClientLua::PushNumber(L, static_cast<double>(NativeVisualDelayLastArg8));
    ClientLua::PushNumber(L, static_cast<double>(NativeVisualDelayLastArg9));
    ClientLua::PushNumber(L, static_cast<double>(NativeVisualDelayLastArg10));
    ClientLua::PushNumber(L, static_cast<double>(NativeVisualDelayLastArg11));
    ClientLua::PushNumber(L, static_cast<double>(delay));
    ClientLua::PushNumber(L, static_cast<double>(NativeVisualDelayLastCaller));
    return 16;
}

DISABLED_LUA_FN(StreamedSpellCache_DebugVisualPipeline, (lua_State* L))
{
    if (!ClientLua::IsNumber(L, 1))
        return 0;

    uint32_t const spellId = static_cast<uint32_t>(ClientLua::GetNumber(L, 1, 0));
    auto hits = StreamedVisualPipelineHits.find(spellId);
    auto stage = StreamedVisualPipelineLastStage.find(spellId);
    auto caller = StreamedVisualPipelineLastCaller.find(spellId);
    auto visual = StreamedVisualPipelineLastVisual.find(spellId);
    auto kit = StreamedVisualPipelineLastKit.find(spellId);
    auto kitType = StreamedVisualPipelineLastKitType.find(spellId);
    auto result = StreamedVisualPipelineLastResult.find(spellId);
    auto missileModel = StreamedVisualPipelineLastMissileModel.find(spellId);
    auto missileMotion = StreamedVisualPipelineLastMissileMotion.find(spellId);

    ClientLua::PushNumber(L, static_cast<double>(spellId));
    ClientLua::PushNumber(L, hits != StreamedVisualPipelineHits.end() ? static_cast<double>(hits->second) : 0.0);
    ClientLua::PushNumber(L, stage != StreamedVisualPipelineLastStage.end() ? static_cast<double>(stage->second) : 0.0);
    ClientLua::PushNumber(L, caller != StreamedVisualPipelineLastCaller.end() ? static_cast<double>(caller->second) : 0.0);
    ClientLua::PushNumber(L, visual != StreamedVisualPipelineLastVisual.end() ? static_cast<double>(visual->second) : 0.0);
    ClientLua::PushNumber(L, kit != StreamedVisualPipelineLastKit.end() ? static_cast<double>(kit->second) : 0.0);
    ClientLua::PushNumber(L, kitType != StreamedVisualPipelineLastKitType.end() ? static_cast<double>(kitType->second) : 0.0);
    ClientLua::PushNumber(L, result != StreamedVisualPipelineLastResult.end() ? static_cast<double>(result->second) : 0.0);
    ClientLua::PushNumber(L, missileModel != StreamedVisualPipelineLastMissileModel.end() ? static_cast<double>(missileModel->second) : 0.0);
    ClientLua::PushNumber(L, missileMotion != StreamedVisualPipelineLastMissileMotion.end() ? static_cast<double>(missileMotion->second) : 0.0);
    return 10;
}
#endif

CLIENT_DETOUR(Spell_C_CancelSpell_StreamingFoundation, 0x00806200, __cdecl, void, (void* spellCast, char a2, char a3, int spellCastResult))
{
    if (!spellCast)
    {
        static uint32_t logCount = 0;
        if (logCount < 20)
        {
            LOG_ERROR << "Blocked Spell_C_CancelSpell null spellCast";
            ++logCount;
        }
        return;
    }

    Spell_C_CancelSpell_StreamingFoundation(spellCast, a2, a3, spellCastResult);
}

CLIENT_DETOUR_THISCALL(CGUnit_C__IsSpellKnown_StreamingFoundation, 0x007260E0, bool, (uint32_t spellId))
{
    if (!SPELL_CACHE_ENABLE_UI_INTEGRATION)
        return CGUnit_C__IsSpellKnown_StreamingFoundation(self, spellId);

    PumpSpellCacheRequests();

    uint64_t const activePlayerGuid = ClntObjMgr::GetActivePlayer();
    if (UnitGuid(self) != activePlayerGuid)
        return CGUnit_C__IsSpellKnown_StreamingFoundation(self, spellId);

    if (!HasReasonableSpellId(spellId))
        return false;

    SyncKnownSpellRowsPlayer(activePlayerGuid);
    if (HasKnownSpellRow(spellId))
        return true;

    uintptr_t const caller = reinterpret_cast<uintptr_t>(_ReturnAddress());
    bool const fromActionButtonsPacket = caller >= 0x006D8750 && caller < 0x006D8900;
    if (fromActionButtonsPacket)
    {
        static uint32_t logCount = 0;
        ForcedStreamedSpellRows.insert(spellId);
        SpellCacheStreaming::RequestSpell(spellId);

        if (logCount < 80)
        {
            LOG_INFO << "Accepted streamed actionbar spell before known-row arrival"
                << "spell" << spellId
                << "knownCount" << static_cast<uint32_t>(KnownSpellRows.size())
                << "cached" << SpellCacheStreaming::HasSpell(spellId)
                << "caller" << _ReturnAddress();
            ++logCount;
        }

        return true;
    }

    return false;
}

CLIENT_DETOUR_THISCALL(CGPlayer_C__RemoveKnownSpell_SpellbookFoundation, 0x006E71D0, void, (int32_t spellId, int32_t silent))
{
    if (!SPELL_CACHE_ENABLE_UI_INTEGRATION)
    {
        CGPlayer_C__RemoveKnownSpell_SpellbookFoundation(self, spellId, silent);
        return;
    }

    static uint32_t logCount = 0;
    void* const returnAddress = _ReturnAddress();
    uint64_t const activePlayerGuid = ClntObjMgr::GetActivePlayer();

    if (UnitGuid(self) != activePlayerGuid)
    {
        CGPlayer_C__RemoveKnownSpell_SpellbookFoundation(self, spellId, silent);
        return;
    }

    SyncKnownSpellRowsPlayer(activePlayerGuid);

    if (spellId <= 0 || !HasReasonableSpellId(static_cast<uint32_t>(spellId)))
    {
        if (logCount < 80)
        {
            LOG_ERROR << "Blocked invalid native RemoveKnownSpell"
                << "spell" << spellId
                << "silent" << silent
                << "caller" << returnAddress;
            ++logCount;
        }
        return;
    }

    uint32_t const knownSpellId = static_cast<uint32_t>(spellId);
    bool const removed = RemoveKnownSpellRow(knownSpellId);

    RebuildKnownSpellbookOrder();
    PublishedKnownSpellbookRevision = KnownSpellbookRevision;
    SignalStreamedSpellbookRefresh();
    if (ClientLua::State())
    {
        ClientLua::DoString(
            "if DHStreamedShapeshiftRefresh then DHStreamedShapeshiftRefresh() end",
            ClientLua::State());
    }

    if (removed || logCount < 160)
    {
        LOG_INFO << "Coopted native RemoveKnownSpell; skipped native spellbook delete side effects"
            << "spell" << knownSpellId
            << "removed" << removed
            << "knownCount" << static_cast<uint32_t>(KnownSpellRows.size())
            << "model" << static_cast<uint32_t>(KnownSpellbookEntries.size())
            << "visible" << static_cast<uint32_t>(KnownSpellbookOrder.size())
            << "tabs" << static_cast<uint32_t>(KnownSpellbookTabs.size())
            << "silent" << silent
            << "caller" << returnAddress;
        ++logCount;
    }
}

CLIENT_DETOUR_THISCALL(CGPlayer_C__AddKnownSpell_SpellbookFoundation, 0x006E7B00, void, (int32_t spellId, int32_t spellCategory, int32_t learned, int32_t addToSpellbook))
{
    if (!SPELL_CACHE_ENABLE_UI_INTEGRATION)
    {
        CGPlayer_C__AddKnownSpell_SpellbookFoundation(self, spellId, spellCategory, learned, addToSpellbook);
        return;
    }

    static uint32_t logCount = 0;
    void* const returnAddress = _ReturnAddress();
    uint64_t const activePlayerGuid = ClntObjMgr::GetActivePlayer();
    SyncKnownSpellRowsPlayer(activePlayerGuid);

    if (spellId <= 0 || !HasReasonableSpellId(static_cast<uint32_t>(spellId)))
    {
        if (logCount < 80)
        {
            LOG_ERROR << "Blocked invalid native AddKnownSpell"
                << "spell" << spellId
                << "category" << spellCategory
                << "learned" << learned
                << "addToSpellbook" << addToSpellbook
                << "caller" << returnAddress;
            ++logCount;
        }

        return;
    }

    uint32_t const knownSpellId = static_cast<uint32_t>(spellId);
    bool const insertedKnownSpell = AddKnownSpellRow(knownSpellId);
    ForcedStreamedSpellRows.insert(knownSpellId);

    if (!SpellCacheStreaming::HasSpell(knownSpellId))
    {
        TrackPendingKnownSpellAdd(self, knownSpellId, spellCategory, learned, addToSpellbook);
        SpellCacheStreaming::RequestSpell(knownSpellId);
    }
    else
    {
        ScheduleKnownSpellUiWork({ self, knownSpellId, spellCategory, learned, addToSpellbook }, "native-add-cache-hit");
    }

    if ((insertedKnownSpell || logCount < 40) && logCount < 160)
    {
        LOG_INFO << "Native AddKnownSpell streamed foundation"
            << "spell" << knownSpellId
            << "cached" << SpellCacheStreaming::HasSpell(knownSpellId)
            << "knownCount" << static_cast<uint32_t>(KnownSpellRows.size())
            << "category" << spellCategory
            << "learned" << learned
            << "addToSpellbook" << addToSpellbook
            << "caller" << returnAddress;
        ++logCount;
    }

    if (KnownSpellNativeFallthroughDebugLogCount < 240)
    {
        LOG_INFO << "Coopted native AddKnownSpell; skipped native spellbook side effects"
            << "spell" << knownSpellId
            << "cached" << SpellCacheStreaming::HasSpell(knownSpellId)
            << "pending" << static_cast<uint32_t>(PendingKnownSpellAdds.size())
            << "queuedRequests" << static_cast<uint32_t>(QueuedSpellRequests.size())
            << "knownCount" << static_cast<uint32_t>(KnownSpellRows.size())
            << "category" << spellCategory
            << "learned" << learned
            << "addToSpellbook" << addToSpellbook
            << "caller" << returnAddress;
        ++KnownSpellNativeFallthroughDebugLogCount;
    }

    return;
}

CLIENT_DETOUR(Script_GetNumSpellTabs_SpellbookFoundation, 0x0053B5C0, __cdecl, int, (lua_State* L))
{
    return Script_GetNumSpellTabs_SpellbookFoundation(L);
}

CLIENT_DETOUR(Script_GetSpellTabInfo_SpellbookFoundation, 0x0053BE70, __cdecl, int, (lua_State* L))
{
    return Script_GetSpellTabInfo_SpellbookFoundation(L);
}

    void LogLocalizedRowDecision(char const* kind, uint32_t spellId, void* rowBuffer, void* caller)
    {
        static uint32_t logCount = 0;
        if (logCount >= 160)
            return;

        LOG_DEBUG << "Spell cache localized row"
            << "kind" << kind
            << "spell" << spellId
            << "buffer" << rowBuffer
            << "caller" << caller;
        ++logCount;
    }

    bool TryBuildStreamedSpellRowForNative(uint32_t spellId, void* rowBuffer, void* caller)
    {
        if (!HasReasonableSpellId(spellId))
            return false;

        if (!IsReadableMemory(rowBuffer, sizeof(ClientData::SpellRow)))
        {
            LogLocalizedRowDecision("invalid-buffer", spellId, rowBuffer, caller);
            return false;
        }

        ClientData::SpellRow row{};
        if (!SpellCacheStreaming::TryBuildSpellRow(spellId, row))
        {
            LogLocalizedRowDecision("miss", spellId, rowBuffer, caller);
            return false;
        }

        std::memcpy(rowBuffer, &row, sizeof(row));
        RecordStreamedSpellDbLookup(spellId, row, caller);
        LogLocalizedRowDecision("streamed-hit", spellId, rowBuffer, caller);
        return true;
    }

    bool TryGetStreamedSpellLuaRow(lua_State* L, uint32_t& spellId, ClientData::SpellRow& row)
    {
        spellId = 0;
        if (!TryResolveSpellLuaArg(L, spellId))
            return false;

        if (SpellCacheStreaming::TryBuildSpellRow(spellId, row))
            return true;

        return false;
    }

    bool TryGetActionBarStreamedSpell(uint32_t zeroBasedSlot, uint32_t& spellId, uint32_t& isPet)
    {
        spellId = 0;
        isPet = 0;
        if (!IsValidActionBarSlot(zeroBasedSlot))
            return false;

        if (TryGetStreamedActionBarEntry(zeroBasedSlot, spellId, true))
            return true;

        if (TryGetStreamedActionBarEntryRaw(zeroBasedSlot, spellId))
            return false;

        spellId = CGActionBar__GetSpell_SpellCache(zeroBasedSlot, &isPet);
        if (!spellId)
            return false;

        if (SpellCacheStreaming::HasSpell(spellId))
        {
            ForcedStreamedSpellRows.insert(spellId);
            return true;
        }

        ForceActionBarStreamedSpell(spellId, true);

        return false;
    }

    bool TryActivateStreamedActionBarSlot(uint32_t zeroBasedSlot, uint64_t targetGuid)
    {
        uint32_t spellId = 0;
        if (!TryGetStreamedActionBarEntryRaw(zeroBasedSlot, spellId))
            return false;

        ClientData::SpellRow row{};
        if (!SpellCacheStreaming::TryBuildSpellRow(spellId, row))
        {
            SpellCacheStreaming::RequestSpell(spellId);
            LOG_INFO << "Blocked uncached streamed actionbar cast"
                << "slot" << (zeroBasedSlot + 1)
                << "spell" << spellId;
            return true;
        }

        LOG_INFO << "Casting streamed actionbar spell"
            << "slot" << (zeroBasedSlot + 1)
            << "spell" << spellId
            << "target" << targetGuid;
        Spell_C_CastActionSpell_StreamedActionBar(spellId, 0, targetGuid);
        return true;
    }

    bool TryPickupStreamedActionBarSlot(uint32_t zeroBasedSlot)
    {
        uint32_t spellId = 0;
        if (!TryGetStreamedActionBarEntryRaw(zeroBasedSlot, spellId))
            return false;

        ClientData::SpellRow row{};
        if (!SpellCacheStreaming::TryBuildSpellRow(spellId, row))
        {
            SpellCacheStreaming::RequestSpell(spellId);
            LOG_INFO << "Blocked uncached streamed actionbar pickup"
                << "slot" << (zeroBasedSlot + 1)
                << "spell" << spellId;
            return true;
        }

        CGSpellBook__SetCursorSpell_StreamedActionBar(spellId);
        StreamedActionBarCursorActive = true;
        StreamedActionBarCursorSpellId = spellId;
        StreamedActionBarCursorSourceSlot = zeroBasedSlot;

        LOG_INFO << "Picked up streamed actionbar spell"
            << "slot" << (zeroBasedSlot + 1)
            << "spell" << spellId;
        return true;
    }

    bool TryPlaceStreamedActionBarCursor(uint32_t zeroBasedSlot)
    {
        if (!StreamedActionBarCursorActive || !HasReasonableSpellId(StreamedActionBarCursorSpellId))
            return false;

        uint32_t const cursorSpell = CGGameUI__GetCursorSpell_StreamedActionBar();
        if (cursorSpell != StreamedActionBarCursorSpellId)
            return false;

        uint32_t const spellId = StreamedActionBarCursorSpellId;
        uint32_t const sourceSlot = StreamedActionBarCursorSourceSlot;
        StreamedActionBarCursorActive = false;
        StreamedActionBarCursorSpellId = 0;
        StreamedActionBarCursorSourceSlot = 0xFFFFFFFF;

        if (!SetStreamedActionBarEntry(zeroBasedSlot, spellId, true))
            return true;

        if (IsValidActionBarSlot(sourceSlot) && sourceSlot != zeroBasedSlot)
            ClearStreamedActionBarEntry(sourceSlot);

        CGGameUI__ClearCursor_StreamedActionBar(1, 1);
        LOG_INFO << "Placed streamed actionbar cursor"
            << "slot" << (zeroBasedSlot + 1)
            << "spell" << spellId;
        return true;
    }

    void ClearStreamedActionBarCursor()
    {
        StreamedActionBarCursorActive = false;
        StreamedActionBarCursorSpellId = 0;
        StreamedActionBarCursorSourceSlot = 0xFFFFFFFF;
    }

    uint32_t GetStreamedSpellCooldownSeconds(uint32_t spellId, uint32_t isPet, uint32_t& startSeconds, uint32_t& durationSeconds, uint32_t& enable)
    {
        uint32_t durationMs = 0;
        uint32_t startMs = 0;
        uint32_t enabled = 0;
        Spell_C__GetSpellCooldown_SpellCache(spellId, isPet, &durationMs, &startMs, &enabled);

        startSeconds = startMs / 1000u;
        durationSeconds = durationMs / 1000u;
        enable = enabled;
        return durationMs;
    }

CLIENT_DETOUR_THISCALL(ClientDb_GetLocalizedRow_ActionbarFoundation, 0x004CFD20, int, (uint32_t rowId, void* rowBuffer))
{
    if (!SPELL_CACHE_ENABLE_UI_INTEGRATION)
        return ClientDb_GetLocalizedRow_ActionbarFoundation(self, rowId, rowBuffer);

    if (self != reinterpret_cast<void*>(SPELL_DB_ADDRESS))
        return ClientDb_GetLocalizedRow_ActionbarFoundation(self, rowId, rowBuffer);

    if (ForcedStreamedSpellRows.find(rowId) != ForcedStreamedSpellRows.end())
    {
        if (TryBuildStreamedSpellRowForNative(rowId, rowBuffer, _ReturnAddress()))
            return 1;

        if (IsReadableMemory(rowBuffer, sizeof(ClientData::SpellRow)))
            std::memset(rowBuffer, 0, sizeof(ClientData::SpellRow));
        LogLocalizedRowDecision("forced-streamed-miss", rowId, rowBuffer, _ReturnAddress());
        return 0;
    }

    int const nativeResult = ClientDb_GetLocalizedRow_ActionbarFoundation(self, rowId, rowBuffer);

    if (nativeResult)
    {
        LogLocalizedRowDecision("native-hit", rowId, rowBuffer, _ReturnAddress());
        return nativeResult;
    }

    return TryBuildStreamedSpellRowForNative(rowId, rowBuffer, _ReturnAddress()) ? 1 : 0;
}

CLIENT_DETOUR(Script_GetSpellInfo_ActionbarFoundation, 0x00540A30, __cdecl, int, (lua_State* L))
{
    if (!SPELL_CACHE_ENABLE_UI_INTEGRATION)
        return Script_GetSpellInfo_ActionbarFoundation(L);

    ClientData::SpellRow row{};
    uint32_t spellId = 0;
    if (!TryGetStreamedSpellLuaRow(L, spellId, row))
    {
        if (IsSpellbookSpellLuaArg(L))
            return PushNil(L);
        return Script_GetSpellInfo_ActionbarFoundation(L);
    }

    char const* texture = GetSpellIconTexture(spellId);
    ClientLua::PushString(L, row.m_name_lang ? row.m_name_lang : "");
    ClientLua::PushString(L, row.m_nameSubtext_lang ? row.m_nameSubtext_lang : "");
    ClientLua::PushString(L, texture ? texture : "");
    ClientLua::PushNumber(L, 0);
    ClientLua::PushBoolean(L, false);
    ClientLua::PushNumber(L, static_cast<double>(row.m_powerType));
    ClientLua::PushNumber(L, (row.m_attributes & STREAMED_SPELL_ATTR0_PASSIVE) ? 0.0 : static_cast<double>(SpellRec_C::GetCastTime(&row, 0, 0, 1)));
    ClientLua::PushNumber(L, 0);
    ClientLua::PushNumber(L, 0);
    return 9;
}

CLIENT_DETOUR(Script_GetSpellName_ActionbarFoundation, 0x005407F0, __cdecl, int, (lua_State* L))
{
    if (!SPELL_CACHE_ENABLE_UI_INTEGRATION)
        return Script_GetSpellName_ActionbarFoundation(L);

    ClientData::SpellRow row{};
    uint32_t spellId = 0;
    if (!TryGetStreamedSpellLuaRow(L, spellId, row))
    {
        if (IsSpellbookSpellLuaArg(L))
            return PushTwoNils(L);
        return Script_GetSpellName_ActionbarFoundation(L);
    }

    ClientLua::PushString(L, row.m_name_lang ? row.m_name_lang : "");
    ClientLua::PushString(L, row.m_nameSubtext_lang ? row.m_nameSubtext_lang : "");
    return 2;
}

CLIENT_DETOUR(Script_GetSpellLink_ActionbarFoundation, 0x005408E0, __cdecl, int, (lua_State* L))
{
    if (!SPELL_CACHE_ENABLE_UI_INTEGRATION)
        return Script_GetSpellLink_ActionbarFoundation(L);

    ClientData::SpellRow row{};
    uint32_t spellId = 0;
    if (!TryGetStreamedSpellLuaRow(L, spellId, row))
    {
        if (IsSpellbookSpellLuaArg(L))
            return PushTwoNils(L);
        return Script_GetSpellLink_ActionbarFoundation(L);
    }

    char const* name = row.m_name_lang && *row.m_name_lang ? row.m_name_lang : "Unknown";
    std::string link = "|cff71d5ff|Hspell:";
    link += std::to_string(spellId);
    link += "|h[";
    link += name;
    link += "]|h|r";
    ClientLua::PushString(L, link.c_str());
    ClientLua::PushNil(L);
    return 2;
}

CLIENT_DETOUR(Script_GetSpellTexture_ActionbarFoundation, 0x00540D70, __cdecl, int, (lua_State* L))
{
    if (!SPELL_CACHE_ENABLE_UI_INTEGRATION)
        return Script_GetSpellTexture_ActionbarFoundation(L);

    uint32_t spellId = 0;
    if (!TryResolveSpellLuaArg(L, spellId))
    {
        if (IsSpellbookSpellLuaArg(L))
            return PushNil(L);
        return Script_GetSpellTexture_ActionbarFoundation(L);
    }

    if (!SpellCacheStreaming::HasSpell(spellId))
    {
        if (IsSpellbookSpellLuaArg(L))
            return PushNil(L);
        return Script_GetSpellTexture_ActionbarFoundation(L);
    }

    if (char const* texture = GetSpellIconTexture(spellId))
        ClientLua::PushString(L, texture);
    else
        ClientLua::PushNil(L);
    return 1;
}

CLIENT_DETOUR(Script_GetSpellCooldown_ActionbarFoundation, 0x00540E80, __cdecl, int, (lua_State* L))
{
    if (!SPELL_CACHE_ENABLE_UI_INTEGRATION)
        return Script_GetSpellCooldown_ActionbarFoundation(L);

    uint32_t spellId = 0;
    if (!TryResolveSpellLuaArg(L, spellId) || !SpellCacheStreaming::HasSpell(spellId))
    {
        if (IsSpellbookSpellLuaArg(L))
            return PushThreeZeros(L);
        return Script_GetSpellCooldown_ActionbarFoundation(L);
    }

    uint32_t start = 0;
    uint32_t duration = 0;
    uint32_t enable = 0;
    GetStreamedSpellCooldownSeconds(spellId, 0, start, duration, enable);

    ClientLua::PushNumber(L, static_cast<double>(start));
    ClientLua::PushNumber(L, static_cast<double>(duration));
    ClientLua::PushNumber(L, static_cast<double>(enable));
    return 3;
}

CLIENT_DETOUR(Script_IsPassiveSpell_ActionbarFoundation, 0x00541340, __cdecl, int, (lua_State* L))
{
    if (!SPELL_CACHE_ENABLE_UI_INTEGRATION)
        return Script_IsPassiveSpell_ActionbarFoundation(L);

    ClientData::SpellRow row{};
    uint32_t spellId = 0;
    if (!TryGetStreamedSpellLuaRow(L, spellId, row))
    {
        if (IsSpellbookSpellLuaArg(L))
            return PushNil(L);
        return Script_IsPassiveSpell_ActionbarFoundation(L);
    }

    if ((row.m_attributes & STREAMED_SPELL_ATTR0_PASSIVE) != 0)
        ClientLua::PushNumber(L, 1.0);
    else
        ClientLua::PushNil(L);
    return 1;
}

CLIENT_DETOUR(Script_IsUsableSpell_ActionbarFoundation, 0x00541680, __cdecl, int, (lua_State* L))
{
    if (!SPELL_CACHE_ENABLE_UI_INTEGRATION)
        return Script_IsUsableSpell_ActionbarFoundation(L);

    uint32_t spellId = 0;
    if (!TryResolveSpellLuaArg(L, spellId) || !SpellCacheStreaming::HasSpell(spellId))
    {
        if (IsSpellbookSpellLuaArg(L))
            return PushTwoNils(L);
        return Script_IsUsableSpell_ActionbarFoundation(L);
    }

    ClientData::SpellRow row{};
    if (!SpellCacheStreaming::TryBuildSpellRow(spellId, row))
    {
        if (IsSpellbookSpellLuaArg(L))
            return PushTwoNils(L);
        return Script_IsUsableSpell_ActionbarFoundation(L);
    }

    void* localPlayer = ClntObjMgrObjectPtr_ActionState_SpellCache(
        ClntObjMgrGetActivePlayer_ActionState_SpellCache(),
        STREAMED_TYPEMASK_PLAYER);
    bool usable = false;
    bool lacksPower = false;
    GetStreamedSpellUsableState(row, localPlayer, usable, lacksPower);

    if (usable)
        ClientLua::PushNumber(L, 1.0);
    else
        ClientLua::PushNil(L);
    if (lacksPower)
        ClientLua::PushNumber(L, 1.0);
    else
        ClientLua::PushNil(L);
    return 2;
}

CLIENT_DETOUR(Script_SpellHasRange_ActionbarFoundation, 0x00541AF0, __cdecl, int, (lua_State* L))
{
    if (!SPELL_CACHE_ENABLE_UI_INTEGRATION)
        return Script_SpellHasRange_ActionbarFoundation(L);

    ClientData::SpellRow row{};
    uint32_t spellId = 0;
    if (!TryGetStreamedSpellLuaRow(L, spellId, row))
    {
        if (IsSpellbookSpellLuaArg(L))
            return PushNil(L);
        return Script_SpellHasRange_ActionbarFoundation(L);
    }

    if (row.m_rangeIndex)
        ClientLua::PushNumber(L, 1.0);
    else
        ClientLua::PushNil(L);
    return 1;
}

CLIENT_DETOUR(Script_IsSpellInRange_ActionbarFoundation, 0x00541C60, __cdecl, int, (lua_State* L))
{
    if (!SPELL_CACHE_ENABLE_UI_INTEGRATION)
        return Script_IsSpellInRange_ActionbarFoundation(L);

    ClientData::SpellRow row{};
    uint32_t spellId = 0;
    if (!TryGetStreamedSpellLuaRow(L, spellId, row))
    {
        if (IsSpellbookSpellLuaArg(L))
            return PushNil(L);
        return Script_IsSpellInRange_ActionbarFoundation(L);
    }

    if (!row.m_rangeIndex)
        ClientLua::PushNil(L);
    else
        ClientLua::PushNumber(L, 1.0);
    return 1;
}

CLIENT_DETOUR(Script_GetActionInfo_ActionbarFoundation, 0x005A8F10, __cdecl, int, (lua_State* L))
{
    if (!SPELL_CACHE_ENABLE_UI_INTEGRATION)
        return Script_GetActionInfo_ActionbarFoundation(L);

    if (!ClientLua::IsNumber(L, 1))
        return Script_GetActionInfo_ActionbarFoundation(L);

    uint32_t const oneBasedSlot = static_cast<uint32_t>(ClientLua::GetNumber(L, 1, 0));
    if (!oneBasedSlot || oneBasedSlot > 144)
        return Script_GetActionInfo_ActionbarFoundation(L);

    uint32_t spellId = 0;
    uint32_t isPet = 0;
    if (TryGetStreamedActionBarEntryRaw(oneBasedSlot - 1, spellId))
    {
        if (!SpellCacheStreaming::HasSpell(spellId))
            SpellCacheStreaming::RequestSpell(spellId);

        ClientLua::PushString(L, "spell");
        ClientLua::PushNumber(L, static_cast<double>(spellId));
        ClientLua::PushString(L, "spell");
        ClientLua::PushNumber(L, static_cast<double>(spellId));
        return 4;
    }

    if (TryGetPendingActionBarStreamedSpell(oneBasedSlot - 1, spellId))
    {
        ClientLua::PushString(L, "spell");
        ClientLua::PushNumber(L, static_cast<double>(spellId));
        ClientLua::PushString(L, "spell");
        ClientLua::PushNumber(L, static_cast<double>(spellId));
        return 4;
    }

    if (TryGetActionBarStreamedSpell(oneBasedSlot - 1, spellId, isPet)
        && ForcedStreamedSpellRows.find(spellId) != ForcedStreamedSpellRows.end())
    {
        ClientLua::PushString(L, "spell");
        ClientLua::PushNumber(L, static_cast<double>(spellId));
        ClientLua::PushString(L, isPet ? "pet" : "spell");
        ClientLua::PushNumber(L, static_cast<double>(spellId));
        return 4;
    }

    int const nativeCount = Script_GetActionInfo_ActionbarFoundation(L);
    if (nativeCount > 0)
        return nativeCount;

    if (!TryGetActionBarStreamedSpell(oneBasedSlot - 1, spellId, isPet))
        return 0;

    ClientLua::PushString(L, "spell");
    ClientLua::PushNumber(L, static_cast<double>(spellId));
    ClientLua::PushString(L, isPet ? "pet" : "spell");
    ClientLua::PushNumber(L, static_cast<double>(spellId));
    return 4;
}

CLIENT_DETOUR(CGActionBar__GetTexture_ActionbarFoundation, 0x005A97F0, __cdecl, char*, (uint32_t slot))
{
    if (!SPELL_CACHE_ENABLE_UI_INTEGRATION)
        return CGActionBar__GetTexture_ActionbarFoundation(slot);

    uint32_t spellId = 0;
    uint32_t isPet = 0;
    if (TryGetActionBarStreamedSpell(slot, spellId, isPet))
        if (char const* texture = GetSpellIconTexture(spellId))
            return const_cast<char*>(texture);

    if (TryGetPendingActionBarStreamedSpell(slot, spellId))
        return nullptr;

    return CGActionBar__GetTexture_ActionbarFoundation(slot);
}

CLIENT_DETOUR(Script_GetActionTexture_ActionbarFoundation, 0x005A9B30, __cdecl, int, (lua_State* L))
{
    if (!SPELL_CACHE_ENABLE_UI_INTEGRATION)
        return Script_GetActionTexture_ActionbarFoundation(L);

    if (ClientLua::IsNumber(L, 1))
    {
        uint32_t const oneBasedSlot = static_cast<uint32_t>(ClientLua::GetNumber(L, 1, 0));
        uint32_t spellId = 0;
        uint32_t isPet = 0;
        if (oneBasedSlot && TryGetActionBarStreamedSpell(oneBasedSlot - 1, spellId, isPet))
        {
            if (char const* texture = GetSpellIconTexture(spellId))
            {
                ClientLua::PushString(L, texture);
                return 1;
            }
        }

        if (oneBasedSlot && TryGetPendingActionBarStreamedSpell(oneBasedSlot - 1, spellId))
        {
            ClientLua::PushNil(L);
            return 1;
        }
    }

    return Script_GetActionTexture_ActionbarFoundation(L);
}

CLIENT_DETOUR(CGActionBar__GetCooldown_ActionbarFoundation, 0x005A8E40, __cdecl, void, (uint32_t slot, uint32_t* start, uint32_t* duration, uint32_t* enable))
{
    if (!SPELL_CACHE_ENABLE_UI_INTEGRATION)
    {
        CGActionBar__GetCooldown_ActionbarFoundation(slot, start, duration, enable);
        return;
    }

    CGActionBar__GetCooldown_ActionbarFoundation(slot, start, duration, enable);
    if ((duration && *duration) || (start && *start))
        return;

    uint32_t spellId = 0;
    uint32_t isPet = 0;
    if (!TryGetActionBarStreamedSpell(slot, spellId, isPet))
        return;

    uint32_t streamedStart = 0;
    uint32_t streamedDuration = 0;
    uint32_t streamedEnable = 0;
    GetStreamedSpellCooldownSeconds(spellId, isPet, streamedStart, streamedDuration, streamedEnable);
    if (start)
        *start = streamedStart;
    if (duration)
        *duration = streamedDuration;
    if (enable)
        *enable = streamedEnable;
}

CLIENT_DETOUR(CGActionBar__ActionHasRange_ActionbarFoundation, 0x005A95E0, __cdecl, int, (uint32_t slot))
{
    if (!SPELL_CACHE_ENABLE_UI_INTEGRATION)
        return CGActionBar__ActionHasRange_ActionbarFoundation(slot);

    int const nativeResult = CGActionBar__ActionHasRange_ActionbarFoundation(slot);
    if (nativeResult)
        return nativeResult;

    uint32_t spellId = 0;
    uint32_t isPet = 0;
    if (!TryGetActionBarStreamedSpell(slot, spellId, isPet))
        return nativeResult;

    ClientData::SpellRow row{};
    return SpellCacheStreaming::TryBuildSpellRow(spellId, row) && row.m_rangeIndex ? 1 : 0;
}

CLIENT_DETOUR(CGActionBar__UpdateUsableAction_ActionbarFoundation, 0x005A9E20, __cdecl, int, (uint32_t slot, bool* notEnoughMana))
{
    if (!SPELL_CACHE_ENABLE_UI_INTEGRATION)
        return CGActionBar__UpdateUsableAction_ActionbarFoundation(slot, notEnoughMana);

    uint32_t spellId = 0;
    uint32_t isPet = 0;
    if (!TryGetActionBarStreamedSpell(slot, spellId, isPet))
        return CGActionBar__UpdateUsableAction_ActionbarFoundation(slot, notEnoughMana);

    bool usable = false;
    bool lacksPower = false;
    GetStreamedSpellUsableState(spellId, usable, lacksPower);
    if (notEnoughMana)
        *notEnoughMana = lacksPower;
    return usable ? 1 : 0;
}

CLIENT_DETOUR(CGActionBar__UpdateUsablePower_ActionbarFoundation, 0x005A8D30, __cdecl, void, ())
{
    if (!SPELL_CACHE_ENABLE_UI_INTEGRATION)
    {
        CGActionBar__UpdateUsablePower_ActionbarFoundation();
        return;
    }

    CGActionBar__UpdateUsablePower_ActionbarFoundation();
}

CLIENT_DETOUR(CGActionBar__UseAction_ActionbarFoundation, 0x005ABBC0, __cdecl, void, (uint32_t slot, uint64_t* targetGuid))
{
    if (!SPELL_CACHE_ENABLE_UI_INTEGRATION)
    {
        CGActionBar__UseAction_ActionbarFoundation(slot, targetGuid);
        return;
    }

    if (IsValidActionBarSlot(slot))
    {
        uint64_t const guid = targetGuid ? *targetGuid : 0;
        if (TryActivateStreamedActionBarSlot(slot, guid))
            return;

        uint32_t pendingSpellId = 0;
        if (TryGetPendingActionBarStreamedSpell(slot, pendingSpellId))
        {
            LOG_INFO << "Blocked pending streamed actionbar cast"
                << "slot" << (slot + 1)
                << "spell" << pendingSpellId;
            return;
        }
    }

    CGActionBar__UseAction_ActionbarFoundation(slot, targetGuid);
}

bool SpellCacheStreaming::HasSpell(uint32_t spellId)
{
    SpellCacheRow* spell = GlobalCDBCMap.getRow<SpellCacheRow>("Spell", int(spellId));
    bool const result = spell
        && spell->cacheVersion == SPELL_CACHE_STREAM_VERSION
        && (spell->spellDataHash != 0 || StreamedSpellRows.find(spellId) != StreamedSpellRows.end());

    static uint32_t logCount = 0;
    if (logCount < 500 && HasReasonableSpellId(spellId))
    {
        LOG_INFO << "Spell cache has check"
            << "spell" << spellId
            << "hashMode" << "any"
            << "requestedHash" << 0
            << "result" << result
            << "row" << (spell ? 1 : 0)
            << "rowVersion" << (spell ? spell->cacheVersion : 0)
            << "rowHash" << (spell ? spell->spellDataHash : 0)
            << "streamed" << (StreamedSpellRows.find(spellId) != StreamedSpellRows.end())
            << "missing" << (MissingSpellRows.find(spellId) != MissingSpellRows.end())
            << "queued" << (QueuedSpellRequestIds.find(spellId) != QueuedSpellRequestIds.end())
            << "pending" << (PendingSpellRequests.find(spellId) != PendingSpellRequests.end());
        ++logCount;
    }

    return result;
}

bool SpellCacheStreaming::HasSpell(uint32_t spellId, uint32_t spellDataHash)
{
    SpellCacheRow* spell = GlobalCDBCMap.getRow<SpellCacheRow>("Spell", int(spellId));
    bool const result = spell && spell->cacheVersion == SPELL_CACHE_STREAM_VERSION && spell->spellDataHash == spellDataHash;

    static uint32_t logCount = 0;
    if (logCount < 500 && HasReasonableSpellId(spellId))
    {
        LOG_INFO << "Spell cache has check"
            << "spell" << spellId
            << "hashMode" << "exact"
            << "requestedHash" << spellDataHash
            << "result" << result
            << "row" << (spell ? 1 : 0)
            << "rowVersion" << (spell ? spell->cacheVersion : 0)
            << "rowHash" << (spell ? spell->spellDataHash : 0)
            << "streamed" << (StreamedSpellRows.find(spellId) != StreamedSpellRows.end())
            << "missing" << (MissingSpellRows.find(spellId) != MissingSpellRows.end())
            << "queued" << (QueuedSpellRequestIds.find(spellId) != QueuedSpellRequestIds.end())
            << "pending" << (PendingSpellRequests.find(spellId) != PendingSpellRequests.end());
        ++logCount;
    }

    return result;
}

bool SpellCacheStreaming::IsRequestPending(uint32_t spellId)
{
    return PendingSpellRequests.find(spellId) != PendingSpellRequests.end();
}

uint32_t SpellCacheStreaming::ForEachSpellEffect(
    uint32_t spellId,
    const std::function<bool(const SpellEffectCacheRow&)>& visitor)
{
    if (!spellId)
        return 0;

    uint32_t visited = 0;
    for (uint32_t effectIndex = 0; effectIndex < 32; ++effectIndex)
    {
        SpellEffectCacheRow* effect = GlobalCDBCMap.getRow<SpellEffectCacheRow>(
            "SpellEffect", SpellEffectCacheKey(spellId, effectIndex));
        if (!effect)
            continue;

        ++visited;
        if (!visitor(*effect))
            break;
    }

    return visited;
}

uint32_t SpellCacheStreaming::GetSpellEffectCount(uint32_t spellId)
{
    return ForEachSpellEffect(spellId, [](const SpellEffectCacheRow&) {
        return true;
    });
}

bool SpellCacheStreaming::TryGetSpellEffect(
    uint32_t spellId,
    uint32_t effectIndex,
    const SpellEffectCacheRow*& out)
{
    out = nullptr;
    if (!spellId || effectIndex >= 32)
        return false;

    out = GlobalCDBCMap.getRow<SpellEffectCacheRow>(
        "SpellEffect", SpellEffectCacheKey(spellId, effectIndex));
    return out != nullptr;
}

bool SpellCacheStreaming::HasSpellEffect(uint32_t spellId, uint32_t effectId)
{
    const SpellEffectCacheRow* effect = nullptr;
    return TryFindSpellEffectByEffect(spellId, effectId, effect);
}

bool SpellCacheStreaming::HasSpellAura(uint32_t spellId, uint32_t auraId)
{
    const SpellEffectCacheRow* effect = nullptr;
    return TryFindSpellEffectByAura(spellId, auraId, effect);
}

bool SpellCacheStreaming::HasAnySpellAura(
    uint32_t spellId,
    const uint32_t* auraIds,
    uint32_t auraIdCount)
{
    if (!auraIds || !auraIdCount)
        return false;

    bool found = false;
    ForEachSpellEffect(spellId, [&](const SpellEffectCacheRow& effect) {
        for (uint32_t i = 0; i < auraIdCount; ++i)
        {
            if (effect.effectApplyAuraName == auraIds[i])
            {
                found = true;
                return false;
            }
        }

        return true;
    });

    return found;
}

bool SpellCacheStreaming::TryFindSpellEffectByEffect(
    uint32_t spellId,
    uint32_t effectId,
    const SpellEffectCacheRow*& out)
{
    out = nullptr;
    ForEachSpellEffect(spellId, [&](const SpellEffectCacheRow& effect) {
        if (effect.effect == effectId)
        {
            out = &effect;
            return false;
        }

        return true;
    });

    return out != nullptr;
}

bool SpellCacheStreaming::TryFindSpellEffectByAura(
    uint32_t spellId,
    uint32_t auraId,
    const SpellEffectCacheRow*& out)
{
    out = nullptr;
    ForEachSpellEffect(spellId, [&](const SpellEffectCacheRow& effect) {
        if (effect.effectApplyAuraName == auraId)
        {
            out = &effect;
            return false;
        }

        return true;
    });

    return out != nullptr;
}

bool SpellCacheStreaming::TryBuildSpellRow(uint32_t spellId, ClientData::SpellRow& out)
{
    if (!HasSpell(spellId))
        return false;

    SpellCacheRow* cached = GlobalCDBCMap.getRow<SpellCacheRow>("Spell", int(spellId));
    if (!cached)
        return false;

    std::memset(&out, 0, sizeof(out));
    out.m_ID = cached->id;
    out.m_category = cached->category;
    out.m_dispelType = cached->dispel;
    out.m_mechanic = cached->mechanic;
    out.m_attributes = cached->attributes;
    out.m_attributesEx = cached->attributesEx;
    out.m_attributesExB = cached->attributesEx2;
    out.m_attributesExC = cached->attributesEx3;
    out.m_attributesExD = cached->attributesEx4;
    if (out.m_ID == 82893 && (out.m_attributesExD & 0x40000u) != 0)
    {
        out.m_attributesExD &= ~0x40000u;
        static bool loggedTrajectoryBitClear = false;
        if (!loggedTrajectoryBitClear)
        {
            LOG_INFO << "Cleared streamed SpellRec missile trajectory attributesExD bit for native client"
                << "spell" << out.m_ID
                << "clearedBit" << 0x40000u
                << "attributesExD" << out.m_attributesExD;
            loggedTrajectoryBitClear = true;
        }
    }
    out.m_attributesExE = cached->attributesEx5;
    out.m_attributesExF = cached->attributesEx6;
    out.m_attributesExG = cached->attributesEx7;
    out.m_shapeshiftMask[0] = cached->stances;
    out.m_shapeshiftExclude[0] = cached->stancesNot;
    out.m_targets = cached->targets;
    out.m_targetCreatureType = cached->targetCreatureType;
    out.m_requiresSpellFocus = cached->requiresSpellFocus;
    out.m_facingCasterFlags = cached->facingCasterFlags;
    out.m_casterAuraState = cached->casterAuraState;
    out.m_targetAuraState = cached->targetAuraState;
    out.m_excludeCasterAuraState = cached->excludeCasterAuraState;
    out.m_excludeTargetAuraState = cached->excludeTargetAuraState;
    out.m_casterAuraSpell = cached->casterAuraSpell;
    out.m_targetAuraSpell = cached->targetAuraSpell;
    out.m_excludeCasterAuraSpell = cached->excludeCasterAuraSpell;
    out.m_excludeTargetAuraSpell = cached->excludeTargetAuraSpell;
    out.m_castingTimeIndex = cached->castingTimeIndex;
    out.m_recoveryTime = cached->recoveryTime;
    out.m_categoryRecoveryTime = cached->categoryRecoveryTime;
    out.m_interruptFlags = cached->interruptFlags;
    out.m_auraInterruptFlags = cached->auraInterruptFlags;
    out.m_channelInterruptFlags = cached->channelInterruptFlags;
    out.m_procTypeMask = cached->procFlags;
    out.m_procChance = cached->procChance;
    out.m_procCharges = cached->procCharges;
    out.m_maxLevel = cached->maxLevel;
    out.m_baseLevel = cached->baseLevel;
    out.m_spellLevel = cached->spellLevel;
    out.m_durationIndex = cached->durationIndex;
    out.m_powerType = static_cast<uint32_t>(cached->powerType);
    out.m_manaCost = cached->manaCost;
    out.m_manaCostPerLevel = cached->manaCostPerLevel;
    out.m_manaPerSecond = cached->manaPerSecond;
    out.m_manaPerSecondPerLevel = cached->manaPerSecondPerLevel;
    out.m_rangeIndex = cached->rangeIndex;
    out.m_speed = cached->speed;
    out.m_modalNextSpell = cached->modalNextSpell;
    out.m_cumulativeAura = cached->stackAmount;
    out.m_totem0[0] = cached->totem1;
    out.m_totem0[1] = cached->totem2;
    out.m_reagent[0] = static_cast<uint32_t>(cached->reagent1);
    out.m_reagent[1] = static_cast<uint32_t>(cached->reagent2);
    out.m_reagent[2] = static_cast<uint32_t>(cached->reagent3);
    out.m_reagent[3] = static_cast<uint32_t>(cached->reagent4);
    out.m_reagent[4] = static_cast<uint32_t>(cached->reagent5);
    out.m_reagent[5] = static_cast<uint32_t>(cached->reagent6);
    out.m_reagent[6] = static_cast<uint32_t>(cached->reagent7);
    out.m_reagent[7] = static_cast<uint32_t>(cached->reagent8);
    out.m_reagentCount[0] = static_cast<uint32_t>(cached->reagentCount1);
    out.m_reagentCount[1] = static_cast<uint32_t>(cached->reagentCount2);
    out.m_reagentCount[2] = static_cast<uint32_t>(cached->reagentCount3);
    out.m_reagentCount[3] = static_cast<uint32_t>(cached->reagentCount4);
    out.m_reagentCount[4] = static_cast<uint32_t>(cached->reagentCount5);
    out.m_reagentCount[5] = static_cast<uint32_t>(cached->reagentCount6);
    out.m_reagentCount[6] = static_cast<uint32_t>(cached->reagentCount7);
    out.m_reagentCount[7] = static_cast<uint32_t>(cached->reagentCount8);
    out.m_equippedItemClass = static_cast<uint32_t>(cached->equippedItemClass);
    out.m_equippedItemSubclass = static_cast<uint32_t>(cached->equippedItemSubClassMask);
    out.m_equippedItemInvTypes = static_cast<uint32_t>(cached->equippedItemInventoryTypeMask);
    out.m_spellVisualID[0] = cached->spellVisualID1;
    out.m_spellVisualID[1] = cached->spellVisualID2;
    out.m_spellIconID = cached->spellIconID;
    out.m_activeIconID = cached->activeIconID;
    out.m_spellPriority = cached->spellPriority;
    out.m_name_lang = cached->spellName;
    out.m_nameSubtext_lang = cached->spellRank;
    out.m_description_lang = cached->description;
    out.m_auraDescription_lang = cached->auraDescription;
    out.m_manaCostPct = cached->manaCostPct;
    out.m_startRecoveryCategory = cached->startRecoveryCategory;
    out.m_startRecoveryTime = cached->startRecoveryTime;
    out.m_maxTargetLevel = cached->maxTargetLevel;
    out.m_spellClassSet = cached->spellFamilyName;
    out.m_spellClassMask[0] = cached->spellFamilyFlags1;
    out.m_spellClassMask[1] = cached->spellFamilyFlags2;
    out.m_spellClassMask[2] = cached->spellFamilyFlags3;
    out.m_maxTargets = cached->maxAffectedTargets;
    out.m_defenseType = cached->dmgClass;
    out.m_preventionType = cached->preventionType;
    out.m_stanceBarOrder = cached->stanceBarOrder;
    out.m_minFactionID = cached->minFactionID;
    out.m_minReputation = cached->minReputation;
    out.m_requiredAuraVision = cached->requiredAuraVision;
    out.m_requiredTotemCategoryID[0] = cached->requiredTotemCategoryID1;
    out.m_requiredTotemCategoryID[1] = cached->requiredTotemCategoryID2;
    out.m_requiredAreasID = static_cast<uint32_t>(cached->areaGroupId);
    out.m_schoolMask = cached->schoolMask;
    out.m_runeCostID = cached->runeCostID;
    out.m_spellMissileID = cached->spellMissileID;
    out.m_powerDisplayID = static_cast<uint32_t>(cached->powerDisplayID);
    out.m_descriptionVariablesID = cached->descriptionVariablesID;
    out.m_difficulty = cached->difficulty;

    for (uint32_t i = 0; i < 3; ++i)
    {
        SpellEffectCacheRow* effect = GlobalCDBCMap.getRow<SpellEffectCacheRow>(
            "SpellEffect", SpellEffectCacheKey(spellId, i));
        if (!effect)
            continue;

        out.m_effect[i] = effect->effect;
        out.m_effectDieSides[i] = static_cast<uint32_t>(effect->effectDieSides);
        out.m_effectRealPointsPerLevel[i] = effect->effectRealPointsPerLevel;
        out.m_effectBasePoints[i] = static_cast<uint32_t>(effect->effectBasePoints);
        out.m_effectMechanic[i] = effect->effectMechanic;
        out.m_implicitTargetA[i] = effect->effectImplicitTargetA;
        out.m_implicitTargetB[i] = effect->effectImplicitTargetB;
        out.m_effectRadiusIndex[i] = effect->effectRadiusIndex;
        out.m_effectAura[i] = effect->effectApplyAuraName;
        out.m_effectAuraPeriod[i] = effect->effectAmplitude;
        out.m_effectAmplitude[i] = effect->effectMultipleValue;
        out.m_effectChainTargets[i] = effect->effectChainTargets;
        out.m_effectItemType[i] = static_cast<uint32_t>(effect->effectItemType);
        out.m_effectMiscValue[i] = static_cast<uint32_t>(effect->effectMiscValue);
        out.m_effectMiscValueB[i] = static_cast<uint32_t>(effect->effectMiscValueB);
        out.m_effectTriggerSpell[i] = effect->effectTriggerSpell;
        out.m_effectPointsPerCombo[i] = effect->effectPointsPerCombo;
        out.m_effectSpellClassMask[i][0] = effect->effectSpellClassMaskA;
        out.m_effectSpellClassMask[i][1] = effect->effectSpellClassMaskB;
        out.m_effectSpellClassMask[i][2] = effect->effectSpellClassMaskC;
        out.m_effectChainAmplitude[i] = effect->effectChainAmplitude;
        out.m_effectBonusCoefficient[i] = effect->effectBonusMultiplier;
    }

    return true;
}

bool SpellCacheStreaming::TryGetSpellRow(uint32_t spellId, ClientData::SpellRow& out, bool requestOnMiss)
{
    if (!spellId)
        return false;

    if (TryBuildSpellRow(spellId, out))
        return true;

    std::memset(&out, 0, sizeof(out));
    if (NativeSpellDbCanLookup(spellId)
        && ClientDb_GetLocalizedRow_ActionbarFoundation(reinterpret_cast<void*>(SPELL_DB_ADDRESS), spellId, &out))
    {
        return true;
    }

    if (requestOnMiss)
        RequestSpell(spellId);

    return false;
}

bool SpellCacheStreaming::TryResolveKnownSpellbookSlot(uint32_t oneBasedSlot, uint32_t& spellId)
{
    return TryGetKnownSpellbookSpellBySlot(oneBasedSlot, spellId);
}

void SpellCacheStreaming::RequestSpell(uint32_t spellId, uint32_t spellDataHash)
{
    if (!HasReasonableSpellId(spellId))
    {
        static uint32_t logCount = 0;
        if (spellId && logCount < 20)
        {
            LOG_ERROR << "Blocked unreasonable spell cache request" << spellId;
            ++logCount;
        }

        return;
    }

    if (MissingSpellRows.find(spellId) != MissingSpellRows.end())
        return;

    if (spellDataHash && HasSpell(spellId, spellDataHash))
        return;

    if (!spellDataHash && HasSpell(spellId))
        return;

    uint32_t const nowMs = GetTickCount();
    auto pending = PendingSpellRequests.find(spellId);
    if (pending != PendingSpellRequests.end())
    {
        if (nowMs - pending->second < SPELL_CACHE_REQUEST_TIMEOUT_MS)
            return;

        PendingSpellRequests.erase(pending);
    }

    if (QueuedSpellRequestIds.insert(spellId).second)
    {
        QueuedSpellRequests.push_back(spellId);
        QueuedSpellRequestHashes[spellId] = spellDataHash;
        if (SpellCacheRequestDebugLogCount < 120)
        {
            LOG_INFO << "Queued spell cache request"
                << "spell" << spellId
                << "hash" << spellDataHash
                << "queued" << static_cast<uint32_t>(QueuedSpellRequests.size())
                << "pendingRequests" << static_cast<uint32_t>(PendingSpellRequests.size());
            ++SpellCacheRequestDebugLogCount;
        }
    }
    else if (spellDataHash)
    {
        QueuedSpellRequestHashes[spellId] = spellDataHash;
        if (SpellCacheRequestDebugLogCount < 120)
        {
            LOG_INFO << "Updated queued spell cache request hash"
                << "spell" << spellId
                << "hash" << spellDataHash
                << "queued" << static_cast<uint32_t>(QueuedSpellRequests.size());
            ++SpellCacheRequestDebugLogCount;
        }
    }
}

CLIENT_DETOUR(FrameScript_FireOnUpdate_SpellCache, 0x00495810, __cdecl, int, (int a1, int a2, int a3, int a4))
{
    PumpSpellCacheRequests();

    return FrameScript_FireOnUpdate_SpellCache(a1, a2, a3, a4);
}

LUA_FUNCTION(RequestSpellCache, (lua_State* L))
{
    SpellCacheStreaming::RequestSpell(
        uint32_t(ClientLua::GetNumber(L, 1, 0)),
        uint32_t(ClientLua::GetNumber(L, 2, 0)));
    return 0;
}

LUA_FUNCTION(HasSpellCache, (lua_State* L))
{
    uint32_t const spellId = uint32_t(ClientLua::GetNumber(L, 1, 0));
    uint32_t const spellDataHash = uint32_t(ClientLua::GetNumber(L, 2, 0));
    ClientLua::PushBoolean(L, spellDataHash
        ? SpellCacheStreaming::HasSpell(spellId, spellDataHash)
        : SpellCacheStreaming::HasSpell(spellId));
    return 1;
}

LUA_FUNCTION(SetSpellCacheForceStreamed, (lua_State* L))
{
    uint32_t const spellId = uint32_t(ClientLua::GetNumber(L, 1, 0));
    bool const enabled = ClientLua::GetTop(L) < 2 || ClientLua::GetNumber(L, 2, 1) != 0.0;

    if (!HasReasonableSpellId(spellId))
    {
        ClientLua::PushBoolean(L, false);
        return 1;
    }

    if (enabled)
    {
        ForcedStreamedSpellRows.insert(spellId);
        if (!SpellCacheStreaming::HasSpell(spellId))
            SpellCacheStreaming::RequestSpell(spellId);
    }
    else
    {
        ForcedStreamedSpellRows.erase(spellId);
    }

    LOG_INFO << "Spell cache force streamed"
        << "spell" << spellId
        << "enabled" << enabled
        << "cached" << SpellCacheStreaming::HasSpell(spellId)
        << "count" << static_cast<uint32_t>(ForcedStreamedSpellRows.size());
    ClientLua::PushBoolean(L, true);
    ClientLua::PushBoolean(L, SpellCacheStreaming::HasSpell(spellId));
    return 2;
}

LUA_FUNCTION(ClearSpellCacheForceStreamed, (lua_State* L))
{
    ForcedStreamedSpellRows.clear();
    LOG_INFO << "Spell cache force streamed cleared";
    return 0;
}

LUA_FUNCTION(IsSpellCacheForceStreamed, (lua_State* L))
{
    uint32_t const spellId = uint32_t(ClientLua::GetNumber(L, 1, 0));
    ClientLua::PushBoolean(L, ForcedStreamedSpellRows.find(spellId) != ForcedStreamedSpellRows.end());
    return 1;
}

LUA_FUNCTION(SetStreamedSpellActionBarSlot, (lua_State* L))
{
    if (!SPELL_CACHE_ENABLE_UI_INTEGRATION)
    {
        ClientLua::PushBoolean(L, false);
        return 1;
    }

    uint32_t const spellId = uint32_t(ClientLua::GetNumber(L, 1, 0));
    uint32_t const oneBasedSlot = uint32_t(ClientLua::GetNumber(L, 2, 0));
    if (!oneBasedSlot || oneBasedSlot > 144 || !HasReasonableSpellId(spellId))
    {
        ClientLua::PushBoolean(L, false);
        return 1;
    }

    bool const ok = SetStreamedActionBarEntry(oneBasedSlot - 1, spellId, true);
    ClientLua::PushBoolean(L, ok);
    ClientLua::PushBoolean(L, ok && SpellCacheStreaming::HasSpell(spellId));
    return 2;
}

LUA_FUNCTION(GetStreamedSpellActionBarSlot, (lua_State* L))
{
    if (!SPELL_CACHE_ENABLE_UI_INTEGRATION)
    {
        ClientLua::PushNil(L);
        return 1;
    }

    uint32_t const oneBasedSlot = uint32_t(ClientLua::GetNumber(L, 1, 0));
    uint32_t spellId = 0;
    if (!oneBasedSlot || oneBasedSlot > 144)
    {
        ClientLua::PushNil(L);
        return 1;
    }

    bool const sidecar = TryGetStreamedActionBarEntryRaw(oneBasedSlot - 1, spellId);
    if (!sidecar)
    {
        uint32_t isPet = 0;
        if (!TryGetActionBarStreamedSpell(oneBasedSlot - 1, spellId, isPet))
        {
            ClientLua::PushNil(L);
            return 1;
        }
    }

    ClientLua::PushNumber(L, static_cast<double>(spellId));
    ClientLua::PushBoolean(L, SpellCacheStreaming::HasSpell(spellId));
    ClientLua::PushBoolean(L, sidecar);
    return 3;
}

LUA_FUNCTION(ClearStreamedSpellActionBarSlot, (lua_State* L))
{
    if (!SPELL_CACHE_ENABLE_UI_INTEGRATION)
    {
        ClientLua::PushBoolean(L, false);
        return 1;
    }

    uint32_t const oneBasedSlot = uint32_t(ClientLua::GetNumber(L, 1, 0));
    if (!oneBasedSlot || oneBasedSlot > 144)
    {
        ClientLua::PushBoolean(L, false);
        return 1;
    }

    ClientLua::PushBoolean(L, ClearStreamedActionBarEntry(oneBasedSlot - 1));
    return 1;
}

LUA_FUNCTION(ClearStreamedSpellActionBarSlots, (lua_State*))
{
    if (!SPELL_CACHE_ENABLE_UI_INTEGRATION)
        return 0;

    ClearAllStreamedActionBarEntries();
    return 0;
}

LUA_FUNCTION(CastStreamedSpellActionBarSlot, (lua_State* L))
{
    if (!SPELL_CACHE_ENABLE_UI_INTEGRATION)
    {
        ClientLua::PushBoolean(L, false);
        return 1;
    }

    uint32_t const oneBasedSlot = uint32_t(ClientLua::GetNumber(L, 1, 0));
    uint32_t spellId = 0;
    if (!oneBasedSlot || oneBasedSlot > 144 || !TryGetStreamedActionBarEntryRaw(oneBasedSlot - 1, spellId))
    {
        ClientLua::PushBoolean(L, false);
        return 1;
    }

    ClientData::SpellRow row{};
    if (!SpellCacheStreaming::TryBuildSpellRow(spellId, row))
    {
        SpellCacheStreaming::RequestSpell(spellId);
        ClientLua::PushBoolean(L, false);
        ClientLua::PushBoolean(L, false);
        return 2;
    }

    std::string const target = ClientLua::IsString(L, 2)
        ? ClientLua::GetString(L, 2, "")
        : "";

    ClientLua::SetTop(L, 0);
    ClientLua::PushNumber(L, static_cast<double>(spellId));
    if (!target.empty())
        ClientLua::PushString(L, target.c_str());

    return Script_CastSpellByID_StreamedActionBar(L);
}

LUA_FUNCTION(GetStreamedSpellBookSpellId, (lua_State* L))
{
    if (!SPELL_CACHE_ENABLE_UI_INTEGRATION)
    {
        ClientLua::PushNil(L);
        return 1;
    }

    uint32_t const oneBasedSlot = static_cast<uint32_t>(ClientLua::GetNumber(L, 1, 0));
    uint32_t spellId = 0;
    if (!TryGetKnownSpellbookSpellBySlot(oneBasedSlot, spellId))
    {
        ClientLua::PushNil(L);
        return 1;
    }

    ClientLua::PushNumber(L, static_cast<double>(spellId));
    return 1;
}

LUA_FUNCTION(GetStreamedSpellBookNumTabs, (lua_State* L))
{
    if (!SPELL_CACHE_ENABLE_UI_INTEGRATION)
    {
        ClientLua::PushNumber(L, 0.0);
        return 1;
    }

    if (!ReadyKnownSpellUiQueue.empty())
    {
        ClientLua::PushNumber(L, 0.0);
        return 1;
    }

    RebuildKnownSpellbookOrder();
    ClientLua::PushNumber(L, static_cast<double>(KnownSpellbookTabs.size()));
    return 1;
}

LUA_FUNCTION(GetStreamedSpellBookRevision, (lua_State* L))
{
    if (!SPELL_CACHE_ENABLE_UI_INTEGRATION)
    {
        ClientLua::PushNumber(L, 0.0);
        return 1;
    }

    if (ReadyKnownSpellUiQueue.empty())
    {
        RebuildKnownSpellbookOrder();
        PublishedKnownSpellbookRevision = KnownSpellbookRevision;
    }

    ClientLua::PushNumber(L, static_cast<double>(PublishedKnownSpellbookRevision));
    return 1;
}

LUA_FUNCTION(GetStreamedSpellBookTabInfo, (lua_State* L))
{
    if (!SPELL_CACHE_ENABLE_UI_INTEGRATION)
        return 0;

    if (!ReadyKnownSpellUiQueue.empty())
        return 0;

    RebuildKnownSpellbookOrder();
    uint32_t const oneBasedTab = static_cast<uint32_t>(ClientLua::GetNumber(L, 1, 0));
    if (!oneBasedTab || oneBasedTab > KnownSpellbookTabs.size())
        return 0;

    KnownSpellbookTab const& tab = KnownSpellbookTabs[oneBasedTab - 1];
    ClientLua::PushString(L, tab.name.c_str());
    ClientLua::PushString(L, tab.icon.c_str());
    ClientLua::PushNumber(L, static_cast<double>(tab.offset));
    ClientLua::PushNumber(L, static_cast<double>(tab.count));
    ClientLua::PushNumber(L, static_cast<double>(tab.offset));
    ClientLua::PushNumber(L, static_cast<double>(tab.count));
    return 6;
}

LUA_FUNCTION(PickupStreamedSpellBookSpell, (lua_State* L))
{
    if (!SPELL_CACHE_ENABLE_UI_INTEGRATION)
    {
        ClientLua::PushBoolean(L, false);
        return 1;
    }

    uint32_t const oneBasedSlot = static_cast<uint32_t>(ClientLua::GetNumber(L, 1, 0));
    uint32_t spellId = 0;
    if (!TryGetKnownSpellbookSpellBySlot(oneBasedSlot, spellId))
    {
        ClientLua::PushBoolean(L, false);
        return 1;
    }

    ClientData::SpellRow row{};
    if (!SpellCacheStreaming::TryBuildSpellRow(spellId, row))
    {
        SpellCacheStreaming::RequestSpell(spellId);
        ClientLua::PushBoolean(L, false);
        return 1;
    }

    CGSpellBook__SetCursorSpell_StreamedActionBar(spellId);
    StreamedActionBarCursorActive = true;
    StreamedActionBarCursorSpellId = spellId;
    StreamedActionBarCursorSourceSlot = 0xFFFFFFFF;

    LOG_INFO << "Picked up streamed spellbook spell"
        << "slot" << oneBasedSlot
        << "spell" << spellId;

    ClientLua::PushBoolean(L, true);
    return 1;
}

LUA_FUNCTION(PlaceStreamedSpellActionBarCursor, (lua_State* L))
{
    if (!SPELL_CACHE_ENABLE_UI_INTEGRATION)
    {
        ClientLua::PushBoolean(L, false);
        return 1;
    }

    uint32_t const oneBasedSlot = static_cast<uint32_t>(ClientLua::GetNumber(L, 1, 0));
    if (!oneBasedSlot || !IsValidActionBarSlot(oneBasedSlot - 1))
    {
        ClientLua::PushBoolean(L, false);
        return 1;
    }

    ClientLua::PushBoolean(L, TryPlaceStreamedActionBarCursor(oneBasedSlot - 1));
    return 1;
}

LUA_FUNCTION(DumpStreamedSpellBook, (lua_State* L))
{
    if (!SPELL_CACHE_ENABLE_UI_INTEGRATION)
    {
        ClientLua::PushNumber(L, 0.0);
        ClientLua::PushNumber(L, 0.0);
        ClientLua::PushNumber(L, 0.0);
        return 3;
    }

    LOG_INFO << "Streamed spellbook dump requested";
    RebuildKnownSpellbookOrder();

    uint32_t const limit = static_cast<uint32_t>(ClientLua::GetNumber(L, 1, 120));
    uint32_t dumped = 0;
    uint32_t missingName = 0;
    uint32_t passive = 0;

    LOG_INFO << "Streamed spellbook dump begin"
        << "visible" << static_cast<uint32_t>(KnownSpellbookOrder.size())
        << "tabs" << static_cast<uint32_t>(KnownSpellbookTabs.size())
        << "model" << static_cast<uint32_t>(KnownSpellbookEntries.size())
        << "limit" << limit;

    for (uint32_t i = 0; i < KnownSpellbookOrder.size() && dumped < limit; ++i)
    {
        uint32_t const spellId = KnownSpellbookOrder[i];
        ClientData::SpellRow row{};
        bool const cached = SpellCacheStreaming::TryBuildSpellRow(spellId, row);
        char const* const name = cached && row.m_name_lang ? row.m_name_lang : "";
        char const* const rank = cached && row.m_nameSubtext_lang ? row.m_nameSubtext_lang : "";
        bool const isPassive = cached && (row.m_attributes & STREAMED_SPELL_ATTR0_PASSIVE) != 0;

        if (!name || !*name)
            ++missingName;
        if (isPassive)
            ++passive;

        auto entry = KnownSpellbookEntries.find(spellId);
        uint32_t tabKey = entry != KnownSpellbookEntries.end() ? entry->second.tabKey : 0;
        LOG_INFO << "Streamed spellbook slot"
            << "slot" << (i + 1)
            << "spell" << spellId
            << "cached" << cached
            << "passive" << isPassive
            << "tab" << tabKey
            << "name" << name
            << "rank" << rank;
        ++dumped;
    }

    LOG_INFO << "Streamed spellbook dump end"
        << "dumped" << dumped
        << "missingName" << missingName
        << "passive" << passive;

    ClientLua::PushNumber(L, static_cast<double>(KnownSpellbookOrder.size()));
    ClientLua::PushNumber(L, static_cast<double>(missingName));
    ClientLua::PushNumber(L, static_cast<double>(passive));
    return 3;
}

int LiveStreamedShapeshift_GetNumForms(lua_State* L)
{
    RebuildKnownSpellbookOrder();
    ClientLua::PushNumber(L, static_cast<double>(GetCachedStreamedStances().size()));
    return 1;
}

int LiveStreamedShapeshift_GetSpellId(lua_State* L)
{
    KnownSpellbookEntry stance{};
    if (!ClientLua::IsNumber(L, 1) || !TryGetStreamedStanceByIndex(static_cast<uint32_t>(ClientLua::GetNumber(L, 1, 0)), stance))
    {
        ClientLua::PushNil(L);
        return 1;
    }

    ClientLua::PushNumber(L, static_cast<double>(stance.spellId));
    return 1;
}

int LiveStreamedShapeshift_GetForm(lua_State* L)
{
    RebuildKnownSpellbookOrder();
    void* localPlayer = ClntObjMgrObjectPtr_ActionState_SpellCache(
        ClntObjMgrGetActivePlayer_ActionState_SpellCache(),
        STREAMED_TYPEMASK_PLAYER);

    auto const& stances = GetCachedStreamedStances();
    for (uint32_t i = 0; i < stances.size(); ++i)
    {
        ClientData::SpellRow row{};
        if (SpellCacheStreaming::TryGetSpellRow(stances[i].spellId, row)
            && StreamedSpellCurrentOnUnit(&row, localPlayer))
        {
            ClientLua::PushNumber(L, static_cast<double>(i + 1));
            return 1;
        }
    }

    ClientLua::PushNumber(L, 0.0);
    return 1;
}

int LiveStreamedShapeshift_GetFormInfo(lua_State* L)
{
    KnownSpellbookEntry stance{};
    if (!ClientLua::IsNumber(L, 1) || !TryGetStreamedStanceByIndex(static_cast<uint32_t>(ClientLua::GetNumber(L, 1, 0)), stance))
        return 0;

    ClientData::SpellRow row{};
    if (!SpellCacheStreaming::TryGetSpellRow(stance.spellId, row))
        return 0;

    void* localPlayer = ClntObjMgrObjectPtr_ActionState_SpellCache(
        ClntObjMgrGetActivePlayer_ActionState_SpellCache(),
        STREAMED_TYPEMASK_PLAYER);
    bool const active = StreamedSpellCurrentOnUnit(&row, localPlayer);
    uint32_t const oneBasedIndex = static_cast<uint32_t>(ClientLua::GetNumber(L, 1, 0));
    bool const castable = GetNativeStreamedShapeshiftCastable(oneBasedIndex, active);

    char const* texture = nullptr;
    if (active && row.m_activeIconID)
        texture = GetSpellIconTextureById(row.m_activeIconID);
    if (!texture)
        texture = GetSpellIconTextureById(row.m_spellIconID);

    ClientLua::PushString(L, texture ? texture : "");
    ClientLua::PushString(L, (row.m_name_lang && *row.m_name_lang) ? row.m_name_lang : "");
    ClientLua::PushBoolean(L, active ? 1 : 0);
    ClientLua::PushBoolean(L, castable ? 1 : 0);
    return 4;
}

int LiveStreamedShapeshift_GetFormCooldown(lua_State* L)
{
    KnownSpellbookEntry stance{};
    if (!ClientLua::IsNumber(L, 1) || !TryGetStreamedStanceByIndex(static_cast<uint32_t>(ClientLua::GetNumber(L, 1, 0)), stance))
    {
        ClientLua::PushNumber(L, 0.0);
        ClientLua::PushNumber(L, 0.0);
        ClientLua::PushNumber(L, 1.0);
        return 3;
    }

    uint32_t start = 0;
    uint32_t duration = 0;
    uint32_t enable = 1;
    Spell_C__GetSpellCooldown_SpellCache(stance.spellId, 0, &start, &duration, &enable);
    ClientLua::PushNumber(L, static_cast<double>(start) * 0.001);
    ClientLua::PushNumber(L, static_cast<double>(duration) * 0.001);
    ClientLua::PushNumber(L, static_cast<double>(enable));
    return 3;
}

int LiveStreamedShapeshift_CastForm(lua_State* L)
{
    KnownSpellbookEntry stance{};
    uint32_t const oneBasedIndex = static_cast<uint32_t>(ClientLua::GetNumber(L, 1, 0));
    if (!TryGetStreamedStanceByIndex(oneBasedIndex, stance))
    {
        LOG_ERROR << "Blocked streamed shapeshift cast; invalid form index" << oneBasedIndex;
        ClientLua::PushBoolean(L, false);
        return 1;
    }

    ClientData::SpellRow row{};
    if (!SpellCacheStreaming::TryBuildSpellRow(stance.spellId, row))
    {
        SpellCacheStreaming::RequestSpell(stance.spellId);
        LOG_ERROR << "Blocked streamed shapeshift cast; spell row missing"
            << "index" << oneBasedIndex
            << "spell" << stance.spellId;
        ClientLua::PushBoolean(L, false);
        return 1;
    }

    static uint32_t logCount = 0;
    if (logCount < 80)
    {
        LOG_INFO << "Handling streamed shapeshift click"
            << "index" << oneBasedIndex
            << "spell" << stance.spellId
            << "name" << (row.m_name_lang ? row.m_name_lang : "");
        ++logCount;
    }

    void* localPlayer = ClntObjMgrObjectPtr_ActionState_SpellCache(
        ClntObjMgrGetActivePlayer_ActionState_SpellCache(),
        STREAMED_TYPEMASK_PLAYER);
    uint32_t activeSpellId = 0;
    auto const& stances = GetCachedStreamedStances();
    for (KnownSpellbookEntry const& candidate : stances)
    {
        ClientData::SpellRow candidateRow{};
        if (SpellCacheStreaming::TryGetSpellRow(candidate.spellId, candidateRow)
            && StreamedSpellCurrentOnUnit(&candidateRow, localPlayer))
        {
            activeSpellId = candidate.spellId;
            break;
        }
    }

    if (activeSpellId)
    {
        if (activeSpellId == stance.spellId)
        {
            LOG_INFO << "Sending streamed shapeshift cancel aura"
                << "activeSpell" << activeSpellId
                << "clickedSpell" << stance.spellId;
            SendStreamedCancelAuraPacket(activeSpellId);
            ClientLua::PushBoolean(L, true);
            return 1;
        }
    }

    bool usable = false;
    bool lacksPower = false;
    GetStreamedSpellUsableState(row, localPlayer, usable, lacksPower);
    if (!usable || lacksPower)
    {
        LOG_INFO << "Blocked streamed shapeshift cast; native usability failed"
            << "spell" << stance.spellId
            << "activeSpell" << activeSpellId
            << "usable" << usable
            << "lacksPower" << lacksPower;
        ClientLua::PushBoolean(L, false);
        return 1;
    }

    LOG_INFO << "Sending streamed shapeshift cast packet"
        << "spell" << stance.spellId
        << "activeSpell" << activeSpellId;
    SendStreamedCastSpellPacket(stance.spellId);
    ClientLua::PushBoolean(L, true);
    return 1;
}

int LiveStreamedShapeshift_SyncNativeState(lua_State* L)
{
    ApplyStreamedShapeshiftState(false);
    ClientLua::PushNumber(L, static_cast<double>(ShapeshiftCount()));
    return 1;
}

void SpellCacheStreaming::Apply()
{
    if (SPELL_CACHE_ENABLE_UI_INTEGRATION)
    {
        PatchKnownSpellMaxIdGate();
        ClientLua::AddFunction("StreamedShapeshift_GetNumForms", LiveStreamedShapeshift_GetNumForms, __FILE__, __LINE__);
        ClientLua::AddFunction("StreamedShapeshift_GetForm", LiveStreamedShapeshift_GetForm, __FILE__, __LINE__);
        ClientLua::AddFunction("StreamedShapeshift_GetSpellId", LiveStreamedShapeshift_GetSpellId, __FILE__, __LINE__);
        ClientLua::AddFunction("StreamedShapeshift_GetFormInfo", LiveStreamedShapeshift_GetFormInfo, __FILE__, __LINE__);
        ClientLua::AddFunction("StreamedShapeshift_GetFormCooldown", LiveStreamedShapeshift_GetFormCooldown, __FILE__, __LINE__);
        ClientLua::AddFunction("StreamedShapeshift_CastForm", LiveStreamedShapeshift_CastForm, __FILE__, __LINE__);
        ClientLua::AddFunction("StreamedShapeshift_SyncNativeState", LiveStreamedShapeshift_SyncNativeState, __FILE__, __LINE__);
        ClientLua::RegisterLua(R"lua(
local Native_CastShapeshiftForm = CastShapeshiftForm
local Native_ShapeshiftBar_Update
local Native_ShapeshiftBar_UpdateState
local DHStreamedShapeshiftRefreshFrame
local DHStreamedShapeshiftUpdateGuardInstalled
local DHStreamedShapeshiftUpdateStateGuardInstalled

GetNumShapeshiftForms = function()
    return StreamedShapeshift_GetNumForms()
end

GetShapeshiftForm = function()
    return StreamedShapeshift_GetForm()
end

GetShapeshiftFormInfo = function(index)
    return StreamedShapeshift_GetFormInfo(index)
end

GetShapeshiftFormCooldown = function(index)
    return StreamedShapeshift_GetFormCooldown(index)
end

CastShapeshiftForm = function(index)
    local spellId = StreamedShapeshift_GetSpellId(index)
    if spellId then
        local ok = StreamedShapeshift_CastForm(index)
        if ok then
            if DHStreamedShapeshiftRefreshState then
                DHStreamedShapeshiftRefreshState()
            end
        end
        return
    end

    if Native_CastShapeshiftForm then
        Native_CastShapeshiftForm(index)
    end
end

local function DHStreamedShapeshiftInCombat()
    return UnitAffectingCombat and UnitAffectingCombat("player")
end

local function DHStreamedShapeshiftEnsureFrame()
    if not DHStreamedShapeshiftRefreshFrame and CreateFrame then
        DHStreamedShapeshiftRefreshFrame = CreateFrame("Frame")
        DHStreamedShapeshiftRefreshFrame:SetScript("OnEvent", function(self)
            self:UnregisterEvent("PLAYER_REGEN_ENABLED")
            if DHStreamedShapeshiftRefresh then
                DHStreamedShapeshiftRefresh()
            end
        end)
    end
    return DHStreamedShapeshiftRefreshFrame
end

function DHStreamedShapeshiftRefresh()
    if DHStreamedShapeshiftInCombat() then
        local f = DHStreamedShapeshiftEnsureFrame()
        if f then
            f:RegisterEvent("PLAYER_REGEN_ENABLED")
        end
        return
    end

    if Native_ShapeshiftBar_Update then
        Native_ShapeshiftBar_Update()
    elseif ShapeshiftBar_Update then
        ShapeshiftBar_Update()
    end
end

local function DHStreamedShapeshiftPaintStateOnly()
    if StreamedShapeshift_SyncNativeState then
        StreamedShapeshift_SyncNativeState()
    end
    if not _G then
        return
    end

    local numForms = GetNumShapeshiftForms and GetNumShapeshiftForms() or 0
    local maxSlots = NUM_SHAPESHIFT_SLOTS or 10
    for i = 1, maxSlots do
        local button = _G["ShapeshiftButton" .. i]
        local icon = _G["ShapeshiftButton" .. i .. "Icon"]
        if button and icon and i <= numForms then
            local texture, name, isActive, isCastable = GetShapeshiftFormInfo(i)
            icon:SetTexture(texture)

            local cooldown = _G["ShapeshiftButton" .. i .. "Cooldown"]
            if cooldown then
                local start, duration, enable = GetShapeshiftFormCooldown(i)
                CooldownFrame_SetTimer(cooldown, start, duration, enable)
            end

            if isActive then
                ShapeshiftBarFrame.lastSelected = button:GetID()
                button:SetChecked(1)
            else
                button:SetChecked(0)
            end

            if isCastable then
                icon:SetVertexColor(1.0, 1.0, 1.0)
            else
                icon:SetVertexColor(0.4, 0.4, 0.4)
            end
        end
    end
end

function DHStreamedShapeshiftRefreshState()
    if DHStreamedShapeshiftInCombat() then
        DHStreamedShapeshiftPaintStateOnly()
        return
    end

    if StreamedShapeshift_SyncNativeState then
        StreamedShapeshift_SyncNativeState()
    end

    if Native_ShapeshiftBar_UpdateState then
        Native_ShapeshiftBar_UpdateState()
    elseif ShapeshiftBar_UpdateState then
        ShapeshiftBar_UpdateState()
    end
end

local function InstallStreamedShapeshiftBarGuards()
    if ShapeshiftBar_Update and not DHStreamedShapeshiftUpdateGuardInstalled then
        Native_ShapeshiftBar_Update = ShapeshiftBar_Update
        ShapeshiftBar_Update = function(...)
            if DHStreamedShapeshiftInCombat() then
                local f = DHStreamedShapeshiftEnsureFrame()
                if f then
                    f:RegisterEvent("PLAYER_REGEN_ENABLED")
                end
                return
            end
            return Native_ShapeshiftBar_Update(...)
        end
        DHStreamedShapeshiftUpdateGuardInstalled = true
    end

    if ShapeshiftBar_UpdateState and not DHStreamedShapeshiftUpdateStateGuardInstalled then
        Native_ShapeshiftBar_UpdateState = ShapeshiftBar_UpdateState
        ShapeshiftBar_UpdateState = function(...)
            if DHStreamedShapeshiftInCombat() then
                DHStreamedShapeshiftPaintStateOnly()
                return
            end
            if StreamedShapeshift_SyncNativeState then
                StreamedShapeshift_SyncNativeState()
            end
            return Native_ShapeshiftBar_UpdateState(...)
        end
        DHStreamedShapeshiftUpdateStateGuardInstalled = true
    end
end

local function InstallStreamedShapeshiftTooltip()
    if not GameTooltip or GameTooltip.StreamedShapeshiftTooltipInstalled then
        return
    end

    local Native_GameTooltip_SetShapeshift = GameTooltip.SetShapeshift
    GameTooltip.SetShapeshift = function(self, index)
        local spellId = StreamedShapeshift_GetSpellId(index)
        if spellId then
            self:SetHyperlink("spell:" .. spellId)
            return 1
        end

        if Native_GameTooltip_SetShapeshift then
            return Native_GameTooltip_SetShapeshift(self, index)
        end
    end
    GameTooltip.StreamedShapeshiftTooltipInstalled = true
end

InstallStreamedShapeshiftBarGuards()
InstallStreamedShapeshiftTooltip()

local StreamedShapeshiftInstaller = CreateFrame and CreateFrame("Frame")
if StreamedShapeshiftInstaller then
    StreamedShapeshiftInstaller:RegisterEvent("PLAYER_LOGIN")
    StreamedShapeshiftInstaller:RegisterEvent("UNIT_AURA")
    StreamedShapeshiftInstaller:RegisterEvent("UPDATE_SHAPESHIFT_FORM")
    StreamedShapeshiftInstaller:RegisterEvent("UPDATE_SHAPESHIFT_USABLE")
    StreamedShapeshiftInstaller:RegisterEvent("UPDATE_SHAPESHIFT_COOLDOWN")
    StreamedShapeshiftInstaller:SetScript("OnEvent", function(self, event, unit)
        InstallStreamedShapeshiftBarGuards()
        InstallStreamedShapeshiftTooltip()
        if event == "UNIT_AURA" then
            if unit and unit ~= "player" then
                return
            end
        end
        if DHStreamedShapeshiftRefreshState then
            DHStreamedShapeshiftRefreshState()
        end
    end)
    StreamedShapeshiftInstaller:SetScript("OnUpdate", function(self)
        if not (GameTooltip and GameTooltip.StreamedShapeshiftTooltipInstalled and Native_ShapeshiftBar_Update and Native_ShapeshiftBar_UpdateState) then
            InstallStreamedShapeshiftBarGuards()
            InstallStreamedShapeshiftTooltip()
        end
        if GameTooltip and GameTooltip.StreamedShapeshiftTooltipInstalled and Native_ShapeshiftBar_Update and Native_ShapeshiftBar_UpdateState then
            self:SetScript("OnUpdate", nil)
        end
    end)
end
)lua", "SpellCacheStreamingShapeshift.lua", __LINE__);
    }

    ClientNetwork::OnCustomPacket(SPELL_CACHE_RESPONSE_OPCODE, [](CustomPacketRead* packet)
    {
        HandleSpellCacheResponse(packet);
    });
}
