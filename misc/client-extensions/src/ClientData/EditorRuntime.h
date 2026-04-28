#pragma once

namespace ClientData
{
    class CGWorldFrameFull;
}

namespace ClientData::EditorRuntime
{
    void Apply();
    void OnGameClientInitialize();
    void OnGameClientDestroy();
    void OnWorldRender(CGWorldFrameFull* worldFrame);
}
