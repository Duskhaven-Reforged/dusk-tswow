#pragma optimize("", off)
#include <CDBCMgr/CDBC.h>
#include <CDBCMgr/CDBCMgr.h>

struct ItemDifficultyTextRow {
    int itemID;
    char* text;
    float red;
    float green;
    float blue;

    int handleLuaPush(lua_State* L) {
        ClientLua::PushNumber(L, itemID);
        ClientLua::PushString(L, text);
        ClientLua::PushNumber(L, red);
        ClientLua::PushNumber(L, green);
        ClientLua::PushNumber(L, blue);
        return 5;
    }
};

class ItemDifficultyText : public CDBC {
public:
    const char* fileName = "ItemDifficultyText";
    ItemDifficultyText() : CDBC() {
        this->numColumns = sizeof(ItemDifficultyTextRow) / 4;
        this->rowSize = sizeof(ItemDifficultyTextRow);
    }

    ItemDifficultyText* LoadDB() {
        GlobalCDBCMap.addCDBC(this->fileName);
        CDBC::LoadDB(this->fileName);
        ItemDifficultyText::setupStringsAndTable();
        CDBCMgr::addCDBCLuaHandler(this->fileName, [this](lua_State* L, int row) { return this->handleLua(L, row); });
        GlobalCDBCMap.setIndexRange(this->fileName, this->minIndex, this->maxIndex);
        return this;
    }

    void setupStringsAndTable() {
        ItemDifficultyTextRow* row = static_cast<ItemDifficultyTextRow*>(this->rows);
        uintptr_t stringTable = reinterpret_cast<uintptr_t>(this->stringTable);
        for (uint32_t i = 0; i < this->numRows; ++i) {
            row->text = reinterpret_cast<char*>(stringTable + reinterpret_cast<uintptr_t>(row->text));
            GlobalCDBCMap.addRow(this->fileName, row->itemID, *row);
            ++row;
        }
    };

    int handleLua(lua_State* L, int row) {
        ItemDifficultyTextRow* r = GlobalCDBCMap.getRow<ItemDifficultyTextRow>(this->fileName, row);
        if (r) return r->handleLuaPush(L);
        return 0;
    }
};
#pragma optimize("", on)
