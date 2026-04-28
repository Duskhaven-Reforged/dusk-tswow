#include <CDBCMgr/CDBCMgr.h>
#include <CDBCMgr/CDBCDefs/LFGRoles.h>
#include <CDBCMgr/CDBCDefs/ItemDifficultyText.h>
#include <CDBCMgr/CDBCDefs/SpellAdditionalAttributes.h>
#include <CDBCMgr/CDBCDefs/SpellAdditionalCostData.h>
#include <CDBCMgr/CDBCDefs/SpellEffectScalars.h>
#include <CDBCMgr/CDBCDefs/ScriptedMissileMotion.h>
#include <CDBCMgr/CDBCDefs/DangerZoneVisualProfile.h>
#include <CDBCMgr/CDBCDefs/ZoneLight.h>
#include <CDBCMgr/CDBCDefs/ZoneLightPoint.h>

CDBCMgr GlobalCDBCMap;
std::unordered_map<std::string, std::function<int(lua_State*,int)>> cdbcLuaHandlers = {};

void CDBCMgr::Load() {
    LFGRoles().LoadDB();
    ItemDifficultyText().LoadDB();
    SpellAdditionalAttributes().LoadDB();
    SpellAdditionalCostData().LoadDB();
    ZoneLight().LoadDB();
    ZoneLightPoint().LoadDB();
    SpellEffectScalars().LoadDB();
    ScriptedMissileMotion().LoadDB();
    DangerZoneVisualProfile().LoadDB();
}

void CDBCMgr::addCDBC(std::string cdbcName) {
    allCDBCs[cdbcName] = CDBC();
    cdbcIndexRanges[cdbcName] = { 0, 0 };
}

CDBCMgr::CDBC CDBCMgr::getCDBC(std::string cdbcName) {
    return allCDBCs[cdbcName];
}

void CDBCMgr::addCDBCLuaHandler(std::string cdbcName, std::function<int(lua_State*,int)> func) {
    cdbcLuaHandlers[cdbcName] = func;
}

int CDBCMgr::handleLua(lua_State* L, std::string cdbcName, int row) {
    auto it = cdbcLuaHandlers.find(cdbcName);
    if (it != cdbcLuaHandlers.end()) {
        return it->second(L, row);
    }
    return 0;
}
