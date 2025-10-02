#pragma once

#include "TSMain.h"
#include "TSWorldObject.h"
#include "TSLua.h"


class TSUnit;
class TSAura;

class TC_GAME_API TSDynObj : public TSWorldObject {
  public:
    DynamicObject* obj;
    TSDynObj(DynamicObject*);

    void Remove();
    void SetDuration(int32 newDuration);
    TSUnit GetCaster();
    TSNumber<uint32> GetSpellId();
    TSSpellInfo GetSpellInfo();
    TSNumber<int32> GetDuration();
    void SetAura(TSAura aura);
    void RemoveAura();
  private:
    friend class TSLua;
};

LUA_PTR_TYPE(TSDynObj)
