#pragma once

#include <ClientData/SharedDefines.h>

using namespace ClientData;

static std::vector<ZoneLightData> GlobalZoneLightData;

class WorldDataExtensions {
private:
    static void Apply();

    static void FillZoneLightData();
    static void FindAndAddZoneLightEx(C3Vector* vec);

    friend class ClientExtensions;
};
