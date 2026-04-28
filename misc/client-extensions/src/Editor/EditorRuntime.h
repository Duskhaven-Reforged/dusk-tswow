#pragma once

#include <ClientData/WorldFrame.h>

#include <cstdint>

namespace EditorRuntime
{
    bool SelectGameObject(uint64_t guid);
    void Apply();
    void OnGameClientInitialize();
    void OnGameClientDestroy();
    void OnWorldRender(ClientData::CGWorldFrameFull* worldFrame);
} // namespace EditorRuntime
