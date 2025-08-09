#pragma optimize("", off)
#include "CDBCMgr/CDBC.h"
#include "CDBCMgr/CDBCMgr.h"

struct OcclusionVolumeRow
{
    int32_t ID;
    char* name;
    int32_t mapID;
    int32_t flags;

    int handleLuaPush(lua_State* L)
    {
        ClientLua::PushNumber(L, ID);
        ClientLua::PushString(L, name);
        ClientLua::PushNumber(L, mapID);
        ClientLua::PushNumber(L, flags);
        return 4;
    }
};

class OcclusionVolume : public CDBC
{
  public:
    const char* fileName = "OcclusionVolume";
    OcclusionVolume() : CDBC()
    {
        this->numColumns = sizeof(OcclusionVolumeRow) / 4;
        this->rowSize    = sizeof(OcclusionVolumeRow);
    }

    OcclusionVolume* LoadDB()
    {
        GlobalCDBCMap.addCDBC(this->fileName);
        CDBC::LoadDB(this->fileName);
        OcclusionVolume::setupStringsAndTable();
        CDBCMgr::addCDBCLuaHandler(this->fileName, [this](lua_State* L, int row) { return this->handleLua(L, row); });
        GlobalCDBCMap.setIndexRange(this->fileName, this->minIndex, this->maxIndex);
        return this;
    };

    void OcclusionVolume::setupStringsAndTable()
    {
        OcclusionVolumeRow* row = static_cast<OcclusionVolumeRow*>(this->rows);
        uintptr_t stringTable = reinterpret_cast<uintptr_t>(this->stringTable);
        for (uint32_t i = 0; i < this->numRows; i++)
        {
            row->name = reinterpret_cast<char*>(stringTable + reinterpret_cast<uintptr_t>(row->name));
            GlobalCDBCMap.addRow(this->fileName, row->ID, *row);
            ++row;
        }
    };

    int handleLua(lua_State* L, int row)
    {
        OcclusionVolumeRow* r = GlobalCDBCMap.getRow<OcclusionVolumeRow>(this->fileName, row);
        if (r)
            return r->handleLuaPush(L);
        return 0;
    }
};

#pragma optimize("", on)
