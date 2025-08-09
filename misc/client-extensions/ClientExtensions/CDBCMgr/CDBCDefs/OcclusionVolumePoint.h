#pragma optimize("", off)
#include "CDBCMgr/CDBC.h"
#include "CDBCMgr/CDBCMgr.h"

struct OcclusionVolumePointRow
{
    int32_t ID;
    int32_t occlusionVolumeID;
    float positionX;
    float positionY;
    float positionZ;

    int handleLuaPush(lua_State* L)
    {
        ClientLua::PushNumber(L, ID);
        ClientLua::PushNumber(L, occlusionVolumeID);
        ClientLua::PushNumber(L, positionX);
        ClientLua::PushNumber(L, positionY);
        ClientLua::PushNumber(L, positionZ);
        return 5;
    }
};

class OcclusionVolumePoint : public CDBC
{
  public:
    const char* fileName = "OcclusionVolumePoint";
    OcclusionVolumePoint() : CDBC()
    {
        this->numColumns = sizeof(OcclusionVolumePointRow) / 4;
        this->rowSize    = sizeof(OcclusionVolumePointRow);
    }

    OcclusionVolumePoint* LoadDB()
    {
        GlobalCDBCMap.addCDBC(this->fileName);
        CDBC::LoadDB(this->fileName);
        OcclusionVolumePoint::setupStringsAndTable();
        CDBCMgr::addCDBCLuaHandler(this->fileName, [this](lua_State* L, int row) { return this->handleLua(L, row); });
        GlobalCDBCMap.setIndexRange(this->fileName, this->minIndex, this->maxIndex);
        return this;
    };

    void OcclusionVolumePoint::setupStringsAndTable()
    {
        OcclusionVolumePointRow* row = static_cast<OcclusionVolumePointRow*>(this->rows);
        uintptr_t stringTable   = reinterpret_cast<uintptr_t>(this->stringTable);
        for (uint32_t i = 0; i < this->numRows; i++)
        {
            GlobalCDBCMap.addRow(this->fileName, row->ID, *row);
            ++row;
        }
    };

    int handleLua(lua_State* L, int row)
    {
        OcclusionVolumePointRow* r = GlobalCDBCMap.getRow<OcclusionVolumePointRow>(this->fileName, row);
        if (r)
            return r->handleLuaPush(L);
        return 0;
    }
};

#pragma optimize("", on)
