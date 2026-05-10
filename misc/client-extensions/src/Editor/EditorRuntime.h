#pragma once

#include <ClientData/WorldFrame.h>
#include <ClientData/GameClient.h>
#include <ClientData/GameObject.h>
#include <cstdint>

namespace EditorRuntime
{
    bool SelectGameObject(uint64_t guid);
    void Apply();
    void OnGameClientInitialize();
    void OnGameClientDestroy();
    void ClearSelection();
    CGGameObject_C * SelectedGameObject();
    uint64_t CurrentSelectedGobGUID();
    void OnWorldRender(ClientData::CGWorldFrameFull* worldFrame);
} // namespace EditorRuntime
