#pragma once

#include "SharedDefines.h"

static std::vector<ZoneLightData> GlobalZoneLightData;
static std::vector<OcclusionVolumeData> GlobalOcclusionVolumeData;

class WorldDataExtensions {
private:
    static void Apply();

    static void FillZoneLightData();
    static void FillOcclusionVolumeData();
    static void FindAndAddZoneLightEx(C3Vector* vec);

    friend class ClientExtensions;
};
