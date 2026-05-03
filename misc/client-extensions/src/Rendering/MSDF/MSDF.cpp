#include "MSDF.h"
#include "MSDFFont.h"
#include "MSDFCache.h"
#include "MSDFManager.h"
#include "MSDFShaders.h"
#include "Hooks.h"
#include <cstdlib>
#include <cstring>
#include <ranges>
#include <immintrin.h>

namespace {
    uint32_t g_runtimeVBSize = 0;

    IDirect3DPixelShader9* s_cachedPS = nullptr;
    IDirect3DVertexShader9* s_cachedVS = nullptr;

    CLIENT_FUNCTION(CGxString__CheckGeometry_ProcessGeometryCallSite, 0x006C4B09, __cdecl, void, ())
    constexpr uintptr_t CGxString__CheckGeometry_ProcessGeometryCallSite_jmpback = 0x006C4B10;

    CLIENT_FUNCTION(CGxString__CheckGeometry_PrefetchLoopSite, 0x006C4AF3, __cdecl, void, ())
    constexpr uintptr_t CGxString__CheckGeometry_PrefetchLoopSite_loopstart = 0x006C4B00;

    CLIENT_FUNCTION(CGxString__GetGlyphYMetrics_BaselineSite, 0x006C8C71, __cdecl, void, ())
    constexpr uintptr_t CGxString__GetGlyphYMetrics_BaselineSite_jmpback = 0x006C8C77;

    CLIENT_FUNCTION(CGxDevice__AllocateFontIndexBuffer_SizeSite, 0x006C480C, __cdecl, void, ())
    constexpr uintptr_t CGxDevice__AllocateFontIndexBuffer_SizeSite_jmpback = 0x006C4811;

    CLIENT_FUNCTION(CGxDevice__InitFontIndexBuffer_PoolCreateSite, 0x006C47BD, __cdecl, void, ())
    constexpr uintptr_t CGxDevice__InitFontIndexBuffer_PoolCreateSite_jmpback = 0x006C47D8;

    CLIENT_FUNCTION(IGxuFontProcessBatch_ReturnSite, 0x006C4CC4, __cdecl, void, ())
    constexpr uintptr_t IGxuFontProcessBatch_ReturnSite_jmpback = 0x006C4CC9;

    CLIENT_FUNCTION(CGxDevice__BufStream_FontVertexCountSite, 0x006C4B40, __cdecl, void, ())
    constexpr uintptr_t CGxDevice__BufStream_FontVertexCountSite_jmpback = 0x006C4B45;

    CLIENT_FUNCTION(CGxString__CheckGeometry_BufferAllocInitSite, 0x006C4B64, __cdecl, void, ())
    constexpr uintptr_t CGxString__CheckGeometry_BufferAllocInitSite_jmpback = 0x006C4B70;

    CLIENT_FUNCTION(CGxString__CheckGeometry_BufferAllocGrowSite, 0x006C4C67, __cdecl, void, ())
    constexpr uintptr_t CGxString__CheckGeometry_BufferAllocGrowSite_jmpback = 0x006C4C8A;

    CLIENT_FUNCTION(CGxString__CheckGeometry_BufferAllocFlushSite, 0x006C4C36, __cdecl, void, ())
    constexpr uintptr_t CGxString__CheckGeometry_BufferAllocFlushSite_jmpback = 0x006C4C4B;

	CVar* s_cvar_MSDFMode;
    int s_msdfMode = 1;
    bool s_freeTypeInitHooked = false;
	std::vector<uint32_t> s_prefetchPayload;
 
    float s_lastControlFlag[4] = { -1.0f, -1.0f, -1.0f, -1.0f };
    IDirect3DTexture9* s_lastAtlasTextures[MSDF::MAX_ATLAS_PAGES] = { nullptr };
    MSDFFont* s_lastFontHandle = nullptr;
 
 
    void __cdecl PrefetchCodepoints(CGxString* pThis) {
        if (s_prefetchPayload.empty()) return;
        if (!pThis || reinterpret_cast<uintptr_t>(pThis) & 1) return;
 
        // sort everything in a batch to avoid redundant texture locks/unlocks in UploadGlyphToAtlas
        if (MSDFFont* fontHandle = MSDFFont::Get(pThis->GetFontFace())) {
            std::ranges::sort(s_prefetchPayload);
            auto [first, last] = std::ranges::unique(s_prefetchPayload);
            s_prefetchPayload.erase(first, last);
            
            for (uint32_t codepoint : s_prefetchPayload) {
                fontHandle->GetGlyph(codepoint);
            }
        }
        s_prefetchPayload.clear();
        s_prefetchPayload.reserve(256); // ensure we don't realloc frequently
    }

    void __fastcall ProcessGeometry(CGxString* pThis) {
        if (!(pThis->m_flags & 0x40000000)) return;
 
        MSDFFont* fontHandle = MSDFFont::Get(pThis->GetFontFace());
        if (!fontHandle) return;
 
        CGxFontGeomBatch* batch = pThis->m_geomBuffers[0];
        if (!batch || batch->m_verts.m_count < 4) return;
 
        TSGrowableArray<CGxFontVertex>& verts = batch->m_verts;
        if (verts.m_count < 4) return;
 		
    	CGxFont* fontObj = pThis->m_fontObj;
        const uint32_t flags = fontObj->m_atlasPages[0].m_flags;
        const bool is3d = pThis->m_flags & 0x80;
        const float fontSizeMult = static_cast<float>(pThis->m_fontSizeMult);
        const float fontOffs = !is3d ? ((flags & 8) ? 4.5f : ((flags & 1) ? 2.5f : 0.0f)) : 0.0f;
        const float baselineOffs = (fontOffs > 0.0f) ? 1.0f : 0.0f;
        const float scale = static_cast<float>((is3d ? fontSizeMult : CGxuFont::GetFontEffectiveHeight(is3d, fontSizeMult) * 0.98) / MSDF::SDF_RENDER_SIZE);
        const float pad = static_cast<float>(MSDF::SDF_SPREAD * scale);
 
        const __m128 vScale = _mm_set1_ps(scale);
        const __m128 vPad = _mm_set1_ps(pad);
        const __m128 vFontOffsHalf = _mm_set1_ps(fontOffs * 0.5f);
        const __m128 vBaselineOffs = _mm_set1_ps(baselineOffs);
 
        for (uint32_t q = 0; q < verts.m_count; q += 4) {
            CGxFontVertex* vBase = &verts.m_data[q];
            if (vBase[0].u > 1.0f) {
                const uint32_t codepoint = static_cast<uint32_t>(vBase[0].u - 1.0f);
 
                const GlyphMetrics* gm = fontHandle->GetGlyph(codepoint);
                if (!gm) continue;
 
                CGxGlyphCacheEntry* entry = fontObj->GetOrCreateGlyphEntry(codepoint);
                if (!entry) continue;
 
                const float leftOffs = static_cast<float>(fontObj->GetBearingX(entry, is3d, fontSizeMult));
                const float bitmapLeft = is3d ? leftOffs : gm->bitmapLeft * scale - leftOffs;
                const float leftCorrection = (bitmapLeft != leftOffs ? bitmapLeft + 1.0f : 0.0f);
 
                const float nLeft = vBase[0].pos.X + leftCorrection - pad + (fontOffs * 0.5f);
                const float nRight = nLeft + (gm->width * scale);
                const float nTop = vBase[1].pos.Y + (gm->bitmapTop * scale) + pad - baselineOffs;
                const float nBottom = nTop - (gm->height * scale);
 
                // Position update
                vBase[0].pos.X = nLeft;  vBase[0].pos.Y = nBottom;
                vBase[1].pos.X = nLeft;  vBase[1].pos.Y = nTop;
                vBase[2].pos.X = nRight; vBase[2].pos.Y = nBottom;
                vBase[3].pos.X = nRight; vBase[3].pos.Y = nTop;
 
                // UV update with page encoding
                const float uSign = (gm->atlasPageIndex & 1) ? -1.0f : 1.0f;
                const float vSign = (gm->atlasPageIndex & 2) ? -1.0f : 1.0f;
                const __m128 vUV_Signs = _mm_setr_ps(uSign, vSign, uSign, vSign);
 
                __m128 uv01 = _mm_setr_ps(gm->u0, gm->v0, gm->u0, gm->v1);
                __m128 uv23 = _mm_setr_ps(gm->u1, gm->v0, gm->u1, gm->v1);
                
                uv01 = _mm_mul_ps(uv01, vUV_Signs);
                uv23 = _mm_mul_ps(uv23, vUV_Signs);
 
                vBase[0].u = _mm_cvtss_f32(uv01);
                vBase[0].v = _mm_cvtss_f32(_mm_shuffle_ps(uv01, uv01, _MM_SHUFFLE(0, 0, 0, 1)));
                vBase[1].u = _mm_cvtss_f32(_mm_shuffle_ps(uv01, uv01, _MM_SHUFFLE(0, 0, 0, 2)));
                vBase[1].v = _mm_cvtss_f32(_mm_shuffle_ps(uv01, uv01, _MM_SHUFFLE(0, 0, 0, 3)));
 
                vBase[2].u = _mm_cvtss_f32(uv23);
                vBase[2].v = _mm_cvtss_f32(_mm_shuffle_ps(uv23, uv23, _MM_SHUFFLE(0, 0, 0, 1)));
                vBase[3].u = _mm_cvtss_f32(_mm_shuffle_ps(uv23, uv23, _MM_SHUFFLE(0, 0, 0, 2)));
                vBase[3].v = _mm_cvtss_f32(_mm_shuffle_ps(uv23, uv23, _MM_SHUFFLE(0, 0, 0, 3)));
            }
        }
        pThis->m_flags &= ~0x40000000;
 
        // store eviction count to later force engine to re-calc geometry when msdf page gets evicted
        uint32_t versionToken = (fontHandle->GetAtlasEvictionCount() & 0x7F) | 0x80;
        pThis->m_flags = (pThis->m_flags & 0x00FFFFFF) | (versionToken << 24);
    }


    bool __fastcall CGxString__CheckGeometryHk(CGxString* pThis) {
        if (MSDFFont* fontHandle = MSDFFont::Get(pThis->GetFontFace())) {
            // force re-calc geometry if any msdf page was evicted
            uint32_t highByte = (pThis->m_flags >> 24) & 0xFF;
            if ((highByte & 0x80) != 0) {
                uint8_t storedVersion = highByte & 0x7F;
                uint8_t currentVersion = static_cast<uint8_t>(fontHandle->GetAtlasEvictionCount() & 0x7F);
                if (storedVersion != currentVersion) {
                    pThis->ClearInstanceData();
                    pThis->m_flags &= 0x00FFFFFF;
                }
            }
        }
        CGxFontGeomBatch* batch = pThis->m_geomBuffers[0];
        if (!batch || !batch->m_verts.m_data) return pThis->CheckGeometry();

        g_runtimeVBSize += pThis->GetVertCountForPage(0);
        return pThis->CheckGeometry();
    }

    void __fastcall CGxString__WriteGeometryHk(CGxString* pThis, void* edx, int destPtr, int index, int vertIndex, int vertCount) {
        pThis->WriteGeometry(destPtr, index, vertIndex, vertCount);

        MSDFFont* fontHandle = MSDFFont::Get(pThis->GetFontFace());
        if (!fontHandle) {
            if (IDirect3DDevice9* device = D3D::GetDevice()) {
                constexpr float resetControl[4] = { 0, 0, 0, 0 };
                device->SetPixelShaderConstantF(MSDF::SDF_SAMPLER_SLOT, resetControl, 1);
                device->SetVertexShaderConstantF(MSDF::SDF_SAMPLER_SLOT, resetControl, 1);
            }
            return;
        }

        IDirect3DDevice9* device = D3D::GetDevice();
        if (!device) return;

        if (s_lastFontHandle != fontHandle) {
            for (uint32_t pageIdx = 0; pageIdx < fontHandle->GetAtlasPageCount(); ++pageIdx) {
                auto* atlasTexture = fontHandle->GetAtlasPage(pageIdx);
                IDirect3DTexture9* tex = atlasTexture ? atlasTexture->texture : nullptr;
 
                if (s_lastAtlasTextures[pageIdx] != tex) {
                    uint32_t slot = (/* max d3d9 tex slots */ 15 - MSDF::MAX_ATLAS_PAGES + 1) + pageIdx;
                    device->SetTexture(slot, tex);
                    if (tex) {
                        device->SetSamplerState(slot, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
                        device->SetSamplerState(slot, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
                        device->SetSamplerState(slot, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
                        device->SetSamplerState(slot, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                        device->SetSamplerState(slot, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
                    }
                    s_lastAtlasTextures[pageIdx] = tex;
                }
            }
            s_lastFontHandle = fontHandle;
        }
 
        const uint32_t flags = pThis->m_fontObj->m_atlasPages[0].m_flags;
        const bool is3d = pThis->m_flags & 0x80;
        const float controlFlag[4] = {
            is3d ? pThis->m_fontObj->m_rasterTargetSize : static_cast<float>(CGxuFont::GetFontEffectiveHeight(is3d, pThis->m_fontSizeMult)),
            is3d ? 0.0f : ((flags & 8) ? 2.0f : ((flags & 1) ? 1.0f : 0.0f)),
            MSDF::SDF_SPREAD, MSDF::ATLAS_SIZE
        };
 
        if (memcmp(s_lastControlFlag, controlFlag, sizeof(controlFlag)) != 0) {
            device->SetPixelShaderConstantF(MSDF::SDF_SAMPLER_SLOT, controlFlag, 1);
            device->SetVertexShaderConstantF(MSDF::SDF_SAMPLER_SLOT, controlFlag, 1);
            memcpy(s_lastControlFlag, controlFlag, sizeof(controlFlag));
        }
    }

    void __fastcall CGxuFontRenderBatchHk(CGxuFont* pThis) {
        pThis->RenderBatch();
 
        // invalidate cache as other UI elements might have changed states
        s_lastFontHandle = nullptr;
        memset(s_lastAtlasTextures, 0, sizeof(s_lastAtlasTextures));
        memset(s_lastControlFlag, 0xFF, sizeof(s_lastControlFlag));
 
        if (IDirect3DDevice9* device = D3D::GetDevice()) {
            // reset since font PS also handles UI elements
            constexpr float resetControl[4] = { 0, 0, 0, 0 };
            device->SetPixelShaderConstantF(MSDF::SDF_SAMPLER_SLOT, resetControl, 1);
            device->SetVertexShaderConstantF(MSDF::SDF_SAMPLER_SLOT, resetControl, 1);
        }
    }

    char __cdecl GxuFontGlyphRenderGlyphHk(FT_Face fontFace, uint32_t fontSize, uint32_t codepoint, uint32_t pageInfo, CGxGlyphMetrics* resultBuffer, uint32_t outline_flag, uint32_t pad) {
        const char result = CGxuFont::RenderGlyph(fontFace, fontSize, codepoint, pageInfo, resultBuffer, outline_flag, pad);
        if (MSDFFont::Get(fontFace)) {
            // verAdv is distance from the top of the quad to the top of the glyph, bearingY is the quad's vert correction (descenders)
            resultBuffer->m_bearingY -= resultBuffer->m_verAdv;
        }
        return result;
    }

    CGxGlyphCacheEntry* __fastcall CGxString__GetOrCreateGlyphEntryHk(CGxFont* fontObj, void* edx, uint32_t codepoint) {
        CGxGlyphCacheEntry* result = fontObj->GetOrCreateGlyphEntry(codepoint);
        if (result && MSDFFont::Get(CGxString::GetFontFace(fontObj->m_ftWrapper))) {
            result->m_metrics.u0 = 1.0f + codepoint; // store codepoint
            // keep everything at page0 - this is required due to how the engine handles strings with text spanning across multiple atlas pages
            // in short, it renders page0 for all CGxString instances, then page1, and so on
            // this makes shadows on adjacent glyphs overlap due to the lack of depth info
            // e.g. engine renders page0 ['some hing'] -> renders page1 ['t'] -> the shadows of ['t'] now overlaps the countour of ['h']
            result->m_cellIndexMin = 0;
            result->m_cellIndexMax = 0;
            result->m_texturePageIndex = 0;
        }
        return result;
    }

    int __fastcall CGxString__InitializeTextLineHk(CGxString* pThis, void* edx, char* text, int textLength, int* a4, C3Vector* startPos, void* a6, int a7) {
        const int result = pThis->InitializeTextLine(text, textLength, a4, startPos, a6, a7);

        // 1-st pass - only collect codepoints
        // all of thes will then get sorted to ensure sequentiality
        // == minimal syscall churn @ GetGlyph -> cold cache path == fewer stutters
        if (pThis->m_flags & 0x40000000) return result;
        for (char* p = pThis->m_text; *p; ++p) {
            s_prefetchPayload.push_back(static_cast<uint8_t>(*p));
        }
        pThis->m_flags |= 0x40000000;
        return result;
    }

    // run the original loop, then prefetch once per frame
    // drop out back to loopstart and loop again (ebx preserved)
    // this time, call processGeom to resolve, now with a warmed up cache
    __declspec(naked) void CGxString__CheckGeometry_PrefetchLoopSiteHk() {
        __asm {
            pushad;
            mov edi, ebx;

            test ebx, ebx;
            jnz pre_pass_loop;

            xor ebx, ebx;
            lea esp, [esp + 0]; // ??? (0x006C4B1C)

        pre_pass_loop:
            test edi, edi;
            jz pre_pass_done;
            test di, 1;
            jnz pre_pass_done;

            mov ecx, edi;
            call CGxString__CheckGeometryHk;

            mov eax, [esi + 1Ch];
            add eax, edi;
            mov edi, [eax + 4];
            jmp pre_pass_loop;

        pre_pass_done:
            popad;
            push ebx;
            call PrefetchCodepoints;
            add esp, 4;
            jmp CGxString__CheckGeometry_PrefetchLoopSite_loopstart;
        }
    }

    __declspec(naked) void CGxString__CheckGeometry_ProcessGeometryCallSiteHk() {
        __asm {
            mov ecx, ebx;
            call ProcessGeometry;
            jmp CGxString__CheckGeometry_ProcessGeometryCallSite_jmpback;
        }
    }

     bool __cdecl MSDFFont_Get(FT_Face face) { return MSDFFont::Get(face); }
    __declspec(naked) void CGxString__GetGlyphYMetrics_BaselineSiteHk() { // skip the orig baseline calc
        __asm {
            mov edx, [ecx + 54h];
            pushad;
            push ecx;
            call MSDFFont_Get;
            add esp, 4;
            test al, al;
            popad;
            jz font_unsafe;
            xor ecx, ecx;
            jmp CGxString__GetGlyphYMetrics_BaselineSite_jmpback;
        font_unsafe:
            mov ecx, [edx + 68h];
            jmp CGxString__GetGlyphYMetrics_BaselineSite_jmpback;
        }
    }

    // allocate as much as needed for every cgxstring in a batch to perform exactly one draw at max
    // makes sure the overlap from having a single piece of text rendered with multiple draws isn't happening
    // this is probably overkill and irrelevant for 99.99% of the actual draw cases where text vertCount is naturally < hardcoded 2004-era 2048 buffer
    // but...
    __declspec(naked) void CGxDevice__AllocateFontIndexBuffer_SizeSiteHk() {
        __asm {
            mov ebx, 3FFFh;
            jmp CGxDevice__AllocateFontIndexBuffer_SizeSite_jmpback;
        }
    }
    __declspec(naked) void CGxDevice__InitFontIndexBuffer_PoolCreateSiteHk() {
        __asm {
            push 30000h;
            push 0;
            push 1;
            call MSDFClient::CGxDevice__PoolCreate;
            mov ecx, dword ptr ds : [0C5DF88h] ; // g_theGxDevicePtr
            push 0;
            push 1801Ah;
            jmp CGxDevice__InitFontIndexBuffer_PoolCreateSite_jmpback;
        }
    }
    __declspec(naked) void IGxuFontProcessBatch_ReturnSiteHk() {
        __asm {
            mov g_runtimeVBSize, 0;
            pop ebx;
            pop esi;
            mov esp, ebp;
            pop ebp;
            jmp IGxuFontProcessBatch_ReturnSite_jmpback;
        }
    }
    __declspec(naked) void CGxDevice__BufStream_FontVertexCountSiteHk() { // clamp to [2048-65532]
        __asm {
            mov eax, g_runtimeVBSize;
            cmp eax, 800h;
            jge check_upper;
            mov eax, 800h;
            jmp do_push;
        check_upper:
            cmp eax, 0FFFCh;
            jle do_push;
            mov eax, 0FFFCh;
        do_push:
            mov g_runtimeVBSize, eax;
            push eax;
            jmp CGxDevice__BufStream_FontVertexCountSite_jmpback;
        }
    }
    __declspec(naked) void CGxString__CheckGeometry_BufferAllocInitSiteHk() {
        __asm {
            xor eax, eax;
            mov esi, 0B4h;
            mov ebx, g_runtimeVBSize;
            jmp CGxString__CheckGeometry_BufferAllocInitSite_jmpback;
        }
    }
    __declspec(naked) void CGxString__CheckGeometry_BufferAllocGrowSiteHk() {
        __asm {
            cmp ebx, g_runtimeVBSize;
            jz orig_skip;
            mov ecx, g_runtimeVBSize;
            sub ecx, ebx;
            push ecx;
            lea edx, [ebp - 18h];
            push edx;
            call MSDFClient::CGxDevice__FlushBuffer;
            add esp, 8;
            mov edi, eax;
            mov ebx, g_runtimeVBSize;
            jmp CGxString__CheckGeometry_BufferAllocGrowSite_jmpback;
        orig_skip:
            jmp CGxString__CheckGeometry_BufferAllocGrowSite_jmpback;
        }
    }
    __declspec(naked) void CGxString__CheckGeometry_BufferAllocFlushSiteHk() {
        __asm {
            push g_runtimeVBSize;
            push eax;
            call MSDFClient::CGxDevice__FlushBuffer;
            add esp, 8;
            mov edi, eax;
            mov ebx, g_runtimeVBSize;
            jmp CGxString__CheckGeometry_BufferAllocFlushSite_jmpback;
        }
    }


    int __cdecl FreeType_NewMemoryFaceHk(FT_Library library, const FT_Byte* file_base,
        FT_Long file_size, FT_Long face_index, FT_Face* aface) {
        if (!MSDF::g_realFtLibrary && FT_Init_FreeType(&MSDF::g_realFtLibrary) != 0) return -1;

        const int result = FT_New_Memory_Face(library, file_base, file_size, face_index, aface);
        if (result != 0 || !aface || !*aface) return result;

        MSDFFont::Register(*aface, file_base, file_size);
        return result;
    }

    int __cdecl FreeType_SetPixelSizesHk(FT_Face face, FT_UInt pixel_width, FT_UInt pixel_height) {
        return FT_Set_Pixel_Sizes(face, pixel_width, pixel_height);
    }

    int __cdecl FreeType_LoadGlyphHk(FT_Face face, FT_ULong glyph_index, FT_Int32 load_flags) {
        return FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER | FT_LOAD_NO_HINTING); // original flags CTD
    }

    FT_UInt __cdecl FreeType_GetCharIndexHk(FT_Face face, FT_ULong charcode) {
        return FT_Get_Char_Index(face, charcode);
    }

    int __cdecl FreeType_GetKerningHk(FT_Face face, FT_UInt left_glyph, FT_UInt right_glyph, FT_UInt kern_mode, FT_Vector* akerning) {
        return FT_Get_Kerning(face, left_glyph, right_glyph, kern_mode, akerning);
    }

    int __cdecl FreeType_Done_FaceHk(FT_Face face) {
        MSDFFont::Unregister(face);
        return FT_Done_Face(face);
    }

    int __cdecl FreeType_Done_FreeTypeHk(FT_Library library) {
        MSDFFont::Shutdown();
        if (MSDF::g_msdfFreetype) {
            msdfgen::deinitializeFreetype(MSDF::g_msdfFreetype);
            MSDF::g_msdfFreetype = nullptr;
        }
        if (MSDF::g_realFtLibrary) {
            FT_Done_FreeType(MSDF::g_realFtLibrary);
            MSDF::g_realFtLibrary = nullptr;
        }
        return 0;
    }

    int __cdecl FreeType_NewFaceHk(int* library, int face_descriptor_ptr) {
        return 1; // ../Fonts/* init at startup, falls back to FreeType_NewMemoryFaceHk
    }

    int __cdecl FreeType_InitHk(void* memory, FT_Library* alibrary) {
        if (!MSDF::INITIALIZED) {
            std::string localeStr = MSDF::GetGameLocale();
            const char* locale = localeStr.c_str();
            MSDF::IS_CJK = locale && (strcmp(locale, "zhCN") == 0 ||
                strcmp(locale, "zhTW") == 0 ||
                strcmp(locale, "koKR") == 0);

            MSDF::INITIALIZED = true;
            MSDF::ALLOW_UNSAFE_FONTS = s_msdfMode > 1;

            if (MSDF::IS_CJK) return MSDFClient::FreeType__Init(memory, alibrary);

            DetourTransactionBegin();
            DetourUpdateThread(GetCurrentThread());
            Hooks::Detour(&MSDFClient::FreeType__NewMemoryFace, FreeType_NewMemoryFaceHk);
            Hooks::Detour(&MSDFClient::FreeType__NewFace, FreeType_NewFaceHk);
            Hooks::Detour(&MSDFClient::FreeType__DoneFace, FreeType_Done_FaceHk);
            Hooks::Detour(&MSDFClient::FreeType__SetPixelSizes, FreeType_SetPixelSizesHk);
            Hooks::Detour(&MSDFClient::FreeType__GetCharIndex, FreeType_GetCharIndexHk);
            Hooks::Detour(&MSDFClient::FreeType__LoadGlyph, FreeType_LoadGlyphHk);
            Hooks::Detour(&MSDFClient::FreeType__GetKerning, FreeType_GetKerningHk);
            Hooks::Detour(&MSDFClient::FreeType__DoneFreeType, FreeType_Done_FreeTypeHk);

            Hooks::Detour(&MSDFClient::CGxuFont__RenderBatch, CGxuFontRenderBatchHk);
            Hooks::Detour(&MSDFClient::CGxuFont__RenderGlyph, GxuFontGlyphRenderGlyphHk);
            Hooks::Detour(&MSDFClient::CGxFont__GetOrCreateGlyphEntry, CGxString__GetOrCreateGlyphEntryHk);

            Hooks::Detour(&CGxDevice__AllocateFontIndexBuffer_SizeSite, CGxDevice__AllocateFontIndexBuffer_SizeSiteHk);
            Hooks::Detour(&CGxDevice__InitFontIndexBuffer_PoolCreateSite, CGxDevice__InitFontIndexBuffer_PoolCreateSiteHk);

            Hooks::Detour(&CGxString__GetGlyphYMetrics_BaselineSite, CGxString__GetGlyphYMetrics_BaselineSiteHk);
            Hooks::Detour(&CGxString__CheckGeometry_PrefetchLoopSite, CGxString__CheckGeometry_PrefetchLoopSiteHk);
            Hooks::Detour(&CGxString__CheckGeometry_ProcessGeometryCallSite, CGxString__CheckGeometry_ProcessGeometryCallSiteHk);

            Hooks::Detour(&IGxuFontProcessBatch_ReturnSite, IGxuFontProcessBatch_ReturnSiteHk);
            Hooks::Detour(&CGxDevice__BufStream_FontVertexCountSite, CGxDevice__BufStream_FontVertexCountSiteHk);
            Hooks::Detour(&CGxString__CheckGeometry_BufferAllocInitSite, CGxString__CheckGeometry_BufferAllocInitSiteHk);
            Hooks::Detour(&CGxString__CheckGeometry_BufferAllocGrowSite, CGxString__CheckGeometry_BufferAllocGrowSiteHk);
            Hooks::Detour(&CGxString__CheckGeometry_BufferAllocFlushSite, CGxString__CheckGeometry_BufferAllocFlushSiteHk);

            Hooks::Detour(&MSDFClient::CGxString__CheckGeometry, CGxString__CheckGeometryHk);
            Hooks::Detour(&MSDFClient::CGxString__WriteGeometry, CGxString__WriteGeometryHk);
            Hooks::Detour(&MSDFClient::CGxString__InitializeTextLine, CGxString__InitializeTextLineHk);
            DetourTransactionCommit();

            D3D::RegisterOnDestroy([]() {
                if (s_cachedPS) { s_cachedPS->Release(); s_cachedPS = nullptr; }
                if (s_cachedVS) { s_cachedVS->Release(); s_cachedVS = nullptr; }
                MSDFFont::ClearAllCache();
                });

            D3D::RegisterPixelShaderInit([](CGxDevice::ShaderData* shaderData) {
                if (shaderData != MSDF::g_FontPixelShader && MSDF::g_FontPixelShader != nullptr) return;
                if (!s_cachedPS) {
                    s_cachedPS = D3D::CompilePixelShader({
                        .shaderCode = pixelShaderHLSL,
                        .target = "ps_3_0"
                        });
                }
                if (s_cachedPS) {
                    if (shaderData->pixel_shader) shaderData->pixel_shader->Release();
                    shaderData->pixel_shader = s_cachedPS;
                    shaderData->compilation_flags = 1;
                }
                });

            s_cachedPS = D3D::CompilePixelShader({
                .shaderCode = pixelShaderHLSL,
                .target = "ps_3_0"
                });
            if (s_cachedPS) {
                if (MSDF::g_FontPixelShader->pixel_shader) MSDF::g_FontPixelShader->pixel_shader->Release();
                MSDF::g_FontPixelShader->pixel_shader = s_cachedPS;
                MSDF::g_FontPixelShader->compilation_flags = 1;
            }

            D3D::RegisterVertexShaderInit([](CGxDevice::ShaderData* shaderData) {
                if (shaderData != MSDF::g_FontVertexShader && MSDF::g_FontVertexShader != nullptr) return;
                if (!s_cachedVS) {
                    s_cachedVS = D3D::CompileVertexShader({
                        .shaderCode = vertexShaderHLSL,
                        .target = "vs_3_0"
                        });
                }
                if (s_cachedVS) {
                    if (shaderData->vertex_shader) shaderData->vertex_shader->Release();
                    shaderData->vertex_shader = s_cachedVS;
                    shaderData->compilation_flags = 1;
                }
                });

            s_cachedVS = D3D::CompileVertexShader({
                .shaderCode = vertexShaderHLSL,
                .target = "vs_3_0"
                });
            if (s_cachedVS) {
                if (MSDF::g_FontVertexShader->vertex_shader) MSDF::g_FontVertexShader->vertex_shader->Release();
                MSDF::g_FontVertexShader->vertex_shader = s_cachedVS;
                MSDF::g_FontVertexShader->compilation_flags = 1;
            }

            s_prefetchPayload.reserve(16383);

            MSDFClient::CGxDevice__InitFontIndexBuffer(); // engine has already run it at this point
        }
        else if (MSDF::IS_CJK) {
            return MSDFClient::FreeType__Init(memory, alibrary);
        }
        if (const FT_Error error = FT_Init_FreeType(&MSDF::g_realFtLibrary)) return error;

        if (alibrary) *alibrary = MSDF::g_realFtLibrary;

        MSDF::g_msdfFreetype = msdfgen::initializeFreetype();
        if (!MSDF::g_msdfFreetype) {
            FT_Done_FreeType(MSDF::g_realFtLibrary);
            MSDF::g_realFtLibrary = nullptr;
            return -1;
        }
        return 0;
    }

    int CVarHandler_MSDFMode(CVar*, const char*, const char* value, void*) {
        s_msdfMode = value ? std::atoi(value) : 1;
        if (s_msdfMode > 0 && !s_freeTypeInitHooked) {
            DetourTransactionBegin();
            DetourUpdateThread(GetCurrentThread());
            Hooks::Detour(&MSDFClient::FreeType__Init, FreeType_InitHk);
            DetourTransactionCommit();
            s_freeTypeInitHooked = true;
        }
        return 1;
    }
}

void MSDF::initialize() {
    s_cvar_MSDFMode = CVar::Register(
        "MSDFMode",
        "0 = Disabled; 1 = Enabled; 2 = Enabled for unsafe/self-intersecting fonts",
        1,
        "1",
        CVarHandler_MSDFMode,
        0, 0, 0, 0);

    const char* value = s_cvar_MSDFMode && s_cvar_MSDFMode->m_str ? s_cvar_MSDFMode->m_str : "1";
    CVarHandler_MSDFMode(s_cvar_MSDFMode, nullptr, value, nullptr);
};
