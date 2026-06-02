#pragma once
#pragma optimize("", off)
#include <CDBCMgr/CDBC.h>
#include <CDBCMgr/CDBCMgr.h>

struct SpellCacheRow {
    uint32_t id;
    uint32_t cacheVersion;
    uint32_t spellDataHash;
    uint32_t category;
    uint32_t dispel;
    uint32_t mechanic;
    uint32_t attributes;
    uint32_t attributesEx;
    uint32_t attributesEx2;
    uint32_t attributesEx3;
    uint32_t attributesEx4;
    uint32_t attributesEx5;
    uint32_t attributesEx6;
    uint32_t attributesEx7;
    uint32_t stances;
    uint32_t stancesNot;
    uint32_t targets;
    uint32_t targetCreatureType;
    uint32_t requiresSpellFocus;
    uint32_t facingCasterFlags;
    uint32_t casterAuraState;
    uint32_t targetAuraState;
    uint32_t excludeCasterAuraState;
    uint32_t excludeTargetAuraState;
    uint32_t casterAuraSpell;
    uint32_t targetAuraSpell;
    uint32_t excludeCasterAuraSpell;
    uint32_t excludeTargetAuraSpell;
    uint32_t castingTimeIndex;
    uint32_t recoveryTime;
    uint32_t categoryRecoveryTime;
    uint32_t interruptFlags;
    uint32_t auraInterruptFlags;
    uint32_t channelInterruptFlags;
    uint32_t procFlags;
    uint32_t procChance;
    uint32_t procCharges;
    uint32_t maxLevel;
    uint32_t baseLevel;
    uint32_t spellLevel;
    uint32_t durationIndex;
    int32_t powerType;
    uint32_t manaCost;
    uint32_t manaCostPerLevel;
    uint32_t manaPerSecond;
    uint32_t manaPerSecondPerLevel;
    uint32_t rangeIndex;
    float speed;
    uint32_t modalNextSpell;
    uint32_t stackAmount;
    uint32_t totem1;
    uint32_t totem2;
    int32_t reagent1;
    int32_t reagent2;
    int32_t reagent3;
    int32_t reagent4;
    int32_t reagent5;
    int32_t reagent6;
    int32_t reagent7;
    int32_t reagent8;
    int32_t reagentCount1;
    int32_t reagentCount2;
    int32_t reagentCount3;
    int32_t reagentCount4;
    int32_t reagentCount5;
    int32_t reagentCount6;
    int32_t reagentCount7;
    int32_t reagentCount8;
    int32_t equippedItemClass;
    int32_t equippedItemSubClassMask;
    int32_t equippedItemInventoryTypeMask;
    uint32_t spellVisualID1;
    uint32_t spellVisualID2;
    uint32_t spellIconID;
    uint32_t activeIconID;
    uint32_t spellPriority;
    char* spellName;
    char* spellRank;
    char* description;
    char* auraDescription;
    uint32_t manaCostPct;
    uint32_t startRecoveryCategory;
    uint32_t startRecoveryTime;
    uint32_t maxTargetLevel;
    uint32_t spellFamilyName;
    uint32_t spellFamilyFlags1;
    uint32_t spellFamilyFlags2;
    uint32_t spellFamilyFlags3;
    uint32_t maxAffectedTargets;
    uint32_t dmgClass;
    uint32_t preventionType;
    uint32_t stanceBarOrder;
    uint32_t minFactionID;
    uint32_t minReputation;
    uint32_t requiredAuraVision;
    uint32_t requiredTotemCategoryID1;
    uint32_t requiredTotemCategoryID2;
    int32_t areaGroupId;
    uint32_t schoolMask;
    uint32_t runeCostID;
    uint32_t spellMissileID;
    int32_t powerDisplayID;
    uint32_t descriptionVariablesID;
    uint32_t difficulty;

    int handleLuaPush(lua_State* L) {
        ClientLua::PushNumber(L, id);
        ClientLua::PushString(L, spellName ? spellName : "");
        ClientLua::PushNumber(L, cacheVersion);
        ClientLua::PushNumber(L, spellDataHash);
        return 4;
    }
};

class SpellCache : public CDBC {
public:
    const char* fileName = "Spell";
    SpellCache() : CDBC() {
        this->numColumns = sizeof(SpellCacheRow) / 4;
        this->rowSize = sizeof(SpellCacheRow);
    }

    SpellCache* LoadDB() {
        GlobalCDBCMap.addCDBC(this->fileName);
        CDBC::LoadDB(this->fileName);
        SpellCache::setupStringsAndTable();
        CDBCMgr::addCDBCLuaHandler(this->fileName, [](lua_State* L, int row) {
            SpellCacheRow* r = GlobalCDBCMap.getRow<SpellCacheRow>("Spell", row);
            if (r) return r->handleLuaPush(L);
            return 0;
        });
        GlobalCDBCMap.setIndexRange(this->fileName, this->minIndex, this->maxIndex);
        return this;
    }

    void setupStringsAndTable() {
        SpellCacheRow* row = static_cast<SpellCacheRow*>(this->rows);
        uintptr_t stringTable = reinterpret_cast<uintptr_t>(this->stringTable);
        for (uint32_t i = 0; i < this->numRows; ++i) {
            row->spellName = reinterpret_cast<char*>(stringTable + reinterpret_cast<uintptr_t>(row->spellName));
            row->spellRank = reinterpret_cast<char*>(stringTable + reinterpret_cast<uintptr_t>(row->spellRank));
            row->description = reinterpret_cast<char*>(stringTable + reinterpret_cast<uintptr_t>(row->description));
            row->auraDescription = reinterpret_cast<char*>(stringTable + reinterpret_cast<uintptr_t>(row->auraDescription));
            GlobalCDBCMap.addRow(this->fileName, row->id, *row);
            ++row;
        }
    }

    int handleLua(lua_State* L, int row) {
        SpellCacheRow* r = GlobalCDBCMap.getRow<SpellCacheRow>(this->fileName, row);
        if (r) return r->handleLuaPush(L);
        return 0;
    }
};
#pragma optimize("", on)
