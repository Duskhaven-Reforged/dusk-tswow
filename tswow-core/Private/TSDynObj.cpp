#include "TSIncludes.h"
#include "TSDynObj.h"
#include "TSGUID.h"

#include "DynamicObject.h"

TSDynObj::TSDynObj(DynamicObject *obj) : TSWorldObject(obj)
{
    this->obj = obj;
}