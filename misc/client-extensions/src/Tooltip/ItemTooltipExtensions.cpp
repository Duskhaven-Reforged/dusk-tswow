#include <Tooltip/ItemTooltipExtensions.h>

#include <CDBCMgr/CDBCDefs/ItemDifficultyText.h>
#include <Character/CharacterDefines.h>
#include <ClientDetours.h>
#include <Logger.h>
#include <Tooltip/SpellTooltipExtensions.h>
#include <Util.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <windows.h>
#include <any>

using namespace ClientData;

namespace {
    constexpr uint32_t kColorWhite = 0xAD2D30;
    constexpr uint32_t kColorGrey0 = 0xAD2D38;
    constexpr uint32_t kColorRed0 = 0xAD2D34;
    constexpr uint32_t kColorRed1 = 0xAD2D48;
    constexpr uint32_t kColorGreen0 = 0xAD2D40;
    constexpr uint32_t kColorDarkYellow = 0xAD2D2C;
    constexpr uint32_t kColorYellow = 0xAD2D50;
    constexpr uint32_t kColorLightBlue1 = 0xAD2DA4;
    constexpr uint32_t kColorLightBlue0 = 0xAD2DA8;
    constexpr uint32_t kItemQualityColors = 0xAD2D84;
    constexpr uintptr_t kAreaTableDB = 0x00AD3134;
    constexpr uintptr_t kChrClassesDB = 0x00AD3404;
    constexpr uintptr_t kChrRacesDB = 0x00AD3428;
    constexpr uintptr_t kGemPropertiesDB = 0x00AD39A4;
    constexpr uintptr_t kFactionDB = 0x00AD3860;
    constexpr uintptr_t kGlyphPropertiesDB = 0x00AD39C8;
    constexpr uintptr_t kItemClassDB = 0x00AD3D94;
    constexpr uintptr_t kItemLimitCategoryDB = 0x00AD3E48;
    constexpr uintptr_t kItemRandomPropertiesDB = 0x00AD3EB4;
    constexpr uintptr_t kItemRandomSuffixDB = 0x00AD3ED8;
    constexpr uintptr_t kItemSetDB = 0x00AD3EFC;
    constexpr uintptr_t kItemSubClassDB = 0x00AD3F44;
    constexpr uintptr_t kLockDB = 0x00AD40F4;
    constexpr uintptr_t kLockTypeDB = 0x00AD4118;
    constexpr uintptr_t kMapDB = 0x00AD4160;
    constexpr uintptr_t kSkillLineDB = 0x00AD45E0;
    constexpr uintptr_t kSpellDB = 0x00AD49D0;
    constexpr uintptr_t kSpellItemEnchantmentDB = 0x00AD48B0;
    constexpr uintptr_t kWdbCacheName = 0x00C5D938;
    constexpr uintptr_t kPendingTradeBindGuid = 0x00BFA3E8;
    constexpr uintptr_t kWdbCacheItem = 0x00C5D828;
    constexpr uintptr_t kWdbCacheItemName = 0x00C5D7A0;
    constexpr uintptr_t kWdbCachePetition = 0x00C5DBE0;
    constexpr uintptr_t kItemClassKeys = 0x00AC7FD8;

    struct ItemCacheTooltipView {
        uint32_t namePtr;
        uint32_t itemClass;
        uint32_t itemSubClass;
        uint32_t unk0C;
        uint32_t displayId;
        uint32_t quality;
        uint32_t flagsAndFaction0;
        uint32_t flagsAndFaction1;
        uint32_t buyPrice;
        uint32_t sellPrice;
        uint32_t inventoryType;
        uint32_t allowableClassMask;
        uint32_t allowableRaceMask;
        uint32_t itemLevel;
        uint32_t requiredLevel;
        uint32_t requiredSkillId;
        uint32_t requiredSkillRank;
        uint32_t requiredSpellId;
        uint32_t requiredPvPRank;
        uint32_t unk4C;
        uint32_t unk50;
        uint32_t unk54;
        int32_t uniqueCount;
        uint32_t unk5C;
        uint32_t containerSlots;
        uint8_t unk64[0x68 - 0x64];
        int32_t statType[10];
        int32_t statValue[10];
        uint32_t unkB8;
        uint32_t armorFlags;
        float damageMin[2];
        float damageMax[2];
        uint32_t damageSchool[2];
        int32_t armor;
        int32_t allResistValue;
        int32_t resistances[5];
        uint32_t delayMs;
        uint8_t unkF8[0x100 - 0xF8];
        uint32_t spellId[5];
        uint32_t spellTrigger[5];
        int32_t spellCharges[5];
        int32_t spellCooldown[5];
        uint32_t spellCategory[5];
        int32_t spellCategoryCooldown[5];
        uint32_t bindType;
        uint32_t descriptionPtr;
        uint8_t unk180[0x18C - 0x180];
        uint32_t startsQuestId;
        uint32_t lockId;
        uint32_t unk194;
        uint32_t unk198;
        uint32_t randomPropertyId;
        uint32_t randomSuffixId;
        int32_t shieldBlock;
        uint32_t itemSetId;
        uint32_t unk1AC;
        uint32_t areaId;
        uint32_t mapId;
        uint8_t unk1B8[0x1C0 - 0x1B8];
        uint32_t socketColor[3];
        uint8_t unk1CC[0x1D8 - 0x1CC];
        uint32_t socketBonusEnchantId;
        uint32_t gemPropertiesId;
        uint32_t requiredDisenchantSkill;
        float bonusArmorQuality;
        int32_t duration;
        uint32_t itemLimitCategory;
        uint32_t holidayId;
    };

    struct ItemLimitCategoryRow {
        uint32_t id;
        char* name;
        uint32_t quantity;
        uint32_t flags;
    };

    struct ItemSetRow {
        uint32_t id;
        char* name;
        uint32_t itemId[17];
        uint32_t spellId[8];
        uint32_t spellThreshold[8];
        uint32_t requiredSkillId;
        uint32_t requiredSkillRank;
    };

    struct RawItemSetRow {
        uint32_t id;
        uint32_t nameLangAndFlags[17];
        uint32_t itemId[17];
        uint32_t spellId[8];
        uint32_t spellThreshold[8];
        uint32_t requiredSkillId;
        uint32_t requiredSkillRank;
    };

    struct ParsedItemSetRow {
        uint32_t id = 0;
        std::string name;
        uint32_t itemId[17] = {};
        uint32_t spellId[8] = {};
        uint32_t spellThreshold[8] = {};
        uint32_t requiredSkillId = 0;
        uint32_t requiredSkillRank = 0;
    };

    struct PetitionCacheTooltipView {
        uint8_t unk00[0x08];
        uint32_t creatorGuidLow;
        uint32_t creatorGuidHigh;
        char title[0x13BC - 0x10];
        uint32_t petitionType;
    };

    std::string GetDirectoryFromPath(const std::string& path) {
        const size_t slash = path.find_last_of("\\/");
        if (slash == std::string::npos) {
            return {};
        }

        return path.substr(0, slash);
    }

    std::string JoinPath(const std::string& left, const char* right) {
        if (left.empty()) {
            return right ? std::string(right) : std::string();
        }

        if (!right || !right[0]) {
            return left;
        }

        std::string result = left;
        if (result.back() != '\\' && result.back() != '/') {
            result += '\\';
        }

        result += right;
        return result;
    }

    std::vector<std::string> GetItemSetDbCandidatePaths() {
        std::vector<std::string> candidates;

        char exePath[MAX_PATH] = {};
        if (GetModuleFileNameA(nullptr, exePath, MAX_PATH) != 0) {
            const std::string exeDir = GetDirectoryFromPath(exePath);
            if (!exeDir.empty()) {
                candidates.push_back(JoinPath(exeDir, "Data\\patch-A.MPQ\\DBFilesClient\\ItemSet.dbc"));
                const std::string repoDir = GetDirectoryFromPath(exeDir);
                if (!repoDir.empty()) {
                    candidates.push_back(JoinPath(repoDir, "client\\Data\\patch-A.MPQ\\DBFilesClient\\ItemSet.dbc"));
                }
            }
        }

        char modulePath[MAX_PATH] = {};
        HMODULE currentModule = nullptr;
        if (GetModuleHandleExA(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCSTR>(&GetItemSetDbCandidatePaths),
                &currentModule) &&
            GetModuleFileNameA(currentModule, modulePath, MAX_PATH) != 0) {
            const std::string moduleDir = GetDirectoryFromPath(modulePath);
            if (!moduleDir.empty()) {
                const std::string repoDir = GetDirectoryFromPath(GetDirectoryFromPath(GetDirectoryFromPath(moduleDir)));
                if (!repoDir.empty()) {
                    candidates.push_back(JoinPath(repoDir, "client\\Data\\patch-A.MPQ\\DBFilesClient\\ItemSet.dbc"));
                }
            }
        }

        candidates.emplace_back("Data\\patch-A.MPQ\\DBFilesClient\\ItemSet.dbc");
        candidates.emplace_back("DBFilesClient\\ItemSet.dbc");
        return candidates;
    }

    struct LockRow {
        uint8_t unk00[0x24];
        uint32_t type[8];
        uint32_t index[8];
        uint8_t unk64[0x64 - 0x64];
    };

    struct LockTypeRow {
        uint32_t id;
        char* name;
    };

    struct GlyphPropertiesRow {
        uint32_t id;
        uint32_t spellId;
        uint32_t type;
    };

    struct GemPropertiesRow {
        uint32_t id;
        uint32_t enchantId;
        uint32_t maxCountInv;
        uint32_t maxCountItem;
        uint32_t type;
    };

    struct ChrRacesRow {
        uint32_t m_ID;
        uint32_t m_flags;
        uint32_t m_factionID;
        uint32_t m_explorationSoundID;
        char* m_name_lang;
    };

    struct ItemClassRow {
        uint32_t id;
        uint32_t flags;
        uint32_t unk08;
        char* name;
    };

    struct AreaTableRow {
        uint8_t pad00[0x2C];
        char* areaName;
    };

    struct SpellItemEnchantmentRow {
        uint32_t id;
        int32_t charges;
        int32_t effect[3];
        int32_t effectPointsMin[3];
        int32_t effectPointsMax[3];
        int32_t effectArg[3];
        char* name;
        uint32_t itemVisual;
        uint32_t flags;
        uint32_t srcItemId;
        uint32_t conditionId;
        int32_t requiredSkillId;
        int32_t requiredSkillRank;
        int32_t minLevel;
    };

    enum : size_t {
        kItemFieldSpellCharges = 16,
        kItemFieldPropertySeed = 58,
        kItemFieldRandomPropertiesId = 59,
        kItemFieldDurability = 60,
        kItemFieldMaxDurability = 61,
    };

    constexpr ptrdiff_t kTooltipRandomEnchantmentBufferOffset = 0x408;
    constexpr ptrdiff_t kTooltipSuffixFactorOffset = 0x498;
    constexpr ptrdiff_t kTooltipRandomPropertyIdOffset = 0x49C;

    CLIENT_FUNCTION(CGTooltip_SetItemLoadingText, 0x623760, __thiscall, void, (void*))
    CLIENT_FUNCTION(CGTooltip_AddTexture, 0x61C8B0, __thiscall, void, (void*, char*, float*, uint32_t*))
    CLIENT_FUNCTION(CGItem_C__BuildItemName, 0x706D70, __cdecl, void, (char*, uint32_t, int, int))
    CLIENT_FUNCTION(CGItem_C__GetEnchantmentIdBySlot, 0x518B30, __thiscall, int32_t, (void*, int))
    CLIENT_FUNCTION(CGItem_C__GetInventoryArt, 0x70A910, __cdecl, const char*, (uint32_t))
    CLIENT_FUNCTION(CGItem_C__GetUseSpell, 0x706B90, __thiscall, uint32_t, (void*, int))
    CLIENT_FUNCTION(CGItem_C__NumBaseCharges, 0x706BF0, __thiscall, int32_t, (void*))
    CLIENT_FUNCTION(CGItem_C__HasSpellItemEnchantment, 0x707DB0, __thiscall, bool, (void*))
    CLIENT_FUNCTION(CGItem_C__IsBound, 0x708520, __thiscall, bool, (void*))
    CLIENT_FUNCTION(CGItem_C__GetRepairCost, 0x708540, __thiscall, int32_t, (void*))
    CLIENT_FUNCTION(CGItem_C__RequestRefundInfo, 0x7089E0, __thiscall, void, (void*))
    CLIENT_FUNCTION(CGItem_C__HasRefundCurrency, 0x708AC0, __thiscall, bool, (void*))
    CLIENT_FUNCTION(CGItem_C__IsPermanentlyBoundForTrade, 0x708B40, __thiscall, bool, (void*))
    CLIENT_FUNCTION(CGItem_C__IsSocketable, 0x7094E0, __thiscall, bool, (void*))
    CLIENT_FUNCTION(CGItem_C__GetAdjustedChargesValue, 0x707DC0, __thiscall, int32_t, (void*))
    CLIENT_FUNCTION(CGItem_C__GetEnchantmentTimeLeft, 0x707120, __thiscall, int32_t, (void*, int))
    CLIENT_FUNCTION(Spell_C_GetItemCooldown, 0x8090C0, __cdecl, int, (void*, uint32_t*, int, uint32_t*))
    CLIENT_FUNCTION(CGCalendar__GetHolidayName, 0x5B9430, __cdecl, char*, (uint32_t))
    CLIENT_FUNCTION(CGPlayer_C__GetVisibleItemEntryId, 0x6DE330, __thiscall, uint32_t, (void*, int))
    CLIENT_FUNCTION(CGPlayer_C__GetPlayedTime, 0x6CF440, __thiscall, uint32_t, (CGPlayer*))
    CLIENT_FUNCTION(CursorGetResetMode, 0x616260, __cdecl, int, ())
    CLIENT_FUNCTION(CGTooltip_RunMoneyScript, 0x61A0D0, __thiscall, void, (void*, int, int))
    CLIENT_FUNCTION(DbNameCache_GetInfoBlockById, 0x67D770, __thiscall, char*, (void*, uint32_t, uint32_t, void*, void*, void*, int))
    CLIENT_FUNCTION(DbItemNameCache_GetInfoBlockById, 0x67C3E0, __thiscall, void*, (void*, uint32_t, void*))
    CLIENT_FUNCTION(DbPetitionCache_GetInfoBlockById, 0x67EF70, __thiscall, void*, (void*, uint32_t, void*, void*, void*, int))
    CLIENT_FUNCTION(BuildEnchantmentConditionText, 0x61C0B0, __cdecl, int, (int, int, unsigned char*, int, int))
    CLIENT_FUNCTION(FormatSocketEnchantmentText, 0x577260, __cdecl, bool, (int, char*, unsigned int, int))
    CLIENT_FUNCTION(ClientDb_StringLookup, 0x634910, __cdecl, char*, (int))
    CLIENT_FUNCTION(CGPlayer_C__GetPVPFactionIndex, 0x6D6E90, __cdecl, int, ())
    CLIENT_FUNCTION(CGUnit_C__IsSpellKnown, 0x7260E0, __thiscall, bool, (void*, uint32_t))
    CLIENT_FUNCTION(DBItemCache__GetItemNameByIndex, 0x4FD200, __thiscall, char*, (void*, int))
    CLIENT_FUNCTION(CVar__Lookup, 0x767440, __cdecl, CVar*, (char*))
    CLIENT_FUNCTION(ItemSocketMatchesGem, 0x5C48D0, __cdecl, bool, (int, int, int))
    CLIENT_FUNCTION(UnitMeetsSpellKnowledgeRequirement, 0x726160, __thiscall, bool, (void*, uint32_t))
    CLIENT_FUNCTION(Spell_C_GetTargetingSpell, 0x7FD630, __cdecl, uint32_t, ())
    CLIENT_FUNCTION(CGPlayer_C__GetSkillValueForLine, 0x6DC2C0, __thiscall, uint32_t, (CGPlayer*, uint32_t))
    CLIENT_FUNCTION(GetSocketEnchantmentScalarForItem, 0x7089B0, __thiscall, int, (void*, int))
    CLIENT_FUNCTION(BuildEquipmentSetList, 0x5AE380, __cdecl, bool, (char*, size_t, const uint32_t*))
    CLIENT_FUNCTION(GetPetitionSignatureCount, 0x61DC90, __thiscall, int, (void*))

    uint8_t FloatColorToByte(const float value) {
        const float clamped = std::clamp(value, 0.0f, 1.0f);
        return static_cast<uint8_t>(clamped * 255.0f + 0.5f);
    }

    uint32_t MakeTooltipColor(const ItemDifficultyTextRow* row) {
        const uint32_t red = FloatColorToByte(row->red);
        const uint32_t green = FloatColorToByte(row->green);
        const uint32_t blue = FloatColorToByte(row->blue);
        return 0xFF000000u | (red << 16) | (green << 8) | blue;
    }

    void* GetQualityColor(const ItemCacheTooltipView* itemCache) {
        if (!itemCache) {
            return reinterpret_cast<void*>(kColorWhite);
        }

        return reinterpret_cast<void*>(kItemQualityColors + itemCache->quality * 4);
    }

    void AddSingleLine(void* tooltip, char* text, void* color, uint32_t wrap = 0) {
        if (text && text[0]) {
            CGTooltip::AddLine(tooltip, text, nullptr, color, color, wrap);
        }
    }

    void AddWhiteLine(void* tooltip, char* text, uint32_t wrap = 0) {
        AddSingleLine(tooltip, text, reinterpret_cast<void*>(kColorWhite), wrap);
    }

    void AddGreyLine(void* tooltip, char* text, uint32_t wrap = 0) {
        AddSingleLine(tooltip, text, reinterpret_cast<void*>(kColorGrey0), wrap);
    }

    void AddRedLine(void* tooltip, char* text, uint32_t wrap = 0) {
        AddSingleLine(tooltip, text, reinterpret_cast<void*>(kColorRed0), wrap);
    }

    void AddGreenLine(void* tooltip, char* text, uint32_t wrap = 0) {
        AddSingleLine(tooltip, text, reinterpret_cast<void*>(kColorGreen0), wrap);
    }

    void AddYellowLine(void* tooltip, char* text, uint32_t wrap = 0) {
        AddSingleLine(tooltip, text, reinterpret_cast<void*>(kColorYellow), wrap);
    }

    void AddDarkYellowLine(void* tooltip, char* text, uint32_t wrap = 0) {
        AddSingleLine(tooltip, text, reinterpret_cast<void*>(kColorDarkYellow), wrap);
    }

    void AddSpacerLine(void* tooltip) {
        if (!tooltip) {
            return;
        }

        CGTooltip::AddLine(
            tooltip,
            const_cast<char*>(" "),
            nullptr,
            reinterpret_cast<void*>(kColorDarkYellow),
            reinterpret_cast<void*>(kColorDarkYellow),
            1);
    }

    void AddLightBlueLine(void* tooltip, char* text, uint32_t wrap = 0) {
        AddSingleLine(tooltip, text, reinterpret_cast<void*>(kColorLightBlue1), wrap);
    }

    uint32_t GetPlayerLevel(const CGPlayer* activePlayer) {
        if (!activePlayer || !activePlayer->unitBase.unitData) {
            return 0;
        }

        return activePlayer->unitBase.unitData->level;
    }

    bool IsColorblindModeEnabled() {
        CVar* colorblindMode = CVar__Lookup(const_cast<char*>("colorblindMode"));
        return colorblindMode && *reinterpret_cast<int*>(reinterpret_cast<char*>(colorblindMode) + 0x30) != 0;
    }

    ItemSubClassRow* FindItemSubClassRow(const ItemCacheTooltipView* itemCache) {
        if (!itemCache) {
            return nullptr;
        }

        uint32_t subNumRecords = *reinterpret_cast<uint32_t*>(kItemSubClassDB + 8);
        ItemSubClassRow* subRecords = *reinterpret_cast<ItemSubClassRow**>(kItemSubClassDB + 0x1C);
        if (!subRecords) {
            return nullptr;
        }

        for (uint32_t i = 0; i < subNumRecords; ++i) {
            ItemSubClassRow* row = &subRecords[i];
            if (static_cast<uint32_t>(row->m_classID) == itemCache->itemClass &&
                static_cast<uint32_t>(row->m_subClassID) == itemCache->itemSubClass) {
                return row;
            }
        }

        return nullptr;
    }

    char* GetItemSlotText(const ItemCacheTooltipView* itemCache) {
        if (!itemCache) {
            return nullptr;
        }

        // Stock gem tooltips do not emit a generic class header like "Gem"
        // or a subclass-only line such as "Blue" / "Red" here; they rely on
        // the specialized gem branch later in SetItem.
        if (itemCache->itemClass == 3) {
            return nullptr;
        }

        if (itemCache->itemClass == 6) {
            auto* itemClassRow = reinterpret_cast<ItemClassRow*>(
                ClientDB::GetRow(reinterpret_cast<void*>(kItemClassDB), itemCache->itemClass));
            if (itemClassRow && itemClassRow->name && itemClassRow->name[0]) {
                return itemClassRow->name;
            }
        }

        char** keys = reinterpret_cast<char**>(kItemClassKeys);
        char* key = keys ? keys[itemCache->inventoryType] : nullptr;
        if (!key || !key[0]) {
            return nullptr;
        }

        char* localized = FrameScript::GetText(key, -1, 0);
        return (localized && localized[0]) ? localized : nullptr;
    }

    const char* GetSubClassTextForTooltip(const ItemCacheTooltipView* itemCache, const ItemSubClassRow* subClassRow) {
        if (!itemCache || !subClassRow) {
            return nullptr;
        }

        if (itemCache->itemClass == 3) {
            return nullptr;
        }

        // Stock item tooltips do not show a right-side subclass label for
        // armor "Miscellaneous" items such as rings, trinkets, cloaks, shirts,
        // and similar inventory-type-driven entries.
        if (itemCache->itemClass == 4 && itemCache->itemSubClass == 0) {
            return nullptr;
        }

        const char* displayName =
            (subClassRow->m_displayName && subClassRow->m_displayName[0]) ? subClassRow->m_displayName : nullptr;
        const char* verboseName =
            (subClassRow->m_verboseName && subClassRow->m_verboseName[0]) ? subClassRow->m_verboseName : nullptr;

        const char* chosen = displayName ? displayName : verboseName;
        if (!chosen || !chosen[0]) {
            return nullptr;
        }

        if (_stricmp(chosen, "Misc") == 0) {
            return nullptr;
        }

        if (_stricmp(chosen, "Junk") == 0) {
            return nullptr;
        }

        return chosen;
    }

    bool BuildDamageText(
        const ItemCacheTooltipView* itemCache,
        char* outText,
        size_t outTextSize,
        float* totalDamageOut)
    {
        if (!itemCache || !outText || outTextSize == 0) {
            return false;
        }

        outText[0] = '\0';
        if (totalDamageOut) {
            *totalDamageOut = 0.0f;
        }

        float minDamage = itemCache->damageMin[0];
        float maxDamage = itemCache->damageMax[0];
        uint32_t school = itemCache->damageSchool[0];
        if (minDamage <= 0.0f && maxDamage <= 0.0f) {
            return false;
        }

        const int minValue = static_cast<int>(std::floor(minDamage));
        const int maxValue = static_cast<int>(std::ceil(maxDamage));
        if (totalDamageOut) {
            *totalDamageOut = (minDamage + maxDamage) * 0.5f;
        }

        char* formatText = nullptr;
        if (school != 0) {
            char schoolKey[64] = {};
            SStr::Printf(schoolKey, sizeof(schoolKey), const_cast<char*>("SPELL_SCHOOL%d_CAP"), school);
            char* schoolText = FrameScript::GetText(schoolKey, -1, 0);
            if (!schoolText || !schoolText[0]) {
                schoolText = const_cast<char*>("Physical");
            }

            formatText = FrameScript::GetText(
                const_cast<char*>(minValue == maxValue ? "SINGLE_DAMAGE_TEMPLATE_WITH_SCHOOL" : "DAMAGE_TEMPLATE_WITH_SCHOOL"),
                -1,
                0);
            if (!formatText || !formatText[0]) {
                return false;
            }

            if (minValue == maxValue) {
                SStr::Printf(outText, outTextSize, formatText, minValue, schoolText);
            } else {
                SStr::Printf(outText, outTextSize, formatText, minValue, maxValue, schoolText);
            }
            return outText[0] != '\0';
        }

        if (itemCache->itemClass == 6) {
            formatText = FrameScript::GetText(const_cast<char*>("AMMO_DAMAGE_TEMPLATE"), -1, 0);
            if (formatText && formatText[0]) {
                SStr::Printf(outText, outTextSize, formatText, minDamage + maxDamage);
                return outText[0] != '\0';
            }
            return false;
        }

        formatText = FrameScript::GetText(
            const_cast<char*>(minValue == maxValue ? "SINGLE_DAMAGE_TEMPLATE" : "DAMAGE_TEMPLATE"),
            -1,
            0);
        if (!formatText || !formatText[0]) {
            return false;
        }

        if (minValue == maxValue) {
            SStr::Printf(outText, outTextSize, formatText, minValue);
        } else {
            SStr::Printf(outText, outTextSize, formatText, minValue, maxValue);
        }

        return outText[0] != '\0';
    }

    bool BuildArmorText(const ItemCacheTooltipView* itemCache, char* outText, size_t outTextSize, bool* bonusArmorOut) {
        if (!itemCache || !outText || outTextSize == 0) {
            return false;
        }

        outText[0] = '\0';
        if (bonusArmorOut) {
            *bonusArmorOut = false;
        }

        if (itemCache->armor <= 0) {
            return false;
        }

        char* armorFormat = FrameScript::GetText(const_cast<char*>("ARMOR_TEMPLATE"), -1, 0);
        if (!armorFormat || !armorFormat[0]) {
            return false;
        }

        SStr::Printf(outText, outTextSize, armorFormat, itemCache->armor);
        if (bonusArmorOut) {
            *bonusArmorOut = itemCache->bonusArmorQuality > 0.0f;
        }

        return outText[0] != '\0';
    }

    bool BuildShieldBlockText(const ItemCacheTooltipView* itemCache, char* outText, size_t outTextSize) {
        if (!itemCache || !outText || outTextSize == 0) {
            return false;
        }

        outText[0] = '\0';
        if (itemCache->shieldBlock <= 0) {
            return false;
        }

        char* blockFormat = FrameScript::GetText(const_cast<char*>("SHIELD_BLOCK_TEMPLATE"), -1, 0);
        if (!blockFormat || !blockFormat[0]) {
            return false;
        }

        SStr::Printf(outText, outTextSize, blockFormat, itemCache->shieldBlock);
        return outText[0] != '\0';
    }

    char* GetItemModKey(int32_t statType) {
        switch (statType) {
        case 0: return const_cast<char*>("ITEM_MOD_MANA");
        case 1: return const_cast<char*>("ITEM_MOD_HEALTH");
        case 3: return const_cast<char*>("ITEM_MOD_AGILITY");
        case 4: return const_cast<char*>("ITEM_MOD_STRENGTH");
        case 5: return const_cast<char*>("ITEM_MOD_INTELLECT");
        case 6: return const_cast<char*>("ITEM_MOD_SPIRIT");
        case 7: return const_cast<char*>("ITEM_MOD_STAMINA");
        case 12: return const_cast<char*>("ITEM_MOD_DEFENSE_SKILL_RATING");
        case 13: return const_cast<char*>("ITEM_MOD_DODGE_RATING");
        case 14: return const_cast<char*>("ITEM_MOD_PARRY_RATING");
        case 15: return const_cast<char*>("ITEM_MOD_BLOCK_RATING");
        case 16: return const_cast<char*>("ITEM_MOD_HIT_MELEE_RATING");
        case 17: return const_cast<char*>("ITEM_MOD_HIT_RANGED_RATING");
        case 18: return const_cast<char*>("ITEM_MOD_HIT_SPELL_RATING");
        case 19: return const_cast<char*>("ITEM_MOD_CRIT_MELEE_RATING");
        case 20: return const_cast<char*>("ITEM_MOD_CRIT_RANGED_RATING");
        case 21: return const_cast<char*>("ITEM_MOD_CRIT_SPELL_RATING");
        case 22: return const_cast<char*>("ITEM_MOD_HIT_TAKEN_MELEE_RATING");
        case 23: return const_cast<char*>("ITEM_MOD_HIT_TAKEN_RANGED_RATING");
        case 24: return const_cast<char*>("ITEM_MOD_HIT_TAKEN_SPELL_RATING");
        case 25: return const_cast<char*>("ITEM_MOD_CRIT_TAKEN_MELEE_RATING");
        case 26: return const_cast<char*>("ITEM_MOD_CRIT_TAKEN_RANGED_RATING");
        case 27: return const_cast<char*>("ITEM_MOD_CRIT_TAKEN_SPELL_RATING");
        case 28: return const_cast<char*>("ITEM_MOD_HASTE_MELEE_RATING");
        case 29: return const_cast<char*>("ITEM_MOD_HASTE_RANGED_RATING");
        case 30: return const_cast<char*>("ITEM_MOD_HASTE_SPELL_RATING");
        case 31: return const_cast<char*>("ITEM_MOD_HIT_RATING");
        case 32: return const_cast<char*>("ITEM_MOD_CRIT_RATING");
        case 33: return const_cast<char*>("ITEM_MOD_HIT_TAKEN_RATING");
        case 34: return const_cast<char*>("ITEM_MOD_CRIT_TAKEN_RATING");
        case 35: return const_cast<char*>("ITEM_MOD_RESILIENCE_RATING");
        case 36: return const_cast<char*>("ITEM_MOD_HASTE_RATING");
        case 37: return const_cast<char*>("ITEM_MOD_EXPERTISE_RATING");
        case 38: return const_cast<char*>("ITEM_MOD_ATTACK_POWER");
        case 39: return const_cast<char*>("ITEM_MOD_RANGED_ATTACK_POWER");
        case 41: return const_cast<char*>("ITEM_MOD_SPELL_HEALING_DONE");
        case 42: return const_cast<char*>("ITEM_MOD_SPELL_DAMAGE_DONE");
        case 43: return const_cast<char*>("ITEM_MOD_MANA_REGENERATION");
        case 44: return const_cast<char*>("ITEM_MOD_ARMOR_PENETRATION_RATING");
        case 45: return const_cast<char*>("ITEM_MOD_SPELL_POWER");
        case 46: return const_cast<char*>("ITEM_MOD_HEALTH_REGEN");
        case 47: return const_cast<char*>("ITEM_MOD_SPELL_PENETRATION");
        default: return nullptr;
        }
    }

    bool IsEquipStyleStatType(int32_t statType) {
        return statType >= 12 && statType <= 47;
    }

    void AddFormattedItemModLine(void* tooltip, char* formatKey, int32_t value, bool equipStyle) {
        if (!tooltip || !formatKey || !formatKey[0] || value == 0) {
            return;
        }

        char* formatText = FrameScript::GetText(formatKey, -1, 0);
        if (!formatText || !formatText[0]) {
            return;
        }

        LOG_DEBUG << formatKey << " : " << formatText;

        char line[512] = {};
        SStr::Printf(
            line,
            sizeof(line),
            formatText,
            value <= 0 ? '-' : '+',
            value < 0 ? -value : value);

        uint32_t lineColor = equipStyle ? kColorGreen0 : kColorWhite;
        AddSingleLine(tooltip, line, reinterpret_cast<void*>(lineColor), 1);
    }

    void AddDirectStatLines(void* tooltip, const ItemCacheTooltipView* itemCache) {
        if (!tooltip || !itemCache || itemCache->armorFlags != 0) {
            return;
        }

        for (size_t i = 0; i < std::size(itemCache->statType); ++i) {
            if (itemCache->statType[i] < 0) {
                continue;
            }

            char* key = GetItemModKey(itemCache->statType[i]);
            if (key) {
                AddFormattedItemModLine(tooltip, key, itemCache->statValue[i], IsEquipStyleStatType(itemCache->statType[i]));
            }
        }
    }

    void AddDurabilityLine(void* tooltip, void* itemObject) {
        if (!tooltip || !itemObject) {
            return;
        }

        CGObject* object = reinterpret_cast<CGObject*>(itemObject);
        if (!object || !object->ObjectData) {
            return;
        }

        uint32_t* itemFields = reinterpret_cast<uint32_t*>(object->ObjectData);
        const uint32_t currentDurability = itemFields[kItemFieldDurability];
        const uint32_t maxDurability = itemFields[kItemFieldMaxDurability];
        if (maxDurability == 0) {
            return;
        }

        char* formatText = FrameScript::GetText(const_cast<char*>("DURABILITY_TEMPLATE"), -1, 0);
        if (!formatText || !formatText[0]) {
            return;
        }

        char line[256] = {};
        SStr::Printf(line, sizeof(line), formatText, currentDurability, maxDurability);
        AddWhiteLine(tooltip, line);
    }

    void AddResistanceLines(void* tooltip, const ItemCacheTooltipView* itemCache) {
        if (!tooltip || !itemCache) {
            return;
        }

        if (itemCache->allResistValue > 0) {
            bool allEqual = true;
            for (int32_t resistance : itemCache->resistances) {
                if (resistance != itemCache->allResistValue) {
                    allEqual = false;
                    break;
                }
            }

            if (allEqual) {
                char* formatText = FrameScript::GetText(const_cast<char*>("ITEM_RESIST_ALL"), -1, 0);
                if (formatText && formatText[0]) {
                    char line[512] = {};
                    SStr::Printf(
                        line,
                        sizeof(line),
                        formatText,
                        '+',
                        itemCache->allResistValue);
                    AddWhiteLine(tooltip, line);
                    return;
                }
            }
        }

        for (int school = 2; school <= 6; ++school) {
            int32_t value = itemCache->resistances[school - 2];
            if (value == 0) {
                continue;
            }

            char schoolKey[64] = {};
            SStr::Printf(schoolKey, sizeof(schoolKey), const_cast<char*>("SPELL_SCHOOL%d_CAP"), school);
            char* schoolText = FrameScript::GetText(schoolKey, -1, 0);
            char* formatText = FrameScript::GetText(const_cast<char*>("ITEM_RESIST_SINGLE"), -1, 0);
            if (!schoolText || !schoolText[0] || !formatText || !formatText[0]) {
                continue;
            }

            char line[512] = {};
            SStr::Printf(
                line,
                sizeof(line),
                formatText,
                value <= 0 ? '-' : '+',
                value < 0 ? -value : value,
                schoolText);
            AddWhiteLine(tooltip, line);
        }
    }

    const char* GetSocketColorName(uint32_t socketColor) {
        switch (socketColor) {
        case 1: return "Meta";
        case 2: return "Red";
        case 4: return "Yellow";
        case 8: return "Blue";
        default: return nullptr;
        }
    }

    int32_t GetSocketEnchantmentId(void* itemObject, size_t socketIndex) {
        if (!itemObject || socketIndex >= 3) {
            return 0;
        }

        void* enchantmentState = reinterpret_cast<char*>(itemObject) + 0xD0;
        return CGItem_C__GetEnchantmentIdBySlot(enchantmentState, static_cast<int>(socketIndex + 2));
    }

    int32_t GetItemEnchantmentIdBySlot(void* itemObject, int slotIndex) {
        if (!itemObject || slotIndex < 0) {
            return 0;
        }

        void* enchantmentState = reinterpret_cast<char*>(itemObject) + 0xD0;
        return CGItem_C__GetEnchantmentIdBySlot(enchantmentState, slotIndex);
    }

    bool TryGetItemEnchantmentStateData(void* itemObject, int slotIndex, int16_t* outCharges, uint8_t* outStateFlags) {
        if (!itemObject || slotIndex < 0) {
            return false;
        }

        char* enchantmentState = reinterpret_cast<char*>(itemObject) + 0xD0;
        auto*** vtable = reinterpret_cast<void***>(enchantmentState);
        if (!vtable || !*vtable || !(*vtable)[0]) {
            return false;
        }

        using QueryStateFn = int(__thiscall*)(void*, int);
        const int stateBits = reinterpret_cast<QueryStateFn>((*vtable)[0])(enchantmentState, 0);
        if ((stateBits & 0x2000) != 0) {
            return false;
        }

        void* rawEnchantmentData = *reinterpret_cast<void**>(enchantmentState + 4);
        if (!rawEnchantmentData) {
            return false;
        }

        if (outCharges) {
            *outCharges = *reinterpret_cast<int16_t*>(reinterpret_cast<char*>(rawEnchantmentData) + 0x48 + slotIndex * 12);
        }

        if (outStateFlags) {
            void* visualState = *reinterpret_cast<void**>(reinterpret_cast<char*>(itemObject) + 0xD4);
            *outStateFlags = visualState
                ? *reinterpret_cast<uint8_t*>(reinterpret_cast<char*>(visualState) + 0x4A + slotIndex * 12)
                : 0;
        }

        return true;
    }

    SpellItemEnchantmentRow* FindSpellItemEnchantmentRow(uint32_t enchantmentId) {
        if (enchantmentId == 0) {
            return nullptr;
        }

        auto* db = reinterpret_cast<WoWClientDB*>(kSpellItemEnchantmentDB);
        if (!db || !db->Rows) {
            return nullptr;
        }

        if (static_cast<int32_t>(enchantmentId) < db->minIndex || static_cast<int32_t>(enchantmentId) > db->maxIndex) {
            return nullptr;
        }

        auto** recordsById = reinterpret_cast<SpellItemEnchantmentRow**>(db->Rows);
        return recordsById[enchantmentId - static_cast<uint32_t>(db->minIndex)];
    }

    SpellItemEnchantmentRow* FindSpellItemEnchantmentRowSigned(int32_t enchantmentId) {
        if (enchantmentId == 0) {
            return nullptr;
        }

        const uint32_t normalizedId = static_cast<uint32_t>(enchantmentId < 0 ? -enchantmentId : enchantmentId);
        return FindSpellItemEnchantmentRow(normalizedId);
    }

    void ClearTooltipRandomPropertyState(void* tooltip) {
        if (!tooltip) {
            return;
        }

        std::memset(
            static_cast<char*>(tooltip) + kTooltipRandomEnchantmentBufferOffset,
            0,
            sizeof(uint32_t) * 5);
        *reinterpret_cast<uint16_t*>(static_cast<char*>(tooltip) + kTooltipSuffixFactorOffset) = 0;
        *reinterpret_cast<int32_t*>(static_cast<char*>(tooltip) + kTooltipRandomPropertyIdOffset) = 0;
    }

    int32_t GetLiveItemRandomPropertyId(void* itemObject) {
        if (!itemObject) {
            return 0;
        }

        CGObject* object = reinterpret_cast<CGObject*>(itemObject);
        if (!object || !object->ObjectData) {
            return 0;
        }

        uint32_t* itemFields = reinterpret_cast<uint32_t*>(object->ObjectData);
        return static_cast<int32_t>(itemFields[kItemFieldRandomPropertiesId]);
    }

    uint16_t GetLiveItemSuffixFactor(void* itemObject) {
        if (!itemObject) {
            return 0;
        }

        CGObject* object = reinterpret_cast<CGObject*>(itemObject);
        if (!object || !object->ObjectData) {
            return 0;
        }

        uint32_t* itemFields = reinterpret_cast<uint32_t*>(object->ObjectData);
        return static_cast<uint16_t>(itemFields[kItemFieldPropertySeed] & 0xFFFFu);
    }

    int32_t ResolveRandomPropertySelector(void* tooltip, void* itemObject, int fallbackSelector) {
        if (!tooltip) {
            return fallbackSelector;
        }

        const uint32_t tooltipState = *reinterpret_cast<uint32_t*>(static_cast<char*>(tooltip) + 0x4C0);
        const int32_t cachedSelector =
            *reinterpret_cast<int32_t*>(static_cast<char*>(tooltip) + kTooltipRandomPropertyIdOffset);
        if (tooltipState != 0 && cachedSelector != 0) {
            return cachedSelector;
        }

        const int32_t liveSelector = GetLiveItemRandomPropertyId(itemObject);
        if (liveSelector != 0) {
            return liveSelector;
        }

        if (cachedSelector != 0) {
            return cachedSelector;
        }

        return fallbackSelector;
    }

    void PopulateTooltipRandomPropertyBuffer(void* tooltip, int32_t randomSelector) {
        if (!tooltip) {
            return;
        }

        auto* outValues = reinterpret_cast<uint32_t*>(static_cast<char*>(tooltip) + kTooltipRandomEnchantmentBufferOffset);
        std::memset(outValues, 0, sizeof(uint32_t) * 5);
        if (randomSelector == 0) {
            return;
        }

        if (randomSelector > 0) {
            auto* db = reinterpret_cast<WoWClientDB*>(kItemRandomPropertiesDB);
            if (!db || !db->Rows || randomSelector < db->minIndex || randomSelector > db->maxIndex) {
                return;
            }

            auto** recordsById = reinterpret_cast<uint32_t**>(db->Rows);
            uint32_t* row = recordsById[randomSelector - db->minIndex];
            if (!row) {
                return;
            }

            std::memcpy(outValues, reinterpret_cast<const char*>(row) + 0x08, sizeof(uint32_t) * 5);
            return;
        }

        const int32_t suffixId = -randomSelector;
        auto* db = reinterpret_cast<WoWClientDB*>(kItemRandomSuffixDB);
        if (!db || !db->Rows || suffixId < db->minIndex || suffixId > db->maxIndex) {
            return;
        }

        auto** recordsById = reinterpret_cast<uint32_t**>(db->Rows);
        uint32_t* row = recordsById[suffixId - db->minIndex];
        if (!row) {
            return;
        }

        std::memcpy(outValues, reinterpret_cast<const char*>(row) + 0x0C, sizeof(uint32_t) * 5);
    }

    GemPropertiesRow* FindGemPropertiesRowById(uint32_t gemPropertiesId) {
        if (gemPropertiesId == 0) {
            return nullptr;
        }

        auto* db = reinterpret_cast<WoWClientDB*>(kGemPropertiesDB);
        if (!db || !db->Rows) {
            return nullptr;
        }

        if (static_cast<int32_t>(gemPropertiesId) < db->minIndex || static_cast<int32_t>(gemPropertiesId) > db->maxIndex) {
            return nullptr;
        }

        auto** recordsById = reinterpret_cast<GemPropertiesRow**>(db->Rows);
        return recordsById[gemPropertiesId - static_cast<uint32_t>(db->minIndex)];
    }

    ItemCacheTooltipView* GetGemItemCacheByEnchantmentId(void* tooltip, int32_t enchantmentId) {
        if (!tooltip || enchantmentId == 0) {
            return nullptr;
        }

        auto* enchantRow = FindSpellItemEnchantmentRowSigned(enchantmentId);
        if (!enchantRow || enchantRow->srcItemId == 0) {
            return nullptr;
        }

        uint64_t relatedGuid = 0;
        void* gemItemBlock = DBItemCache_GetInfoBlockByID(
            reinterpret_cast<void*>(kWdbCacheItem),
            enchantRow->srcItemId,
            &relatedGuid,
            nullptr,
            nullptr,
            0);
        return reinterpret_cast<ItemCacheTooltipView*>(gemItemBlock);
    }

    bool AreAllSocketBonusesActive(void* tooltip, const ItemCacheTooltipView* itemCache, void* itemObject) {
        if (!tooltip || !itemCache || !itemObject) {
            return false;
        }

        bool hasSockets = false;
        for (size_t socketIndex = 0; socketIndex < std::size(itemCache->socketColor); ++socketIndex) {
            if (itemCache->socketColor[socketIndex] == 0) {
                continue;
            }

            hasSockets = true;
            ItemCacheTooltipView* gemItemCache =
                GetGemItemCacheByEnchantmentId(tooltip, GetSocketEnchantmentId(itemObject, socketIndex));
            if (!gemItemCache) {
                return false;
            }

            if (!ItemSocketMatchesGem(
                    reinterpret_cast<int>(const_cast<ItemCacheTooltipView*>(itemCache)),
                    reinterpret_cast<int>(gemItemCache),
                    static_cast<int>(socketIndex))) {
                return false;
            }
        }

        return hasSockets;
    }

    void AddEnchantmentRequirementLines(void* tooltip, const SpellItemEnchantmentRow* enchantRow, CGPlayer* activePlayer);

    void AddSocketTexture(void* tooltip, const char* texturePath) {
        if (!tooltip || !texturePath || !texturePath[0]) {
            return;
        }

        float texCoords[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
        uint32_t vertexColor = 0xFFFFFFFFu;
        CGTooltip_AddTexture(tooltip, const_cast<char*>(texturePath), texCoords, &vertexColor);
    }

    void AddEmptySocketLine(void* tooltip, const char* colorName) {
        if (!tooltip || !colorName || !colorName[0]) {
            return;
        }

        char key[128] = {};
        SStr::Printf(key, sizeof(key), const_cast<char*>("EMPTY_SOCKET_%s"), colorName);
        for (char* ch = key; *ch; ++ch) {
            if (*ch >= 'a' && *ch <= 'z') {
                *ch = static_cast<char>(*ch - ('a' - 'A'));
            }
        }

        char* lineText = FrameScript::GetText(key, -1, 0);
        if (lineText && lineText[0]) {
            AddGreyLine(tooltip, lineText);
        }

        char texturePath[256] = {};
        SStr::Printf(
            texturePath,
            sizeof(texturePath),
            const_cast<char*>("Interface\\ItemSocketingFrame\\UI-EmptySocket-%s"),
            const_cast<char*>(colorName));

        AddSocketTexture(tooltip, texturePath);
    }

    void AddPrismaticSocketLine(void* tooltip, bool requirementsMet) {
        if (!tooltip) {
            return;
        }

        char* lineText = FrameScript::GetText(const_cast<char*>("EMPTY_SOCKET_NO_COLOR"), -1, 0);
        if (lineText && lineText[0]) {
            AddSingleLine(
                tooltip,
                lineText,
                reinterpret_cast<void*>(requirementsMet ? kColorGrey0 : kColorRed0),
                0);
        }

        AddSocketTexture(tooltip, const_cast<char*>("Interface\\ItemSocketingFrame\\UI-EmptySocket.blp"));
    }

    bool BuildSocketedGemLineText(
        void* itemObject,
        SpellItemEnchantmentRow* enchantRow,
        size_t socketIndex,
        char* outText,
        size_t outTextSize)
    {
        if (!enchantRow || !outText || outTextSize == 0) {
            return false;
        }

        outText[0] = '\0';

        int scalarValue = 0;
        if (itemObject) {
            scalarValue = GetSocketEnchantmentScalarForItem(itemObject, static_cast<int>(socketIndex) + 2);
        }

        if (FormatSocketEnchantmentText(
                reinterpret_cast<int>(enchantRow),
                outText,
                static_cast<unsigned int>(outTextSize),
                scalarValue) &&
            outText[0]) {
            return true;
        }

        if (enchantRow->name && enchantRow->name[0]) {
            SStr::Copy(outText, enchantRow->name, outTextSize);
            return outText[0] != '\0';
        }

        return false;
    }

    void AddFilledSocketLine(
        void* tooltip,
        const ItemCacheTooltipView* itemCache,
        ItemCacheTooltipView* gemItemCache,
        SpellItemEnchantmentRow* enchantRow,
        void* itemObject,
        size_t socketIndex)
    {
        if (!tooltip || !itemCache || !gemItemCache || !enchantRow || enchantRow->srcItemId == 0) {
            return;
        }

        char socketText[1024] = {};
        if (BuildSocketedGemLineText(itemObject, enchantRow, socketIndex, socketText, sizeof(socketText))) {
            AddSingleLine(
                tooltip,
                socketText,
                reinterpret_cast<void*>(kColorWhite),
                0);
        }

        char* iconRoot = ClientDb_StringLookup(3);
        const char* iconName = CGItem_C__GetInventoryArt(gemItemCache->displayId);
        if (!iconRoot || !iconRoot[0] || !iconName || !iconName[0]) {
            return;
        }

        const char* separator = iconRoot[0] ? "\\" : "";
        char texturePath[260] = {};
        SStr::Printf(
            texturePath,
            sizeof(texturePath),
            const_cast<char*>("%s%s%s"),
            iconRoot,
            const_cast<char*>(separator),
            const_cast<char*>(iconName));
        AddSocketTexture(tooltip, texturePath);
    }

    void AddSocketLines(void* tooltip, const ItemCacheTooltipView* itemCache, void* itemObject, CGPlayer* activePlayer) {
        if (!tooltip || !itemCache) {
            return;
        }

        for (size_t socketIndex = 0; socketIndex < std::size(itemCache->socketColor); ++socketIndex) {
            uint32_t socketColor = itemCache->socketColor[socketIndex];
            const char* colorName = GetSocketColorName(socketColor);
            if (!colorName || !colorName[0]) {
                continue;
            }

            int32_t enchantmentId = GetSocketEnchantmentId(itemObject, socketIndex);
            auto* enchantRow = FindSpellItemEnchantmentRowSigned(enchantmentId);
            ItemCacheTooltipView* gemItemCache = GetGemItemCacheByEnchantmentId(tooltip, enchantmentId);
            if (gemItemCache && enchantRow) {
                AddFilledSocketLine(tooltip, itemCache, gemItemCache, enchantRow, itemObject, socketIndex);
                AddEnchantmentRequirementLines(tooltip, enchantRow, activePlayer);
            } else {
                AddEmptySocketLine(tooltip, colorName);
            }
        }
    }

    void AddSocketBonusLine(void* tooltip, const ItemCacheTooltipView* itemCache, void* itemObject) {
        if (!tooltip || !itemCache || itemCache->socketBonusEnchantId == 0) {
            return;
        }

        auto* enchantRow = FindSpellItemEnchantmentRowSigned(static_cast<int32_t>(itemCache->socketBonusEnchantId));
        if (!enchantRow || !enchantRow->name || !enchantRow->name[0]) {
            return;
        }

        char* formatText = FrameScript::GetText(const_cast<char*>("ITEM_SOCKET_BONUS"), -1, 0);
        if (!formatText || !formatText[0]) {
            return;
        }

        char line[1024] = {};
        SStr::Printf(line, sizeof(line), formatText, enchantRow->name);
        AddSingleLine(
            tooltip,
            line,
            reinterpret_cast<void*>(AreAllSocketBonusesActive(tooltip, itemCache, itemObject) ? kColorGreen0 : kColorGrey0),
            0);
    }

    SkillLineRow* FindSkillLineRowById(uint32_t skillLineId);

    SkillLineRow* FindSkillLineRow(uint32_t skillLineId) {
        if (skillLineId == 0) {
            return nullptr;
        }

        if (auto* rowById = FindSkillLineRowById(skillLineId)) {
            if (rowById->m_displayName_lang && rowById->m_displayName_lang[0]) {
                return rowById;
            }
        }

        auto* skillLineDb = reinterpret_cast<WoWClientDB*>(kSkillLineDB);
        if (!skillLineDb || skillLineId < skillLineDb->minIndex || skillLineId > skillLineDb->maxIndex) {
            return nullptr;
        }

        static SkillLineRow localizedRow = {};
        if (!ClientDB::GetLocalizedRow(reinterpret_cast<void*>(kSkillLineDB), skillLineId, &localizedRow)) {
            return nullptr;
        }

        return &localizedRow;
    }

    SkillLineRow* FindSkillLineRowById(uint32_t skillLineId) {
        if (skillLineId == 0) {
            return nullptr;
        }

        auto* db = reinterpret_cast<WoWClientDB*>(kSkillLineDB);
        if (!db || !db->Rows) {
            return nullptr;
        }

        if (static_cast<int32_t>(skillLineId) < db->minIndex || static_cast<int32_t>(skillLineId) > db->maxIndex) {
            return nullptr;
        }

        auto** recordsById = reinterpret_cast<SkillLineRow**>(db->Rows);
        return recordsById[skillLineId - static_cast<uint32_t>(db->minIndex)];
    }

    uint32_t GetPlayerSkillRank(CGPlayer* activePlayer, uint32_t skillLineId);

    const std::vector<ParsedItemSetRow>* GetParsedItemSets() {
        static bool loaded = false;
        static bool loadSucceeded = false;
        static std::vector<ParsedItemSetRow> cachedRows;
        if (!loaded) {
            loaded = true;

            std::ifstream file;
            const std::vector<std::string> candidatePaths = GetItemSetDbCandidatePaths();
            for (const std::string& path : candidatePaths) {
                file.open(path, std::ios::binary);
                if (file.is_open()) {
                    LOG_DEBUG << "ItemTooltip set-file: opened path=" << path;
                    break;
                }
            }

            if (!file.is_open()) {
                LOG_DEBUG << "ItemTooltip set-file: unable to open ItemSet.dbc";
            } else {
                uint32_t magic = 0;
                uint32_t recordCount = 0;
                uint32_t fieldCount = 0;
                uint32_t recordSize = 0;
                uint32_t stringBlockSize = 0;

                file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
                file.read(reinterpret_cast<char*>(&recordCount), sizeof(recordCount));
                file.read(reinterpret_cast<char*>(&fieldCount), sizeof(fieldCount));
                file.read(reinterpret_cast<char*>(&recordSize), sizeof(recordSize));
                file.read(reinterpret_cast<char*>(&stringBlockSize), sizeof(stringBlockSize));

                if (!file || magic != 0x43424457 || fieldCount != 53 || recordSize != sizeof(RawItemSetRow)) {
                    LOG_DEBUG << "ItemTooltip set-file: invalid header magic=" << std::hex << magic << std::dec
                              << " fields=" << fieldCount
                              << " recordSize=" << recordSize;
                } else {
                    std::vector<RawItemSetRow> rows(recordCount);
                    std::vector<char> stringBlock(stringBlockSize);
                    file.read(reinterpret_cast<char*>(rows.data()), static_cast<std::streamsize>(recordCount * sizeof(RawItemSetRow)));
                    file.read(stringBlock.data(), static_cast<std::streamsize>(stringBlockSize));

                    if (file) {
                        for (const auto& row : rows) {
                            ParsedItemSetRow parsed = {};
                            parsed.id = row.id;
                            std::copy(std::begin(row.itemId), std::end(row.itemId), std::begin(parsed.itemId));
                            std::copy(std::begin(row.spellId), std::end(row.spellId), std::begin(parsed.spellId));
                            std::copy(std::begin(row.spellThreshold), std::end(row.spellThreshold), std::begin(parsed.spellThreshold));
                            parsed.requiredSkillId = row.requiredSkillId;
                            parsed.requiredSkillRank = row.requiredSkillRank;

                            const uint32_t nameOffset = row.nameLangAndFlags[0];
                            if (nameOffset < stringBlock.size()) {
                                parsed.name = &stringBlock[nameOffset];
                            }

                            cachedRows.push_back(std::move(parsed));
                        }

                        loadSucceeded = !cachedRows.empty();
                        LOG_DEBUG << "ItemTooltip set-file: loaded rows=" << cachedRows.size();
                    } else {
                        LOG_DEBUG << "ItemTooltip set-file: failed reading records";
                    }
                }
            }
        }

        if (!loadSucceeded) {
            return nullptr;
        }

        return &cachedRows;
    }

    bool GetLocalizedItemSetRowBySetId(uint32_t setId, ItemSetRow* outRow) {
        if (!outRow) {
            return false;
        }

        if (setId == 0) {
            return false;
        }

        if (const auto* parsedItemSets = GetParsedItemSets()) {
            for (const ParsedItemSetRow& parsed : *parsedItemSets) {
                if (parsed.id != setId) {
                    continue;
                }

                std::memset(outRow, 0, sizeof(*outRow));
                outRow->id = parsed.id;
                outRow->name = const_cast<char*>(parsed.name.c_str());
                std::copy(std::begin(parsed.itemId), std::end(parsed.itemId), std::begin(outRow->itemId));
                std::copy(std::begin(parsed.spellId), std::end(parsed.spellId), std::begin(outRow->spellId));
                std::copy(std::begin(parsed.spellThreshold), std::end(parsed.spellThreshold), std::begin(outRow->spellThreshold));
                outRow->requiredSkillId = parsed.requiredSkillId;
                outRow->requiredSkillRank = parsed.requiredSkillRank;
                return true;
            }
        }

        return false;
    }

    bool GetLocalizedItemSetRowByItemId(uint32_t itemId, ItemSetRow* outRow) {
        if (!outRow) {
            return false;
        }

        if (itemId == 0) {
            return false;
        }

        if (const auto* parsedItemSets = GetParsedItemSets()) {
            for (const ParsedItemSetRow& parsed : *parsedItemSets) {
                bool found = false;
                for (uint32_t setItemId : parsed.itemId) {
                    if (setItemId == itemId) {
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    continue;
                }

                return GetLocalizedItemSetRowBySetId(parsed.id, outRow);
            }
        }

        auto* itemSetDb = reinterpret_cast<WoWClientDB*>(kItemSetDB);
        if (!itemSetDb) {
            return false;
        }

        uint32_t minId = static_cast<uint32_t>(itemSetDb->minIndex);
        uint32_t maxId = static_cast<uint32_t>(itemSetDb->maxIndex);
        for (uint32_t setId = minId; setId <= maxId; ++setId) {
            auto* row = reinterpret_cast<RawItemSetRow*>(ClientDB::GetRow(itemSetDb, setId));
            if (!row) {
                continue;
            }

            for (uint32_t setItemId : row->itemId) {
                if (setItemId == itemId) {
                    return GetLocalizedItemSetRowBySetId(setId, outRow);
                }
            }
        }

        return false;
    }

    bool IsSocketEnchantmentSlot(int slotIndex);

    bool HasStatSectionLines(const ItemCacheTooltipView* itemCache) {
        if (!itemCache) {
            return false;
        }

        if (itemCache->armor != 0 || itemCache->shieldBlock != 0) {
            return true;
        }

        for (size_t i = 0; i < std::size(itemCache->statType); ++i) {
            if (itemCache->statType[i] >= 0 && itemCache->statValue[i] != 0) {
                return true;
            }
        }

        for (int32_t resistance : itemCache->resistances) {
            if (resistance != 0) {
                return true;
            }
        }

        return false;
    }

    bool HasItemSpellEntries(const ItemCacheTooltipView* itemCache) {
        if (!itemCache) {
            return false;
        }

        for (uint32_t spellId : itemCache->spellId) {
            if (spellId != 0) {
                return true;
            }
        }

        return false;
    }

    bool HasAppliedEnchantmentEntries(const ItemCacheTooltipView* itemCache, void* itemObject) {
        if (!itemCache || !itemObject) {
            return false;
        }

        for (int slotIndex = 0; slotIndex < 12; ++slotIndex) {
            if (IsSocketEnchantmentSlot(slotIndex)) {
                continue;
            }

            const int32_t enchantmentId = GetItemEnchantmentIdBySlot(itemObject, slotIndex);
            if (enchantmentId == 0) {
                continue;
            }

            auto* enchantRow = FindSpellItemEnchantmentRowSigned(enchantmentId);
            if (!enchantRow) {
                continue;
            }

            if (itemCache->socketBonusEnchantId != 0 &&
                static_cast<uint32_t>(enchantmentId < 0 ? -enchantmentId : enchantmentId) == itemCache->socketBonusEnchantId) {
                continue;
            }

            return true;
        }

        return false;
    }

    uint32_t GetItemSetItemCount(const ItemSetRow* itemSetRow) {
        if (!itemSetRow) {
            return 0;
        }

        uint32_t count = 0;
        for (uint32_t setItemId : itemSetRow->itemId) {
            if (setItemId != 0) {
                ++count;
            }
        }

        return count;
    }

    uint32_t GetVisibleItemEntryIdValue(CGPlayer* activePlayer, int slot) {
        if (!activePlayer || slot < 0 || slot > 0x12) {
            return 0;
        }

        auto* visibleItem = reinterpret_cast<uint32_t*>(CGPlayer_C__GetVisibleItemEntryId(activePlayer, slot));
        return visibleItem ? visibleItem[0] : 0;
    }

    uint32_t BuildItemSetEquippedMask(CGPlayer* activePlayer, const ItemSetRow* itemSetRow, bool equippedMask[17]) {
        if (!itemSetRow || !equippedMask) {
            return 0;
        }

        std::fill(equippedMask, equippedMask + 17, false);
        if (!activePlayer) {
            return 0;
        }

        uint32_t equippedItemIds[19] = {};
        for (int slot = 0; slot <= 0x12; ++slot) {
            equippedItemIds[slot] = GetVisibleItemEntryIdValue(activePlayer, slot);
        }

        uint32_t equippedCount = 0;
        for (size_t setIndex = 0; setIndex < std::size(itemSetRow->itemId); ++setIndex) {
            uint32_t setItemId = itemSetRow->itemId[setIndex];
            if (setItemId == 0) {
                continue;
            }

            for (uint32_t equippedItemId : equippedItemIds) {
                if (equippedItemId != 0 && equippedItemId == setItemId) {
                    equippedMask[setIndex] = true;
                    ++equippedCount;
                    break;
                }
            }
        }

        return equippedCount;
    }

    const char* GetItemSetMemberName(void* tooltip, uint32_t itemId, char* fallbackBuffer, size_t fallbackBufferSize) {
        if (itemId == 0) {
            return nullptr;
        }

        if (fallbackBuffer && fallbackBufferSize > 0) {
            fallbackBuffer[0] = '\0';
            CGItem_C__BuildItemName(fallbackBuffer, static_cast<int>(fallbackBufferSize), itemId, 0);
            if (fallbackBuffer[0]) {
                return fallbackBuffer;
            }
        }

        uint64_t relatedGuid = 0;
        void* itemBlock = DBItemCache_GetInfoBlockByID(
            reinterpret_cast<void*>(kWdbCacheItem),
            itemId,
            &relatedGuid,
            nullptr,
            nullptr,
            0);
        if (itemBlock) {
            char* itemName = DBItemCache__GetItemNameByIndex(itemBlock, 0);
            if (itemName && itemName[0]) {
                return itemName;
            }
        }

        itemBlock = DBItemCache_GetInfoBlockByID(
            reinterpret_cast<void*>(kWdbCacheItem),
            itemId,
            &relatedGuid,
            reinterpret_cast<void*>(0x626650),
            tooltip,
            1);
        if (itemBlock) {
            char* itemName = DBItemCache__GetItemNameByIndex(itemBlock, 0);
            if (itemName && itemName[0]) {
                return itemName;
            }
        }

        return nullptr;
    }

    bool SafeParseSpellText(SpellRow* spellRow, char* outText, size_t outTextSize) {
        if (!spellRow || !outText || outTextSize == 0) {
            return false;
        }

        outText[0] = '\0';
#ifdef _MSC_VER
        __try {
            SpellParser::ParseText(spellRow, outText, static_cast<uint32_t>(outTextSize), 0, 0, 0, 0, 1, 0);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            LOG_DEBUG << "ItemTooltip set-bonus parse failed for spellId=" << spellRow->m_ID;
            outText[0] = '\0';
            return false;
        }
#else
        SpellParser::ParseText(spellRow, outText, static_cast<uint32_t>(outTextSize), 0, 0, 0, 0, 1, 0);
#endif
        return outText[0] != '\0';
    }

    bool SafeCopySpellTextField(const char* src, char* dest, size_t destSize) {
        if (!src || !dest || destSize == 0) {
            return false;
        }

        dest[0] = '\0';
#ifdef _MSC_VER
        __try {
            SStr::Copy(dest, const_cast<char*>(src), static_cast<uint32_t>(destSize));
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            dest[0] = '\0';
            return false;
        }
#else
        SStr::Copy(dest, const_cast<char*>(src), static_cast<uint32_t>(destSize));
#endif
        return dest[0] != '\0';
    }

    bool IsMostlyPrintableText(const char* text) {
        if (!text || !text[0]) {
            return false;
        }

        size_t checked = 0;
        for (const unsigned char* cur = reinterpret_cast<const unsigned char*>(text); *cur && checked < 256; ++cur, ++checked) {
            const unsigned char ch = *cur;
            if (ch < 0x20 && ch != '\t' && ch != '\r' && ch != '\n') {
                return false;
            }
        }

        return true;
    }

    bool ContainsUnresolvedSpellTokens(const char* text) {
        return text && strchr(text, '$') != nullptr;
    }

    const char* GetItemSetBonusText(SpellRow* spellRow, char* outText, size_t outTextSize) {
        if (!spellRow || !outText || outTextSize == 0) {
            return nullptr;
        }

        outText[0] = '\0';
        if (spellRow->m_description_lang && spellRow->m_description_lang[0]) {
            SafeParseSpellText(spellRow, outText, outTextSize);
        }

        if (ContainsUnresolvedSpellTokens(outText)) {
            outText[0] = '\0';
        }

        if (!outText[0] && spellRow->m_description_lang && spellRow->m_description_lang[0]) {
            SafeCopySpellTextField(spellRow->m_description_lang, outText, outTextSize);
            if (ContainsUnresolvedSpellTokens(outText)) {
                outText[0] = '\0';
            }
        }

        if (!outText[0] && spellRow->m_name_lang && spellRow->m_name_lang[0]) {
            SafeCopySpellTextField(spellRow->m_name_lang, outText, outTextSize);
        }

        return outText[0] ? outText : nullptr;
    }

    void AddItemSetLines(void* tooltip, const ItemCacheTooltipView* itemCache, uint32_t itemId, CGPlayer* activePlayer) {
        if (!tooltip || !itemCache || !activePlayer || itemId == 0) {
            return;
        }

        if (itemCache->itemSetId == 0) {
            return;
        }

        ItemSetRow itemSetRow = {};
        if (!GetLocalizedItemSetRowBySetId(itemCache->itemSetId, &itemSetRow) || !itemSetRow.name || !itemSetRow.name[0]) {
            LOG_DEBUG << "ItemTooltip set-lines: no localized row for itemId=" << itemId;
            return;
        }

        char safeSetName[512] = {};
        if (!SafeCopySpellTextField(itemSetRow.name, safeSetName, sizeof(safeSetName)) ||
            !IsMostlyPrintableText(safeSetName)) {
            LOG_DEBUG << "ItemTooltip set-lines: rejected unsafe set name for itemId=" << itemId
                      << " setId=" << itemSetRow.id;
            return;
        }

        uint32_t totalItemCount = GetItemSetItemCount(&itemSetRow);
        if (totalItemCount == 0) {
            LOG_DEBUG << "ItemTooltip set-lines: zero item count for itemId=" << itemId << " setId=" << itemSetRow.id;
            return;
        }

        bool equippedMask[17] = {};
        uint32_t equippedItemCount = BuildItemSetEquippedMask(activePlayer, &itemSetRow, equippedMask);

        char* formatText = FrameScript::GetText(const_cast<char*>("ITEM_SET_NAME"), -1, 0);
        if (formatText && formatText[0]) {
            char line[1024] = {};
            SStr::Printf(line, sizeof(line), formatText, safeSetName, equippedItemCount, totalItemCount);
            AddDarkYellowLine(tooltip, line);
        }

        if (itemSetRow.requiredSkillId != 0) {
            SkillLineRow* skillLineRow = FindSkillLineRow(itemSetRow.requiredSkillId);
            const char* skillName =
                (skillLineRow && skillLineRow->m_displayName_lang && skillLineRow->m_displayName_lang[0])
                    ? skillLineRow->m_displayName_lang
                    : "UNKNOWN";

            char* reqFormat = FrameScript::GetText(
                const_cast<char*>(itemSetRow.requiredSkillRank != 0 ? "ITEM_MIN_SKILL" : "ITEM_REQ_SKILL"),
                -1,
                0);
            if (reqFormat && reqFormat[0]) {
                char line[1024] = {};
                if (itemSetRow.requiredSkillRank != 0) {
                    SStr::Printf(line, sizeof(line), reqFormat, skillName, itemSetRow.requiredSkillRank);
                } else {
                    SStr::Printf(line, sizeof(line), reqFormat, skillName);
                }

                uint32_t playerSkillRank = GetPlayerSkillRank(activePlayer, itemSetRow.requiredSkillId);
                bool meetsRequirement =
                    itemSetRow.requiredSkillRank == 0 ? playerSkillRank > 0 : playerSkillRank >= itemSetRow.requiredSkillRank;
                AddSingleLine(
                    tooltip,
                    line,
                    reinterpret_cast<void*>(meetsRequirement ? kColorWhite : kColorRed0),
                    0);
            }
        }

        for (size_t itemIndex = 0; itemIndex < std::size(itemSetRow.itemId); ++itemIndex) {
            uint32_t setItemId = itemSetRow.itemId[itemIndex];
            if (setItemId == 0) {
                continue;
            }

            char fallbackName[256] = {};
            const char* memberName = GetItemSetMemberName(tooltip, setItemId, fallbackName, sizeof(fallbackName));
            if (!memberName || !memberName[0]) {
                continue;
            }

            char memberLine[512] = {};
            SStr::Printf(memberLine, sizeof(memberLine), const_cast<char*>("  %s"), memberName);
            AddSingleLine(
                tooltip,
                memberLine,
                reinterpret_cast<void*>(equippedMask[itemIndex] ? kColorYellow : kColorGrey0),
                0);
        }

        std::vector<size_t> bonusIndices;
        bonusIndices.reserve(std::size(itemSetRow.spellId));
        for (size_t i = 0; i < std::size(itemSetRow.spellId); ++i) {
            if (itemSetRow.spellId[i] != 0) {
                bonusIndices.push_back(i);
            }
        }

        std::stable_sort(
            bonusIndices.begin(),
            bonusIndices.end(),
            [&itemSetRow](size_t lhs, size_t rhs) {
                if (itemSetRow.spellThreshold[lhs] != itemSetRow.spellThreshold[rhs]) {
                    return itemSetRow.spellThreshold[lhs] < itemSetRow.spellThreshold[rhs];
                }
                return itemSetRow.spellId[lhs] < itemSetRow.spellId[rhs];
            });

        bool addedBonusLine = false;
        for (size_t index : bonusIndices) {
            uint32_t spellId = itemSetRow.spellId[index];
            if (spellId == 0) {
                continue;
            }

            SpellRow spellRow = {};
            if (!ClientDB::GetLocalizedRow(reinterpret_cast<void*>(kSpellDB), spellId, &spellRow)) {
                continue;
            }

            char parsedBonusText[1024] = {};
            const char* spellText = GetItemSetBonusText(&spellRow, parsedBonusText, sizeof(parsedBonusText));
            if (!spellText) {
                continue;
            }

            uint32_t threshold = itemSetRow.spellThreshold[index];
            bool isActive = equippedItemCount >= threshold;
            char* formatText = FrameScript::GetText(
                const_cast<char*>(isActive ? "ITEM_SET_BONUS" : "ITEM_SET_BONUS_GRAY"),
                -1,
                0);
            if (!formatText || !formatText[0]) {
                continue;
            }

            if (!addedBonusLine) {
                AddSpacerLine(tooltip);
            }

            char line[1024] = {};
            if (isActive) {
                SStr::Printf(line, sizeof(line), formatText, spellText);
                AddGreenLine(tooltip, line, 1);
            } else {
                SStr::Printf(line, sizeof(line), formatText, threshold, spellText);
                AddGreyLine(tooltip, line, 1);
            }

            addedBonusLine = true;
        }

        if (addedBonusLine) {
            AddSpacerLine(tooltip);
        }
    }

    uint32_t GetPlayerSkillRank(CGPlayer* activePlayer, uint32_t skillLineId) {
        if (!activePlayer || skillLineId == 0) {
            return 0;
        }

        constexpr ptrdiff_t kPlayerSkillInfoOffset = 0x1008;
        constexpr size_t kPlayerSkillEntryCount = 128;

        auto* skillInfo = reinterpret_cast<uint32_t*>(reinterpret_cast<char*>(activePlayer) + kPlayerSkillInfoOffset);
        if (!skillInfo) {
            return 0;
        }

        for (size_t i = 0; i < kPlayerSkillEntryCount; ++i) {
            uint32_t lineAndStep = skillInfo[i * 2];
            uint32_t rankAndMax = skillInfo[i * 2 + 1];
            uint32_t currentLineId = lineAndStep & 0xFFFFu;
            if (currentLineId != skillLineId) {
                continue;
            }

            return rankAndMax & 0xFFFFu;
        }

        return 0;
    }

    void AddEnchantmentRequirementLines(void* tooltip, const SpellItemEnchantmentRow* enchantRow, CGPlayer* activePlayer) {
        if (!tooltip || !enchantRow || !activePlayer) {
            return;
        }

        if (enchantRow->requiredSkillId > 0) {
            auto* skillLineRow = FindSkillLineRow(static_cast<uint32_t>(enchantRow->requiredSkillId));
            const char* skillName = (skillLineRow && skillLineRow->m_displayName_lang && skillLineRow->m_displayName_lang[0])
                ? skillLineRow->m_displayName_lang
                : "UNKNOWN";
            uint32_t playerSkillRank = GetPlayerSkillRank(activePlayer, static_cast<uint32_t>(enchantRow->requiredSkillId));

            if (enchantRow->requiredSkillRank > 0) {
                if (playerSkillRank < static_cast<uint32_t>(enchantRow->requiredSkillRank)) {
                    char* formatText = FrameScript::GetText(const_cast<char*>("ENCHANT_ITEM_MIN_SKILL"), -1, 0);
                    if (formatText && formatText[0]) {
                        char line[1024] = {};
                        SStr::Printf(line, sizeof(line), formatText, skillName, enchantRow->requiredSkillRank);
                        AddRedLine(tooltip, line);
                    }
                }
            } else if (playerSkillRank == 0) {
                char* formatText = FrameScript::GetText(const_cast<char*>("ENCHANT_ITEM_REQ_SKILL"), -1, 0);
                if (formatText && formatText[0]) {
                    char line[1024] = {};
                    SStr::Printf(line, sizeof(line), formatText, skillName);
                    AddRedLine(tooltip, line);
                }
            }
        }

        const uint32_t playerLevel = GetPlayerLevel(activePlayer);
        if (enchantRow->minLevel > 0 && playerLevel < static_cast<uint32_t>(enchantRow->minLevel)) {
            char* formatText = FrameScript::GetText(const_cast<char*>("ENCHANT_ITEM_REQ_LEVEL"), -1, 0);
            if (formatText && formatText[0]) {
                char line[512] = {};
                SStr::Printf(line, sizeof(line), formatText, enchantRow->minLevel);
                AddRedLine(tooltip, line);
            }
        }
    }

    bool PlayerMeetsEnchantmentRequirements(const SpellItemEnchantmentRow* enchantRow, CGPlayer* activePlayer) {
        if (!enchantRow || !activePlayer) {
            return false;
        }

        if (enchantRow->requiredSkillId > 0) {
            const uint32_t playerSkillRank = GetPlayerSkillRank(activePlayer, static_cast<uint32_t>(enchantRow->requiredSkillId));
            if (enchantRow->requiredSkillRank > 0) {
                if (playerSkillRank < static_cast<uint32_t>(enchantRow->requiredSkillRank)) {
                    return false;
                }
            } else if (playerSkillRank == 0) {
                return false;
            }
        }

        const uint32_t playerLevel = GetPlayerLevel(activePlayer);
        if (enchantRow->minLevel > 0 && playerLevel < static_cast<uint32_t>(enchantRow->minLevel)) {
            return false;
        }

        return true;
    }

    bool EnchantmentHasPrismaticSocketEffect(const SpellItemEnchantmentRow* enchantRow) {
        if (!enchantRow) {
            return false;
        }

        for (int effectIndex = 0; effectIndex < 3; ++effectIndex) {
            if (enchantRow->effect[effectIndex] == 8) {
                return true;
            }
        }

        return false;
    }

    size_t FindFirstReservedSocketIndex(const ItemCacheTooltipView* itemCache) {
        if (!itemCache) {
            return static_cast<size_t>(-1);
        }

        for (size_t socketIndex = 0; socketIndex < std::size(itemCache->socketColor); ++socketIndex) {
            if (itemCache->socketColor[socketIndex] == 0) {
                return socketIndex;
            }
        }

        return static_cast<size_t>(-1);
    }

    void AddPrismaticSocketRequirementLines(void* tooltip, const SpellItemEnchantmentRow* enchantRow, CGPlayer* activePlayer) {
        if (!tooltip || !enchantRow || !activePlayer) {
            return;
        }

        if (enchantRow->requiredSkillId > 0) {
            auto* skillLineRow = FindSkillLineRow(static_cast<uint32_t>(enchantRow->requiredSkillId));
            const char* skillName =
                (skillLineRow && skillLineRow->m_displayName_lang && skillLineRow->m_displayName_lang[0])
                    ? skillLineRow->m_displayName_lang
                    : "UNKNOWN";
            const uint32_t playerSkillRank = GetPlayerSkillRank(activePlayer, static_cast<uint32_t>(enchantRow->requiredSkillId));

            if (enchantRow->requiredSkillRank > 0) {
                char* formatText = FrameScript::GetText(const_cast<char*>("SOCKET_ITEM_MIN_SKILL"), -1, 0);
                if (formatText && formatText[0] &&
                    playerSkillRank < static_cast<uint32_t>(enchantRow->requiredSkillRank)) {
                    char line[1024] = {};
                    SStr::Printf(line, sizeof(line), formatText, skillName, enchantRow->requiredSkillRank);
                    AddRedLine(tooltip, line);
                }
            } else {
                char* formatText = FrameScript::GetText(const_cast<char*>("SOCKET_ITEM_REQ_SKILL"), -1, 0);
                if (formatText && formatText[0] && playerSkillRank == 0) {
                    char line[1024] = {};
                    SStr::Printf(line, sizeof(line), formatText, skillName);
                    AddRedLine(tooltip, line);
                }
            }
        }

        const uint32_t playerLevel = GetPlayerLevel(activePlayer);
        if (enchantRow->minLevel > 0) {
            char* formatText = FrameScript::GetText(const_cast<char*>("SOCKET_ITEM_REQ_LEVEL"), -1, 0);
            if (formatText && formatText[0] && playerLevel < static_cast<uint32_t>(enchantRow->minLevel)) {
                char line[512] = {};
                SStr::Printf(line, sizeof(line), formatText, enchantRow->minLevel);
                AddRedLine(tooltip, line);
            }
        }
    }

    bool IsSocketEnchantmentSlot(int slotIndex) {
        return slotIndex >= 2 && slotIndex <= 4;
    }

    void AddAppliedEnchantmentLines(
        void* tooltip,
        const ItemCacheTooltipView* itemCache,
        void* itemObject,
        CGPlayer* activePlayer)
    {
        if (!tooltip || !itemCache || !itemObject || !activePlayer) {
            return;
        }

        for (int slotIndex = 0; slotIndex < 12; ++slotIndex) {
            if (IsSocketEnchantmentSlot(slotIndex)) {
                continue;
            }

            const int32_t enchantmentId = GetItemEnchantmentIdBySlot(itemObject, slotIndex);
            if (enchantmentId == 0) {
                continue;
            }

            SpellItemEnchantmentRow* enchantRow = FindSpellItemEnchantmentRowSigned(enchantmentId);
            if (!enchantRow) {
                continue;
            }

            if (itemCache->socketBonusEnchantId != 0 &&
                static_cast<uint32_t>(enchantmentId < 0 ? -enchantmentId : enchantmentId) == itemCache->socketBonusEnchantId) {
                continue;
            }

            if (slotIndex == 6 && EnchantmentHasPrismaticSocketEffect(enchantRow)) {
                const bool requirementsMet = PlayerMeetsEnchantmentRequirements(enchantRow, activePlayer);
                const size_t reservedSocketIndex = FindFirstReservedSocketIndex(itemCache);
                if (reservedSocketIndex != static_cast<size_t>(-1)) {
                    const int32_t gemEnchantmentId = GetSocketEnchantmentId(itemObject, reservedSocketIndex);
                    auto* gemEnchantRow = FindSpellItemEnchantmentRowSigned(gemEnchantmentId);
                    ItemCacheTooltipView* gemItemCache = GetGemItemCacheByEnchantmentId(tooltip, gemEnchantmentId);
                    if (gemItemCache && gemEnchantRow) {
                        AddFilledSocketLine(
                            tooltip,
                            itemCache,
                            gemItemCache,
                            gemEnchantRow,
                            itemObject,
                            reservedSocketIndex);
                        AddEnchantmentRequirementLines(tooltip, gemEnchantRow, activePlayer);
                    } else {
                        AddPrismaticSocketLine(tooltip, requirementsMet);
                    }
                } else {
                    AddPrismaticSocketLine(tooltip, requirementsMet);
                }

                AddPrismaticSocketRequirementLines(tooltip, enchantRow, activePlayer);
                continue;
            }

            if (slotIndex == 6) {
                continue;
            }

            char line[1024] = {};
            const int scalarValue = GetSocketEnchantmentScalarForItem(itemObject, slotIndex);
            if (!FormatSocketEnchantmentText(
                    reinterpret_cast<int>(enchantRow),
                    line,
                    static_cast<unsigned int>(sizeof(line)),
                    scalarValue) ||
                !line[0]) {
                if (!enchantRow->name || !enchantRow->name[0]) {
                    continue;
                }

                SStr::Copy(line, enchantRow->name, sizeof(line));
            }

            int16_t enchantCharges = 0;
            uint8_t stateFlags = 0;
            TryGetItemEnchantmentStateData(itemObject, slotIndex, &enchantCharges, &stateFlags);

            char finalLine[1200] = {};
            SStr::Copy(finalLine, line, sizeof(finalLine));
            if (enchantCharges > 0) {
                char* chargesFormat = FrameScript::GetText(const_cast<char*>("ITEM_SPELL_CHARGES"), -1, 0);
                if (chargesFormat && chargesFormat[0]) {
                    char chargesText[128] = {};
                    SStr::Printf(chargesText, sizeof(chargesText), chargesFormat, enchantCharges);
                    char packedLine[1200] = {};
                    SStr::Printf(packedLine, sizeof(packedLine), const_cast<char*>("%s (%s)"), finalLine, chargesText);
                    SStr::Copy(finalLine, packedLine, sizeof(finalLine));
                }
            }

            uint32_t lineColor = kColorWhite;
            if ((stateFlags & 0x0F) != 0) {
                lineColor = kColorGrey0;
            } else if (slotIndex < 2 || (slotIndex > 4 && slotIndex < 7)) {
                lineColor = enchantmentId > 0 ? kColorGreen0 : kColorRed1;
            }

            const int32_t remainingTimeMs = CGItem_C__GetEnchantmentTimeLeft(itemObject, slotIndex);
            if (remainingTimeMs > 0) {
                char durationLine[1200] = {};
                CGTooltip::GetDurationString(
                    durationLine,
                    sizeof(durationLine),
                    static_cast<uint64_t>(remainingTimeMs),
                    const_cast<char*>("ITEM_ENCHANT_TIME_LEFT"),
                    reinterpret_cast<uint32_t>(finalLine),
                    1,
                    0);
                if (durationLine[0]) {
                    SStr::Copy(finalLine, durationLine, sizeof(finalLine));
                }
            }

            char prefixedLine[1280] = {};
            SStr::Printf(prefixedLine, sizeof(prefixedLine), const_cast<char*>("Enchanted: %s"), finalLine);
            AddSingleLine(tooltip, prefixedLine, reinterpret_cast<void*>(lineColor), 0);
            AddEnchantmentRequirementLines(tooltip, enchantRow, activePlayer);

            if (slotIndex >= 7) {
                char conditionText[1024] = {};
                BuildEnchantmentConditionText(
                    reinterpret_cast<int>(activePlayer),
                    reinterpret_cast<int>(enchantRow),
                    reinterpret_cast<unsigned char*>(conditionText),
                    sizeof(conditionText),
                    0);
                if (conditionText[0]) {
                    AddWhiteLine(tooltip, conditionText, 1);
                }
            }
        }
    }

    void AddRandomEnchantLine(void* tooltip, const ItemCacheTooltipView* itemCache) {
        if (!tooltip || !itemCache) {
            return;
        }

        if (*reinterpret_cast<int32_t*>(static_cast<char*>(tooltip) + kTooltipRandomPropertyIdOffset) != 0) {
            return;
        }

        if (itemCache->randomPropertyId == 0 && itemCache->randomSuffixId == 0) {
            return;
        }

        char* randomEnchantText = FrameScript::GetText(const_cast<char*>("ITEM_RANDOM_ENCHANT"), -1, 0);
        if (!randomEnchantText || !randomEnchantText[0]) {
            return;
        }

        AddGreenLine(tooltip, randomEnchantText);
    }

    void AddProposedEnchantLines(void* tooltip) {
        if (!tooltip) {
            return;
        }

        uint32_t proposedEnchantState = *reinterpret_cast<uint32_t*>(static_cast<char*>(tooltip) + 0x4C0);
        uint32_t proposedSpellId = *reinterpret_cast<uint32_t*>(static_cast<char*>(tooltip) + 0x480);
        if (proposedEnchantState == 0 || proposedSpellId == 0) {
            return;
        }

        SpellRow spellRow = {};
        if (!ClientDB::GetLocalizedRow(reinterpret_cast<void*>(kSpellDB), proposedSpellId, &spellRow) ||
            !spellRow.m_name_lang || !spellRow.m_name_lang[0]) {
            return;
        }

        char* proposedFormat = FrameScript::GetText(const_cast<char*>("ITEM_PROPOSED_ENCHANT"), -1, 0);
        if (proposedFormat && proposedFormat[0]) {
            char line[1024] = {};
            SStr::Printf(line, sizeof(line), proposedFormat, spellRow.m_name_lang);
            AddGreenLine(tooltip, line);
        }

        char* disclaimerText = FrameScript::GetText(const_cast<char*>("ITEM_ENCHANT_DISCLAIMER"), -1, 0);
        if (disclaimerText && disclaimerText[0]) {
            AddRedLine(tooltip, disclaimerText);
        }
    }

    void AddSocketRequirementLines(void* tooltip, const ItemCacheTooltipView* itemCache, CGPlayer* activePlayer) {
        if (!tooltip || !itemCache || itemCache->socketBonusEnchantId == 0 || !activePlayer) {
            return;
        }

        auto* enchantRow = FindSpellItemEnchantmentRowSigned(static_cast<int32_t>(itemCache->socketBonusEnchantId));
        if (!enchantRow) {
            return;
        }

        if (enchantRow->requiredSkillId > 0) {
            auto* skillLineRow = FindSkillLineRow(static_cast<uint32_t>(enchantRow->requiredSkillId));
            char* skillName = (skillLineRow && skillLineRow->m_displayName_lang && skillLineRow->m_displayName_lang[0])
                ? skillLineRow->m_displayName_lang
                : nullptr;

            if (skillName && skillName[0]) {
                uint32_t playerSkillRank = GetPlayerSkillRank(activePlayer, static_cast<uint32_t>(enchantRow->requiredSkillId));
                bool hasRequiredSkill = playerSkillRank > 0;
                uint32_t requiredSkillRank =
                    enchantRow->requiredSkillRank > 0 ? static_cast<uint32_t>(enchantRow->requiredSkillRank) : 0u;
                bool meetsSkillRank = playerSkillRank >= requiredSkillRank;

                if (enchantRow->requiredSkillRank > 0) {
                    char* formatText = FrameScript::GetText(const_cast<char*>("SOCKET_ITEM_MIN_SKILL"), -1, 0);
                    if (formatText && formatText[0]) {
                        char line[1024] = {};
                        SStr::Printf(line, sizeof(line), formatText, skillName, enchantRow->requiredSkillRank);
                        AddSingleLine(
                            tooltip,
                            line,
                            reinterpret_cast<void*>(meetsSkillRank ? kColorGrey0 : kColorRed0),
                            0);
                    }
                } else {
                    char* formatText = FrameScript::GetText(const_cast<char*>("SOCKET_ITEM_REQ_SKILL"), -1, 0);
                    if (formatText && formatText[0]) {
                        char line[1024] = {};
                        SStr::Printf(line, sizeof(line), formatText, skillName);
                        AddSingleLine(
                            tooltip,
                            line,
                            reinterpret_cast<void*>(hasRequiredSkill ? kColorGrey0 : kColorRed0),
                            0);
                    }
                }
            }
        }

        const uint32_t playerLevel = GetPlayerLevel(activePlayer);
        if (enchantRow->minLevel > 0 && playerLevel != 0) {
            char* formatText = FrameScript::GetText(const_cast<char*>("SOCKET_ITEM_REQ_LEVEL"), -1, 0);
            if (formatText && formatText[0]) {
                char line[512] = {};
                SStr::Printf(line, sizeof(line), formatText, enchantRow->minLevel);
                AddSingleLine(
                    tooltip,
                    line,
                    reinterpret_cast<void*>(
                        playerLevel >= static_cast<uint32_t>(enchantRow->minLevel)
                            ? kColorGrey0
                            : kColorRed0),
                    0);
            }
        }
    }

    void AddGemPropertiesLines(void* tooltip, const ItemCacheTooltipView* itemCache, CGPlayer* activePlayer, int arg10) {
        if (!tooltip || !itemCache || !activePlayer || arg10 != 0 || itemCache->gemPropertiesId == 0) {
            return;
        }

        auto* gemPropertiesRow = FindGemPropertiesRowById(itemCache->gemPropertiesId);
        if (!gemPropertiesRow || gemPropertiesRow->enchantId == 0) {
            LOG_DEBUG << "ItemTooltip gem: missing gemProperties row for id=" << itemCache->gemPropertiesId;
            return;
        }

        auto* enchantRow = FindSpellItemEnchantmentRowSigned(static_cast<int32_t>(gemPropertiesRow->enchantId));
        if (!enchantRow) {
            LOG_DEBUG << "ItemTooltip gem: missing enchant row for enchantId=" << gemPropertiesRow->enchantId;
            return;
        }

        bool skillRequirementMet = true;
        if (itemCache->requiredSkillId > 0) {
            uint32_t playerSkillRank = GetPlayerSkillRank(activePlayer, itemCache->requiredSkillId);
            const uint32_t requiredSkillRank = itemCache->requiredSkillRank;
            skillRequirementMet = requiredSkillRank == 0 ? playerSkillRank > 0 : playerSkillRank >= requiredSkillRank;
        }

        if (enchantRow->name && enchantRow->name[0]) {
            AddSingleLine(
                tooltip,
                enchantRow->name,
                reinterpret_cast<void*>(skillRequirementMet ? kColorWhite : kColorRed0),
                0);
        }

        char description[1024] = {};
        BuildEnchantmentConditionText(
            reinterpret_cast<int>(activePlayer),
            reinterpret_cast<int>(enchantRow),
            reinterpret_cast<unsigned char*>(description),
            sizeof(description),
            0);
        if (description[0]) {
            AddWhiteLine(tooltip, description, 1);
        } else {
            LOG_DEBUG << "ItemTooltip gem: empty effect text for gemPropertiesId=" << itemCache->gemPropertiesId
                      << " enchantId=" << gemPropertiesRow->enchantId;
        }
    }

    void AddItemDescriptionLine(void* tooltip, const ItemCacheTooltipView* itemCache, int arg5) {
        if (!tooltip || !itemCache || arg5 != 0 || itemCache->descriptionPtr == 0) {
            return;
        }

        for (size_t index = 0; index < std::size(itemCache->spellId); ++index) {
            if (itemCache->spellId[index] != 0 && itemCache->spellTrigger[index] == 6) {
                return;
            }
        }

        char description[1024] = {};
        if (!SafeCopySpellTextField(reinterpret_cast<const char*>(itemCache->descriptionPtr), description, sizeof(description))) {
            return;
        }

        if (!IsMostlyPrintableText(description)) {
            return;
        }

        char quotedDescription[1100] = {};
        SStr::Printf(quotedDescription, sizeof(quotedDescription), const_cast<char*>("\"%s\""), description);
        AddDarkYellowLine(tooltip, quotedDescription, 1);
    }

    void AddRequiredReputationLine(void* tooltip, const ItemCacheTooltipView* itemCache, CGPlayer* activePlayer, int arg10) {
        if (!tooltip || !itemCache || !activePlayer || arg10 != 0 || itemCache->unk50 == 0) {
            return;
        }

        static const int32_t kReputationThresholds[] = {
            -42000, -6000, -3000, 0, 3000, 9000, 21000, 42000
        };

        const uint32_t requiredFactionId = itemCache->unk50;
        const uint32_t requiredStanding = itemCache->unk54;
        if (requiredStanding >= std::size(kReputationThresholds)) {
            return;
        }

        auto* factionDb = reinterpret_cast<WoWClientDB*>(kFactionDB);
        if (!factionDb || !factionDb->Rows) {
            return;
        }

        FactionRec* factionRow = reinterpret_cast<FactionRec*>(
            ClientDB::GetRow(reinterpret_cast<void*>(kFactionDB), requiredFactionId));
        if (!factionRow &&
            static_cast<int32_t>(requiredFactionId) >= factionDb->minIndex &&
            static_cast<int32_t>(requiredFactionId) <= factionDb->maxIndex) {
            auto** recordsById = reinterpret_cast<FactionRec**>(factionDb->Rows);
            factionRow = recordsById[requiredFactionId - static_cast<uint32_t>(factionDb->minIndex)];
        }

        const char* factionName = (factionRow && factionRow->m_name && factionRow->m_name[0])
            ? factionRow->m_name
            : "UNKNOWN";

        char standingKey[64] = {};
        SStr::Printf(
            standingKey,
            sizeof(standingKey),
            const_cast<char*>("FACTION_STANDING_LABEL%d"),
            requiredStanding + 1);
        const char* standingText = FrameScript__GetLocalizedText(activePlayer, standingKey, -1);
        if (!standingText || !standingText[0]) {
            return;
        }

        char* formatText = FrameScript::GetText(const_cast<char*>("ITEM_REQ_REPUTATION"), -1, 0);
        if (!formatText || !formatText[0]) {
            return;
        }

        char line[1024] = {};
        SStr::Printf(line, sizeof(line), formatText, factionName, standingText);

        const int32_t playerRepValue = GetRepListRepValue(requiredFactionId);
        AddSingleLine(
            tooltip,
            line,
            reinterpret_cast<void*>(playerRepValue >= kReputationThresholds[requiredStanding] ? kColorWhite : kColorRed0),
            0);
    }

    void AddItemDurationLine(void* tooltip, const ItemCacheTooltipView* itemCache, int arg10) {
        if (!tooltip || !itemCache || arg10 != 0 || itemCache->duration <= 0) {
            return;
        }

        char line[1024] = {};
        CGTooltip::GetDurationString(
            line,
            sizeof(line),
            static_cast<uint64_t>(static_cast<uint32_t>(itemCache->duration)),
            const_cast<char*>("ITEM_DURATION"),
            0,
            1,
            1);
        if (line[0]) {
            AddWhiteLine(tooltip, line);
        }
    }

    void AddHolidayLine(void* tooltip, const ItemCacheTooltipView* itemCache, int arg10) {
        if (!tooltip || !itemCache || arg10 != 0 || itemCache->holidayId == 0) {
            return;
        }

        char* holidayName = CGCalendar__GetHolidayName(itemCache->holidayId);
        if (!holidayName || !holidayName[0]) {
            return;
        }

        AddWhiteLine(tooltip, holidayName);
    }

    void AppendRestrictionName(char* buffer, size_t bufferSize, const char* text) {
        if (!buffer || bufferSize == 0 || !text || !text[0]) {
            return;
        }

        if (!buffer[0]) {
            SStr::Copy(buffer, const_cast<char*>(text), bufferSize);
            return;
        }

        char combined[1024] = {};
        SStr::Printf(combined, sizeof(combined), const_cast<char*>("%s, %s"), buffer, text);
        SStr::Copy(buffer, combined, bufferSize);
    }

    bool HasMultipleRestrictionBits(uint32_t value) {
        return ((value - 1u) & value) != 0;
    }

    constexpr size_t kChrClassesRecordStride = 48;

    uint32_t* GetChrClassesRawRowByOrdinal(uint32_t ordinal) {
        auto* classesDb = reinterpret_cast<WoWClientDB*>(kChrClassesDB);
        if (!classesDb || !classesDb->FirstRow || ordinal >= static_cast<uint32_t>(classesDb->numRows)) {
            return nullptr;
        }

        auto* firstRowBytes = reinterpret_cast<unsigned char*>(classesDb->FirstRow);
        return reinterpret_cast<uint32_t*>(firstRowBytes + ordinal * kChrClassesRecordStride);
    }

    bool IsRestrictedRaceMask(const ItemCacheTooltipView* itemCache) {
        if (!itemCache) {
            return false;
        }

        auto* racesDb = reinterpret_cast<WoWClientDB*>(kChrRacesDB);
        if (!racesDb) {
            return false;
        }

        for (uint32_t raceId = static_cast<uint32_t>(racesDb->minIndex);
             raceId <= static_cast<uint32_t>(racesDb->maxIndex);
             ++raceId) {
            auto* row = reinterpret_cast<ChrRacesRow*>(ClientDB::GetRow(reinterpret_cast<void*>(kChrRacesDB), raceId));
            if (!row || !row->m_ID || !row->m_name_lang || !row->m_name_lang[0] || (row->m_flags & 1u) != 0) {
                continue;
            }

            const uint32_t bit = 1u << (row->m_ID - 1);
            if ((itemCache->allowableRaceMask & bit) == 0) {
                return true;
            }
        }

        return false;
    }

    bool IsRestrictedClassMask(const ItemCacheTooltipView* itemCache) {
        if (!itemCache) {
            return false;
        }

        auto* classesDb = reinterpret_cast<WoWClientDB*>(kChrClassesDB);
        if (!classesDb || !classesDb->FirstRow || classesDb->numRows <= 0) {
            return false;
        }

        for (uint32_t ordinal = 0; ordinal < static_cast<uint32_t>(classesDb->numRows); ++ordinal) {
            auto* row = GetChrClassesRawRowByOrdinal(ordinal);
            if (!row) {
                continue;
            }

            const uint32_t rowId = row[0];
            const char* rowName = reinterpret_cast<const char*>(row[4]);
            if (!rowId || !rowName || !rowName[0]) {
                continue;
            }

            const uint32_t bit = 1u << (rowId - 1);
            if ((itemCache->allowableClassMask & bit) == 0) {
                return true;
            }
        }

        return false;
    }

    uint32_t BuildAllowedRaceNames(char* buffer, size_t bufferSize, const ItemCacheTooltipView* itemCache, CGPlayer* activePlayer, bool* matchesPlayer) {
        if (!buffer || !itemCache) {
            return 0;
        }

        buffer[0] = 0;
        uint32_t count = 0;
        bool anyMatch = false;
        auto* racesDb = reinterpret_cast<WoWClientDB*>(kChrRacesDB);
        if (!racesDb) {
            return 0;
        }

        for (uint32_t raceId = static_cast<uint32_t>(racesDb->minIndex);
             raceId <= static_cast<uint32_t>(racesDb->maxIndex);
             ++raceId) {
            auto* row = reinterpret_cast<ChrRacesRow*>(ClientDB::GetRow(reinterpret_cast<void*>(kChrRacesDB), raceId));
            if (!row || !row->m_ID || !row->m_name_lang || !row->m_name_lang[0] || (row->m_flags & 1u) != 0) {
                continue;
            }

            uint32_t bit = 1u << (row->m_ID - 1);
            if ((itemCache->allowableRaceMask & bit) == 0) {
                continue;
            }

            AppendRestrictionName(buffer, bufferSize, row->m_name_lang);
            ++count;
            if (activePlayer && activePlayer->unitBase.unitData &&
                activePlayer->unitBase.unitData->unitBytes0.raceID == row->m_ID) {
                anyMatch = true;
            }
        }

        if (matchesPlayer) {
            *matchesPlayer = count == 0 || !activePlayer || !activePlayer->unitBase.unitData ? true : anyMatch;
        }

        return count;
    }

    uint32_t BuildAllowedClassNames(char* buffer, size_t bufferSize, const ItemCacheTooltipView* itemCache, CGPlayer* activePlayer, bool* matchesPlayer) {
        if (!buffer || !itemCache) {
            return 0;
        }

        buffer[0] = 0;
        uint32_t count = 0;
        bool anyMatch = false;
        auto* classesDb = reinterpret_cast<WoWClientDB*>(kChrClassesDB);
        if (!classesDb || !classesDb->FirstRow || classesDb->numRows <= 0) {
            return 0;
        }

        for (uint32_t ordinal = 0; ordinal < static_cast<uint32_t>(classesDb->numRows); ++ordinal) {
            auto* row = GetChrClassesRawRowByOrdinal(ordinal);
            if (!row) {
                continue;
            }

            const uint32_t rowId = row[0];
            const char* rowName = reinterpret_cast<const char*>(row[4]);
            if (!rowId || !rowName || !rowName[0]) {
                continue;
            }

            uint32_t bit = 1u << (rowId - 1);
            if ((itemCache->allowableClassMask & bit) == 0) {
                continue;
            }

            AppendRestrictionName(buffer, bufferSize, rowName);
            ++count;
            if (activePlayer && activePlayer->unitBase.unitData &&
                activePlayer->unitBase.unitData->unitBytes0.classID == rowId) {
                anyMatch = true;
            }
        }

        if (matchesPlayer) {
            *matchesPlayer = count == 0 || !activePlayer || !activePlayer->unitBase.unitData ? true : anyMatch;
        }

        return count;
    }

    void AddRaceClassRestrictionLine(void* tooltip, const ItemCacheTooltipView* itemCache, CGPlayer* activePlayer, int arg10) {
        if (!tooltip || !itemCache || !activePlayer || arg10 != 0) {
            return;
        }

        if (HasMultipleRestrictionBits(itemCache->allowableRaceMask) ||
            HasMultipleRestrictionBits(itemCache->allowableClassMask)) {
            return;
        }

        char combined[1024] = {};
        bool matchesPlayer = true;
        auto* racesDb = reinterpret_cast<WoWClientDB*>(kChrRacesDB);
        if (racesDb) {
            for (uint32_t raceId = static_cast<uint32_t>(racesDb->minIndex);
                 raceId <= static_cast<uint32_t>(racesDb->maxIndex);
                 ++raceId) {
                auto* row = reinterpret_cast<ChrRacesRow*>(ClientDB::GetRow(reinterpret_cast<void*>(kChrRacesDB), raceId));
                if (!row || !row->m_ID || !row->m_name_lang || !row->m_name_lang[0] || (row->m_flags & 1u) != 0) {
                    continue;
                }

                const uint32_t bit = 1u << (row->m_ID - 1);
                if ((itemCache->allowableRaceMask & bit) == 0) {
                    continue;
                }

                SStr::Printf(combined, sizeof(combined), const_cast<char*>("%s "), row->m_name_lang);
                if (activePlayer->unitBase.unitData->unitBytes0.raceID != row->m_ID) {
                    matchesPlayer = false;
                }
                break;
            }
        }

        auto* classesDb = reinterpret_cast<WoWClientDB*>(kChrClassesDB);
        if (classesDb && classesDb->FirstRow && classesDb->numRows > 0) {
            for (uint32_t ordinal = 0; ordinal < static_cast<uint32_t>(classesDb->numRows); ++ordinal) {
                auto* row = GetChrClassesRawRowByOrdinal(ordinal);
                if (!row) {
                    continue;
                }

                const uint32_t rowId = row[0];
                const char* rowName = reinterpret_cast<const char*>(row[4]);
                if (!rowId || !rowName || !rowName[0]) {
                    continue;
                }

                const uint32_t bit = 1u << (rowId - 1);
                if ((itemCache->allowableClassMask & bit) == 0) {
                    continue;
                }

                SStr::Append(combined, const_cast<char*>(rowName), static_cast<uint32_t>(sizeof(combined)));
                if (activePlayer->unitBase.unitData->unitBytes0.classID != rowId) {
                    matchesPlayer = false;
                }
                break;
            }
        }

        if (!combined[0]) {
            return;
        }

        char* formatText = FrameScript::GetText(const_cast<char*>("RACE_CLASS_ONLY"), -1, 0);
        if (!formatText || !formatText[0]) {
            return;
        }

        char line[1024] = {};
        SStr::Printf(line, sizeof(line), formatText, combined);
        AddSingleLine(
            tooltip,
            line,
            reinterpret_cast<void*>(matchesPlayer ? kColorWhite : kColorRed0),
            0);
    }

    void AddAllowedRaceLine(void* tooltip, const ItemCacheTooltipView* itemCache, CGPlayer* activePlayer, int arg10) {
        if (!tooltip || !itemCache || !activePlayer || arg10 != 0) {
            return;
        }

        if (!HasMultipleRestrictionBits(itemCache->allowableRaceMask) &&
            !HasMultipleRestrictionBits(itemCache->allowableClassMask)) {
            return;
        }

        if (!IsRestrictedRaceMask(itemCache)) {
            return;
        }

        char raceNames[512] = {};
        bool matchesPlayer = true;
        uint32_t count = BuildAllowedRaceNames(raceNames, sizeof(raceNames), itemCache, activePlayer, &matchesPlayer);
        if (count == 0 || !raceNames[0]) {
            return;
        }

        char* formatText = FrameScript::GetText(const_cast<char*>("ITEM_RACES_ALLOWED"), -1, 0);
        if (!formatText || !formatText[0]) {
            return;
        }

        char line[1024] = {};
        SStr::Printf(line, sizeof(line), formatText, raceNames);
        AddSingleLine(
            tooltip,
            line,
            reinterpret_cast<void*>(matchesPlayer ? kColorWhite : kColorRed0),
            0);
    }

    void AddAllowedClassLine(void* tooltip, const ItemCacheTooltipView* itemCache, CGPlayer* activePlayer, int arg10) {
        if (!tooltip || !itemCache || !activePlayer || arg10 != 0) {
            return;
        }

        if (!HasMultipleRestrictionBits(itemCache->allowableRaceMask) &&
            !HasMultipleRestrictionBits(itemCache->allowableClassMask)) {
            return;
        }

        if (!IsRestrictedClassMask(itemCache)) {
            return;
        }

        char classNames[512] = {};
        bool matchesPlayer = true;
        uint32_t count = BuildAllowedClassNames(classNames, sizeof(classNames), itemCache, activePlayer, &matchesPlayer);
        if (count == 0 || !classNames[0]) {
            return;
        }

        char* formatText = FrameScript::GetText(const_cast<char*>("ITEM_CLASSES_ALLOWED"), -1, 0);
        if (!formatText || !formatText[0]) {
            return;
        }

        char line[1024] = {};
        SStr::Printf(line, sizeof(line), formatText, classNames);
        AddSingleLine(
            tooltip,
            line,
            reinterpret_cast<void*>(matchesPlayer ? kColorWhite : kColorRed0),
            0);
    }

    void AddRequiredLevelLine(void* tooltip, const ItemCacheTooltipView* itemCache, CGPlayer* activePlayer, int arg10) {
        if (!tooltip || !itemCache || !activePlayer || arg10 != 0 || itemCache->requiredLevel <= 1) {
            return;
        }

        char* formatText = FrameScript::GetText(const_cast<char*>("ITEM_MIN_LEVEL"), -1, 0);
        if (!formatText || !formatText[0]) {
            return;
        }

        char line[512] = {};
        SStr::Printf(line, sizeof(line), formatText, itemCache->requiredLevel);
        AddSingleLine(
            tooltip,
            line,
            reinterpret_cast<void*>(
                GetPlayerLevel(activePlayer) >= itemCache->requiredLevel
                    ? kColorWhite
                    : kColorRed0),
            0);
    }

    void AddItemLevelLine(void* tooltip, const ItemCacheTooltipView* itemCache, int arg10) {
        if (!tooltip || !itemCache || arg10 != 0) {
            return;
        }

        CVar* showItemLevel = CVar__Lookup(const_cast<char*>("showItemLevel"));
        if (!showItemLevel || *reinterpret_cast<int*>(reinterpret_cast<char*>(showItemLevel) + 0x30) == 0) {
            return;
        }

        switch (itemCache->itemClass) {
        case 2:
        case 4:
        case 5:
        case 6:
            break;
        default:
            return;
        }

        if (itemCache->itemLevel == 0) {
            return;
        }

        char* formatText = FrameScript::GetText(const_cast<char*>("ITEM_LEVEL"), -1, 0);
        if (!formatText || !formatText[0]) {
            return;
        }

        char line[512] = {};
        SStr::Printf(line, sizeof(line), formatText, itemCache->itemLevel);
        AddDarkYellowLine(tooltip, line);
    }

    void AddHeaderDescriptorLine(void* tooltip, const ItemCacheTooltipView* itemCache, int arg5) {
        if (!tooltip || !itemCache) {
            return;
        }

        if ((itemCache->flagsAndFaction0 & 0x8u) == 0) {
            return;
        }

        char* descriptorText = nullptr;
        if (IsColorblindModeEnabled()) {
            descriptorText = FrameScript::GetText(const_cast<char*>("ITEM_HEROIC_EPIC"), -1, 0);
            if (descriptorText && descriptorText[0]) {
                AddWhiteLine(tooltip, descriptorText);
            }
            return;
        }

        descriptorText = FrameScript::GetText(const_cast<char*>("ITEM_HEROIC"), -1, 0);
        if (descriptorText && descriptorText[0]) {
            AddGreenLine(tooltip, descriptorText);
        }
    }

    void AddRequiredSkillLine(void* tooltip, const ItemCacheTooltipView* itemCache, CGPlayer* activePlayer, int arg10) {
        if (!tooltip || !itemCache || !activePlayer || arg10 != 0 || itemCache->requiredSkillId == 0) {
            return;
        }

        auto* skillLineRow = FindSkillLineRow(itemCache->requiredSkillId);
        const char* skillName =
            (skillLineRow && skillLineRow->m_displayName_lang && skillLineRow->m_displayName_lang[0])
                ? skillLineRow->m_displayName_lang
                : "UNKNOWN";
        uint32_t playerSkillRank = GetPlayerSkillRank(activePlayer, itemCache->requiredSkillId);
        bool meetsRequirement =
            itemCache->requiredSkillRank == 0 ? playerSkillRank > 0 : playerSkillRank >= itemCache->requiredSkillRank;

        if ((itemCache->flagsAndFaction0 & 0x00040000u) != 0 && playerSkillRank > 0) {
            char* text = FrameScript::GetText(const_cast<char*>("ITEM_PROSPECTABLE"), -1, 0);
            AddSingleLine(
                tooltip,
                text,
                reinterpret_cast<void*>(meetsRequirement ? kColorWhite : kColorRed0),
                0);
            return;
        }

        if ((itemCache->flagsAndFaction0 & 0x20000000u) != 0 && playerSkillRank > 0) {
            char* text = FrameScript::GetText(const_cast<char*>("ITEM_MILLABLE"), -1, 0);
            AddSingleLine(
                tooltip,
                text,
                reinterpret_cast<void*>(meetsRequirement ? kColorWhite : kColorRed0),
                0);
            return;
        }

        char* formatText = FrameScript::GetText(
            const_cast<char*>(itemCache->requiredSkillRank ? "ITEM_MIN_SKILL" : "ITEM_REQ_SKILL"),
            -1,
            0);
        if (!formatText || !formatText[0]) {
            return;
        }

        char line[1024] = {};
        if (itemCache->requiredSkillRank) {
            SStr::Printf(line, sizeof(line), formatText, skillName, itemCache->requiredSkillRank);
        } else {
            SStr::Printf(line, sizeof(line), formatText, skillName);
        }

        AddSingleLine(
            tooltip,
            line,
            reinterpret_cast<void*>(meetsRequirement ? kColorWhite : kColorRed0),
            0);
    }

    bool UnitKnowsSpell(void* unit, uint32_t spellId) {
        if (!unit || spellId == 0) {
            return false;
        }

        return CGUnit_C__IsSpellKnown(unit, spellId) || UnitMeetsSpellKnowledgeRequirement(unit, spellId);
    }

    uint32_t ResolveLearnedSpellFromItemSpell(const ItemCacheTooltipView* itemCache, const SpellRow* itemSpellRow) {
        if (!itemCache || !itemSpellRow) {
            return 0;
        }

        if (itemSpellRow->m_effect[0] != 36) {
            return 0;
        }

        if (itemSpellRow->m_effectTriggerSpell[0] != 0) {
            return static_cast<uint32_t>(itemSpellRow->m_effectTriggerSpell[0]);
        }

        for (size_t index = 0; index < std::size(itemCache->spellTrigger); ++index) {
            if (itemCache->spellTrigger[index] == 6 && itemCache->spellId[index] != 0) {
                return itemCache->spellId[index];
            }
        }

        return 0;
    }

    void* GetItemSpellRequirementUnit(CGPlayer* activePlayer, const SpellRow* itemSpellRow) {
        if (!activePlayer || !itemSpellRow) {
            return nullptr;
        }

        if (itemSpellRow->m_implicitTargetA[0] == 5 || itemSpellRow->m_implicitTargetB[0] == 5) {
            return reinterpret_cast<void*>(activePlayer);
        }

        return reinterpret_cast<void*>(activePlayer);
    }

    void AddItemSpellKnownLine(void* tooltip, const ItemCacheTooltipView* itemCache, CGPlayer* activePlayer) {
        if (!tooltip || !itemCache || !activePlayer || itemCache->spellId[0] == 0) {
            return;
        }

        SpellRow itemSpellRow = {};
        if (!ClientDB::GetLocalizedRow(reinterpret_cast<void*>(kSpellDB), itemCache->spellId[0], &itemSpellRow)) {
            return;
        }

        uint32_t learnedSpellId = ResolveLearnedSpellFromItemSpell(itemCache, &itemSpellRow);
        if (learnedSpellId == 0) {
            return;
        }

        void* unit = GetItemSpellRequirementUnit(activePlayer, &itemSpellRow);
        if (!UnitKnowsSpell(unit, learnedSpellId)) {
            return;
        }

        char* text = FrameScript::GetText(const_cast<char*>("ITEM_SPELL_KNOWN"), -1, 0);
        AddRedLine(tooltip, text);
    }

    void AddRequiredSpellLine(void* tooltip, const ItemCacheTooltipView* itemCache, CGPlayer* activePlayer) {
        if (!tooltip || !itemCache || !activePlayer || itemCache->requiredSpellId == 0) {
            return;
        }

        SpellRow spellRow = {};
        if (!ClientDB::GetLocalizedRow(reinterpret_cast<void*>(kSpellDB), itemCache->requiredSpellId, &spellRow) ||
            !spellRow.m_name_lang || !spellRow.m_name_lang[0]) {
            return;
        }

        char* formatText = FrameScript::GetText(const_cast<char*>("ITEM_REQ_SKILL"), -1, 0);
        if (!formatText || !formatText[0]) {
            return;
        }

        char line[1024] = {};
        SStr::Printf(line, sizeof(line), formatText, spellRow.m_name_lang);
        AddSingleLine(
            tooltip,
            line,
            reinterpret_cast<void*>(UnitKnowsSpell(reinterpret_cast<void*>(activePlayer), itemCache->requiredSpellId)
                ? kColorWhite
                : kColorRed0),
            0);
    }

    const char* GetPvPRankName(CGPlayer* activePlayer, uint32_t rank) {
        if (!activePlayer || rank == 0) {
            return nullptr;
        }

        char key[32] = {};
        SStr::Printf(key, sizeof(key), const_cast<char*>("PVP_RANK_%d_%d"), rank, CGPlayer_C__GetPVPFactionIndex());
        return FrameScript__GetLocalizedText(activePlayer, key, -1);
    }

    void AddRequiredPvPRankLine(void* tooltip, const ItemCacheTooltipView* itemCache, CGPlayer* activePlayer) {
        if (!tooltip || !itemCache || !activePlayer || itemCache->requiredPvPRank == 0) {
            return;
        }

        const char* rankName = GetPvPRankName(activePlayer, itemCache->requiredPvPRank);
        if (!rankName || !rankName[0]) {
            return;
        }

        char* formatText = FrameScript::GetText(const_cast<char*>("ITEM_REQ_SKILL"), -1, 0);
        if (!formatText || !formatText[0]) {
            return;
        }

        char line[1024] = {};
        SStr::Printf(line, sizeof(line), formatText, rankName);
        uint8_t playerRank = *reinterpret_cast<uint8_t*>(reinterpret_cast<char*>(activePlayer) + 0x1008 + 0x1067);
        AddSingleLine(
            tooltip,
            line,
            reinterpret_cast<void*>(playerRank >= itemCache->requiredPvPRank ? kColorWhite : kColorRed0),
            0);
    }

    bool BuildItemSpellParsedText(const SpellRow* spellRow, char* outText, size_t outTextSize) {
        if (!spellRow || !outText || outTextSize == 0) {
            return false;
        }

        outText[0] = '\0';
        if (!spellRow->m_description_lang || !spellRow->m_description_lang[0]) {
            return false;
        }

#ifdef _MSC_VER
        __try {
            SpellParser::ParseText(const_cast<SpellRow*>(spellRow), outText, static_cast<uint32_t>(outTextSize), 0, 0, 0, 0, 1, 0);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            LOG_ERROR << "ItemTooltip item-spell parse failed for spellId=" << spellRow->m_ID;
            outText[0] = '\0';
            return false;
        }
#else
        SpellParser::ParseText(const_cast<SpellRow*>(spellRow), outText, static_cast<uint32_t>(outTextSize), 0, 0, 0, 0, 1, 0);
#endif

        if (ContainsUnresolvedSpellTokens(outText)) {
            outText[0] = '\0';
        }

        return outText[0] != '\0';
    }

    bool IsItemSpellCreateItemEffect(const SpellRow* spellRow) {
        if (!spellRow) {
            return false;
        }

        auto isCreateItemLike = [](const SpellRow& row) -> bool {
            switch (row.m_effect[0]) {
            case 24:
            case 59:
            case 157:
                return row.m_effectItemType[0] != 0;
            default:
                return false;
            }
        };

        if (isCreateItemLike(*spellRow)) {
            return true;
        }

        if (spellRow->m_effect[0] == 36 && spellRow->m_effectTriggerSpell[0] != 0) {
            SpellRow triggeredSpellRow = {};
            if (ClientDB::GetLocalizedRow(
                    reinterpret_cast<void*>(kSpellDB),
                    spellRow->m_effectTriggerSpell[0],
                    &triggeredSpellRow) &&
                isCreateItemLike(triggeredSpellRow)) {
                return true;
            }
        }

        return false;
    }

    uint32_t GetFirstEmbeddedItemId(const SpellRow* spellRow) {
        if (!spellRow) {
            return 0;
        }

        for (size_t effectIndex = 0; effectIndex < std::size(spellRow->m_effectItemType); ++effectIndex) {
            if (spellRow->m_effectItemType[effectIndex] != 0) {
                return spellRow->m_effectItemType[effectIndex];
            }
        }

        return 0;
    }

    uint32_t GetSpellChargeRemainingCooldown(uint32_t spellId, uint32_t fallbackCooldown) {
        auto it = CharacterDefines::spellChargeMap.find(spellId);
        if (it == CharacterDefines::spellChargeMap.end()) {
            return fallbackCooldown;
        }

        CharacterDefines::SpellCharge& charge = it->second;
        uint32_t remainingCooldown = fallbackCooldown;

        if (charge.remainingCooldown >= fallbackCooldown) {
            uint32_t currAsync = OsGetAsyncTimeMs();
            if (charge.remainingCooldown > (currAsync - charge.async)) {
                remainingCooldown = charge.remainingCooldown + (charge.async - currAsync);
            } else {
                remainingCooldown = 0;
            }

            charge.remainingCooldown = remainingCooldown;
            charge.async = currAsync;
        }

        return remainingCooldown;
    }

    void AddItemSpellReagentLine(void* tooltip, SpellRow* spellRow, CGPlayer* activePlayer) {
        if (!tooltip || !spellRow || !activePlayer) {
            return;
        }

        if (Player_CanCastSpellInCurrentForm(reinterpret_cast<int>(activePlayer), reinterpret_cast<int>(spellRow))) {
            return;
        }

        char reagentLine[4096] = {};
        bool firstReagent = true;
        bool allMet = true;

        for (size_t reagentIndex = 0; reagentIndex < std::size(spellRow->m_reagent); ++reagentIndex) {
            uint32_t reagentId = spellRow->m_reagent[reagentIndex];
            if (reagentId == 0) {
                continue;
            }

            uint64_t guid = static_cast<uint64_t>(spellRow->m_ID) | 0x1FE0000000000000ULL;
            void* itemBlock = DBItemCache_GetInfoBlockByID(
                reinterpret_cast<void*>(kWdbCacheItem),
                reagentId,
                &guid,
                nullptr,
                nullptr,
                0);
            if (!itemBlock) {
                (*reinterpret_cast<uint32_t*>(static_cast<char*>(tooltip) + 242))++;
                continue;
            }

            if (firstReagent) {
                char* label = FrameScript::GetText(const_cast<char*>("SPELL_REAGENTS"), -1, 0);
                if (label && label[0]) {
                    SStr::Copy(reagentLine, label, static_cast<uint32_t>(sizeof(reagentLine)));
                }
                firstReagent = false;
            } else {
                SStr::Append(reagentLine, const_cast<char*>(", "), static_cast<uint32_t>(sizeof(reagentLine)));
            }

            const char* itemName = DBItemCache__GetItemNameByIndex(itemBlock, 0);
            if (!itemName) {
                itemName = "";
            }

            char part[256] = {};
            if (spellRow->m_reagentCount[reagentIndex] <= 1) {
                SStr::Copy(part, const_cast<char*>(itemName), static_cast<uint32_t>(sizeof(part)));
            } else {
                SStr::Printf(
                    part,
                    sizeof(part),
                    const_cast<char*>("%s (%d)"),
                    itemName,
                    spellRow->m_reagentCount[reagentIndex]);
            }

            void* bagBase = reinterpret_cast<char*>(activePlayer) + sizeof(CGUnit);
            uint32_t haveCount = CGBag_C__GetItemTypeCount(bagBase, reagentId, 0);
            if (haveCount >= static_cast<uint32_t>(spellRow->m_reagentCount[reagentIndex])) {
                SStr::Append(reagentLine, part, static_cast<uint32_t>(sizeof(reagentLine)));
            } else {
                SStr::Append(reagentLine, reinterpret_cast<char*>(0xAD2A5C), static_cast<uint32_t>(sizeof(reagentLine)));
                SStr::Append(reagentLine, part, static_cast<uint32_t>(sizeof(reagentLine)));
                SStr::Append(reagentLine, const_cast<char*>("|r"), static_cast<uint32_t>(sizeof(reagentLine)));
                allMet = false;
            }
        }

        if (!firstReagent && (!allMet)) {
            AddSingleLine(tooltip, reagentLine, reinterpret_cast<void*>(kColorWhite), 1);
        }
    }

    bool BuildItemSpellTriggerLine(
        const ItemCacheTooltipView* itemCache,
        const SpellRow* spellRow,
        uint32_t trigger,
        size_t spellIndex,
        char* outText,
        size_t outTextSize)
    {
        if (!itemCache || !spellRow || !outText || outTextSize == 0) {
            return false;
        }

        outText[0] = '\0';

        char parsedText[1024] = {};
        const bool hasParsedText = BuildItemSpellParsedText(spellRow, parsedText, sizeof(parsedText));

        const char* triggerKey = nullptr;
        bool includeCooldown = false;
        switch (trigger) {
        case 0:
            triggerKey = "ITEM_SPELL_TRIGGER_ONUSE";
            includeCooldown = true;
            break;
        case 6:
            triggerKey = "ITEM_SPELL_TRIGGER_ONUSE";
            break;
        case 1:
            triggerKey = "ITEM_SPELL_TRIGGER_ONEQUIP";
            break;
        case 2:
            triggerKey = "ITEM_SPELL_TRIGGER_ONPROC";
            break;
        default:
            if (hasParsedText) {
                SStr::Copy(outText, parsedText, static_cast<uint32_t>(outTextSize));
                return outText[0] != '\0';
            }
            return false;
        }

        char* triggerFormat = FrameScript::GetText(const_cast<char*>(triggerKey), -1, 0);
        if (!triggerFormat || !triggerFormat[0]) {
            return false;
        }

        if (trigger == 6) {
            char description[1024] = {};
            if (!itemCache->descriptionPtr ||
                !SafeCopySpellTextField(reinterpret_cast<const char*>(itemCache->descriptionPtr), description, sizeof(description)) ||
                !description[0]) {
                return false;
            }

            SStr::Printf(outText, sizeof(char) * outTextSize, const_cast<char*>("%s %s"), triggerFormat, description);
            return outText[0] != '\0';
        }

        if (!hasParsedText && trigger != 6) {
            return false;
        }

        bool appendedCooldown = false;
        if (includeCooldown && spellIndex < std::size(itemCache->spellCooldown)) {
            const int32_t explicitItemCooldown = itemCache->spellCooldown[spellIndex];
            const int32_t explicitCategoryCooldown = itemCache->spellCategoryCooldown[spellIndex];
            if (explicitItemCooldown >= 0 || explicitCategoryCooldown >= 0) {
                includeCooldown = false;

                if (explicitItemCooldown > 0) {
                    char cooldownText[512] = {};
                    CGTooltip::GetDurationString(
                        cooldownText,
                        sizeof(cooldownText),
                        static_cast<uint32_t>(explicitItemCooldown),
                        const_cast<char*>("ITEM_COOLDOWN_TOTAL"),
                        0,
                        1,
                        0);
                    if (cooldownText[0] && hasParsedText) {
                        SStr::Printf(outText, sizeof(char) * outTextSize, const_cast<char*>("%s %s %s"), triggerFormat, parsedText, cooldownText);
                        appendedCooldown = outText[0] != '\0';
                    }
                }
            }
        }

        if (includeCooldown) {
            int32_t recoveryTime = spellRow->m_recoveryTime;
            int32_t categoryRecoveryTime = spellRow->m_categoryRecoveryTime;
            SpellRec_C::GetModifiedStatValue(const_cast<SpellRow*>(spellRow), &recoveryTime, 0x0B);
            SpellRec_C::GetModifiedStatValue(const_cast<SpellRow*>(spellRow), &categoryRecoveryTime, 0x0B);

            const int32_t cooldownMs =
                categoryRecoveryTime > recoveryTime ? categoryRecoveryTime : recoveryTime;
            if (cooldownMs > 0) {
                char cooldownText[512] = {};
                CGTooltip::GetDurationString(
                    cooldownText,
                    sizeof(cooldownText),
                    static_cast<uint32_t>(cooldownMs),
                    const_cast<char*>("ITEM_COOLDOWN_TOTAL"),
                    0,
                    1,
                    0);
                if (cooldownText[0] && hasParsedText) {
                    SStr::Printf(outText, sizeof(char) * outTextSize, const_cast<char*>("%s %s %s"), triggerFormat, parsedText, cooldownText);
                    return outText[0] != '\0';
                }
            }
        }

        if (appendedCooldown) {
            return true;
        }

        if (hasParsedText) {
            SStr::Printf(outText, sizeof(char) * outTextSize, const_cast<char*>("%s %s"), triggerFormat, parsedText);
            return outText[0] != '\0';
        }

        return false;
    }

    bool TryGetLiveItemSpellCharges(void* itemObject, size_t spellIndex, int32_t& outCharges) {
        if (!itemObject || spellIndex >= 5) {
            return false;
        }

        CGObject* object = reinterpret_cast<CGObject*>(itemObject);
        if (!object || !object->ObjectData) {
            return false;
        }

        uint32_t* itemFields = reinterpret_cast<uint32_t*>(object->ObjectData);
        outCharges = static_cast<int32_t>(itemFields[kItemFieldSpellCharges + spellIndex]);
        return true;
    }

    bool IsReadableItem(void* itemObject) {
        if (!itemObject) {
            return false;
        }

#ifdef _MSC_VER
        __try {
#endif
            void** vtable = *reinterpret_cast<void***>(itemObject);
            if (vtable) {
                using IsReadableFn = int(__thiscall*)(void*, int);
                IsReadableFn isReadableFn = reinterpret_cast<IsReadableFn>(vtable[0xDC / sizeof(void*)]);
                if (isReadableFn && isReadableFn(itemObject, 0) != 0) {
                    return true;
                }
            }

            uint32_t* itemState = *reinterpret_cast<uint32_t**>(static_cast<char*>(itemObject) + 0xD4);
            if (!itemState) {
                return false;
            }

            return ((itemState[0x3C / sizeof(uint32_t)] >> 9) & 0x1u) != 0;
#ifdef _MSC_VER
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
#endif
    }

    void AddReadableLine(void* tooltip, void* itemObject) {
        if (!tooltip || !IsReadableItem(itemObject)) {
            return;
        }

        char* readableText = FrameScript::GetText(const_cast<char*>("ITEM_READABLE"), -1, 0);
        if (readableText && readableText[0]) {
            AddSingleLine(tooltip, readableText, reinterpret_cast<void*>(kColorGreen0), 0);
        }
    }

    void AddSocketableLine(void* tooltip, void* itemObject) {
        if (!tooltip || !itemObject || !CGItem_C__IsSocketable(itemObject)) {
            return;
        }

        char* socketableText = FrameScript::GetText(const_cast<char*>("ITEM_SOCKETABLE"), -1, 0);
        if (socketableText && socketableText[0]) {
            AddSingleLine(tooltip, socketableText, reinterpret_cast<void*>(kColorGreen0), 0);
        }
    }

    void AddTargetingDisenchantLine(void* tooltip, const ItemCacheTooltipView* itemCache, CGPlayer* activePlayer) {
        if (!tooltip || !itemCache || !activePlayer || !Spell_C::IsTargeting()) {
            return;
        }

#ifdef _MSC_VER
        __try {
#endif
        const uint32_t targetingSpellId = Spell_C_GetTargetingSpell();
        if (targetingSpellId == 0) {
            LOG_DEBUG << "ItemTooltip targeting-disenchant: no targeting spell";
            return;
        }

        const uint8_t raceId = activePlayer->unitBase.unitData->unitBytes0.raceID;
        const uint8_t classId = activePlayer->unitBase.unitData->unitBytes0.classID;
        SkillLineAbilityRow* abilityRow = sub_812410(raceId, classId, targetingSpellId);
        if (!abilityRow || abilityRow->m_skillLine == 0) {
            LOG_DEBUG << "ItemTooltip targeting-disenchant: no ability row for spellId=" << targetingSpellId;
            return;
        }

        SkillLineRow* skillLineRow = FindSkillLineRowById(abilityRow->m_skillLine);
        if ((!skillLineRow || !skillLineRow->m_displayName_lang || !skillLineRow->m_displayName_lang[0]) &&
            abilityRow->m_skillLine != 0) {
            skillLineRow = FindSkillLineRow(abilityRow->m_skillLine);
        }
        if (!skillLineRow || !skillLineRow->m_displayName_lang || !skillLineRow->m_displayName_lang[0]) {
            LOG_DEBUG << "ItemTooltip targeting-disenchant: no skill line row for skillLineId=" << abilityRow->m_skillLine;
            return;
        }

        SpellRow localizedTargetingSpellRow = {};
        if (!ClientDB::GetLocalizedRow(reinterpret_cast<void*>(kSpellDB), targetingSpellId, &localizedTargetingSpellRow)) {
            LOG_DEBUG << "ItemTooltip targeting-disenchant: no spell row for spellId=" << targetingSpellId;
            return;
        }
        if (localizedTargetingSpellRow.m_effect[0] != 99) {
            LOG_DEBUG << "ItemTooltip targeting-disenchant: spellId=" << targetingSpellId
                      << " firstEffect=" << localizedTargetingSpellRow.m_effect[0];
            return;
        }

        const uint32_t playerSkillRank = CGPlayer_C__GetSkillValueForLine(activePlayer, abilityRow->m_skillLine);
        const int32_t requiredSkill = static_cast<int32_t>(itemCache->requiredDisenchantSkill);

        char line[1024] = {};
        uint32_t color = kColorRed0;

        if (requiredSkill > 0) {
            char* formatText = FrameScript::GetText(const_cast<char*>("ITEM_DISENCHANT_MIN_SKILL"), -1, 0);
            if (!formatText || !formatText[0]) {
                return;
            }

            SStr::Printf(line, sizeof(line), formatText, skillLineRow->m_displayName_lang, requiredSkill);
            color = playerSkillRank >= static_cast<uint32_t>(requiredSkill) ? kColorLightBlue1 : kColorRed0;
        } else if (requiredSkill == 0) {
            char* formatText = FrameScript::GetText(const_cast<char*>("ITEM_DISENCHANT_ANY_SKILL"), -1, 0);
            if (!formatText || !formatText[0]) {
                return;
            }

            SStr::Printf(line, sizeof(line), formatText);
            color = kColorLightBlue1;
        } else {
            char* formatText = FrameScript::GetText(const_cast<char*>("ITEM_DISENCHANT_NOT_DISENCHANTABLE"), -1, 0);
            if (!formatText || !formatText[0]) {
                return;
            }

            SStr::Printf(line, sizeof(line), formatText);
            color = kColorRed0;
        }

        if (line[0]) {
            AddSingleLine(tooltip, line, reinterpret_cast<void*>(color), 0);
        }
#ifdef _MSC_VER
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            LOG_DEBUG << "ItemTooltip targeting-disenchant branch failed safely";
            return;
        }
#endif
    }

    void AddEquipmentSetsLine(void* tooltip, void* objectGuidPtr, uint32_t objectGuidLow, uint32_t objectGuidHigh) {
        if (!tooltip) {
            return;
        }

        uint32_t guidParts[2] = {};
        const uint32_t* guid = nullptr;
        if (objectGuidPtr) {
            guid = reinterpret_cast<const uint32_t*>(objectGuidPtr);
        } else if (objectGuidLow || objectGuidHigh) {
            guidParts[0] = objectGuidLow;
            guidParts[1] = objectGuidHigh;
            guid = guidParts;
        }

        if (!guid || (guid[0] == 0 && guid[1] == 0)) {
            return;
        }

        CVar* equipmentManager = CVar__Lookup(const_cast<char*>("equipmentManager"));
        if (!equipmentManager || *reinterpret_cast<int*>(reinterpret_cast<char*>(equipmentManager) + 0x30) == 0) {
            return;
        }

        CVar* hideEquipmentSets = CVar__Lookup(const_cast<char*>("dontShowEquipmentSetsOnItems"));
        if (hideEquipmentSets && *reinterpret_cast<int*>(reinterpret_cast<char*>(hideEquipmentSets) + 0x30) != 0) {
            return;
        }

        char* formatText = FrameScript::GetText(const_cast<char*>("EQUIPMENT_SETS"), -1, 0);
        if (!formatText || !formatText[0]) {
            return;
        }

        char setNames[1024] = {};
        if (!BuildEquipmentSetList(setNames, sizeof(setNames), guid) || !setNames[0]) {
            return;
        }

        char line[1024] = {};
        SStr::Printf(line, sizeof(line), formatText, setNames);
        AddDarkYellowLine(tooltip, line, 1);
    }

    void FormatRefundDuration(char* dest, size_t destSize, uint32_t remainingSeconds) {
        if (!dest || destSize == 0) {
            return;
        }

        dest[0] = '\0';
        if (remainingSeconds == 0) {
            return;
        }

        if (remainingSeconds > 3660 && remainingSeconds <= 7199) {
            const uint32_t hours = remainingSeconds / 3600;
            const uint32_t minutes = (remainingSeconds - 3600) / 60;

            char hoursText[256] = {};
            char minutesText[256] = {};
            char combined[512] = {};

            char* hoursFormat = FrameScript::GetText(const_cast<char*>("INT_SPELL_DURATION_HOURS"), 1, 1);
            char* minutesFormat = FrameScript::GetText(const_cast<char*>("INT_SPELL_DURATION_MIN"), minutes, 1);
            char* delimiter = FrameScript::GetText(const_cast<char*>("TIME_UNIT_DELIMITER"), -1, 0);

            if (hoursFormat && hoursFormat[0] && minutesFormat && minutesFormat[0] && delimiter && delimiter[0]) {
                SStr::Printf(hoursText, sizeof(hoursText), hoursFormat, hours);
                SStr::Printf(minutesText, sizeof(minutesText), minutesFormat, minutes);
                SStr::Printf(combined, sizeof(combined), const_cast<char*>("%s%s%s"), hoursText, delimiter, minutesText);
                SStr::Copy(dest, combined, static_cast<uint32_t>(destSize));
                return;
            }
        }

        CGTooltip::GetDurationString(
            dest,
            static_cast<uint32_t>(destSize),
            remainingSeconds,
            const_cast<char*>("INT_SPELL_DURATION"),
            0,
            1,
            0);
    }

    void AddRefundTimeLine(void* tooltip, void* itemObject, CGPlayer* activePlayer) {
        if (!tooltip || !itemObject || !activePlayer) {
            return;
        }

#ifdef _MSC_VER
        __try {
#endif
            void* refundInfo = *reinterpret_cast<void**>(static_cast<char*>(itemObject) + 0x3D4);
            if (!refundInfo) {
                uint32_t stateFlags = *reinterpret_cast<uint32_t*>(static_cast<char*>(itemObject) + 0x18);
                uint32_t refundFlags = *reinterpret_cast<uint32_t*>(static_cast<char*>(itemObject) + 0x394);
                if ((stateFlags & 0x1000u) != 0 && ((refundFlags >> 2) & 0x1u) == 0) {
                    CGItem_C__RequestRefundInfo(itemObject);
                }
                return;
            }

            uint32_t playedTime = CGPlayer_C__GetPlayedTime(activePlayer);
            int32_t remainingSeconds =
                *reinterpret_cast<int32_t*>(static_cast<char*>(refundInfo) + 0x38) -
                static_cast<int32_t>(playedTime) + 0x1C20;
            if (remainingSeconds <= 0) {
                return;
            }

            if (CGItem_C__HasRefundCurrency(itemObject)) {
                return;
            }

            char durationText[512] = {};
            FormatRefundDuration(durationText, sizeof(durationText), static_cast<uint32_t>(remainingSeconds));
            if (!durationText[0]) {
                return;
            }

            char* refundFormat = FrameScript::GetText(const_cast<char*>("REFUND_TIME_REMAINING"), -1, 0);
            if (!refundFormat || !refundFormat[0]) {
                return;
            }

            char line[1024] = {};
            SStr::Printf(line, sizeof(line), refundFormat, durationText);
            AddDarkYellowLine(tooltip, const_cast<char*>(" "), 0);
            AddSingleLine(tooltip, line, reinterpret_cast<void*>(kColorLightBlue0), 1);
#ifdef _MSC_VER
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            LOG_DEBUG << "ItemTooltip refund-time branch failed safely";
            return;
        }
#endif
    }

    void AddBindTradeTimeLine(void* tooltip, void* itemObject, CGPlayer* activePlayer) {
        if (!tooltip || !itemObject || !activePlayer) {
            return;
        }

#ifdef _MSC_VER
        __try {
#endif
            if (!CGItem_C__IsBound(itemObject) || CGItem_C__IsPermanentlyBoundForTrade(itemObject)) {
                return;
            }

            void* itemState = *reinterpret_cast<void**>(static_cast<char*>(itemObject) + 0xD4);
            if (!itemState) {
                return;
            }

            int32_t remainingSeconds =
                *reinterpret_cast<int32_t*>(static_cast<char*>(itemState) + 0xE0) -
                static_cast<int32_t>(CGPlayer_C__GetPlayedTime(activePlayer)) + 0x1C20;
            if (remainingSeconds <= 0) {
                return;
            }

            char durationText[512] = {};
            FormatRefundDuration(durationText, sizeof(durationText), static_cast<uint32_t>(remainingSeconds));
            if (!durationText[0]) {
                return;
            }

            char* bindTradeFormat = FrameScript::GetText(const_cast<char*>("BIND_TRADE_TIME_REMAINING"), -1, 0);
            if (!bindTradeFormat || !bindTradeFormat[0]) {
                return;
            }

            char line[1024] = {};
            SStr::Printf(line, sizeof(line), bindTradeFormat, durationText);
            AddDarkYellowLine(tooltip, const_cast<char*>(" "), 0);
            AddSingleLine(tooltip, line, reinterpret_cast<void*>(kColorLightBlue0), 1);
#ifdef _MSC_VER
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            LOG_DEBUG << "ItemTooltip bind-trade-time branch failed safely";
            return;
        }
#endif
    }

    char* LookupNameCacheEntry(void* tooltip, uint32_t guidLow, uint32_t guidHigh) {
        if (!tooltip || (guidLow | guidHigh) == 0) {
            return nullptr;
        }

        uint32_t guid[2] = { guidLow, guidHigh };
        return DbNameCache_GetInfoBlockById(
            reinterpret_cast<void*>(kWdbCacheName),
            guidLow,
            guidHigh,
            guid,
            nullptr,
            nullptr,
            0);
    }

    enum class PetitionTooltipResult {
        NotPetition,
        AddedLines,
        CacheMiss,
    };

    void* GetPetitionState(void* itemObject) {
        if (!itemObject) {
            return nullptr;
        }

#ifdef _MSC_VER
        __try {
#endif
            return static_cast<char*>(itemObject) + 0xD0;
#ifdef _MSC_VER
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return nullptr;
        }
#endif
    }

    bool TryGetPetitionId(void* petitionState, uint32_t* petitionId) {
        if (!petitionState || !petitionId) {
            return false;
        }

        *petitionId = 0;
#ifdef _MSC_VER
        __try {
#endif
            char* petitionStateBytes = static_cast<char*>(petitionState);
            void** vtable = *reinterpret_cast<void***>(petitionStateBytes);
            if (!vtable || !vtable[0]) {
                LOG_DEBUG << "ItemTooltip petition: no petition-state vtable";
                return false;
            }

            using GetFlagsFn = uint32_t(__thiscall*)(void*, int);
            GetFlagsFn getFlags = reinterpret_cast<GetFlagsFn>(vtable[0]);
            const uint32_t flags0 = getFlags(petitionState, 0);
            const bool hasPetitionBit0 = (((flags0 >> 13) & 0x1u) != 0) || ((flags0 & 0x40u) != 0);

            const uint32_t flags1 = getFlags(petitionState, 0);
            const bool hasPetitionBit1 = (((flags1 >> 13) & 0x1u) != 0) || ((flags1 & 0x40u) != 0);

            void* petitionInfo = *reinterpret_cast<void**>(petitionStateBytes + 4);
            if (!petitionInfo) {
                LOG_DEBUG << "ItemTooltip petition: no petitionInfo block";
                return false;
            }

            const uint32_t candidatePetitionId = *reinterpret_cast<uint32_t*>(static_cast<char*>(petitionInfo) + 0x40);
            if (!hasPetitionBit0) {
                LOG_DEBUG << "ItemTooltip petition: flag0 missing petition bit value=" << flags0
                          << " candidateId=" << candidatePetitionId;
            }
            if (!hasPetitionBit1) {
                LOG_DEBUG << "ItemTooltip petition: flag1 missing petition bit value=" << flags1
                          << " candidateId=" << candidatePetitionId;
            }
            if (!hasPetitionBit0 && !hasPetitionBit1 && candidatePetitionId == 0) {
                return false;
            }

            *petitionId = candidatePetitionId;
            return *petitionId != 0;
#ifdef _MSC_VER
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            LOG_DEBUG << "ItemTooltip petition: TryGetPetitionId faulted";
            *petitionId = 0;
            return false;
        }
#endif
    }

    PetitionTooltipResult AddPetitionLines(void* tooltip, void* itemObject, void* objectGuidPtr) {
        if (!tooltip || !itemObject) {
            return PetitionTooltipResult::NotPetition;
        }

#ifdef _MSC_VER
        __try {
#endif
            void* petitionState = GetPetitionState(itemObject);
            if (!petitionState) {
                return PetitionTooltipResult::NotPetition;
            }

            uint32_t petitionId = 0;
            if (!TryGetPetitionId(petitionState, &petitionId)) {
                return PetitionTooltipResult::NotPetition;
            }

            uint32_t fallbackGuid[2] = {
                *reinterpret_cast<uint32_t*>(static_cast<char*>(tooltip) + 0x340),
                *reinterpret_cast<uint32_t*>(static_cast<char*>(tooltip) + 0x344),
            };
            void* petitionGuid = objectGuidPtr ? objectGuidPtr : fallbackGuid;

            void* petitionBlock = DbPetitionCache_GetInfoBlockById(
                reinterpret_cast<void*>(kWdbCachePetition),
                petitionId,
                petitionGuid,
                reinterpret_cast<void*>(0x626650),
                tooltip,
                1);
            if (!petitionBlock) {
                LOG_DEBUG << "ItemTooltip petition: cache miss for petitionId=" << petitionId;
                return PetitionTooltipResult::CacheMiss;
            }

            PetitionCacheTooltipView* petition = reinterpret_cast<PetitionCacheTooltipView*>(petitionBlock);
            if (!petition->title[0]) {
                LOG_DEBUG << "ItemTooltip petition: empty title for petitionId=" << petitionId;
                return PetitionTooltipResult::AddedLines;
            }

            char* titleFormat = FrameScript::GetText(
                const_cast<char*>(petition->petitionType == 0 ? "GUILD_CHARTER_TITLE" : "PETITION_TITLE"),
                -1,
                0);
            if (titleFormat && titleFormat[0]) {
                char line[1024] = {};
                SStr::Printf(line, sizeof(line), titleFormat, petition->title);
                AddWhiteLine(tooltip, line);
            }

            char* creatorName = LookupNameCacheEntry(tooltip, petition->creatorGuidLow, petition->creatorGuidHigh);
            if (creatorName && creatorName[0]) {
                char* creatorFormat = FrameScript::GetText(
                    const_cast<char*>(petition->petitionType == 0 ? "GUILD_CHARTER_CREATOR" : "PETITION_CREATOR"),
                    -1,
                    0);
                if (creatorFormat && creatorFormat[0]) {
                    char line[1024] = {};
                    SStr::Printf(line, sizeof(line), creatorFormat, creatorName);
                    AddWhiteLine(tooltip, line);
                }
            }

            const int signatures = GetPetitionSignatureCount(petitionState);
            if (signatures > 0) {
                char* signaturesFormat = FrameScript::GetText(const_cast<char*>("PETITION_NUM_SIGNATURES"), -1, 0);
                if (signaturesFormat && signaturesFormat[0]) {
                    char line[1024] = {};
                    SStr::Printf(line, sizeof(line), signaturesFormat, signatures);
                    AddWhiteLine(tooltip, line);
                }
            }
            return PetitionTooltipResult::AddedLines;
#ifdef _MSC_VER
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            LOG_DEBUG << "ItemTooltip petition branch failed safely";
            return PetitionTooltipResult::NotPetition;
        }
#endif
        return PetitionTooltipResult::NotPetition;
    }

    void AddCreatorAndWrappedByLines(void* tooltip, const ItemCacheTooltipView* itemCache, void* itemObject, int arg10) {
        if (!tooltip || !itemCache || arg10 != 0) {
            return;
        }

#ifdef _MSC_VER
        __try {
#endif
            if (!itemObject) {
                return;
            }

            uint32_t* itemState = *reinterpret_cast<uint32_t**>(static_cast<char*>(itemObject) + 0xD4);
            if (!itemState) {
                return;
            }

            const uint32_t creatorGuidLow = itemState[0x10 / sizeof(uint32_t)];
            const uint32_t creatorGuidHigh = itemState[0x14 / sizeof(uint32_t)];
            if ((creatorGuidLow | creatorGuidHigh) != 0) {
                char* creatorName = LookupNameCacheEntry(tooltip, creatorGuidLow, creatorGuidHigh);
                if (creatorName && creatorName[0]) {
                    const uint32_t stateFlags = itemState[0x3C / sizeof(uint32_t)];
                    char* formatText = FrameScript::GetText(
                        const_cast<char*>(((stateFlags >> 9) & 0x1u) != 0 ? "ITEM_WRITTEN_BY" : "ITEM_CREATED_BY"),
                        -1,
                        0);
                    if (formatText && formatText[0]) {
                        char line[1024] = {};
                        SStr::Printf(line, sizeof(line), formatText, creatorName);
                        AddWhiteLine(tooltip, line);
                    }
                }
            }

            const uint32_t wrappedGuidLow = itemState[0x18 / sizeof(uint32_t)];
            const uint32_t wrappedGuidHigh = itemState[0x1C / sizeof(uint32_t)];
            if ((wrappedGuidLow | wrappedGuidHigh) != 0) {
                char* wrappedName = LookupNameCacheEntry(tooltip, wrappedGuidLow, wrappedGuidHigh);
                if (wrappedName && wrappedName[0]) {
                    char* formatText = FrameScript::GetText(const_cast<char*>("ITEM_WRAPPED_BY"), -1, 0);
                    if (formatText && formatText[0]) {
                        char line[1024] = {};
                        SStr::Printf(line, sizeof(line), formatText, wrappedName);
                        AddWhiteLine(tooltip, line);
                    }
                }
            }
#ifdef _MSC_VER
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            LOG_DEBUG << "ItemTooltip creator/wrapped-by branch failed safely";
            return;
        }
#endif
    }

    void AddOpenableLine(void* tooltip, const ItemCacheTooltipView* itemCache, void* itemObject) {
        if (!tooltip || !itemCache || !itemObject) {
            return;
        }

#ifdef _MSC_VER
        __try {
#endif
            if (*reinterpret_cast<uint32_t*>(static_cast<char*>(tooltip) + 0x4C0) != 0) {
                return;
            }

            uint32_t* itemState = *reinterpret_cast<uint32_t**>(static_cast<char*>(itemObject) + 0xD4);
            if (!itemState) {
                return;
            }

            const uint32_t itemFlags = itemCache->flagsAndFaction0;
            const uint32_t stateFlags = itemState[0x3C / sizeof(uint32_t)];

            bool isOpenable = false;
            if ((itemFlags & 0x4u) != 0 && itemCache->lockId != 0) {
                isOpenable = ((stateFlags >> 2) & 0x1u) == 0;
            } else if ((itemFlags & 0x200u) != 0) {
                isOpenable = ((stateFlags >> 3) & 0x1u) != 0;
            }

            if (!isOpenable) {
                return;
            }

            char* text = FrameScript::GetText(const_cast<char*>("ITEM_OPENABLE"), -1, 0);
            if (text && text[0]) {
                AddSingleLine(tooltip, text, reinterpret_cast<void*>(kColorGreen0), 0);
            }
#ifdef _MSC_VER
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            LOG_DEBUG << "ItemTooltip openable branch failed safely";
            return;
        }
#endif
    }

    uint32_t GetItemCacheSellValue(const ItemCacheTooltipView* itemCache) {
        if (!itemCache) {
            return 0;
        }

        return itemCache->sellPrice;
    }

    void AddItemUnsellableLine(void* tooltip) {
        if (!tooltip) {
            return;
        }

        char* text = FrameScript::GetText(const_cast<char*>("ITEM_UNSELLABLE"), -1, 0);
        if (text && text[0]) {
            AddWhiteLine(tooltip, text);
        }
    }

    void AddItemValueTail(void* tooltip, const ItemCacheTooltipView* itemCache, void* itemObject, CGPlayer* activePlayer, int arg16) {
        if (!tooltip || !itemCache || !activePlayer) {
            return;
        }

#ifdef _MSC_VER
        __try {
#endif
            if (CursorGetResetMode() == 0x11) {
                return;
            }

            if (*reinterpret_cast<uint32_t*>(static_cast<char*>(tooltip) + 0x4CC) != 0 || arg16 != 0) {
                return;
            }

            int32_t sellValue = static_cast<int32_t>(GetItemCacheSellValue(itemCache));
            int32_t maxSellValue = -1;

            if (sellValue > 0) {
                if (itemObject) {
                    if (CGItem_C__GetUseSpell(itemObject, 0) != 0 &&
                        !CGItem_C__HasSpellItemEnchantment(itemObject)) {
                        int32_t baseCharges = CGItem_C__NumBaseCharges(itemObject);
                        if (baseCharges >= 0 && baseCharges != 0) {
                            sellValue = (CGItem_C__GetAdjustedChargesValue(itemObject) * sellValue) / baseCharges;
                        }
                    }

                    int32_t repairCost = CGItem_C__GetRepairCost(itemObject);
                    if (repairCost >= sellValue) {
                        sellValue = 1;
                    } else if (repairCost > 0) {
                        sellValue -= repairCost;
                    }

                    uint32_t* itemState = *reinterpret_cast<uint32_t**>(static_cast<char*>(itemObject) + 0xD4);
                    if (itemState) {
                        const int32_t stackCount = static_cast<int32_t>(itemState[0x20 / sizeof(uint32_t)]);
                        if (stackCount > 1) {
                            sellValue *= stackCount;
                        }
                    }
                } else {
                    const uint32_t proposedEnchantState = *reinterpret_cast<uint32_t*>(static_cast<char*>(tooltip) + 0x4C0);
                    if (proposedEnchantState != 0) {
                        const int32_t stackCount = static_cast<int32_t>(*reinterpret_cast<uint32_t*>(static_cast<char*>(tooltip) + 0x4B8));
                        const int32_t maxStackCount = static_cast<int32_t>(*reinterpret_cast<uint32_t*>(static_cast<char*>(tooltip) + 0x4BC));

                        sellValue *= stackCount;
                        if (maxStackCount > stackCount) {
                            maxSellValue = maxStackCount * static_cast<int32_t>(GetItemCacheSellValue(itemCache));
                        }
                    }
                }

                CGTooltip_RunMoneyScript(tooltip, sellValue, maxSellValue);
                return;
            }

            const uint32_t* pendingTradeGuid = reinterpret_cast<const uint32_t*>(kPendingTradeBindGuid);
            if ((pendingTradeGuid[0] | pendingTradeGuid[1]) == 0 || !itemObject) {
                return;
            }

            uint32_t* itemState = *reinterpret_cast<uint32_t**>(static_cast<char*>(itemObject) + 0xD4);
            if (!itemState || !activePlayer || !activePlayer->unitBase.objectBase.ObjectData) {
                return;
            }

            uint32_t* playerGuid = reinterpret_cast<uint32_t*>(activePlayer->unitBase.objectBase.ObjectData);
            if (itemState[0x8 / sizeof(uint32_t)] != playerGuid[0] || itemState[0xC / sizeof(uint32_t)] != playerGuid[1]) {
                AddItemUnsellableLine(tooltip);
                return;
            }

            uint32_t* itemGuid = reinterpret_cast<uint32_t*>(reinterpret_cast<CGObject*>(itemObject)->ObjectData);
            if (!itemGuid) {
                return;
            }

            const uint32_t tradeableCount = *reinterpret_cast<uint32_t*>(reinterpret_cast<char*>(activePlayer) + 0x18F0);
            const uint32_t* tradeableGuids = *reinterpret_cast<uint32_t**>(reinterpret_cast<char*>(activePlayer) + 0x18F4);
            if (!tradeableGuids) {
                return;
            }

            for (uint32_t index = 0; index < tradeableCount; ++index) {
                const uint32_t* guid = tradeableGuids + (index * 2);
                if (guid[0] == itemGuid[0] && guid[1] == itemGuid[1]) {
                    if (index >= 0x17) {
                        AddItemUnsellableLine(tooltip);
                    }
                    return;
                }
            }
#ifdef _MSC_VER
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            LOG_DEBUG << "ItemTooltip value/unsellable tail failed safely";
            return;
        }
#endif
    }

    void SafeAddReadableLine(void* tooltip, void* itemObject) {
#ifdef _MSC_VER
        __try {
#endif
            AddReadableLine(tooltip, itemObject);
#ifdef _MSC_VER
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            LOG_ERROR << "ItemTooltip late-tail failed in AddReadableLine";
        }
#endif
    }

    void SafeAddSocketableLine(void* tooltip, void* itemObject) {
#ifdef _MSC_VER
        __try {
#endif
            AddSocketableLine(tooltip, itemObject);
#ifdef _MSC_VER
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            LOG_ERROR << "ItemTooltip late-tail failed in AddSocketableLine";
        }
#endif
    }

    void SafeAddTargetingDisenchantLine(void* tooltip, const ItemCacheTooltipView* itemCache, CGPlayer* activePlayer) {
#ifdef _MSC_VER
        __try {
#endif
            AddTargetingDisenchantLine(tooltip, itemCache, activePlayer);
#ifdef _MSC_VER
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            LOG_ERROR << "ItemTooltip late-tail failed in AddTargetingDisenchantLine";
        }
#endif
    }

    void SafeAddItemSetLines(void* tooltip, const ItemCacheTooltipView* itemCache, uint32_t itemId, CGPlayer* activePlayer) {
#ifdef _MSC_VER
        __try {
#endif
            AddItemSetLines(tooltip, itemCache, itemId, activePlayer);
#ifdef _MSC_VER
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            LOG_ERROR << "ItemTooltip late-tail failed in AddItemSetLines for itemId=" << itemId
                      << " itemSetId=" << (itemCache ? itemCache->itemSetId : 0);
        }
#endif
    }

    void SafeAddEquipmentSetsLine(void* tooltip, void* objectGuidPtr, uint32_t objectGuidLow, uint32_t objectGuidHigh) {
#ifdef _MSC_VER
        __try {
#endif
            AddEquipmentSetsLine(tooltip, objectGuidPtr, objectGuidLow, objectGuidHigh);
#ifdef _MSC_VER
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            LOG_ERROR << "ItemTooltip late-tail failed in AddEquipmentSetsLine";
        }
#endif
    }

    void SafeAddRefundTimeLine(void* tooltip, void* itemObject, CGPlayer* activePlayer) {
#ifdef _MSC_VER
        __try {
#endif
            AddRefundTimeLine(tooltip, itemObject, activePlayer);
#ifdef _MSC_VER
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            LOG_ERROR << "ItemTooltip late-tail failed in AddRefundTimeLine";
        }
#endif
    }

    void SafeAddBindTradeTimeLine(void* tooltip, void* itemObject, CGPlayer* activePlayer) {
#ifdef _MSC_VER
        __try {
#endif
            AddBindTradeTimeLine(tooltip, itemObject, activePlayer);
#ifdef _MSC_VER
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            LOG_ERROR << "ItemTooltip late-tail failed in AddBindTradeTimeLine";
        }
#endif
    }

    void SafeAddCreatorAndWrappedByLines(void* tooltip, const ItemCacheTooltipView* itemCache, void* itemObject, int arg10) {
#ifdef _MSC_VER
        __try {
#endif
            AddCreatorAndWrappedByLines(tooltip, itemCache, itemObject, arg10);
#ifdef _MSC_VER
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            LOG_ERROR << "ItemTooltip late-tail failed in AddCreatorAndWrappedByLines";
        }
#endif
    }

    void SafeAddOpenableLine(void* tooltip, const ItemCacheTooltipView* itemCache, void* itemObject) {
#ifdef _MSC_VER
        __try {
#endif
            AddOpenableLine(tooltip, itemCache, itemObject);
#ifdef _MSC_VER
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            LOG_ERROR << "ItemTooltip late-tail failed in AddOpenableLine";
        }
#endif
    }

    void SafeAddItemValueTail(void* tooltip, const ItemCacheTooltipView* itemCache, void* itemObject, CGPlayer* activePlayer, int arg16) {
#ifdef _MSC_VER
        __try {
#endif
            AddItemValueTail(tooltip, itemCache, itemObject, activePlayer, arg16);
#ifdef _MSC_VER
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            LOG_ERROR << "ItemTooltip late-tail failed in AddItemValueTail";
        }
#endif
    }

    void SafeRunItemTooltipScript(void* tooltip) {
        if (!tooltip) {
            return;
        }

#ifdef _MSC_VER
        __try {
#endif
            void* scriptObject = static_cast<char*>(tooltip) + 0x4F4;
            if (*reinterpret_cast<void**>(scriptObject)) {
                auto runScript = reinterpret_cast<void(__thiscall*)(void*, void*, int, int)>(0x81A2C0);
                runScript(tooltip, scriptObject, 0, 0);
            }
#ifdef _MSC_VER
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            LOG_ERROR << "ItemTooltip late-tail failed in FrameScript_Object__RunScript";
        }
#endif
    }

    void AddItemSpellLines(
        void* tooltip,
        const ItemCacheTooltipView* itemCache,
        void* itemObject,
        CGPlayer* activePlayer,
        int randomPropertySeed)
    {
        if (!tooltip || !itemCache || !activePlayer) {
            return;
        }

        for (size_t index = 0; index < std::size(itemCache->spellId); ++index) {
            uint32_t spellId = itemCache->spellId[index];
            uint32_t trigger = itemCache->spellTrigger[index];
            if (spellId == 0) {
                continue;
            }

            SpellRow spellRow = {};
            if (!ClientDB::GetLocalizedRow(reinterpret_cast<void*>(kSpellDB), spellId, &spellRow)) {
                continue;
            }

            char line[2048] = {};
            if (!BuildItemSpellTriggerLine(itemCache, &spellRow, trigger, index, line, sizeof(line))) {
                continue;
            }

            const uint32_t color = IsItemSpellCreateItemEffect(&spellRow) ? kColorWhite : kColorGreen0;
            AddSingleLine(tooltip, line, reinterpret_cast<void*>(color), 1);

            const int32_t templateCharges = itemCache->spellCharges[index];
            if (templateCharges != 0 && templateCharges != -1) {
                int32_t charges = templateCharges;
                TryGetLiveItemSpellCharges(itemObject, index, charges);

                char chargeLine[256] = {};
                if (charges == 0) {
                    char* noChargesFormat = FrameScript::GetText(const_cast<char*>("ITEM_SPELL_CHARGES_NONE"), -1, 0);
                    if (noChargesFormat && noChargesFormat[0]) {
                        SStr::Printf(chargeLine, sizeof(chargeLine), noChargesFormat);
                    }
                } else {
                    char* chargesFormat = FrameScript::GetText(const_cast<char*>("ITEM_SPELL_CHARGES"), -1, 0);
                    if (chargesFormat && chargesFormat[0]) {
                        const int32_t chargeCount = charges < 0 ? -charges : charges;
                        SStr::Printf(chargeLine, sizeof(chargeLine), chargesFormat, chargeCount);
                    }
                }

                if (chargeLine[0]) {
                    AddSingleLine(tooltip, chargeLine, reinterpret_cast<void*>(kColorWhite), 1);
                }
            }

            const uint32_t embeddedItemId = GetFirstEmbeddedItemId(&spellRow);
            if (embeddedItemId != 0 && randomPropertySeed != 0) {
                int embeddedArg4 = 0;
                uint64_t embeddedGuid = 0;
                CGTooltipInternal::SetItem(
                    tooltip,
                    static_cast<int>(embeddedItemId),
                    static_cast<unsigned int>(embeddedArg4),
                    reinterpret_cast<void*>(&embeddedGuid),
                    0,
                    0,
                    0,
                    1,
                    0,
                    0,
                    0,
                    0,
                    0,
                    0,
                    0,
                    1);
            }

            AddItemSpellReagentLine(tooltip, &spellRow, activePlayer);
        }
    }

    void AddLiveItemCooldownLine(void* tooltip, int arg10) {
        if (!tooltip) {
            return;
        }

        if (arg10 != 0) {
            return;
        }

        const uint32_t cooldownState = *reinterpret_cast<uint32_t*>(static_cast<char*>(tooltip) + 0x4C0);
        if (cooldownState == 0) {
            return;
        }

        const uint32_t remainingCooldown = *reinterpret_cast<uint32_t*>(static_cast<char*>(tooltip) + 0x47C);
        if (remainingCooldown == 0) {
            return;
        }

        char cooldownLine[256] = {};
        CGTooltip::GetDurationString(
            cooldownLine,
            sizeof(cooldownLine),
            remainingCooldown,
            const_cast<char*>("ITEM_COOLDOWN_TIME"),
            0,
            1,
            0);
        if (cooldownLine[0]) {
            AddSingleLine(tooltip, cooldownLine, reinterpret_cast<void*>(kColorWhite), 0);
        }
    }

    void AddLeaveCombatCooldownLine(void* tooltip, void* itemObject, int arg10) {
        if (!tooltip || !itemObject || arg10 != 0) {
            return;
        }

        uint32_t cooldownState = 0;
        uint32_t cooldownTimer = 0;
        Spell_C_GetItemCooldown(itemObject, &cooldownState, 0, &cooldownTimer);

        if (cooldownState == 0 || cooldownTimer != 0) {
            return;
        }

        char* text = FrameScript::GetText(const_cast<char*>("COOLDOWN_ON_LEAVE_COMBAT"), -1, 0);
        if (text && text[0]) {
            AddSingleLine(tooltip, text, reinterpret_cast<void*>(kColorWhite), 0);
        }
    }

    CLIENT_DETOUR_THISCALL(
        CGTooltip__SetItem_Trampoline,
        0x6277F0,
        int,
        (int itemId,
         unsigned int arg4,
         void* objectGuidPtr,
         int arg5,
         int bagFamily,
         int randomPropertySeed,
         int updateExisting,
         uint32_t objectGuidLow,
         uint32_t objectGuidHigh,
         int arg10,
         void* pFile2,
         int recoveryTime,
         void* enchantment,
         int arg14,
         int subClass,
         int arg16))
    {
        return ItemTooltipExtensions::SetItemTooltipHook(
            self,
            nullptr,
            itemId,
            arg4,
            objectGuidPtr,
            arg5,
            bagFamily,
            randomPropertySeed,
            updateExisting,
            objectGuidLow,
            objectGuidHigh,
            arg10,
            pFile2,
            recoveryTime,
            enchantment,
            arg14,
            subClass,
            arg16);
    }
}

void ItemTooltipExtensions::Apply() {
}

int __fastcall ItemTooltipExtensions::SetItemTooltipHook(
    void* thisPtr,
    void* /*edx*/,
    int itemId,
    unsigned int arg4,
    void* objectGuidPtr,
    int arg5,
    int bagFamily,
    int randomPropertySeed,
    int updateExisting,
    uint32_t objectGuidLow,
    uint32_t objectGuidHigh,
    int arg10,
    void* pFile2,
    int recoveryTime,
    void* enchantment,
    int arg14,
    int subClass,
    int arg16)
{
    static thread_local int s_setItemHookDepth = 0;
    if (s_setItemHookDepth > 0) {
        if (updateExisting != 0) {
            ++s_setItemHookDepth;
            int nestedResult = 0;
            __try {
                nestedResult = SetItemTooltipImpl(
                    thisPtr,
                    itemId,
                    arg4,
                    objectGuidPtr,
                    arg5,
                    bagFamily,
                    randomPropertySeed,
                    updateExisting,
                    objectGuidLow,
                    objectGuidHigh,
                    arg10,
                    pFile2,
                    recoveryTime,
                    enchantment,
                    arg14,
                    subClass,
                    arg16);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                LOG_ERROR << "ItemTooltip nested SetItem replacement failed for itemId=" << itemId;
                nestedResult = 0;
            }
            --s_setItemHookDepth;
            return nestedResult;
        }
        return CGTooltip__SetItem_Trampoline(
            thisPtr,
            itemId,
            arg4,
            objectGuidPtr,
            arg5,
            bagFamily,
            randomPropertySeed,
            updateExisting,
            objectGuidLow,
            objectGuidHigh,
            arg10,
            pFile2,
            recoveryTime,
            enchantment,
            arg14,
            subClass,
            arg16);
    }

    ++s_setItemHookDepth;
    int result = 0;
    __try {
        result = SetItemTooltipImpl(
            thisPtr,
            itemId,
            arg4,
            objectGuidPtr,
            arg5,
            bagFamily,
            randomPropertySeed,
            updateExisting,
            objectGuidLow,
            objectGuidHigh,
            arg10,
            pFile2,
            recoveryTime,
            enchantment,
            arg14,
            subClass,
            arg16);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        LOG_ERROR << "ItemTooltip SetItem hook failed, clearing tooltip safely";
        __try {
            if (thisPtr) {
                CGTooltipInternal::ClearTooltip(thisPtr);
                CSimpleFrame::Hide(thisPtr);
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
        result = 0;
    }
    --s_setItemHookDepth;
    return result;
}

int ItemTooltipExtensions::SetItemTooltipImpl(
    void* tooltip,
    int itemId,
    unsigned int arg4,
    void* objectGuidPtr,
    int arg5,
    int bagFamily,
    int randomPropertySeed,
    int updateExisting,
    uint32_t objectGuidLow,
    uint32_t objectGuidHigh,
    int arg10,
    void* pFile2,
    int recoveryTime,
    void* enchantment,
    int arg14,
    int subClass,
    int arg16)
{
    if (!tooltip) {
        return 0;
    }

    if (!updateExisting) {
        CGTooltipInternal::ClearTooltip(tooltip);

        if (objectGuidPtr) {
            const uint32_t* sourceGuid = reinterpret_cast<const uint32_t*>(objectGuidPtr);
            *reinterpret_cast<uint32_t*>(static_cast<char*>(tooltip) + 0x340) = sourceGuid[0];
            *reinterpret_cast<uint32_t*>(static_cast<char*>(tooltip) + 0x344) = sourceGuid[1];
        } else {
            *reinterpret_cast<uint32_t*>(static_cast<char*>(tooltip) + 0x340) = 0;
            *reinterpret_cast<uint32_t*>(static_cast<char*>(tooltip) + 0x344) = 0;
        }

        *reinterpret_cast<uint32_t*>(static_cast<char*>(tooltip) + 0x360) = static_cast<uint32_t>(itemId);
        *reinterpret_cast<uint32_t*>(static_cast<char*>(tooltip) + 0x3B4) = static_cast<uint32_t>(arg5);
        *reinterpret_cast<uint32_t*>(static_cast<char*>(tooltip) + 0x3B8) = static_cast<uint32_t>(arg10);
        *reinterpret_cast<uint32_t*>(static_cast<char*>(tooltip) + 0x3BC) = static_cast<uint32_t>(bagFamily);
        *reinterpret_cast<uint32_t*>(static_cast<char*>(tooltip) + 0x3C0) = objectGuidLow;
        *reinterpret_cast<uint32_t*>(static_cast<char*>(tooltip) + 0x3C4) = objectGuidHigh;
        *reinterpret_cast<uint32_t*>(static_cast<char*>(tooltip) + 0x4C0) = static_cast<uint32_t>(randomPropertySeed);
        *reinterpret_cast<uint32_t*>(static_cast<char*>(tooltip) + 0x4C4) = static_cast<uint32_t>(recoveryTime);
        *reinterpret_cast<uint32_t*>(static_cast<char*>(tooltip) + 0x4C8) = static_cast<uint32_t>(arg14);
        *reinterpret_cast<uint32_t*>(static_cast<char*>(tooltip) + 0x4CC) = static_cast<uint32_t>(arg16);
        *reinterpret_cast<uint32_t*>(static_cast<char*>(tooltip) + 0x488) = 0;
        *reinterpret_cast<uint32_t*>(static_cast<char*>(tooltip) + 0x48C) = 0;
        *reinterpret_cast<uint32_t*>(static_cast<char*>(tooltip) + 0x490) = 0;
        *reinterpret_cast<uint32_t*>(static_cast<char*>(tooltip) + 0x494) = 0;
        ClearTooltipRandomPropertyState(tooltip);
    }

    CGPlayer* activePlayer = reinterpret_cast<CGPlayer*>(
        ClntObjMgr::ObjectPtr(ClntObjMgr::GetActivePlayer(), TYPEMASK_PLAYER));
    if (!activePlayer) {
        CSimpleFrame::Hide(tooltip);
        return 0;
    }

    void* itemCacheBlock = DBItemCache_GetInfoBlockByID(
        reinterpret_cast<void*>(kWdbCacheItem),
        static_cast<uint32_t>(itemId),
        reinterpret_cast<void*>(arg4),
        reinterpret_cast<void*>(0x626650),
        tooltip,
        1);
    if (!itemCacheBlock) {
        CGTooltip_SetItemLoadingText(tooltip);
        CSimpleFrame::Show(tooltip);
        CGTooltipInternal::CalculateSize(tooltip);
        return 0;
    }

    ItemCacheTooltipView* itemCache = reinterpret_cast<ItemCacheTooltipView*>(itemCacheBlock);
    void* itemObject = nullptr;
    if (objectGuidPtr) {
        const uint32_t* itemGuidParts = reinterpret_cast<const uint32_t*>(objectGuidPtr);
        const uint64_t itemGuid =
            (static_cast<uint64_t>(itemGuidParts[1]) << 32) | static_cast<uint64_t>(itemGuidParts[0]);
        itemObject = ClntObjMgr::ObjectPtr(itemGuid, TYPEMASK_ITEM);
    } else if (objectGuidLow || objectGuidHigh) {
        const uint64_t itemGuid = (static_cast<uint64_t>(objectGuidHigh) << 32) | objectGuidLow;
        itemObject = ClntObjMgr::ObjectPtr(itemGuid, TYPEMASK_ITEM);
    }

    bool addedStatusLine = false;
    if (bagFamily) {
        char* statusText = FrameScript::GetText(
            const_cast<char*>(itemCache->gemPropertiesId ? "DESTROY_GEM" : "CURRENTLY_EQUIPPED"),
            -1,
            0);
        if (itemCache->gemPropertiesId) {
            AddRedLine(tooltip, statusText);
        } else {
            AddGreyLine(tooltip, statusText);
        }
        addedStatusLine = true;
    }

    const int32_t resolvedRandomPropertySelector = ResolveRandomPropertySelector(tooltip, itemObject, arg14);
    *reinterpret_cast<int32_t*>(static_cast<char*>(tooltip) + kTooltipRandomPropertyIdOffset) = resolvedRandomPropertySelector;
    *reinterpret_cast<uint16_t*>(static_cast<char*>(tooltip) + kTooltipSuffixFactorOffset) = GetLiveItemSuffixFactor(itemObject);
    PopulateTooltipRandomPropertyBuffer(tooltip, resolvedRandomPropertySelector);

    char itemName[1024] = {};
    CGItem_C__BuildItemName(itemName, sizeof(itemName), itemId, resolvedRandomPropertySelector);

    void* nameColor = reinterpret_cast<void*>(arg5 ? kColorWhite : (addedStatusLine ? kColorGrey0 : 0));
    if (!arg5 && !addedStatusLine) {
        nameColor = GetQualityColor(itemCache);
    }
    AddSingleLine(tooltip, itemName, nameColor);

    AddHeaderDescriptorLine(tooltip, itemCache, arg5);

    if (itemObject && (itemCache->flagsAndFaction0 & 0x2000u) != 0) {
        const PetitionTooltipResult petitionResult = AddPetitionLines(tooltip, itemObject, objectGuidPtr);
        if (petitionResult == PetitionTooltipResult::CacheMiss) {
            CSimpleFrame::Show(tooltip);
            CGTooltipInternal::CalculateSize(tooltip);
            return 0;
        }
    }

    if (!arg5) {
        const ItemDifficultyTextRow* row = GlobalCDBCMap.getRow<ItemDifficultyTextRow>("ItemDifficultyText", itemId);
        if (row && row->text && row->text[0]) {
            uint32_t customColor = MakeTooltipColor(row);
            CGTooltip::AddLine(tooltip, row->text, nullptr, &customColor, &customColor, 0);
        }

        AddItemLevelLine(tooltip, itemCache, arg10);
    }

    if ((itemCache->flagsAndFaction0 & 0x2000u) != 0) {
        char* signableText = FrameScript::GetText(const_cast<char*>("ITEM_SIGNABLE"), -1, 0);
        AddGreenLine(tooltip, signableText);
    }

    if (!arg5 && !arg10) {
        if (itemCache->areaId) {
            auto* areaRow = reinterpret_cast<AreaTableRow*>(
                ClientDB::GetRow(reinterpret_cast<void*>(kAreaTableDB), itemCache->areaId));
            if (areaRow && areaRow->areaName && areaRow->areaName[0]) {
                AddWhiteLine(tooltip, areaRow->areaName);
            }
        }

        if (static_cast<int16_t>(itemCache->mapId) > -1) {
            auto* mapRow = reinterpret_cast<MapRow*>(
                ClientDB::GetRow(reinterpret_cast<void*>(kMapDB), itemCache->mapId));
            if (mapRow && mapRow->m_MapName_lang && mapRow->m_MapName_lang[0]) {
                AddWhiteLine(tooltip, mapRow->m_MapName_lang);
            }
        }

        if ((itemCache->flagsAndFaction0 & 0x2u) != 0) {
            char* conjuredText = FrameScript::GetText(const_cast<char*>("ITEM_CONJURED"), -1, 0);
            AddWhiteLine(tooltip, conjuredText);
        }
    }

    char* bindText = nullptr;

    if (itemObject && CGItem_C__IsBound(itemObject)) {
        if (itemCache->bindType == 4) {
            bindText = FrameScript::GetText(const_cast<char*>("ITEM_BIND_QUEST"), -1, 0);
        } else if (itemCache->itemClass != 10) {
            bindText = FrameScript::GetText(
                const_cast<char*>((itemCache->flagsAndFaction0 & 0x08000000u) ? "ITEM_ACCOUNTBOUND" : "ITEM_SOULBOUND"),
                -1,
                0);
        }
    }

    if (!bindText && itemCache->bindType != 0) {
        if ((itemCache->flagsAndFaction0 & 0x08000000u) != 0) {
            bindText = FrameScript::GetText(const_cast<char*>("ITEM_BIND_TO_ACCOUNT"), -1, 0);
        } else {
            switch (itemCache->bindType) {
                case 1:
                    bindText = FrameScript::GetText(const_cast<char*>("ITEM_BIND_ON_PICKUP"), -1, 0);
                    break;
                case 2:
                    bindText = FrameScript::GetText(const_cast<char*>("ITEM_BIND_ON_EQUIP"), -1, 0);
                    break;
                case 3:
                    bindText = FrameScript::GetText(const_cast<char*>("ITEM_BIND_ON_USE"), -1, 0);
                    break;
                case 4:
                    bindText = FrameScript::GetText(const_cast<char*>("ITEM_BIND_QUEST"), -1, 0);
                    break;
                default:
                    break;
            }
        }
    }

    AddWhiteLine(tooltip, bindText);

    if (itemCache->uniqueCount > 0) {
        char uniqueText[1024] = {};
        if ((itemCache->flagsAndFaction0 & 0x80000u) != 0) {
            char* format = FrameScript::GetText(const_cast<char*>("ITEM_UNIQUE_EQUIPPABLE"), -1, 0);
            if (format) {
                SStr::Printf(uniqueText, sizeof(uniqueText), format);
            }
        } else if (itemCache->uniqueCount == 1) {
            char* format = FrameScript::GetText(const_cast<char*>("ITEM_UNIQUE"), -1, 0);
            if (format) {
                SStr::Printf(uniqueText, sizeof(uniqueText), format);
            }
        } else {
            char* format = FrameScript::GetText(const_cast<char*>("ITEM_UNIQUE_MULTIPLE"), -1, 0);
            if (format) {
                SStr::Printf(uniqueText, sizeof(uniqueText), format);
                char formattedText[1024] = {};
                SStr::Printf(formattedText, sizeof(formattedText), uniqueText, itemCache->uniqueCount);
                SStr::Copy(uniqueText, formattedText, sizeof(uniqueText));
            }
        }

        AddWhiteLine(tooltip, uniqueText);
    } else if (itemCache->itemLimitCategory) {
        auto* limitRow = reinterpret_cast<ItemLimitCategoryRow*>(
            ClientDB::GetRow(reinterpret_cast<void*>(kItemLimitCategoryDB), itemCache->itemLimitCategory));
        if (limitRow && limitRow->name && limitRow->name[0]) {
            char* baseFormat = FrameScript::GetText(
                const_cast<char*>((limitRow->flags & 0x1u) ? "ITEM_LIMIT_CATEGORY_MULTIPLE" : "ITEM_LIMIT_CATEGORY"),
                -1,
                0);
            if (baseFormat) {
                char localizedFormat[512] = {};
                char limitText[1024] = {};
                SStr::Printf(localizedFormat, sizeof(localizedFormat), baseFormat);
                SStr::Printf(limitText, sizeof(limitText), localizedFormat, limitRow->name, limitRow->quantity);
                AddWhiteLine(tooltip, limitText);
            }
        }
    }

    if (itemCache->startsQuestId) {
        char* startsQuestText = FrameScript::GetText(const_cast<char*>("ITEM_STARTS_QUEST"), -1, 0);
        AddWhiteLine(tooltip, startsQuestText);
    }

    if (itemCache->lockId) {
        auto* lockRow = reinterpret_cast<LockRow*>(
            ClientDB::GetRow(reinterpret_cast<void*>(kLockDB), itemCache->lockId));
        if (lockRow) {
            bool isEncrypted = false;
            for (uint32_t lockType : lockRow->type) {
                if (lockType == 0x14) {
                    isEncrypted = true;
                    break;
                }
            }

            char* lockText = FrameScript::GetText(
                const_cast<char*>(isEncrypted ? "ENCRYPTED" : "LOCKED"),
                -1,
                0);
            if (isEncrypted) {
                AddGreenLine(tooltip, lockText);
            } else {
                AddWhiteLine(tooltip, lockText);
            }

            for (size_t i = 0; i < std::size(lockRow->type); ++i) {
                if (lockRow->type[i] != 2) {
                    continue;
                }

                auto* lockTypeRow = reinterpret_cast<LockTypeRow*>(
                    ClientDB::GetRow(reinterpret_cast<void*>(kLockTypeDB), lockRow->type[i]));
                char* lockTypeName = (lockTypeRow && lockTypeRow->name && lockTypeRow->name[0])
                    ? lockTypeRow->name
                    : const_cast<char*>("UNKNOWN");

                char* minSkillFormat = FrameScript::GetText(const_cast<char*>("ITEM_MIN_SKILL"), -1, 0);
                if (minSkillFormat && minSkillFormat[0]) {
                    char minSkillText[1024] = {};
                    SStr::Printf(
                        minSkillText,
                        sizeof(minSkillText),
                        minSkillFormat,
                        lockTypeName,
                        lockRow->index[i]);
                    AddWhiteLine(tooltip, minSkillText);
                }

                break;
            }

            for (size_t i = 0; i < std::size(lockRow->type); ++i) {
                if (lockRow->type[i] != 1) {
                    continue;
                }

                uint64_t relatedGuid = 0;
                void* relatedItemBlock = DBItemCache_GetInfoBlockByID(
                    reinterpret_cast<void*>(kWdbCacheItem),
                    lockRow->index[i],
                    &relatedGuid,
                    nullptr,
                    nullptr,
                    0);
                if (!relatedItemBlock) {
                    continue;
                }

                char* itemName = DBItemCache__GetItemNameByIndex(relatedItemBlock, 0);
                char* lockedWithItemFormat = FrameScript::GetText(const_cast<char*>("LOCKED_WITH_ITEM"), -1, 0);
                if (itemName && itemName[0] && lockedWithItemFormat && lockedWithItemFormat[0]) {
                    char lockItemText[1024] = {};
                    SStr::Printf(lockItemText, sizeof(lockItemText), lockedWithItemFormat, itemName);
                    AddWhiteLine(tooltip, lockItemText);
                }

                break;
            }

            for (size_t i = 0; i < std::size(lockRow->type); ++i) {
                if (lockRow->type[i] != 3) {
                    continue;
                }

                SpellRow spellRow = {};
                if (!ClientDB::GetLocalizedRow(reinterpret_cast<void*>(kSpellDB), lockRow->index[i], &spellRow)) {
                    continue;
                }

                char* lockedWithSpellFormat = FrameScript::GetText(const_cast<char*>("LOCKED_WITH_SPELL"), -1, 0);
                if (spellRow.m_name_lang && spellRow.m_name_lang[0] && lockedWithSpellFormat && lockedWithSpellFormat[0]) {
                    char lockSpellText[1024] = {};
                    SStr::Printf(lockSpellText, sizeof(lockSpellText), lockedWithSpellFormat, spellRow.m_name_lang);
                    AddWhiteLine(tooltip, lockSpellText);
                }

                break;
            }
        }
    }

    ItemSubClassRow* subClassRow = FindItemSubClassRow(itemCache);

    if (itemCache->containerSlots > 0 &&
        subClassRow &&
        GetSubClassTextForTooltip(itemCache, subClassRow)) {
        char* containerSlotsFormat = FrameScript::GetText(const_cast<char*>("CONTAINER_SLOTS"), -1, 0);
        if (containerSlotsFormat && containerSlotsFormat[0]) {
            char containerSlotsText[1024] = {};
            SStr::Printf(
                containerSlotsText,
                sizeof(containerSlotsText),
                containerSlotsFormat,
                itemCache->containerSlots,
                GetSubClassTextForTooltip(itemCache, subClassRow));
            AddWhiteLine(tooltip, containerSlotsText);
        }
    }

    if (itemCache->itemClass == 16) {
        for (uint32_t spellId : itemCache->spellId) {
            if (!spellId) {
                continue;
            }

            SpellRow spellRow = {};
            if (!ClientDB::GetLocalizedRow(reinterpret_cast<void*>(kSpellDB), spellId, &spellRow)) {
                continue;
            }

            bool glyphLineAdded = false;
            for (size_t effectIndex = 0; effectIndex < 3; ++effectIndex) {
                if (spellRow.m_effect[effectIndex] != 74) {
                    continue;
                }

                auto* glyphRow = reinterpret_cast<GlyphPropertiesRow*>(
                    ClientDB::GetRow(reinterpret_cast<void*>(kGlyphPropertiesDB), spellRow.m_effectMiscValue[effectIndex]));
                if (!glyphRow) {
                    continue;
                }

                char* glyphText = FrameScript::GetText(
                    const_cast<char*>(glyphRow->type != 0 ? "MINOR_GLYPH" : "MAJOR_GLYPH"),
                    -1,
                    0);
                AddLightBlueLine(tooltip, glyphText);
                glyphLineAdded = true;
                break;
            }

            if (glyphLineAdded) {
                break;
            }
        }
    }

    if (itemCache->containerSlots == 0) {
        char* classText = GetItemSlotText(itemCache);
        const char* subClassText = GetSubClassTextForTooltip(itemCache, subClassRow);

        if (classText && classText[0]) {
            CGTooltip::AddLine(
                tooltip,
                classText,
                const_cast<char*>(subClassText && subClassText[0] ? subClassText : nullptr),
                reinterpret_cast<void*>(kColorWhite),
                reinterpret_cast<void*>(kColorWhite),
                0);
        } else if (subClassText && subClassText[0]) {
            AddWhiteLine(tooltip, const_cast<char*>(subClassText));
        }
    }

    char damageText[1024] = {};
    float totalDamage = 0.0f;
    if (BuildDamageText(itemCache, damageText, sizeof(damageText), &totalDamage)) {
        char speedText[128] = {};
        char* speedLabel = nullptr;
        char* rightText = nullptr;

        if (itemCache->itemClass == 2 && itemCache->delayMs != 0) {
            speedLabel = FrameScript::GetText(const_cast<char*>("SPEED"), -1, 0);
            if (speedLabel && speedLabel[0]) {
                SStr::Printf(speedText, sizeof(speedText), const_cast<char*>("%s %.2f"), speedLabel, itemCache->delayMs * 0.001f);
                rightText = speedText;
            }
        }

        CGTooltip::AddLine(
            tooltip,
            damageText,
            rightText,
            reinterpret_cast<void*>(kColorWhite),
            reinterpret_cast<void*>(kColorWhite),
            0);

        if (itemCache->itemClass == 2 && itemCache->delayMs != 0 && totalDamage > 0.0f) {
            char* dpsFormat = FrameScript::GetText(const_cast<char*>("DPS_TEMPLATE"), -1, 0);
            if (dpsFormat && dpsFormat[0]) {
                char dpsText[256] = {};
                const float dps = totalDamage / (itemCache->delayMs * 0.001f);
                SStr::Printf(dpsText, sizeof(dpsText), dpsFormat, dps);
                AddWhiteLine(tooltip, dpsText);
            }
        }
    }

    char armorText[256] = {};
    bool isBonusArmor = false;
    if (BuildArmorText(itemCache, armorText, sizeof(armorText), &isBonusArmor)) {
        AddSingleLine(
            tooltip,
            armorText,
            reinterpret_cast<void*>(isBonusArmor ? kColorGreen0 : kColorWhite),
            0);
    }

    char shieldBlockText[256] = {};
    if (BuildShieldBlockText(itemCache, shieldBlockText, sizeof(shieldBlockText))) {
        AddWhiteLine(tooltip, shieldBlockText);
    }

    AddDirectStatLines(tooltip, itemCache);
    AddResistanceLines(tooltip, itemCache);

    if (HasStatSectionLines(itemCache) && (HasAppliedEnchantmentEntries(itemCache, itemObject) || HasItemSpellEntries(itemCache))) {
        AddSpacerLine(tooltip);
    }

    AddAppliedEnchantmentLines(tooltip, itemCache, itemObject, activePlayer);
    AddProposedEnchantLines(tooltip);

    AddSocketLines(tooltip, itemCache, itemObject, activePlayer);
    AddGemPropertiesLines(tooltip, itemCache, activePlayer, arg10);
    AddSocketRequirementLines(tooltip, itemCache, activePlayer);
    AddSocketBonusLine(tooltip, itemCache, itemObject);
    AddRandomEnchantLine(tooltip, itemCache);
    AddItemDurationLine(tooltip, itemCache, arg10);
    AddHolidayLine(tooltip, itemCache, arg10);
    AddRequiredSkillLine(tooltip, itemCache, activePlayer, arg10);
    AddItemSpellKnownLine(tooltip, itemCache, activePlayer);
    AddRequiredSpellLine(tooltip, itemCache, activePlayer);
    AddRequiredPvPRankLine(tooltip, itemCache, activePlayer);
    AddRequiredReputationLine(tooltip, itemCache, activePlayer, arg10);
    AddItemSpellLines(tooltip, itemCache, itemObject, activePlayer, randomPropertySeed);
    AddLiveItemCooldownLine(tooltip, arg10);
    AddLeaveCombatCooldownLine(tooltip, itemObject, arg10);
    SafeAddItemSetLines(tooltip, itemCache, static_cast<uint32_t>(itemId), activePlayer);

    // Stock returns here for the compare/loading-style path after cooldown handling.
    if (arg5 != 0) {
        SafeRunItemTooltipScript(tooltip);
        CSimpleFrame::Show(tooltip);
        CGTooltipInternal::CalculateSize(tooltip);
        return 0;
    }

    // Stock skips the creator/readable/openable/socketable/disenchant block when a10 is set,
    // then continues with equipment sets and the refund/bind/value tail.
    if (arg10 == 0) {
        SafeAddCreatorAndWrappedByLines(tooltip, itemCache, itemObject, arg10);
        SafeAddOpenableLine(tooltip, itemCache, itemObject);
        SafeAddReadableLine(tooltip, itemObject);
        SafeAddSocketableLine(tooltip, itemObject);
        SafeAddTargetingDisenchantLine(tooltip, itemCache, activePlayer);
    }

    SafeAddEquipmentSetsLine(tooltip, objectGuidPtr, objectGuidLow, objectGuidHigh);
    SafeAddRefundTimeLine(tooltip, itemObject, activePlayer);
    SafeAddBindTradeTimeLine(tooltip, itemObject, activePlayer);
    AddRequiredLevelLine(tooltip, itemCache, activePlayer, arg10);
    AddRaceClassRestrictionLine(tooltip, itemCache, activePlayer, arg10);
    AddAllowedRaceLine(tooltip, itemCache, activePlayer, arg10);
    AddAllowedClassLine(tooltip, itemCache, activePlayer, arg10);
    SafeAddItemValueTail(tooltip, itemCache, itemObject, activePlayer, arg16);
    AddItemDescriptionLine(tooltip, itemCache, arg5);
    SafeRunItemTooltipScript(tooltip);

    CSimpleFrame::Show(tooltip);
    CGTooltipInternal::CalculateSize(tooltip);
    return 0;
}
