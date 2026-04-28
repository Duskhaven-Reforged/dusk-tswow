#pragma once

#include <ClientMacros.h>
#include <ClientData/GxBatch.h>
#include <ClientData/GxTypes.h>
#include <ClientData/MathTypes.h>

#include <cstdint>

namespace ClientData
{
    class CGxDevice;
    CLIENT_FUNCTION(CGxDevice_Push, 0x409670, __thiscall, void, (CGxDevice* device))
    CLIENT_FUNCTION(CGxDevice_Pop, 0x685FB0, __thiscall, void, (CGxDevice* device))

    struct CGxFormat
    {
        uint32_t apiSpecificModeID;
        bool hwTnL;
        bool hwCursor;
        int8_t fixLag;
        int8_t window;
        bool aspect;
        int32_t maximize;
        CGxFormat__Format depthFormat;
        C2iVector size;
        uint32_t backBufferCount;
        uint32_t multisampleCount;
        float multisampleQuality;
        CGxFormat__Format colorFormat;
        uint32_t refreshRate;
        uint32_t vsync;
        bool stereoEnabled;
        int32_t fixedFunction[6];
        C2iVector pos;
    };

    struct CGxCaps
    {
        int32_t m_numTmus;
        int32_t m_pixelCenterOnEdge;
        int32_t m_texelCenterOnEdge;
        int32_t m_numStreams;
        int32_t int10;
        EGxColorFormat m_colorFormat;
        uint32_t unk18;
        uint32_t m_maxIndex;
        int32_t m_generateMipMaps;
        int32_t m_texFmt[13];
        int32_t m_texTarget[4];
        uint32_t m_texNonPow2;
        uint32_t m_texMaxSize[4];
        int32_t m_rttFormat[13];
        uint32_t unkB0;
        int32_t m_shaderTargets[6];
        uint32_t m_shaderConstants[6];
        int32_t m_texFilterTrilinear;
        int32_t m_texFilterAnisotropic;
        uint32_t m_maxTexAnisotropy;
        int32_t m_depthBias;
        int32_t m_colorWrite;
        int32_t m_maxClipPlanes;
        int32_t m_hwCursor;
        int32_t m_occlusionQuery;
        int32_t m_pointParameters;
        float m_pointScaleMax;
        int32_t m_pointSprite;
        uint32_t m_blendFactor;
        uint32_t unk114[5];
        int32_t m_oglRtAlpha;
        int32_t m_stereoAvailable;
        int32_t int130;
        int32_t int134;
        int32_t int138;
    };

    struct CGxGammaRamp
    {
        uint16_t red[256];
        uint16_t green[256];
        uint16_t blue[256];
    };

    struct CGxMatrixStack
    {
        uint32_t m_level;
        int8_t m_dirty;
        C44Matrix m_mtx[4];
        uint32_t m_flags[4];
    };

    class CGxDevice
    {
      public:
        static CGxDevice* Get()
        {
            return *reinterpret_cast<CGxDevice**>(0x00C5DF88);
        }

        virtual void unk0();
        virtual void unk1();
        virtual void unk2();
        virtual void unk3();
        virtual void unk4();
        virtual void unk5();
        virtual void unk6();
        virtual void unk7();
        virtual void unk8();
        virtual void unk9();
        virtual void unk10();
        virtual void unk11();
        virtual void unk12();
        virtual void unk13();
        virtual void unk14();
        virtual void unk15();
        virtual void unk16();
        virtual void unk17();
        virtual void unk18();
        virtual void unk19();
        virtual void unk20();
        virtual void unk21();
        virtual void unk22();
        virtual void unk23();
        virtual void unk24();
        virtual void unk25();
        virtual void unk26();
        virtual void unk27();
        virtual void unk28();
        virtual void unk29();
        virtual void unk30();
        virtual void unk31();
        virtual void unk32();
        virtual void unk33();
        virtual void unk34();
        virtual void unk35();
        virtual void unk36();
        virtual void unk37();
        virtual void ScenePresent();
        virtual void unk39();
        virtual void XformSetProjection(C44Matrix* matrix);
        virtual void XformSetView(C44Matrix* matrix);
        virtual void Draw(CGxBatch* batch, int32_t indexed);

        void Push() { CGxDevice_Push(this); }
        void Pop() { CGxDevice_Pop(this); }

        char m_pushedStates[0x10];
        char m_stackOffsets[0x10];
        char m_dirtyStates[0x10];
        EGxPrim m_primType;
        int32_t m_indexLocked;
        int32_t m_vertexLocked;
        int32_t m_inBeginEnd;
        C3Vector m_primVertex;
        C2Vector m_primTexCoord[8];
        C3Vector m_primNormal;
        CImVector m_primColor;
        char m_primVertexArray[0x10];
        char m_primTexCoordArray[8][0x10];
        char m_primNormalArray[0x10];
        char m_primColorArray[0x10];
        char m_primIndexArray[0x10];
        uint32_t m_primMask;
        CRect m_defWindowRect;
        CRect m_curWindowRect;
        char m_deviceRestoredCallbacks[0x10];
        char m_textureRecreationCallbacks[0x10];
        char m_stereoChangedCallbacks[0x10];
        EGxApi m_api;
        uint32_t m_cpuFeatures;
        CGxFormat m_format;
        CGxCaps m_caps;
        uint32_t m_baseMipLevel;
        CGxGammaRamp m_gammaRamp;
        CGxGammaRamp m_systemGammaRamp;
        int32_t(__stdcall* m_windowProc)(void* window, uint32_t message, uintptr_t wparam, intptr_t lparam);
        int32_t m_context;
        int32_t intF5C;
        int32_t m_windowVisible;
        int32_t m_windowFocus;
        int32_t m_frameCount;
        int32_t m_viewportDirty;
        CBoundingBox m_viewport;
        C44Matrix m_projection;
        C44Matrix m_projNative;
        CGxMatrixStack m_xforms[11];
        CGxMatrixStack m_texGen[8];
        uint32_t m_clipPlaneMask;
    };
}
