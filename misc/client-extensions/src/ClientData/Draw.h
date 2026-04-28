#pragma once

#include <ClientMacros.h>
#include <ClientData/GxDevice.h>
#include <ClientData/GxTypes.h>
#include <ClientData/MathTypes.h>
#include <ClientData/SharedDefines.h>

#include <cstdint>

namespace ClientData
{
    CLIENT_FUNCTION(GxSceneClear, 0x6813B0, __cdecl, void, (uint32_t, CImVector))
    CLIENT_FUNCTION(GxRsSet, 0x408240, __cdecl, void, (EGxRenderState, void*))
    CLIENT_FUNCTION(GxRsSet_int32_t, 0x408BF0, __cdecl, void, (EGxRenderState, int32_t))
    CLIENT_FUNCTION(GxPrimIndexPtr, 0x681AB0, __cdecl, void, (uint32_t, uint16_t*))
    CLIENT_FUNCTION(GxPrimVertexPtr, 0x682400, __cdecl, void,
                    (uint32_t, C3Vector*, uint32_t, C3Vector*, uint32_t, CImVector*, uint32_t, C2Vector*, uint32_t,
                     C2Vector*, uint32_t))
    CLIENT_FUNCTION(GxXformViewProjNativeTranspose, 0x408110, __cdecl, void, (C44Matrix*))
    CLIENT_FUNCTION(GxXformPush, 0x616AD0, __cdecl, void, (EGxXform, C44Matrix*))
}
