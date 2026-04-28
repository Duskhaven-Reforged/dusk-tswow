#pragma once

#include <ClientData/WorldFrame.h>

namespace EditorRuntime
{
    void Apply();
    void OnGameClientInitialize();
    void OnGameClientDestroy();
    void OnWorldRender(ClientData::CGWorldFrameFull* worldFrame);
} // namespace EditorRuntime
