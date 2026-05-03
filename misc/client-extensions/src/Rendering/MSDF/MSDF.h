#pragma once
#include <windows.h>
#undef min
#undef max

#include <ClientMacros.h>
#include "GameClient.h"
#include "D3D.h"

#include <cstdint>
#include <string>
#include <vector>

#include <msdfgen.h>
#include <msdfgen-ext.h>

// Forward declarations moved to MSDFCacheTypes.h or kept here if needed
class MSDFCache;
class MSDFFont;

namespace MSDF {
	// ----  if you want overkill quality, try raising these
	inline constexpr uint32_t ATLAS_SIZE = 1024; // 1024-2048
	inline constexpr uint32_t PREGEN_START_KEY = VK_F11;
	inline constexpr uint32_t SDF_SAMPLER_SLOT = 23;
	inline constexpr uint32_t ATLAS_GUTTER = 12; // usually spread + 2-4
	inline constexpr uint32_t SDF_RENDER_SIZE = 64; // 48-128
	inline constexpr uint32_t SDF_SPREAD = 8; // 6-12
	inline constexpr D3DFORMAT D3DFMT = D3DFMT_A8R8G8B8; // D3DFMT_A8R8G8B8-D3DFMT_A16B16G16R16
    inline constexpr size_t CJK_CACHE_THRESHOLD = 10000;
	// ----

    CLIENT_ADDRESS(CGxDevice::ShaderData*, g_FontPixelShaderPtr, 0x00C7D2CC)
    CLIENT_ADDRESS(CGxDevice::ShaderData*, g_FontVertexShaderPtr, 0x00C7D2D0)
	inline CGxDevice::ShaderData*& g_FontPixelShader = *g_FontPixelShaderPtr;
	inline CGxDevice::ShaderData*& g_FontVertexShader = *g_FontVertexShaderPtr;

	inline FT_Library g_realFtLibrary = nullptr;
	inline msdfgen::FreetypeHandle* g_msdfFreetype = nullptr;

    inline constexpr uint32_t MAX_ATLAS_PAGES = 4;

	inline bool IS_CJK = false;
	inline bool INITIALIZED = false;
	inline bool ALLOW_UNSAFE_FONTS = false; // due to how distance fields are calculated, some fonts with self-intersecting contours (e.g. diediedie) will break

	inline const bool IS_WIN10 = []() {
		HMODULE hKernel = GetModuleHandleW(L"kernelbase.dll");
		if (!hKernel) return false;

		return (GetProcAddress(hKernel, "VirtualAlloc2") != nullptr &&
			GetProcAddress(hKernel, "MapViewOfFile3") != nullptr &&
			GetProcAddress(hKernel, "UnmapViewOfFile2") != nullptr);
		}();

	inline std::string GetGameLocale() {
		CVar* locale = CVar::Get("locale");
		return (locale && locale->m_str) ? locale->m_str : std::string{};
	}

    void initialize();
};
