#include <CDBCMgr/CDBCMgr.h>
#include <ClientExtensions.h>
#include <Logger.h>

#include <Windows.h>

void ClientExtensions::initialize() {
    CDBCMgr::Load();
    LOG_INFO << "Custom DBCs loaded";
    CharacterFixes::CharacterCreationFixes();
    LOG_INFO << "Character creation fixes applied";
    CharacterExtensions::Apply();
    LOG_INFO << "Character extensions applied";
    VisibleItemOverrides::Apply();
    LOG_INFO << "Visible item overrides applied";
    WorldDataExtensions::Apply();
    LOG_INFO << "World data extensions applied";
    ExtendedTerrainTextures::Apply();
    LOG_INFO << "Extended terrain texture extension initialized";
    WMOLogging::Apply();
    LOG_INFO << "WMO logging applied";
    ItemTooltipExtensions::Apply();
    LOG_INFO << "Item tooltip extensions applied";
    TooltipExtensions::Apply();
    LOG_INFO << "Tooltip extensions applied";
    MiscFixes::Apply();
    LOG_INFO << "Misc fixes applied";
    Spells::Apply();
    LOG_INFO << "Spell extensions applied";
    EditorRuntime::Apply();
    LOG_INFO << "Client data editor runtime applied";
}
