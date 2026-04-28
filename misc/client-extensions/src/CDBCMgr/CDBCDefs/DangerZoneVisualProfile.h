#pragma optimize("", off)
#include <CDBCMgr/CDBC.h>
#include <CDBCMgr/CDBCMgr.h>

struct DangerZoneVisualProfileRow {
    int id;
    int schoolMask;
    int shapeMask;
    int flags;
    char* modelPath;
    float modelRadius;
    float radiusScale;
    float zOffset;
    int minDurationMs;
    int maxDurationMs;
    float red;
    float green;
    float blue;
    float alpha;
    int debugSpellId;

    int handleLuaPush(lua_State* L) {
        ClientLua::PushNumber(L, id);
        ClientLua::PushNumber(L, schoolMask);
        ClientLua::PushNumber(L, shapeMask);
        ClientLua::PushNumber(L, flags);
        ClientLua::PushString(L, modelPath);
        ClientLua::PushNumber(L, modelRadius);
        ClientLua::PushNumber(L, radiusScale);
        ClientLua::PushNumber(L, zOffset);
        ClientLua::PushNumber(L, minDurationMs);
        ClientLua::PushNumber(L, maxDurationMs);
        ClientLua::PushNumber(L, red);
        ClientLua::PushNumber(L, green);
        ClientLua::PushNumber(L, blue);
        ClientLua::PushNumber(L, alpha);
        ClientLua::PushNumber(L, debugSpellId);
        return 15;
    }
};

class DangerZoneVisualProfile : public CDBC {
public:
    const char* fileName = "DangerZoneVisualProfile";
    DangerZoneVisualProfile() : CDBC() {
        this->numColumns = sizeof(DangerZoneVisualProfileRow) / 4;
        this->rowSize = sizeof(DangerZoneVisualProfileRow);
    }

    DangerZoneVisualProfile* LoadDB() {
        GlobalCDBCMap.addCDBC(this->fileName);
        CDBC::LoadDB(this->fileName);
        DangerZoneVisualProfile::setupStringsAndTable();
        CDBCMgr::addCDBCLuaHandler(this->fileName, [this](lua_State* L, int row) { return this->handleLua(L, row); });
        GlobalCDBCMap.setIndexRange(this->fileName, this->minIndex, this->maxIndex);
        return this;
    }

    void setupStringsAndTable() {
        DangerZoneVisualProfileRow* row = static_cast<DangerZoneVisualProfileRow*>(this->rows);
        uintptr_t stringTable = reinterpret_cast<uintptr_t>(this->stringTable);
        for (uint32_t i = 0; i < this->numRows; ++i) {
            row->modelPath = reinterpret_cast<char*>(stringTable + reinterpret_cast<uintptr_t>(row->modelPath));
            GlobalCDBCMap.addRow(this->fileName, row->id, *row);
            ++row;
        }
    };

    int handleLua(lua_State* L, int row) {
        DangerZoneVisualProfileRow* r = GlobalCDBCMap.getRow<DangerZoneVisualProfileRow>(this->fileName, row);
        if (r) return r->handleLuaPush(L);
        return 0;
    }
};
#pragma optimize("", on)
