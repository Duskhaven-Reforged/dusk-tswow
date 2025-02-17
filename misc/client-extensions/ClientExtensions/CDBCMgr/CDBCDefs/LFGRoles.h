#pragma optimize("", off)
#include "CDBCMgr/CDBC.h"
#include "CDBCMgr/CDBCMgr.h"

struct LFGRolesRow {
    uint32_t ClassID;
    uint32_t Roles;

    int handleLuaPush(lua_State* L) {
        ClientLua::PushNumber(L, ClassID);
        ClientLua::PushNumber(L, Roles);
        return 2;
    }
};

class LFGRoles : public CDBC {
public:
    const char* fileName = "DBFilesClient\\LFGRoles.cdbc";
    LFGRoles() : CDBC() {
        this->numColumns = sizeof(LFGRolesRow) / 4;
        this->rowSize = sizeof(LFGRolesRow);
    }

    LFGRoles* LoadDB() {
        GlobalCDBCMap.addCDBC("LFGRoles");
        CDBC::LoadDB(this->fileName);
        CDBCMgr::addCDBCLuaHandler("LFGRoles", LFGRoles::handleLua);
        return this;
    };

    static int handleLua(lua_State* L, int row) {
        LFGRolesRow* r = GlobalCDBCMap.getRow<LFGRolesRow>("LFGRoles", row);
        if (r) return r->handleLuaPush(L);
        return 0;
    }
};
#pragma optimize("", on)
