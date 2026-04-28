#pragma optimize("", off)
#include <CDBCMgr/CDBC.h>
#include <CDBCMgr/CDBCMgr.h>

struct ScriptedMissileMotionRow {
    int id;
    int motion;
    int count;
    int durationMs;
    int tickMs;
    int flags;
    float collisionRadius;
    float radius;
    float radiusVelocity;
    float startDistance;
    float height;
    float verticalVelocity;
    float forwardSpeed;
    float angularSpeed;
    float sineAmplitude;
    float sineFrequency;
    char* modelPath;
    int visualFlags;
    int attachMode;
    int attachPoint;
    float visualScale;

    int handleLuaPush(lua_State* L) {
        ClientLua::PushNumber(L, id);
        ClientLua::PushNumber(L, motion);
        ClientLua::PushNumber(L, count);
        ClientLua::PushNumber(L, durationMs);
        ClientLua::PushNumber(L, tickMs);
        ClientLua::PushNumber(L, flags);
        ClientLua::PushNumber(L, collisionRadius);
        ClientLua::PushNumber(L, radius);
        ClientLua::PushNumber(L, radiusVelocity);
        ClientLua::PushNumber(L, startDistance);
        ClientLua::PushNumber(L, height);
        ClientLua::PushNumber(L, verticalVelocity);
        ClientLua::PushNumber(L, forwardSpeed);
        ClientLua::PushNumber(L, angularSpeed);
        ClientLua::PushNumber(L, sineAmplitude);
        ClientLua::PushNumber(L, sineFrequency);
        ClientLua::PushString(L, modelPath);
        ClientLua::PushNumber(L, visualFlags);
        ClientLua::PushNumber(L, attachMode);
        ClientLua::PushNumber(L, attachPoint);
        ClientLua::PushNumber(L, visualScale);
        return 21;
    }
};

class ScriptedMissileMotion : public CDBC {
public:
    const char* fileName = "ScriptedMissileMotion";
    ScriptedMissileMotion() : CDBC() {
        this->numColumns = sizeof(ScriptedMissileMotionRow) / 4;
        this->rowSize = sizeof(ScriptedMissileMotionRow);
    }

    ScriptedMissileMotion* LoadDB() {
        GlobalCDBCMap.addCDBC(this->fileName);
        CDBC::LoadDB(this->fileName);
        ScriptedMissileMotion::setupStringsAndTable();
        CDBCMgr::addCDBCLuaHandler(this->fileName, [this](lua_State* L, int row) { return this->handleLua(L, row); });
        GlobalCDBCMap.setIndexRange(this->fileName, this->minIndex, this->maxIndex);
        return this;
    }

    void setupStringsAndTable() {
        ScriptedMissileMotionRow* row = static_cast<ScriptedMissileMotionRow*>(this->rows);
        uintptr_t stringTable = reinterpret_cast<uintptr_t>(this->stringTable);
        for (uint32_t i = 0; i < this->numRows; ++i) {
            row->modelPath = reinterpret_cast<char*>(stringTable + reinterpret_cast<uintptr_t>(row->modelPath));
            GlobalCDBCMap.addRow(this->fileName, row->id, *row);
            ++row;
        }
    };

    int handleLua(lua_State* L, int row) {
        ScriptedMissileMotionRow* r = GlobalCDBCMap.getRow<ScriptedMissileMotionRow>(this->fileName, row);
        if (r) return r->handleLuaPush(L);
        return 0;
    }
};
#pragma optimize("", on)
