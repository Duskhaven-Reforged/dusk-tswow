#pragma once

#include <ClientMacros.h>

#include <cstdint>

namespace ClientData::GameClient
{
    CLIENT_FUNCTION(InitializeGame, 0x405540, __cdecl, void, (int32_t, float, float, float))
    CLIENT_FUNCTION(DestroyGame, 0x406510, __cdecl, void, (bool, bool, bool))
    CLIENT_FUNCTION(DDCToNDC, 0x47C020, __cdecl, void, (float, float, float*, float*))
    CLIENT_FUNCTION(NDCToDDC, 0x47BFF0, __cdecl, void, (float, float, float*, float*))

    inline bool IsInitialized()
    {
        return *reinterpret_cast<bool*>(0x00B2F9A0);
    }
}
