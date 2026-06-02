#pragma once
#pragma optimize("", off)
#include <CDBCMgr/CDBC.h>
#include <CDBCMgr/CDBCMgr.h>

inline int SpellEffectCacheKey(uint32_t spellId, uint32_t effectIndex) {
    return int((spellId << 5) | (effectIndex & 0x1F));
}

struct SpellEffectCacheRow {
    uint32_t spellID;
    uint32_t effectIndex;
    uint32_t effect;
    int32_t effectDieSides;
    float effectRealPointsPerLevel;
    int32_t effectBasePoints;
    uint32_t effectMechanic;
    uint32_t effectImplicitTargetA;
    uint32_t effectImplicitTargetB;
    uint32_t effectRadiusIndex;
    uint32_t effectApplyAuraName;
    uint32_t effectAmplitude;
    float effectMultipleValue;
    uint32_t effectChainTargets;
    int32_t effectItemType;
    int32_t effectMiscValue;
    int32_t effectMiscValueB;
    uint32_t effectTriggerSpell;
    float effectPointsPerCombo;
    uint32_t effectSpellClassMaskA;
    uint32_t effectSpellClassMaskB;
    uint32_t effectSpellClassMaskC;
    float effectChainAmplitude;
    float effectBonusMultiplier;

    int handleLuaPush(lua_State* L) {
        ClientLua::PushNumber(L, spellID);
        ClientLua::PushNumber(L, effectIndex);
        ClientLua::PushNumber(L, effect);
        ClientLua::PushNumber(L, effectApplyAuraName);
        ClientLua::PushNumber(L, effectBasePoints);
        return 5;
    }
};

class SpellEffectCache : public CDBC {
public:
    const char* fileName = "SpellEffect";
    SpellEffectCache() : CDBC() {
        this->numColumns = sizeof(SpellEffectCacheRow) / 4;
        this->rowSize = sizeof(SpellEffectCacheRow);
    }

    SpellEffectCache* LoadDB() {
        GlobalCDBCMap.addCDBC(this->fileName);
        CDBC::LoadDB(this->fileName);
        SpellEffectCache::setupTable();
        CDBCMgr::addCDBCLuaHandler(this->fileName, [](lua_State* L, int row) {
            SpellEffectCacheRow* r = GlobalCDBCMap.getRow<SpellEffectCacheRow>("SpellEffect", row);
            if (r) return r->handleLuaPush(L);
            return 0;
        });
        GlobalCDBCMap.setIndexRange(this->fileName, this->minIndex, this->maxIndex);
        return this;
    }

    void setupTable() {
        SpellEffectCacheRow* row = static_cast<SpellEffectCacheRow*>(this->rows);
        for (uint32_t i = 0; i < this->numRows; ++i) {
            GlobalCDBCMap.addRow(this->fileName, SpellEffectCacheKey(row->spellID, row->effectIndex), *row);
            ++row;
        }
    }

    int handleLua(lua_State* L, int row) {
        SpellEffectCacheRow* r = GlobalCDBCMap.getRow<SpellEffectCacheRow>(this->fileName, row);
        if (r) return r->handleLuaPush(L);
        return 0;
    }
};
#pragma optimize("", on)
