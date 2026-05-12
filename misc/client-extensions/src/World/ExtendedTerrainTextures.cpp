#include <World/ExtendedTerrainTextures.h>

#include <ClientDetours.h>
#include <ClientLua.h>
#include <Logger.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstring>
#include <cstdint>
#include <deque>
#include <functional>
#include <fstream>
#include <cmath>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <Windows.h>
#include <d3d9.h>
#include <numeric>

namespace D3D
{
    using ResourceCallback = std::function<void()>;
    using DrawPrimitiveCallback = std::function<void(IDirect3DDevice9*, D3DPRIMITIVETYPE, UINT, UINT)>;
    using DrawIndexedPrimitiveCallback =
        std::function<void(IDirect3DDevice9*, D3DPRIMITIVETYPE, INT, UINT, UINT, UINT, UINT)>;
    IDirect3DDevice9* GetDevice();
    void RegisterOnRelease(ResourceCallback const& callback);
    void RegisterOnRestore(ResourceCallback const& callback);
    void RegisterDrawPrimitiveCallback(DrawPrimitiveCallback const& callback);
    void RegisterDrawIndexedPrimitiveCallback(DrawIndexedPrimitiveCallback const& callback);
}

namespace
{
    constexpr uint8_t kNativeTerrainLayerCount = 4;
    constexpr uint8_t kMaxExtendedTerrainLayers = 16;
    constexpr char const* kForcedExtensionBlpPath = "tileset\\the blasted lands\\blastedlandsdirt01.blp";

    struct ChunkRequest
    {
        uintptr_t chunkPtr       = 0;
        int32_t localChunkX      = 0;
        int32_t localChunkY      = 0;
        int32_t worldChunkX      = 0;
        int32_t worldChunkY      = 0;
        int32_t adtTileX         = -1;
        int32_t adtTileY         = -1;
        int32_t chunkIndex       = -1;
        uintptr_t textureSetPtr  = 0;
        uintptr_t nativeChunkPtr = 0;
        uint32_t ownerGeneration = 0;
        bool tileLoadIdentity    = false;
        uint8_t nativeLayerCount = 0;
        uint32_t firstSeenMs     = 0;
        uint32_t lastSeenMs      = 0;
        uint32_t generation      = 0;
        bool stale               = false;
        bool coordsValid         = false;
        bool adtTileValid        = false;
        bool cacheLookupAttempted = false;
        bool cacheLookupHit       = false;
        std::string sourceStackHash;
    };

    struct PayloadRecord
    {
        uintptr_t chunkPtr       = 0;
        int32_t worldChunkX      = 0;
        int32_t worldChunkY      = 0;
        int32_t adtTileX         = -1;
        int32_t adtTileY         = -1;
        int32_t chunkIndex       = -1;
        uintptr_t textureSetPtr  = 0;
        uintptr_t nativeChunkPtr = 0;
        uint32_t ownerGeneration = 0;
        bool tileLoadIdentity    = false;
        int32_t textureBytes     = 0;
        int32_t materialBytes    = 0;
        int32_t textureHandle    = 0;
        int32_t retiredHandle    = 0;
        uint8_t savedLayerCount  = 0;
        uint32_t savedSlot0DiffuseHandle = 0;
        uint32_t savedCompositeHandle = 0;
        int32_t uploadAttempts   = 0;
        uint32_t firstSeenMs     = 0;
        uint32_t lastSeenMs      = 0;
        uint32_t lastGeneration  = 0;
        uint32_t lastUploadMs    = 0;
        bool resident            = false;
        bool pendingUpload       = false;
        bool uploadFailed        = false;
        bool swapped             = false;
        bool attached            = false;
        bool materialMapLoaded   = false;
        int32_t materialWidth    = 0;
        int32_t materialHeight   = 0;
        std::string sourceStackHash;
        std::string quality;
        std::string texturePath;
        std::string manifestPath;
        std::string materialRawPath;
        std::vector<uint8_t> materialMap;
        std::vector<int32_t> layerMaterialIds;
    };

    struct NamedCounter
    {
        std::string name;
        std::string detail;
        uint32_t count  = 0;
        uint32_t lastMs = 0;
    };

    struct OpaqueBLPFile
    {
        std::array<uint8_t, 0x4B4> storage;
    };

    struct CImVector
    {
        uint8_t b;
        uint8_t g;
        uint8_t r;
        uint8_t a;
    };

    struct TSGrowableArray_CImVector
    {
        uint32_t m_alloc;
        uint32_t m_count;
        CImVector* m_data;
        uint32_t m_chunk;
    };

    struct DirectTextureCacheEntry
    {
        TSGrowableArray_CImVector image = {};
        std::vector<CImVector> pixels;
        uint32_t width = 0;
        uint32_t height = 0;
        std::string debugName;
    };

    struct CachedPayloadLookup
    {
        std::string sourceHash;
        std::string manifestPath;
        std::string blpPath;
        std::string materialRawPath;
        int32_t textureBytes = 0;
        int32_t materialBytes = 0;
        int32_t materialWidth = 0;
        int32_t materialHeight = 0;
        std::vector<uint8_t> materialMap;
        std::vector<int32_t> layerMaterialIds;
        bool hit = false;
    };

    struct TerrainDrawOverride
    {
        bool active = false;
        bool targetedForce = false;
        uintptr_t chunkPtr = 0;
        uint8_t layerCount = 0;
        uint32_t slot0TextureHandle = 0;
        uint32_t compositeTextureHandle = 0;
    };

    struct GpuCompositeRt
    {
        IDirect3DTexture9* texture = nullptr;
        IDirect3DTexture9* uploadTexture = nullptr;
        IDirect3DSurface9* sysmemSurface = nullptr;
        uint32_t width = 0;
        uint32_t height = 0;
    };

    struct D3DOverlayState
    {
        IDirect3DDevice9* device = nullptr;
        IDirect3DPixelShader9* pixelShader = nullptr;
        DWORD alphaBlendEnable = 0;
        DWORD srcBlend = 0;
        DWORD destBlend = 0;
        DWORD blendOp = 0;
        DWORD zWriteEnable = 0;
        DWORD alphaTestEnable = 0;
        DWORD alphaRef = 0;
        DWORD alphaFunc = 0;
        DWORD colorOp0 = 0;
        DWORD colorArg10 = 0;
        DWORD colorArg20 = 0;
        DWORD alphaOp0 = 0;
        DWORD alphaArg10 = 0;
        DWORD colorOp1 = 0;
        DWORD alphaOp1 = 0;
        bool active = false;
    };

    struct RuntimeState
    {
        std::mutex mutex;
        ExtendedTerrainTextureSettings config;
        bool configured          = false;
        bool circuitOpen         = false;
        bool runtimeDisabled     = false;
        int32_t helperFailures   = 0;
        int32_t helperSuccesses  = 0;
        uint32_t circuitOpens    = 0;
        uint32_t generation      = 0;
        uint32_t ownerGeneration = 0;
        uint32_t observations    = 0;
        uint32_t queueDrops      = 0;
        uint32_t staleDrops      = 0;
        uint32_t memoryEvictions = 0;
        uint32_t budgetFallbacks = 0;
        uint32_t fallbackCount = 0;
        uint32_t lifecycleCount = 0;
        uint32_t fallbackFrames  = 0;
        uint32_t lastSummaryMs   = 0;
        uint32_t uploadFrameId   = 0;
        uint32_t uploadFrameCount = 0;
        uint32_t uploadFrameMs    = 0;
        uint32_t uploadAccepted   = 0;
        uint32_t uploadDeferred   = 0;
        uint32_t uploadCompleted  = 0;
        uint32_t uploadFailed     = 0;
        uint32_t textureCreateAttempts = 0;
        uint32_t textureCreateFailures = 0;
        uint32_t retiredTextureHandles = 0;
        uint32_t deviceLossCount = 0;
        uint32_t deviceRestoreCount = 0;
        uint32_t deviceReuploadQueued = 0;
        uint32_t drawSetupCalls = 0;
        uint32_t drawOverrideInstalls = 0;
        uint32_t drawOverrideRestores = 0;
        uint32_t drawWrapperCalls = 0;
        uint32_t drawWrapperNoNative = 0;
        uint32_t drawWrapperNoResidents = 0;
        uint32_t overlayPassAttempts = 0;
        uint32_t overlayPassApplied = 0;
        uint32_t overlayPassStateFailures = 0;
        uint32_t swapActivations = 0;
        uint32_t swapFallbacks = 0;
        uint32_t staleOwnerRejects = 0;
        uint32_t generationMismatchRejects = 0;
        uint32_t offTargetRejects = 0;
        uint32_t nativeCleanupInvalidations = 0;
        uint32_t lifecycleAttachCount = 0;
        uint32_t lifecycleDetachCount = 0;
        uintptr_t lastAttachedTextureSetPtr = 0;
        uintptr_t lastAttachedNativeChunkPtr = 0;
        uint32_t lastAttachedOwnerGeneration = 0;
        int32_t lastAttachedChunkIndex = -1;
        int32_t lastAttachedAdtTileX = -1;
        int32_t lastAttachedAdtTileY = -1;
        int32_t lastAttachedHandle = 0;
        std::string lastAttachRejectReason;
        std::string lastCleanupReason;
        uintptr_t lastDrawFunction = 0;
        uintptr_t lastNativeDrawFunction = 0;
        uintptr_t lastDrawChunkPtr = 0;
        uintptr_t lastSwapMissChunkPtr = 0;
        uintptr_t lastSwapActivationChunkPtr = 0;
        uint32_t lastSwapActivationMs = 0;
        int32_t lastSwapActivationLocalChunkX = 0;
        int32_t lastSwapActivationLocalChunkY = 0;
        int32_t lastSwapActivationWorldChunkX = 0;
        int32_t lastSwapActivationWorldChunkY = 0;
        int32_t lastSwapActivationAdtTileX = -1;
        int32_t lastSwapActivationAdtTileY = -1;
        int32_t lastSwapMissAdtTileX = -1;
        int32_t lastSwapMissAdtTileY = -1;
        int32_t lastSwapMissWorldChunkX = 0;
        int32_t lastSwapMissWorldChunkY = 0;
        std::string lastSwapMissReason;
        uint32_t payloadValidationOk = 0;
        uint32_t payloadValidationFailed = 0;
        uint32_t materialLookupAttempts = 0;
        uint32_t materialLookupHits = 0;
        uint32_t materialLookupFallbacks = 0;
        uint32_t autoCacheLookups = 0;
        uint32_t autoCacheHits = 0;
        uint32_t autoCacheMisses = 0;
        uint32_t autoCacheSidecarMisses = 0;
        uint32_t autoCacheInvalidIndexMisses = 0;
        uint32_t autoCacheHashMisses = 0;
        uint32_t autoCachePayloadMisses = 0;
        uint32_t compositeReadCallbackCalls = 0;
        uint32_t compositeReadOverrides = 0;
        uint32_t compositeReadSkipNoPayload = 0;
        uint32_t compositeReadSkipGate = 0;
        uint32_t gpuCompositeCallbacks = 0;
        uint32_t gpuCompositeAttempts = 0;
        uint32_t gpuCompositeSuccesses = 0;
        uint32_t gpuCompositeFailures = 0;
        uint32_t gpuCompositeNoDevice = 0;
        uint32_t gpuCompositeBuildMsTotal = 0;
        uint32_t gpuOverlayAttempts = 0;
        uint32_t gpuOverlayApplied = 0;
        uint32_t gpuOverlaySkipNoPayload = 0;
        uint32_t gpuOverlaySkipNoCache = 0;
        uint32_t gpuOverlayFallbackApplied = 0;
        int32_t lastOwnerAdtX = -1;
        int32_t lastOwnerAdtY = -1;
        int32_t lastOwnerChunkIndex = -1;
        int32_t lastOwnerWorldChunkX = -1;
        int32_t lastOwnerWorldChunkY = -1;
        bool lastOwnerHadResidentPayload = false;
        int32_t lastGpuCompositeEvent = 0;
        uint8_t lastGpuCompositeLayerCount = 0;
        uintptr_t lastGpuCompositeTextureSet = 0;
        std::array<uint32_t, 4> lastGpuNativeDiffuseHandles = { 0, 0, 0, 0 };
        uint32_t lastGpuNativeCompositeHandle = 0;
        uint32_t gpuCompositeSeed = 0;
        uint32_t lastGpuSlotHandleHash = 0;
        uint8_t lastGpuSlotCountCaptured = 0;
        uint8_t maxGpuSlotCountCaptured = 0;
        int32_t maxGpuSlotAdtX = -1;
        int32_t maxGpuSlotAdtY = -1;
        int32_t maxGpuSlotWorldChunkX = -1;
        int32_t maxGpuSlotWorldChunkY = -1;
        std::atomic<uintptr_t> nativeTerrainDrawFn { 0 };
        std::atomic<uint32_t> residentPayloadCount { 0 };
        uint64_t liveTextureBytes = 0;
        uint64_t liveMaterialBytes = 0;
        uint64_t pendingPayloadBytes = 0;
        uint64_t uploadFrameBytes = 0;
        uint32_t uploadQueueDepth = 0;
        uint32_t autoQueueFrameId = 0;
        uint32_t autoQueueFrameCount = 0;
        uint32_t autoQueueDeferred = 0;
        uint32_t observerFrameId = 0;
        uint32_t observerFrameNewChunks = 0;
        uint32_t observerThrottled = 0;
        uint32_t observerRefreshSkips = 0;
        int32_t focusWorldChunkX = 0;
        int32_t focusWorldChunkY = 0;
        int32_t lastObservedLocalChunkX = 0;
        int32_t lastObservedLocalChunkY = 0;
        int32_t lastObservedWorldChunkX = 0;
        int32_t lastObservedWorldChunkY = 0;
        int32_t lastObservedChunkIndex = -1;
        uintptr_t lastObservedTextureSetPtr = 0;
        uintptr_t lastObservedNativeChunkPtr = 0;
        uint32_t lastObservedOwnerGeneration = 0;
        int32_t lastAutoCacheChunkIndex = -1;
        bool hasFocusChunk = false;
        std::string disabledReason;
        std::string lastAutoCacheMissReason;
        std::string lastAutoCacheMissDetail;
        std::string lastFallbackReason;
        std::string lastFallbackDetail;
        std::string lastLifecycleStage;
        std::string lastLifecycleDetail;
        std::string cachedStackPath;
        std::string cachedStackText;
        int32_t cachedStackTileX = -1;
        int32_t cachedStackTileY = -1;
        bool cachedStackTileValid = false;
        bool cachedStackLoaded = false;
        bool cachedStackFailed = false;
        std::vector<std::string> cachedStackHashes;
        std::vector<CachedPayloadLookup> cachedPayloadLookups;
        std::deque<uintptr_t> queue;
        std::vector<ChunkRequest> requests;
        std::vector<PayloadRecord> payloads;
        std::unordered_map<uintptr_t, size_t> requestIndex;
        std::unordered_map<uintptr_t, size_t> payloadIndex;
        std::unordered_map<uintptr_t, size_t> payloadNativeIndex;
        std::unordered_map<uint64_t, size_t> payloadTileIndex;
        std::unordered_set<uintptr_t> queuedRequests;
        std::unordered_set<uintptr_t> pendingForceReattachOwners;
        std::unordered_map<uintptr_t, size_t> textureSetNativeOffsetCache;
        std::unordered_map<int32_t, std::unique_ptr<DirectTextureCacheEntry>> directTextureCache;
        std::unordered_map<uintptr_t, GpuCompositeRt> gpuCompositeRts;
        std::vector<NamedCounter> fallbackReasons;
        std::vector<NamedCounter> lifecycleStages;
    };

    RuntimeState& State()
    {
        static RuntimeState state;
        return state;
    }

    using TerrainDrawFn = void(__cdecl*)(int);
    using TerrainDrawThiscallFn = void(__thiscall*)(void*);
    constexpr uintptr_t kTerrainDrawFunctionPointer = 0x00D25098;
    thread_local bool gTerrainDrawOverrideActive = false;
    thread_local bool gTerrainOverlayPassActive = false;
    thread_local uintptr_t gActiveTextureSetContext = 0;
    thread_local bool gActiveTextureSetIdentityValid = false;
    thread_local int32_t gActiveTextureSetAdtX = -1;
    thread_local int32_t gActiveTextureSetAdtY = -1;
    thread_local int32_t gActiveTextureSetChunkIndex = -1;

    CLIENT_FUNCTION(HandleClose_ExtTex, 0x0047BF30, __cdecl, int, (int handle))
    CLIENT_FUNCTION(CMap__LoadTexture_ExtTex, 0x007D9990, __cdecl, int, (char* path))
    CLIENT_FUNCTION(Sub4B58D0_CBLPFileCtor_ExtTex, 0x004B58D0, __thiscall, void, (OpaqueBLPFile* file))
    CLIENT_FUNCTION(CBLPFile__Source_ExtTex, 0x006AE900, __thiscall, int, (OpaqueBLPFile* file, void* fileBits))
    CLIENT_FUNCTION(Sub6AF990_LoadBLP_ExtTex, 0x006AF990, __thiscall, int, (OpaqueBLPFile* file, int pixelFormat, int mip, int* outPixels, uint32_t* outInfo))
    CLIENT_FUNCTION(Sub6AF6E0_FreeBLPDecode_ExtTex, 0x006AF6E0, __thiscall, void, (OpaqueBLPFile* file, int))
    CLIENT_FUNCTION(CBLPFile__Close_ExtTex, 0x006AE8B0, __thiscall, void, (OpaqueBLPFile* file))
    CLIENT_FUNCTION(CGxTexFlags__constructor_ExtTex, 0x00681BE0, __thiscall, uint32_t*, (uint32_t* flags, int, int, int, int, int, int, unsigned int, int, int, int))
    CLIENT_FUNCTION(TextureCreateDirect_ExtTex, 0x004B9200, __cdecl, int, (uint32_t width, uint32_t height, int, int, uint32_t flags, TSGrowableArray_CImVector* vec, int callback, const char* name, int))
    CLIENT_FUNCTION(CMapChunkTextureCallback_CreateCompositeTexture_ExtTex, 0x7B9C60, __cdecl, void*,
                    (int event, int width, int height, int pitch, int format, void* textureSet, int* outStride,
                     void** outBits))

    uint32_t UploadFrameId(uint32_t nowMs)
    {
        return nowMs / 16u;
    }

    uint32_t EstimateUploadMs(int32_t textureBytes)
    {
        uint32_t const bytes = static_cast<uint32_t>((std::max)(0, textureBytes));
        uint32_t const chunks = (std::max)(1u, (bytes + (256u * 1024u) - 1u) / (256u * 1024u));
        return chunks;
    }

    bool IsLocalChunkCoord(int32_t value)
    {
        return value >= 0 && value < 16;
    }

    int32_t PositiveMod16(int32_t value)
    {
        int32_t result = value % 16;
        return result < 0 ? result + 16 : result;
    }

    bool IsReasonableGlobalChunkCoord(int32_t value)
    {
        return value >= 0 && value < 1024;
    }

    bool IsValidGlobalChunkPair(int32_t x, int32_t y)
    {
        return IsReasonableGlobalChunkCoord(x) && IsReasonableGlobalChunkCoord(y);
    }

    int32_t ReadInt32At(void* base, size_t offset);
    uint32_t ReadU32At(void* base, size_t offset);
    bool IsReadablePointer(void const* ptr, size_t minBytes);

    bool TryReadChunkCoords(uintptr_t ptr, int32_t& globalChunkX, int32_t& globalChunkY)
    {
        if (!ptr || !IsReadablePointer(reinterpret_cast<void const*>(ptr), 0x3C))
            return false;
        int32_t const localChunkX = ReadInt32At(reinterpret_cast<void*>(ptr), 0x24);
        int32_t const localChunkY = ReadInt32At(reinterpret_cast<void*>(ptr), 0x28);
        int32_t const gx = ReadInt32At(reinterpret_cast<void*>(ptr), 0x34);
        int32_t const gy = ReadInt32At(reinterpret_cast<void*>(ptr), 0x38);
        if (!IsLocalChunkCoord(localChunkX) || !IsLocalChunkCoord(localChunkY) || !IsValidGlobalChunkPair(gx, gy))
            return false;
        globalChunkX = gx;
        globalChunkY = gy;
        return true;
    }

    bool TryReadNativeChunkAtOffset(void* textureSet, size_t offset, uintptr_t& nativeChunkPtr)
    {
        nativeChunkPtr = 0;
        uintptr_t const candidate = ReadU32At(textureSet, offset);
        int32_t gx = 0;
        int32_t gy = 0;
        if (!TryReadChunkCoords(candidate, gx, gy))
            return false;

        nativeChunkPtr = candidate;
        return true;
    }

    bool ResolveNativeChunkFromTextureSet(RuntimeState& state, void* textureSet, uintptr_t& nativeChunkPtr)
    {
        nativeChunkPtr = 0;
        if (!textureSet)
            return false;

        uintptr_t const textureSetPtr = reinterpret_cast<uintptr_t>(textureSet);
        auto cached = state.textureSetNativeOffsetCache.find(textureSetPtr);
        if (cached != state.textureSetNativeOffsetCache.end())
        {
            if (TryReadNativeChunkAtOffset(textureSet, cached->second, nativeChunkPtr))
                return true;
            state.textureSetNativeOffsetCache.erase(cached);
        }

        size_t const candidateOffsets[] = { 0x10, 0x0C, 0x14, 0x08, 0x18, 0x04, 0x1C, 0x20 };
        for (size_t offset : candidateOffsets)
        {
            if (!TryReadNativeChunkAtOffset(textureSet, offset, nativeChunkPtr))
                continue;

            state.textureSetNativeOffsetCache[textureSetPtr] = offset;
            return true;
        }

        return false;
    }

    void ResetUploadFrameIfNeeded(RuntimeState& state, uint32_t nowMs)
    {
        uint32_t const frameId = UploadFrameId(nowMs);
        if (state.uploadFrameId == frameId)
            return;

        state.uploadFrameId = frameId;
        state.uploadFrameCount = 0;
        state.uploadFrameBytes = 0;
        state.uploadFrameMs = 0;
    }

    void ResetAutoQueueFrameIfNeeded(RuntimeState& state, uint32_t nowMs)
    {
        uint32_t const frameId = UploadFrameId(nowMs);
        if (state.autoQueueFrameId == frameId)
            return;

        state.autoQueueFrameId = frameId;
        state.autoQueueFrameCount = 0;
    }

    void ResetObserverFrameIfNeeded(RuntimeState& state, uint32_t nowMs)
    {
        uint32_t const frameId = UploadFrameId(nowMs);
        if (state.observerFrameId == frameId)
            return;

        state.observerFrameId = frameId;
        state.observerFrameNewChunks = 0;
    }

    std::string ToLowerCopy(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        return value;
    }

    void LogConfig(ExtendedTerrainTextureSettings const& config)
    {
        LOG_INFO << "ExtendedTerrainTextures: enabled=" << (config.enabled ? 1 : 0)
                 << " nativeUpload=" << (config.nativeUpload ? 1 : 0)
                 << " renderSwap=" << (config.renderSwap ? 1 : 0)
                 << " drawReplacement=" << (config.drawReplacement ? 1 : 0)
                 << " nativeCompositeGpu=" << (config.nativeCompositeGpu ? 1 : 0)
                 << " autoQueueCache=" << (config.autoQueueCache ? 1 : 0)
                 << " materialLookup=" << (config.materialLookup ? 1 : 0)
                 << " quality=" << config.quality << " maxLayers=" << config.maxLayers
                 << " quickSize=" << config.quickSize << " finalSize=" << config.finalSize
                 << " prefetchRadius=" << config.prefetchRadius << " memoryBudgetMB=" << config.memoryBudgetMB
                 << " uploadBudgetKBPerFrame=" << config.uploadBudgetKBPerFrame
                 << " uploadBudgetCountPerFrame=" << config.uploadBudgetCountPerFrame
                 << " uploadBudgetMsPerFrame=" << config.uploadBudgetMsPerFrame
                 << " autoQueueLookupsPerFrame=" << config.autoQueueLookupsPerFrame
                 << " autoQueuePopsPerFrame=" << config.autoQueuePopsPerFrame
                 << " observerNewChunksPerFrame=" << config.observerNewChunksPerFrame
                 << " observerRefreshMs=" << config.observerRefreshMs
                 << " maxPendingRequests=" << config.maxPendingRequests
                 << " staleRequestMs=" << config.staleRequestMs
                 << " circuitBreakerFailures=" << config.circuitBreakerFailures
                 << " helperPath=" << config.helperPath << " cacheRoot=" << config.cacheRoot
                 << " assetRoot=" << config.assetRoot << " stackPath=" << config.stackPath
                 << " trace=" << (config.trace ? 1 : 0)
                 << " telemetry=" << (config.telemetry ? 1 : 0);
    }

    int32_t ReadInt32At(void* base, size_t offset)
    {
        return *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(base) + offset);
    }

    float ReadFloatAt(void* base, size_t offset)
    {
        return *reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(base) + offset);
    }

    uint8_t ReadU8At(void* base, size_t offset)
    {
        return *reinterpret_cast<uint8_t*>(reinterpret_cast<uint8_t*>(base) + offset);
    }

    uint32_t ReadU32At(void* base, size_t offset)
    {
        return *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(base) + offset);
    }

    uint16_t ReadU16At(void* base, size_t offset)
    {
        return *reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(base) + offset);
    }

    bool IsReadablePointer(void const* ptr, size_t minBytes = sizeof(uint32_t))
    {
        if (!ptr || minBytes == 0)
            return false;

        MEMORY_BASIC_INFORMATION mbi = {};
        if (!VirtualQuery(ptr, &mbi, sizeof(mbi)))
            return false;
        if (mbi.State != MEM_COMMIT)
            return false;
        if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))
            return false;

        uint8_t const* begin = reinterpret_cast<uint8_t const*>(ptr);
        uint8_t const* regionBegin = reinterpret_cast<uint8_t const*>(mbi.BaseAddress);
        uint8_t const* regionEnd = regionBegin + mbi.RegionSize;
        return begin >= regionBegin && begin + minBytes <= regionEnd;
    }

    void WriteU8At(void* base, size_t offset, uint8_t value)
    {
        *reinterpret_cast<uint8_t*>(reinterpret_cast<uint8_t*>(base) + offset) = value;
    }

    void WriteU16At(void* base, size_t offset, uint16_t value)
    {
        *reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(base) + offset) = value;
    }

    void WriteU32At(void* base, size_t offset, uint32_t value)
    {
        *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(base) + offset) = value;
    }

    bool DeriveGlobalChunkCoordsFromRenderOrigin(void* chunk, int32_t& globalChunkX, int32_t& globalChunkY)
    {
        float const originY = ReadFloatAt(chunk, 0x7C);
        float const originX = ReadFloatAt(chunk, 0x80);
        if (!std::isfinite(originX) || !std::isfinite(originY))
            return false;

        constexpr float kMapOrigin = 17066.666f;
        constexpr float kChunkSize = 33.333332f;
        int32_t const derivedX = static_cast<int32_t>(std::lround((kMapOrigin - originX) / kChunkSize));
        int32_t const derivedY = static_cast<int32_t>(std::lround((kMapOrigin - originY) / kChunkSize));
        if (!IsReasonableGlobalChunkCoord(derivedX) || !IsReasonableGlobalChunkCoord(derivedY))
            return false;

        globalChunkX = derivedX;
        globalChunkY = derivedY;
        return true;
    }

    std::string NormalizePathSeparators(std::string value)
    {
        for (char& ch : value)
        {
            if (ch == '/')
                ch = '\\';
        }
        return value;
    }

    bool FileExists(std::string const& path)
    {
        DWORD const attrs = GetFileAttributesA(path.c_str());
        return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }

    bool TelemetryEnabled(RuntimeState const& state)
    {
        return state.config.telemetry || state.config.trace;
    }

    std::string JoinPath(std::string const& left, std::string const& right)
    {
        if (left.empty())
            return NormalizePathSeparators(right);
        if (right.empty())
            return NormalizePathSeparators(left);

        std::string result = NormalizePathSeparators(left);
        if (result.back() != '\\')
            result.push_back('\\');
        result += NormalizePathSeparators(right);
        return result;
    }

    bool ReadTextFile(std::string const& path, std::string& out)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
            return false;

        std::ostringstream stream;
        stream << file.rdbuf();
        out = stream.str();
        return true;
    }

    bool ReadBinaryFile(std::string const& path, std::vector<uint8_t>& out)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
            return false;

        file.seekg(0, std::ios::end);
        std::streamoff const size = file.tellg();
        if (size < 0)
            return false;

        file.seekg(0, std::ios::beg);
        out.resize(static_cast<size_t>(size));
        if (out.empty())
            return true;

        file.read(reinterpret_cast<char*>(out.data()), size);
        return !!file;
    }

    size_t SkipWhitespace(std::string const& text, size_t pos)
    {
        while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos])))
            ++pos;
        return pos;
    }

    std::string ExtractJsonString(std::string const& text, char const* key, size_t start = 0)
    {
        std::string const needle = std::string("\"") + key + "\"";
        size_t keyPos = text.find(needle, start);
        if (keyPos == std::string::npos)
            return "";

        size_t colon = text.find(':', keyPos + needle.size());
        if (colon == std::string::npos)
            return "";

        size_t quote = text.find('"', colon + 1);
        if (quote == std::string::npos)
            return "";

        std::string value;
        for (size_t i = quote + 1; i < text.size(); ++i)
        {
            char const ch = text[i];
            if (ch == '"')
                return value;
            if (ch == '\\' && i + 1 < text.size())
            {
                value.push_back(text[++i]);
                continue;
            }
            value.push_back(ch);
        }

        return "";
    }

    int32_t ExtractJsonInt(std::string const& text, char const* key, size_t start = 0, int32_t fallback = 0)
    {
        std::string const needle = std::string("\"") + key + "\"";
        size_t keyPos = text.find(needle, start);
        if (keyPos == std::string::npos)
            return fallback;

        size_t colon = text.find(':', keyPos + needle.size());
        if (colon == std::string::npos)
            return fallback;

        size_t pos = SkipWhitespace(text, colon + 1);
        bool negative = false;
        if (pos < text.size() && text[pos] == '-')
        {
            negative = true;
            ++pos;
        }

        int32_t value = 0;
        bool any = false;
        while (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos])))
        {
            any = true;
            value = value * 10 + (text[pos] - '0');
            ++pos;
        }

        if (!any)
            return fallback;
        return negative ? -value : value;
    }

    std::vector<int32_t> ExtractJsonIntArray(std::string const& text, char const* key)
    {
        std::vector<int32_t> values;
        std::string const needle = std::string("\"") + key + "\"";
        size_t const keyPos = text.find(needle);
        if (keyPos == std::string::npos)
            return values;

        size_t const open = text.find('[', keyPos + needle.size());
        if (open == std::string::npos)
            return values;

        size_t const close = text.find(']', open + 1);
        if (close == std::string::npos)
            return values;

        size_t pos = open + 1;
        while (pos < close)
        {
            pos = SkipWhitespace(text, pos);
            bool negative = false;
            if (pos < close && text[pos] == '-')
            {
                negative = true;
                ++pos;
            }

            int32_t value = 0;
            bool any = false;
            while (pos < close && std::isdigit(static_cast<unsigned char>(text[pos])))
            {
                any = true;
                value = value * 10 + (text[pos] - '0');
                ++pos;
            }

            if (any)
                values.push_back(negative ? -value : value);
            else
                ++pos;
        }

        return values;
    }

    std::string ExtractChunkStackHash(std::string const& sidecar, int32_t chunkIndex)
    {
        size_t search = 0;
        while (true)
        {
            size_t indexPos = sidecar.find("\"index\"", search);
            if (indexPos == std::string::npos)
                return "";

            int32_t const foundIndex = ExtractJsonInt(sidecar, "index", indexPos, -1);
            size_t const nextIndex = sidecar.find("\"index\"", indexPos + 7);
            if (foundIndex == chunkIndex)
                return ExtractJsonString(sidecar.substr(indexPos, nextIndex == std::string::npos ? std::string::npos : nextIndex - indexPos), "stackHash");

            search = indexPos + 7;
        }
    }

    std::string ManifestPathForHash(ExtendedTerrainTextureSettings const& config, std::string const& hash, bool capped)
    {
        if (hash.size() < 4)
            return "";

        int32_t const size = config.quality == "final" ? config.finalSize : config.quickSize;
        std::ostringstream filename;
        filename << hash << "_" << config.quality << "_" << size;
        if (capped && config.maxLayers >= 4)
            filename << "_layers" << config.maxLayers;
        filename << ".json";

        return JoinPath(JoinPath(JoinPath(config.cacheRoot, hash.substr(0, 2)), hash.substr(2, 2)), filename.str());
    }

    std::string const* CachedSidecar(RuntimeState& state)
    {
        if (state.cachedStackPath != state.config.stackPath)
        {
            state.cachedStackPath = state.config.stackPath;
            state.cachedStackText.clear();
            state.cachedStackHashes.clear();
            state.cachedPayloadLookups.clear();
            state.cachedStackTileX = -1;
            state.cachedStackTileY = -1;
            state.cachedStackTileValid = false;
            state.cachedStackLoaded = false;
            state.cachedStackFailed = false;
        }

        if (state.cachedStackLoaded)
            return &state.cachedStackText;
        if (state.cachedStackFailed)
            return nullptr;

        if (!ReadTextFile(state.config.stackPath, state.cachedStackText))
        {
            state.cachedStackFailed = true;
            return nullptr;
        }

        state.cachedStackLoaded = true;
        size_t const tilePos = state.cachedStackText.find("\"tile\"");
        if (tilePos != std::string::npos)
        {
            size_t const tileEnd = state.cachedStackText.find('}', tilePos);
            std::string const tileText = state.cachedStackText.substr(
                tilePos, tileEnd == std::string::npos ? std::string::npos : tileEnd - tilePos);
            state.cachedStackTileX = ExtractJsonInt(tileText, "x", 0, -1);
            state.cachedStackTileY = ExtractJsonInt(tileText, "y", 0, -1);
            state.cachedStackTileValid = state.cachedStackTileX >= 0 && state.cachedStackTileY >= 0;
        }

        state.cachedStackHashes.assign(256, "");
        size_t search = 0;
        while (true)
        {
            size_t const indexPos = state.cachedStackText.find("\"index\"", search);
            if (indexPos == std::string::npos)
                break;

            int32_t const foundIndex = ExtractJsonInt(state.cachedStackText, "index", indexPos, -1);
            size_t const nextIndex = state.cachedStackText.find("\"index\"", indexPos + 7);
            if (foundIndex >= 0 && foundIndex < static_cast<int32_t>(state.cachedStackHashes.size()))
            {
                std::string const chunkText = state.cachedStackText.substr(
                    indexPos, nextIndex == std::string::npos ? std::string::npos : nextIndex - indexPos);
                state.cachedStackHashes[foundIndex] = ExtractJsonString(chunkText, "stackHash");
            }

            search = indexPos + 7;
        }

        return &state.cachedStackText;
    }

    bool ResolveCachedPayload(RuntimeState& state, std::string const& sourceHash,
                              std::string& manifestPath, std::string& blpPath, int32_t& textureBytes,
                              int32_t& materialBytes, std::string& materialRawPath, int32_t& materialWidth,
                              int32_t& materialHeight, std::vector<uint8_t>& materialMap,
                              std::vector<int32_t>& layerMaterialIds)
    {
        if (sourceHash.empty())
            return false;

        auto cached = std::find_if(state.cachedPayloadLookups.begin(), state.cachedPayloadLookups.end(),
                                   [&](CachedPayloadLookup const& item) { return item.sourceHash == sourceHash; });
        if (cached != state.cachedPayloadLookups.end())
        {
            if (!cached->hit)
                return false;

            manifestPath = cached->manifestPath;
            blpPath = cached->blpPath;
            materialRawPath = cached->materialRawPath;
            textureBytes = cached->textureBytes;
            materialBytes = cached->materialBytes;
            materialWidth = cached->materialWidth;
            materialHeight = cached->materialHeight;
            materialMap = cached->materialMap;
            layerMaterialIds = cached->layerMaterialIds;
            return true;
        }

        std::string candidates[2] = {
            ManifestPathForHash(state.config, sourceHash, state.config.maxLayers < 16),
            ManifestPathForHash(state.config, sourceHash, false),
        };

        for (std::string const& candidate : candidates)
        {
            if (candidate.empty() || !FileExists(candidate))
                continue;

            std::string manifest;
            if (!ReadTextFile(candidate, manifest))
                continue;

            std::string relativeBlp = ExtractJsonString(manifest, "blpPath");
            if (relativeBlp.empty())
                continue;

            std::string const fullBlpPath = JoinPath(state.config.cacheRoot, relativeBlp);
            if (!FileExists(fullBlpPath))
                continue;

            std::string const relativeMaterialRaw = ExtractJsonString(manifest, "materialRawPath");
            std::string fullMaterialRaw;
            std::vector<uint8_t> loadedMaterialMap;
            if (!relativeMaterialRaw.empty())
            {
                fullMaterialRaw = JoinPath(state.config.cacheRoot, relativeMaterialRaw);
                if (state.config.materialLookup || state.config.nativeCompositeGpu)
                    ReadBinaryFile(fullMaterialRaw, loadedMaterialMap);
            }

            manifestPath = candidate;
            blpPath = fullBlpPath;
            materialRawPath = fullMaterialRaw;
            textureBytes = ExtractJsonInt(manifest, "blp", 0, 0);
            materialBytes = ExtractJsonInt(manifest, "material", 0, 0);
            materialWidth = ExtractJsonInt(manifest, "width", manifest.find("\"dimensions\""), 0);
            materialHeight = ExtractJsonInt(manifest, "height", manifest.find("\"dimensions\""), 0);
            materialMap = loadedMaterialMap;
            layerMaterialIds = ExtractJsonIntArray(manifest, "layerMaterialIds");
            if (textureBytes <= 0)
                textureBytes = 1;

            CachedPayloadLookup next;
            next.sourceHash = sourceHash;
            next.manifestPath = manifestPath;
            next.blpPath = blpPath;
            next.materialRawPath = materialRawPath;
            next.textureBytes = textureBytes;
            next.materialBytes = materialBytes;
            next.materialWidth = materialWidth;
            next.materialHeight = materialHeight;
            next.materialMap = materialMap;
            next.layerMaterialIds = layerMaterialIds;
            next.hit = true;
            state.cachedPayloadLookups.push_back(next);
            return true;
        }

        CachedPayloadLookup miss;
        miss.sourceHash = sourceHash;
        state.cachedPayloadLookups.push_back(miss);
        return false;
    }

    void RecordLifecycleUnlocked(RuntimeState& state, char const* stage, char const* detail);

    void PrewarmSidecarPayloadMetadata(RuntimeState& state)
    {
        if (state.config.forceTestBlp)
            return;

        std::string const* sidecar = CachedSidecar(state);
        if (!sidecar)
            return;

        std::unordered_set<std::string> seenHashes;
        for (std::string const& sourceHash : state.cachedStackHashes)
        {
            if (sourceHash.empty() || seenHashes.find(sourceHash) != seenHashes.end())
                continue;

            seenHashes.insert(sourceHash);
            std::string manifestPath;
            std::string blpPath;
            std::string materialRawPath;
            int32_t textureBytes = 0;
            int32_t materialBytes = 0;
            int32_t materialWidth = 0;
            int32_t materialHeight = 0;
            std::vector<uint8_t> materialMap;
            std::vector<int32_t> layerMaterialIds;
            ResolveCachedPayload(state, sourceHash, manifestPath, blpPath, textureBytes, materialBytes,
                                 materialRawPath, materialWidth, materialHeight, materialMap, layerMaterialIds);
        }

        RecordLifecycleUnlocked(state, "cache-prewarm", state.config.stackPath.c_str());
    }

    void RebuildRequestIndex(RuntimeState& state)
    {
        state.requestIndex.clear();
        for (size_t i = 0; i < state.requests.size(); ++i)
            state.requestIndex[state.requests[i].chunkPtr] = i;
    }

    uint64_t TilePayloadKey(int32_t adtX, int32_t adtY, int32_t chunkIndex)
    {
        return (static_cast<uint64_t>(static_cast<uint32_t>(adtX)) << 32) |
               (static_cast<uint64_t>(static_cast<uint32_t>(adtY)) << 16) |
               static_cast<uint64_t>(static_cast<uint16_t>(chunkIndex));
    }

    void EnqueueRequest(RuntimeState& state, uintptr_t chunkPtr)
    {
        if (!chunkPtr || state.queuedRequests.find(chunkPtr) != state.queuedRequests.end())
            return;

        state.queue.push_back(chunkPtr);
        state.queuedRequests.insert(chunkPtr);
    }

    void ClearRequestQueue(RuntimeState& state)
    {
        state.queue.clear();
        state.queuedRequests.clear();
    }

    void RebuildPayloadIndex(RuntimeState& state)
    {
        state.payloadIndex.clear();
        state.payloadNativeIndex.clear();
        state.payloadTileIndex.clear();
        for (size_t i = 0; i < state.payloads.size(); ++i)
        {
            state.payloadIndex[state.payloads[i].chunkPtr] = i;
            if (state.payloads[i].nativeChunkPtr)
                state.payloadNativeIndex[state.payloads[i].nativeChunkPtr] = i;
            if (state.payloads[i].adtTileX >= 0 && state.payloads[i].adtTileY >= 0 &&
                state.payloads[i].chunkIndex >= 0 && state.payloads[i].resident &&
                state.payloads[i].textureHandle != 0)
            {
                state.payloadTileIndex[TilePayloadKey(state.payloads[i].adtTileX, state.payloads[i].adtTileY,
                                                      state.payloads[i].chunkIndex)] = i;
            }
        }
    }

    ChunkRequest* FindRequest(RuntimeState& state, uintptr_t chunkPtr)
    {
        auto itr = state.requestIndex.find(chunkPtr);
        if (itr == state.requestIndex.end())
            return nullptr;
        if (itr->second >= state.requests.size() || state.requests[itr->second].chunkPtr != chunkPtr)
        {
            RebuildRequestIndex(state);
            itr = state.requestIndex.find(chunkPtr);
            if (itr == state.requestIndex.end())
                return nullptr;
        }

        return &state.requests[itr->second];
    }

    ChunkRequest* FindRequestByNativeChunk(RuntimeState& state, uintptr_t nativeChunkPtr)
    {
        if (!nativeChunkPtr)
            return nullptr;

        auto itr = std::find_if(state.requests.begin(), state.requests.end(),
                                [&](ChunkRequest const& request) {
                                    return request.nativeChunkPtr == nativeChunkPtr;
                                });
        return itr == state.requests.end() ? nullptr : &*itr;
    }

    PayloadRecord* FindPayload(RuntimeState& state, uintptr_t chunkPtr)
    {
        auto itr = state.payloadIndex.find(chunkPtr);
        if (itr == state.payloadIndex.end())
            return nullptr;
        if (itr->second >= state.payloads.size() || state.payloads[itr->second].chunkPtr != chunkPtr)
        {
            RebuildPayloadIndex(state);
            itr = state.payloadIndex.find(chunkPtr);
            if (itr == state.payloadIndex.end())
                return nullptr;
        }

        return &state.payloads[itr->second];
    }

    PayloadRecord* FindPayloadByNativeChunk(RuntimeState& state, uintptr_t nativeChunkPtr)
    {
        if (!nativeChunkPtr)
            return nullptr;

        auto itr = state.payloadNativeIndex.find(nativeChunkPtr);
        if (itr == state.payloadNativeIndex.end())
            return nullptr;
        if (itr->second >= state.payloads.size() || state.payloads[itr->second].nativeChunkPtr != nativeChunkPtr)
        {
            RebuildPayloadIndex(state);
            itr = state.payloadNativeIndex.find(nativeChunkPtr);
            if (itr == state.payloadNativeIndex.end())
                return nullptr;
        }

        return &state.payloads[itr->second];
    }

    PayloadRecord* FindPayloadByTileChunkIndex(RuntimeState& state, int32_t adtX, int32_t adtY, int32_t chunkIndex)
    {
        if (adtX < 0 || adtY < 0 || chunkIndex < 0)
            return nullptr;

        uint64_t const key = TilePayloadKey(adtX, adtY, chunkIndex);
        auto itr = state.payloadTileIndex.find(key);
        if (itr == state.payloadTileIndex.end())
            return nullptr;
        if (itr->second >= state.payloads.size() || state.payloads[itr->second].adtTileX != adtX ||
            state.payloads[itr->second].adtTileY != adtY || state.payloads[itr->second].chunkIndex != chunkIndex)
        {
            RebuildPayloadIndex(state);
            itr = state.payloadTileIndex.find(key);
            if (itr == state.payloadTileIndex.end())
                return nullptr;
        }

        PayloadRecord& payload = state.payloads[itr->second];
        return payload.resident && payload.textureHandle != 0 ? &payload : nullptr;
    }

    void ClientChunkToAdtTile(int32_t globalChunkX, int32_t globalChunkY, int32_t& adtX, int32_t& adtY);

    PayloadRecord* FindResidentPayloadForTextureSet(RuntimeState& state, uintptr_t textureSetPtr)
    {
        if (!textureSetPtr)
            return nullptr;

        // Prefer direct payload ownership paths first.
        if (PayloadRecord* owned = FindPayload(state, textureSetPtr))
        {
            if (owned->resident && owned->textureHandle)
                return owned;
        }

        uintptr_t nativeChunkPtr = 0;
        if (ResolveNativeChunkFromTextureSet(state, reinterpret_cast<void*>(textureSetPtr), nativeChunkPtr))
        {
            if (PayloadRecord* byNative = FindPayloadByNativeChunk(state, nativeChunkPtr))
            {
                if (byNative->resident && byNative->textureHandle)
                    return byNative;
            }

            int32_t const localChunkX = ReadInt32At(reinterpret_cast<void*>(nativeChunkPtr), 0x24);
            int32_t const localChunkY = ReadInt32At(reinterpret_cast<void*>(nativeChunkPtr), 0x28);
            int32_t const globalChunkX = ReadInt32At(reinterpret_cast<void*>(nativeChunkPtr), 0x34);
            int32_t const globalChunkY = ReadInt32At(reinterpret_cast<void*>(nativeChunkPtr), 0x38);
            if (IsLocalChunkCoord(localChunkX) && IsLocalChunkCoord(localChunkY) &&
                IsValidGlobalChunkPair(globalChunkX, globalChunkY))
            {
                int32_t adtX = -1;
                int32_t adtY = -1;
                ClientChunkToAdtTile(globalChunkX, globalChunkY, adtX, adtY);
                int32_t const chunkIndex = localChunkY * 16 + localChunkX;

                if (PayloadRecord* byTileChunk = FindPayloadByTileChunkIndex(state, adtX, adtY, chunkIndex))
                {
                    return byTileChunk;
                }
            }
        }

        return nullptr;
    }

    PayloadRecord* FindResidentPayloadForTextureSetAggressive(RuntimeState& state, uintptr_t textureSetPtr)
    {
        if (!textureSetPtr)
            return nullptr;

        if (PayloadRecord* resolved = FindResidentPayloadForTextureSet(state, textureSetPtr))
            return resolved;

        // Try request identity directly (more stable than owner pointers).
        if (ChunkRequest* request = FindRequest(state, textureSetPtr))
        {
            if (request->adtTileValid && request->chunkIndex >= 0)
            {
                if (PayloadRecord* byTileChunk =
                        FindPayloadByTileChunkIndex(state, request->adtTileX, request->adtTileY, request->chunkIndex))
                {
                    if (!request->sourceStackHash.empty() && !byTileChunk->sourceStackHash.empty() &&
                        byTileChunk->sourceStackHash != request->sourceStackHash)
                    {
                        return nullptr;
                    }
                    return byTileChunk;
                }
            }
        }

        // Global-coordinate fallback for cases where local chunk fields are unreliable.
        size_t const candidateOffsets[] = { 0x10, 0x0C, 0x14, 0x08, 0x18, 0x04, 0x1C, 0x20 };
        // First, textureSet pointer itself may already be the native chunk in some call paths.
        {
            int32_t gx = 0, gy = 0;
            if (TryReadChunkCoords(textureSetPtr, gx, gy))
            {
                int32_t adtX = -1, adtY = -1;
                ClientChunkToAdtTile(gx, gy, adtX, adtY);
                int32_t const chunkIndex = PositiveMod16(gy) * 16 + PositiveMod16(gx);
                if (PayloadRecord* byTileChunk = FindPayloadByTileChunkIndex(state, adtX, adtY, chunkIndex))
                    return byTileChunk;
            }
        }

        for (size_t offset : candidateOffsets)
        {
            uintptr_t const nativeChunkPtr = ReadU32At(reinterpret_cast<void*>(textureSetPtr), offset);
            int32_t globalChunkX = 0, globalChunkY = 0;
            if (!TryReadChunkCoords(nativeChunkPtr, globalChunkX, globalChunkY))
                continue;

            int32_t adtX = -1;
            int32_t adtY = -1;
            ClientChunkToAdtTile(globalChunkX, globalChunkY, adtX, adtY);
            if (adtX < 0 || adtY < 0)
                continue;

            int32_t const localX = PositiveMod16(globalChunkX);
            int32_t const localY = PositiveMod16(globalChunkY);
            int32_t const chunkIndex = localY * 16 + localX;
            if (PayloadRecord* byTileChunk = FindPayloadByTileChunkIndex(state, adtX, adtY, chunkIndex))
                return byTileChunk;

            // One dereference hop fallback.
            uintptr_t const hop = ReadU32At(reinterpret_cast<void*>(nativeChunkPtr), 0x10);
            if (!hop)
                continue;
            if (!TryReadChunkCoords(hop, globalChunkX, globalChunkY))
                continue;
            ClientChunkToAdtTile(globalChunkX, globalChunkY, adtX, adtY);
            int32_t const hopIndex = PositiveMod16(globalChunkY) * 16 + PositiveMod16(globalChunkX);
            if (PayloadRecord* byHop = FindPayloadByTileChunkIndex(state, adtX, adtY, hopIndex))
                return byHop;
        }

        if (gActiveTextureSetIdentityValid && gActiveTextureSetChunkIndex >= 0 &&
            gActiveTextureSetAdtX >= 0 && gActiveTextureSetAdtY >= 0)
        {
            if (PayloadRecord* byActive = FindPayloadByTileChunkIndex(
                    state, gActiveTextureSetAdtX, gActiveTextureSetAdtY, gActiveTextureSetChunkIndex))
            {
                return byActive;
            }
        }

        return nullptr;
    }

    PayloadRecord* FindResidentPayloadForActiveIdentity(RuntimeState& state)
    {
        if (!gActiveTextureSetIdentityValid || gActiveTextureSetAdtX < 0 || gActiveTextureSetAdtY < 0 ||
            gActiveTextureSetChunkIndex < 0)
            return nullptr;

        return FindPayloadByTileChunkIndex(state, gActiveTextureSetAdtX, gActiveTextureSetAdtY, gActiveTextureSetChunkIndex);
    }

    uintptr_t ResolveTextureSetOwnerPtr(RuntimeState& state, uintptr_t opaquePtr)
    {
        if (!opaquePtr)
            return 0;
        if (FindRequest(state, opaquePtr))
            return opaquePtr;

        size_t const candidateOffsets[] = { 0x00, 0x04, 0x08, 0x0C, 0x10, 0x14, 0x18, 0x1C, 0x20, 0x24 };
        for (size_t off : candidateOffsets)
        {
            uintptr_t const candidate = ReadU32At(reinterpret_cast<void*>(opaquePtr), off);
            if (!candidate)
                continue;
            if (FindRequest(state, candidate))
                return candidate;
        }
        return opaquePtr;
    }

    PayloadRecord* FindUploadPayload(RuntimeState& state, uintptr_t chunkPtr, std::string const& texturePath)
    {
        PayloadRecord* payload = FindPayload(state, chunkPtr);
        if (!payload || payload->texturePath != texturePath)
            return nullptr;

        return payload;
    }

    PayloadRecord* FindOrCreatePayload(RuntimeState& state, uintptr_t chunkPtr, uint32_t nowMs)
    {
        PayloadRecord* payload = FindPayload(state, chunkPtr);
        if (payload)
            return payload;

        PayloadRecord next;
        next.chunkPtr = chunkPtr;
        next.firstSeenMs = nowMs;
        state.payloads.push_back(next);
        state.payloadIndex[chunkPtr] = state.payloads.size() - 1;
        return &state.payloads.back();
    }

    uint64_t MemoryBudgetBytes(RuntimeState const& state)
    {
        return static_cast<uint64_t>(state.config.memoryBudgetMB) * 1024ull * 1024ull;
    }

    uint64_t LiveBytes(RuntimeState const& state)
    {
        return state.liveTextureBytes + state.liveMaterialBytes;
    }

    void UpsertCounter(std::vector<NamedCounter>& counters, char const* name, char const* detail, uint32_t nowMs)
    {
        std::string const key = name ? name : "<unknown>";
        auto itr = std::find_if(counters.begin(), counters.end(),
                                [&](NamedCounter const& counter) { return counter.name == key; });
        if (itr == counters.end())
        {
            NamedCounter next;
            next.name   = key;
            next.detail = detail ? detail : "";
            next.count  = 1;
            next.lastMs = nowMs;
            counters.push_back(next);
            return;
        }

        ++itr->count;
        itr->detail = detail ? detail : "";
        itr->lastMs = nowMs;
    }

    void RecordLifecycleUnlocked(RuntimeState& state, char const* stage, char const* detail)
    {
        if (!TelemetryEnabled(state))
            return;

        uint32_t const nowMs = GetTickCount();
        state.lastLifecycleStage = stage ? stage : "<unknown>";
        state.lastLifecycleDetail = detail ? detail : "";
        ++state.lifecycleCount;
        UpsertCounter(state.lifecycleStages, stage, detail, nowMs);

        if (state.config.trace)
        {
            LOG_INFO << "ExtendedTerrainTextures: lifecycle stage=" << state.lastLifecycleStage
                     << " detail=" << state.lastLifecycleDetail
                     << " count=" << state.lifecycleCount;
        }
    }

    void RecordFallbackUnlocked(RuntimeState& state, char const* reason, char const* detail)
    {
        if (!TelemetryEnabled(state))
            return;

        uint32_t const nowMs = GetTickCount();
        state.lastFallbackReason = reason ? reason : "<unknown>";
        state.lastFallbackDetail = detail ? detail : "";
        ++state.fallbackCount;
        UpsertCounter(state.fallbackReasons, reason, detail, nowMs);

        if (state.config.trace)
        {
            LOG_INFO << "ExtendedTerrainTextures: fallback reason=" << state.lastFallbackReason
                     << " detail=" << state.lastFallbackDetail
                     << " count=" << state.fallbackCount;
        }
    }

    void AttachChunkCoordsFromRequest(RuntimeState& state, PayloadRecord& payload)
    {
        ChunkRequest* request = FindRequest(state, payload.chunkPtr);
        if (!request)
            return;

        payload.textureSetPtr = request->textureSetPtr;
        payload.nativeChunkPtr = request->nativeChunkPtr;
        payload.ownerGeneration = request->ownerGeneration;
        payload.tileLoadIdentity = request->tileLoadIdentity;
        payload.chunkIndex = request->chunkIndex;
        payload.worldChunkX = request->worldChunkX;
        payload.worldChunkY = request->worldChunkY;
        payload.adtTileX = request->adtTileX;
        payload.adtTileY = request->adtTileY;
    }

    void RecalculatePayloadCounters(RuntimeState& state)
    {
        state.liveTextureBytes   = 0;
        state.liveMaterialBytes  = 0;
        state.pendingPayloadBytes = 0;
        state.uploadQueueDepth   = 0;
        uint32_t residentCount    = 0;

        for (PayloadRecord const& payload : state.payloads)
        {
            uint64_t const totalBytes = static_cast<uint64_t>((std::max)(0, payload.textureBytes)) +
                                        static_cast<uint64_t>((std::max)(0, payload.materialBytes));
            if (payload.resident && payload.textureHandle)
            {
                state.liveTextureBytes += static_cast<uint64_t>((std::max)(0, payload.textureBytes));
                state.liveMaterialBytes += static_cast<uint64_t>((std::max)(0, payload.materialBytes));
                ++residentCount;
            }
            else if (payload.pendingUpload)
            {
                state.pendingPayloadBytes += totalBytes;
                ++state.uploadQueueDepth;
            }
        }

        state.residentPayloadCount.store(residentCount, std::memory_order_release);
    }

    void ReleasePayloadHandle(RuntimeState& state, PayloadRecord& payload, char const* reason);
    void DetachPayloadFromTextureSet(RuntimeState& state, PayloadRecord& payload, char const* reason);

    void RecordAutoCacheMiss(RuntimeState& state, char const* reason, std::string const& detail)
    {
        if (!TelemetryEnabled(state))
            return;

        ++state.autoCacheMisses;
        state.lastAutoCacheMissReason = reason ? reason : "<unknown>";
        state.lastAutoCacheMissDetail = detail;
    }

    bool MarkNativeTextureSetDirtyForComposite(PayloadRecord const& payload)
    {
        if (!payload.textureSetPtr || !IsReadablePointer(reinterpret_cast<void const*>(payload.textureSetPtr), 0x90))
            return false;

        void* textureSet = reinterpret_cast<void*>(payload.textureSetPtr);
        if (ReadU8At(textureSet, 0x09) == 0)
            return false;

        uint16_t const flags = ReadU16At(textureSet, 0x0A);
        WriteU16At(textureSet, 0x0A, static_cast<uint16_t>(flags | 0x20u));
        return true;
    }

    void ClientChunkToAdtTile(int32_t globalChunkX, int32_t globalChunkY, int32_t& adtX, int32_t& adtY)
    {
        if (!IsValidGlobalChunkPair(globalChunkX, globalChunkY))
        {
            adtX = -1;
            adtY = -1;
            return;
        }

        // Native tile load writes chunk+0x34/+0x38 as global chunk coords.
        // Each ADT is a 16x16 chunk block in that same space.
        adtX = globalChunkX / 16;
        adtY = globalChunkY / 16;
        if (adtX < 0 || adtX >= 64 || adtY < 0 || adtY >= 64)
        {
            adtX = -1;
            adtY = -1;
        }
    }

    void AdtTileToServerCell(int32_t adtX, int32_t adtY, int32_t& cellX, int32_t& cellY)
    {
        cellX = 63 - adtY;
        cellY = 63 - adtX;
    }

    bool ResolveRequestAdtTile(ChunkRequest const& request, int32_t& adtX, int32_t& adtY)
    {
        adtX = request.adtTileX;
        adtY = request.adtTileY;

        // Prefer live native chunk coordinates when available; cached request coords can drift.
        if (request.nativeChunkPtr &&
            IsReadablePointer(reinterpret_cast<void const*>(request.nativeChunkPtr), 0x3C))
        {
            int32_t const localChunkX = ReadInt32At(reinterpret_cast<void*>(request.nativeChunkPtr), 0x24);
            int32_t const localChunkY = ReadInt32At(reinterpret_cast<void*>(request.nativeChunkPtr), 0x28);
            int32_t const globalChunkX = ReadInt32At(reinterpret_cast<void*>(request.nativeChunkPtr), 0x34);
            int32_t const globalChunkY = ReadInt32At(reinterpret_cast<void*>(request.nativeChunkPtr), 0x38);
            if (IsLocalChunkCoord(localChunkX) && IsLocalChunkCoord(localChunkY) &&
                IsValidGlobalChunkPair(globalChunkX, globalChunkY))
            {
                ClientChunkToAdtTile(globalChunkX, globalChunkY, adtX, adtY);
                return adtX >= 0 && adtY >= 0;
            }
        }

        if (request.adtTileValid && adtX >= 0 && adtY >= 0)
            return true;

        if (!IsValidGlobalChunkPair(request.worldChunkX, request.worldChunkY))
            return false;

        ClientChunkToAdtTile(request.worldChunkX, request.worldChunkY, adtX, adtY);
        return adtX >= 0 && adtY >= 0;
    }

    bool RequestMatchesCachedStackTile(RuntimeState const& state, ChunkRequest const& request)
    {
        if (!state.cachedStackTileValid)
            return true;

        int32_t adtX = -1;
        int32_t adtY = -1;
        if (!ResolveRequestAdtTile(request, adtX, adtY))
            return false;

        return adtX == state.cachedStackTileX && adtY == state.cachedStackTileY;
    }

    bool PayloadMatchesCachedStackTile(RuntimeState const& state, PayloadRecord const& payload)
    {
        if (!state.cachedStackTileValid)
            return true;

        int32_t adtX = payload.adtTileX;
        int32_t adtY = payload.adtTileY;
        if (adtX < 0 || adtY < 0)
        {
            if (!IsValidGlobalChunkPair(payload.worldChunkX, payload.worldChunkY))
                return false;

            ClientChunkToAdtTile(payload.worldChunkX, payload.worldChunkY, adtX, adtY);
        }

        return adtX == state.cachedStackTileX && adtY == state.cachedStackTileY;
    }

    void FillChunkRequestCoordsFromIdentity(ChunkRequest& request, void* chunk)
    {
        int32_t derivedWorldX = 0;
        int32_t derivedWorldY = 0;
        if (!DeriveGlobalChunkCoordsFromRenderOrigin(chunk, derivedWorldX, derivedWorldY))
        {
            request.coordsValid = false;
            request.adtTileValid = false;
            return;
        }

        int32_t const localChunkX = ReadInt32At(chunk, 0x24);
        int32_t const localChunkY = ReadInt32At(chunk, 0x28);
        request.worldChunkX = derivedWorldX;
        request.worldChunkY = derivedWorldY;
        request.localChunkX = IsLocalChunkCoord(localChunkX) ? localChunkX : PositiveMod16(derivedWorldX);
        request.localChunkY = IsLocalChunkCoord(localChunkY) ? localChunkY : PositiveMod16(derivedWorldY);
        request.coordsValid = true;
        ClientChunkToAdtTile(request.worldChunkX, request.worldChunkY, request.adtTileX, request.adtTileY);
        request.adtTileValid = request.adtTileX >= 0 && request.adtTileY >= 0;
    }

    bool FillChunkRequestCoordsFromLoadFields(ChunkRequest& request, void* chunk)
    {
        int32_t const localChunkX = ReadInt32At(chunk, 0x24);
        int32_t const localChunkY = ReadInt32At(chunk, 0x28);
        int32_t const globalChunkX = ReadInt32At(chunk, 0x34);
        int32_t const globalChunkY = ReadInt32At(chunk, 0x38);
        if (!IsLocalChunkCoord(localChunkX) || !IsLocalChunkCoord(localChunkY) ||
            !IsValidGlobalChunkPair(globalChunkX, globalChunkY))
        {
            return false;
        }

        request.localChunkX = localChunkX;
        request.localChunkY = localChunkY;
        request.worldChunkX = globalChunkX;
        request.worldChunkY = globalChunkY;
        ClientChunkToAdtTile(request.worldChunkX, request.worldChunkY, request.adtTileX, request.adtTileY);
        request.coordsValid = true;
        request.adtTileValid = request.adtTileX >= 0 && request.adtTileY >= 0;
        return request.adtTileValid;
    }

    void FillChunkRequestCoordsFromRenderOrigin(ChunkRequest& request, void* chunk)
    {
        int32_t derivedWorldX = 0;
        int32_t derivedWorldY = 0;
        if (!DeriveGlobalChunkCoordsFromRenderOrigin(chunk, derivedWorldX, derivedWorldY))
            return;

        request.worldChunkX = derivedWorldX;
        request.worldChunkY = derivedWorldY;
        request.localChunkX = PositiveMod16(derivedWorldX);
        request.localChunkY = PositiveMod16(derivedWorldY);
        request.coordsValid = true;
        if (!request.adtTileValid)
        {
            ClientChunkToAdtTile(request.worldChunkX, request.worldChunkY, request.adtTileX, request.adtTileY);
            request.adtTileValid = request.adtTileX >= 0 && request.adtTileY >= 0;
        }
    }

    int32_t ResolveChunkIndex(RuntimeState& state, ChunkRequest const& request)
    {
        state.lastObservedLocalChunkX = request.localChunkX;
        state.lastObservedLocalChunkY = request.localChunkY;
        state.lastObservedWorldChunkX = request.worldChunkX;
        state.lastObservedWorldChunkY = request.worldChunkY;

        if (IsLocalChunkCoord(request.localChunkX) && IsLocalChunkCoord(request.localChunkY))
            return request.localChunkY * 16 + request.localChunkX;

        int32_t const localX = PositiveMod16(request.worldChunkX);
        int32_t const localY = PositiveMod16(request.worldChunkY);
        return localY * 16 + localX;
    }

    void RefreshOwnerIdentity(RuntimeState& state, ChunkRequest& request, void* owner, bool newGeneration)
    {
        if (!owner)
            return;

        request.textureSetPtr = reinterpret_cast<uintptr_t>(owner);
        request.nativeChunkPtr = ReadU32At(owner, 0x10);
        request.chunkIndex = ResolveChunkIndex(state, request);
        if (newGeneration || request.ownerGeneration == 0)
            request.ownerGeneration = ++state.ownerGeneration;

        state.lastObservedChunkIndex = request.chunkIndex;
        state.lastObservedTextureSetPtr = request.textureSetPtr;
        state.lastObservedNativeChunkPtr = request.nativeChunkPtr;
        state.lastObservedOwnerGeneration = request.ownerGeneration;
    }

    void CopyOwnerIdentityToPayload(PayloadRecord& payload, ChunkRequest const& request)
    {
        payload.textureSetPtr = request.textureSetPtr;
        payload.nativeChunkPtr = request.nativeChunkPtr;
        payload.ownerGeneration = request.ownerGeneration;
        payload.tileLoadIdentity = request.tileLoadIdentity;
        payload.chunkIndex = request.chunkIndex;
        payload.worldChunkX = request.worldChunkX;
        payload.worldChunkY = request.worldChunkY;
        payload.adtTileX = request.adtTileX;
        payload.adtTileY = request.adtTileY;
    }

    bool PayloadOwnerMatchesRequest(PayloadRecord const& payload, ChunkRequest const& request)
    {
        // Fast exact identity when ownership is stable.
        if (payload.chunkPtr == request.chunkPtr &&
            payload.textureSetPtr == request.textureSetPtr &&
            payload.nativeChunkPtr == request.nativeChunkPtr &&
            payload.ownerGeneration == request.ownerGeneration &&
            payload.tileLoadIdentity == request.tileLoadIdentity &&
            payload.chunkIndex == request.chunkIndex &&
            payload.adtTileX == request.adtTileX &&
            payload.adtTileY == request.adtTileY)
            return true;

        // Stable fallback: allow reuse across owner-generation churn when chunk identity and tile identity agree.
        if (payload.nativeChunkPtr && request.nativeChunkPtr &&
            payload.nativeChunkPtr == request.nativeChunkPtr &&
            payload.chunkIndex == request.chunkIndex &&
            payload.adtTileX == request.adtTileX &&
            payload.adtTileY == request.adtTileY)
            return true;

        // Last-resort fallback for occlusion/streaming pointer reuse: require same ADT chunk slot.
        return payload.chunkIndex == request.chunkIndex &&
               payload.adtTileX == request.adtTileX &&
               payload.adtTileY == request.adtTileY;
    }

    void RecordAttachReject(RuntimeState& state, char const* reason, char const* detail)
    {
        if (!TelemetryEnabled(state))
            return;

        state.lastAttachRejectReason = reason ? reason : "<unknown>";
        if (std::string(state.lastAttachRejectReason).find("off-target") != std::string::npos)
            ++state.offTargetRejects;
        RecordFallbackUnlocked(state, reason, detail);
    }

    ChunkRequest* FindOrCreateRequest(RuntimeState& state, uintptr_t chunkPtr, uint32_t nowMs)
    {
        ChunkRequest* request = FindRequest(state, chunkPtr);
        if (request)
            return request;

        ChunkRequest next;
        next.chunkPtr = chunkPtr;
        next.firstSeenMs = nowMs;
        next.lastSeenMs = nowMs;
        next.generation = state.generation;
        state.requests.push_back(next);
        state.requestIndex[chunkPtr] = state.requests.size() - 1;
        return &state.requests.back();
    }

    void SyncTextureSetIdentityFromNativeChunk(RuntimeState& state, void* textureSet, uint32_t nowMs)
    {
        if (!textureSet)
            return;

        uintptr_t const textureSetPtr = reinterpret_cast<uintptr_t>(textureSet);
        uintptr_t nativeChunkPtr = 0;
        if (!ResolveNativeChunkFromTextureSet(state, textureSet, nativeChunkPtr))
            return;

        ChunkRequest* nativeRequest = FindRequest(state, nativeChunkPtr);
        if (!nativeRequest)
        {
            nativeRequest = FindOrCreateRequest(state, nativeChunkPtr, nowMs);
            nativeRequest->nativeLayerCount = ReadU8At(reinterpret_cast<void*>(nativeChunkPtr), 0x09);
            bool const trustedLoadIdentity =
                FillChunkRequestCoordsFromLoadFields(*nativeRequest, reinterpret_cast<void*>(nativeChunkPtr));
            if (trustedLoadIdentity)
                nativeRequest->tileLoadIdentity = true;
            if (!trustedLoadIdentity)
                FillChunkRequestCoordsFromIdentity(*nativeRequest, reinterpret_cast<void*>(nativeChunkPtr));
            if (!nativeRequest->coordsValid)
                FillChunkRequestCoordsFromRenderOrigin(*nativeRequest, reinterpret_cast<void*>(nativeChunkPtr));
            RefreshOwnerIdentity(state, *nativeRequest, reinterpret_cast<void*>(nativeChunkPtr), true);
        }

        if (!nativeRequest->tileLoadIdentity)
            return;

        ChunkRequest* textureSetRequest = FindOrCreateRequest(state, textureSetPtr, nowMs);
        bool const identityChanged = textureSetRequest->coordsValid &&
                                     (textureSetRequest->worldChunkX != nativeRequest->worldChunkX ||
                                      textureSetRequest->worldChunkY != nativeRequest->worldChunkY ||
                                      textureSetRequest->nativeChunkPtr != nativeChunkPtr);

        textureSetRequest->localChunkX = nativeRequest->localChunkX;
        textureSetRequest->localChunkY = nativeRequest->localChunkY;
        textureSetRequest->worldChunkX = nativeRequest->worldChunkX;
        textureSetRequest->worldChunkY = nativeRequest->worldChunkY;
        textureSetRequest->adtTileX = nativeRequest->adtTileX;
        textureSetRequest->adtTileY = nativeRequest->adtTileY;
        textureSetRequest->chunkIndex = nativeRequest->chunkIndex;
        textureSetRequest->coordsValid = nativeRequest->coordsValid;
        textureSetRequest->adtTileValid = nativeRequest->adtTileValid;
        textureSetRequest->tileLoadIdentity = true;
        textureSetRequest->textureSetPtr = textureSetPtr;
        textureSetRequest->nativeChunkPtr = nativeChunkPtr;
        textureSetRequest->ownerGeneration = nativeRequest->ownerGeneration;
        textureSetRequest->nativeLayerCount = ReadU8At(textureSet, 0x09);
        textureSetRequest->lastSeenMs = nowMs;
        textureSetRequest->generation = state.generation;

        if (identityChanged)
        {
            bool preservedPayload = false;
            if (PayloadRecord* payload = FindPayload(state, textureSetPtr))
            {
                bool const sameLogicalChunk = payload->adtTileX == textureSetRequest->adtTileX &&
                                              payload->adtTileY == textureSetRequest->adtTileY &&
                                              payload->chunkIndex == textureSetRequest->chunkIndex;
                bool const payloadUsable = payload->resident && payload->textureHandle != 0;
                if (sameLogicalChunk && payloadUsable)
                {
                    // Preserve resident payload across texture-set pointer churn and rebind ownership.
                    payload->chunkPtr = textureSetPtr;
                    payload->textureSetPtr = textureSetPtr;
                    payload->nativeChunkPtr = nativeChunkPtr;
                    payload->ownerGeneration = textureSetRequest->ownerGeneration;
                    payload->tileLoadIdentity = textureSetRequest->tileLoadIdentity;
                    payload->worldChunkX = textureSetRequest->worldChunkX;
                    payload->worldChunkY = textureSetRequest->worldChunkY;
                    payload->adtTileX = textureSetRequest->adtTileX;
                    payload->adtTileY = textureSetRequest->adtTileY;
                    payload->chunkIndex = textureSetRequest->chunkIndex;
                    payload->lastSeenMs = nowMs;
                    payload->lastGeneration = state.generation;

                    textureSetRequest->cacheLookupAttempted = true;
                    textureSetRequest->cacheLookupHit = !payload->sourceStackHash.empty();
                    textureSetRequest->sourceStackHash = payload->sourceStackHash;
                    preservedPayload = true;
                    RecordLifecycleUnlocked(state, "texture-set-identity-remap", "preserved-resident-payload");
                }
                else
                {
                    ReleasePayloadHandle(state, *payload, "texture-set-identity-changed");
                    state.payloads.erase(std::remove_if(state.payloads.begin(), state.payloads.end(),
                                                        [textureSetPtr](PayloadRecord const& item) {
                                                            return item.chunkPtr == textureSetPtr;
                                                        }),
                                         state.payloads.end());
                }
            }

            if (!preservedPayload)
            {
                textureSetRequest->cacheLookupAttempted = false;
                textureSetRequest->cacheLookupHit = false;
                textureSetRequest->sourceStackHash.clear();
            }

            RebuildPayloadIndex(state);
            RecalculatePayloadCounters(state);
        }

        if (state.config.autoQueueCache && !textureSetRequest->cacheLookupAttempted)
            EnqueueRequest(state, textureSetPtr);
    }

    void TryAutoQueueCachedPayloadUnlocked(RuntimeState& state, ChunkRequest& request)
    {
        if (!state.config.autoQueueCache || request.cacheLookupAttempted)
            return;

        request.cacheLookupAttempted = true;
        ++state.autoCacheLookups;

        if (!request.tileLoadIdentity)
        {
            RecordAutoCacheMiss(state, "untrusted-owner-identity", "waiting-for-tile-load");
            return;
        }

        if (PayloadRecord* existing = FindPayload(state, request.chunkPtr))
        {
            if (existing->worldChunkX == request.worldChunkX && existing->worldChunkY == request.worldChunkY)
                return;

            ReleasePayloadHandle(state, *existing, "chunk-pointer-reused");
            uintptr_t const reusedChunkPtr = request.chunkPtr;
            state.payloads.erase(std::remove_if(state.payloads.begin(), state.payloads.end(),
                                                [reusedChunkPtr](PayloadRecord const& item) {
                                                    return item.chunkPtr == reusedChunkPtr;
                                                }),
                                 state.payloads.end());
            RebuildPayloadIndex(state);
            RecalculatePayloadCounters(state);
        }

        int32_t const chunkIndex = ResolveChunkIndex(state, request);
        state.lastAutoCacheChunkIndex = chunkIndex;
        std::string manifestPath;
        std::string blpPath;
        std::string sourceHash;
        std::string materialRawPath;
        int32_t textureBytes = 0;
        int32_t materialBytes = 0;
        int32_t materialWidth = 0;
        int32_t materialHeight = 0;
        std::vector<uint8_t> materialMap;
        std::vector<int32_t> layerMaterialIds;
        std::string const* sidecar = nullptr;
        if (!state.config.forceTestBlp)
        {
            sidecar = CachedSidecar(state);
            if (!sidecar)
            {
                ++state.autoCacheSidecarMisses;
                RecordAutoCacheMiss(state, "sidecar-load-failed", state.config.stackPath);
                return;
            }
        }

        if (!state.config.forceTestBlp && !RequestMatchesCachedStackTile(state, request))
        {
            int32_t resolvedAdtX = request.adtTileX;
            int32_t resolvedAdtY = request.adtTileY;
            if (!request.adtTileValid)
                ClientChunkToAdtTile(request.worldChunkX, request.worldChunkY, resolvedAdtX, resolvedAdtY);
            int32_t resolvedCellX = 0;
            int32_t resolvedCellY = 0;
            AdtTileToServerCell(resolvedAdtX, resolvedAdtY, resolvedCellX, resolvedCellY);
            std::ostringstream detail;
            detail << "stackTile=(" << state.cachedStackTileX << "," << state.cachedStackTileY << ")"
                   << " resolvedAdt=(" << resolvedAdtX << "," << resolvedAdtY << ")"
                   << " resolvedCell=(" << resolvedCellX << "," << resolvedCellY << ")"
                   << " local=(" << request.localChunkX << "," << request.localChunkY << ")"
                   << " world=(" << request.worldChunkX << "," << request.worldChunkY << ")";
            RecordAutoCacheMiss(state, "outside-stack-tile", detail.str());
            return;
        }

        if (state.config.forceTestBlp)
        {
            uint32_t const nowMs = GetTickCount();
            request.cacheLookupHit = true;
            request.sourceStackHash = "forced-extension-test";

            PayloadRecord* payload = FindOrCreatePayload(state, request.chunkPtr, nowMs);
            CopyOwnerIdentityToPayload(*payload, request);
            payload->textureBytes = 65536;
            payload->materialBytes = 0;
            payload->resident = false;
            payload->pendingUpload = true;
            payload->uploadFailed = false;
            payload->swapped = false;
            payload->lastSeenMs = nowMs;
            payload->lastGeneration = state.generation;
            payload->sourceStackHash = request.sourceStackHash;
            payload->quality = state.config.quality;
            payload->texturePath = kForcedExtensionBlpPath;
            payload->manifestPath = "forced-extension-test";
            payload->materialRawPath.clear();
            payload->materialWidth = 0;
            payload->materialHeight = 0;
            payload->materialMap.clear();
            payload->layerMaterialIds.clear();
            payload->materialMapLoaded = false;

            ++state.autoCacheHits;
            ++state.uploadAccepted;
            ++state.payloadValidationOk;
            RebuildPayloadIndex(state);
            RecalculatePayloadCounters(state);
            return;
        }

        if (chunkIndex < 0 || chunkIndex >= static_cast<int32_t>(state.cachedStackHashes.size()))
        {
            ++state.autoCacheInvalidIndexMisses;
            std::ostringstream detail;
            detail << "chunkIndex=" << chunkIndex
                   << " local=(" << request.localChunkX << "," << request.localChunkY << ")"
                   << " world=(" << request.worldChunkX << "," << request.worldChunkY << ")";
            RecordAutoCacheMiss(state, "invalid-chunk-index", detail.str());
            return;
        }

        if (sidecar && chunkIndex >= 0 && chunkIndex < static_cast<int32_t>(state.cachedStackHashes.size()))
            sourceHash = state.cachedStackHashes[chunkIndex];

        if (sourceHash.empty())
        {
            ++state.autoCacheHashMisses;
            std::ostringstream detail;
            detail << "chunkIndex=" << chunkIndex
                   << " local=(" << request.localChunkX << "," << request.localChunkY << ")"
                   << " world=(" << request.worldChunkX << "," << request.worldChunkY << ")";
            RecordAutoCacheMiss(state, "stack-hash-missing", detail.str());
            return;
        }

        if (!ResolveCachedPayload(state, sourceHash, manifestPath, blpPath, textureBytes, materialBytes,
                                  materialRawPath, materialWidth, materialHeight, materialMap, layerMaterialIds))
        {
            ++state.autoCachePayloadMisses;
            std::ostringstream detail;
            detail << "chunkIndex=" << chunkIndex << " hash=" << sourceHash
                   << " quickSize=" << state.config.quickSize << " quality=" << state.config.quality
                   << " cacheRoot=" << state.config.cacheRoot;
            RecordAutoCacheMiss(state, "payload-manifest-missing", detail.str());
            return;
        }

        uint32_t const nowMs = GetTickCount();
        request.cacheLookupHit = true;
        request.sourceStackHash = sourceHash;

        PayloadRecord* payload = FindOrCreatePayload(state, request.chunkPtr, nowMs);
        CopyOwnerIdentityToPayload(*payload, request);
        payload->textureBytes = textureBytes;
        payload->materialBytes = materialBytes;
        payload->resident = false;
        payload->pendingUpload = true;
        payload->uploadFailed = false;
        payload->swapped = false;
        payload->lastSeenMs = nowMs;
        payload->lastGeneration = state.generation;
        payload->sourceStackHash = sourceHash;
        payload->quality = state.config.quality;
        payload->texturePath = blpPath;
        payload->manifestPath = manifestPath;
        payload->materialRawPath = materialRawPath;
        payload->materialWidth = materialWidth;
        payload->materialHeight = materialHeight;
        payload->materialMap = materialMap;
        payload->layerMaterialIds = layerMaterialIds;
        payload->materialMapLoaded = !payload->materialMap.empty() &&
                                     payload->materialWidth > 0 &&
                                     payload->materialHeight > 0 &&
                                     payload->materialMap.size() >=
                                         static_cast<size_t>(payload->materialWidth * payload->materialHeight);

        ++state.autoCacheHits;
        ++state.uploadAccepted;
        ++state.payloadValidationOk;
        RebuildPayloadIndex(state);
        RecalculatePayloadCounters(state);
    }

    void PumpAutoQueueBudgetUnlocked(RuntimeState& state, uint32_t nowMs)
    {
        if (!state.config.autoQueueCache)
            return;

        ResetAutoQueueFrameIfNeeded(state, nowMs);
        uint32_t const maxLookups = static_cast<uint32_t>((std::max)(0, state.config.autoQueueLookupsPerFrame));
        if (maxLookups == 0)
            return;

        uint32_t const maxPops = static_cast<uint32_t>((std::max)(1, state.config.autoQueuePopsPerFrame));
        uint32_t pops = 0;
        while (state.autoQueueFrameCount < maxLookups && pops < maxPops && !state.queue.empty())
        {
            uintptr_t const chunkPtr = state.queue.front();
            state.queue.pop_front();
            state.queuedRequests.erase(chunkPtr);
            ++pops;

            ChunkRequest* request = FindRequest(state, chunkPtr);
            if (!request || request->cacheLookupAttempted)
                continue;

            ++state.autoQueueFrameCount;
            TryAutoQueueCachedPayloadUnlocked(state, *request);
        }

        if (!state.queue.empty())
            ++state.autoQueueDeferred;
    }

    bool BeginTerrainDrawOverride(int chunk, TerrainDrawOverride& restore)
    {
        RuntimeState& state = State();
        std::unique_lock<std::mutex> lock(state.mutex, std::try_to_lock);
        if (!lock.owns_lock())
            return false;

        if (!state.configured || !state.config.enabled || !state.config.nativeUpload ||
            state.runtimeDisabled || state.circuitOpen)
        {
            state.lastSwapMissReason = "gate-disabled";
            return false;
        }

        uintptr_t const chunkPtr = static_cast<uintptr_t>(chunk);
        state.lastDrawChunkPtr = chunkPtr;
        ChunkRequest* request = FindRequest(state, chunkPtr);
        if (!request)
            request = FindRequestByNativeChunk(state, chunkPtr);
        bool const hasForceTicket =
            state.pendingForceReattachOwners.find(chunkPtr) != state.pendingForceReattachOwners.end() ||
            (request && request->nativeChunkPtr &&
             state.pendingForceReattachOwners.find(request->nativeChunkPtr) != state.pendingForceReattachOwners.end());
        bool const allowDrawSwap = state.config.drawReplacement || hasForceTicket;
        if (!allowDrawSwap)
        {
            state.lastSwapMissReason = "draw-swap-not-armed";
            return false;
        }

        PayloadRecord* payload = FindPayload(state, chunkPtr);
        if (!payload)
            payload = FindPayloadByNativeChunk(state, chunkPtr);
        if (!payload && request && request->nativeChunkPtr)
            payload = FindPayloadByNativeChunk(state, request->nativeChunkPtr);
        if (!payload)
        {
            int32_t const localChunkX = ReadInt32At(reinterpret_cast<void*>(chunkPtr), 0x24);
            int32_t const localChunkY = ReadInt32At(reinterpret_cast<void*>(chunkPtr), 0x28);
            int32_t const globalChunkX = ReadInt32At(reinterpret_cast<void*>(chunkPtr), 0x34);
            int32_t const globalChunkY = ReadInt32At(reinterpret_cast<void*>(chunkPtr), 0x38);
            if (IsLocalChunkCoord(localChunkX) && IsLocalChunkCoord(localChunkY) &&
                IsValidGlobalChunkPair(globalChunkX, globalChunkY))
            {
                int32_t adtX = -1;
                int32_t adtY = -1;
                ClientChunkToAdtTile(globalChunkX, globalChunkY, adtX, adtY);
                int32_t const chunkIndex = localChunkY * 16 + localChunkX;
                payload = FindPayloadByTileChunkIndex(state, adtX, adtY, chunkIndex);
                if (payload && request)
                {
                    request->adtTileX = adtX;
                    request->adtTileY = adtY;
                    request->adtTileValid = adtX >= 0 && adtY >= 0;
                    request->localChunkX = localChunkX;
                    request->localChunkY = localChunkY;
                    request->worldChunkX = globalChunkX;
                    request->worldChunkY = globalChunkY;
                    request->chunkIndex = chunkIndex;
                }
            }
        }
        if (!payload || !payload->resident || !payload->textureHandle)
        {
            ++state.swapFallbacks;
            state.lastSwapMissChunkPtr = chunkPtr;
            state.lastSwapMissReason = !payload ? "payload-not-found" :
                                       (!payload->resident ? "payload-not-resident" : "texture-handle-missing");
            if (request)
            {
                state.lastSwapMissAdtTileX = request->adtTileX;
                state.lastSwapMissAdtTileY = request->adtTileY;
                state.lastSwapMissWorldChunkX = request->worldChunkX;
                state.lastSwapMissWorldChunkY = request->worldChunkY;
            }
            return false;
        }

        if (!request || !request->coordsValid || !RequestMatchesCachedStackTile(state, *request))
        {
            ++state.swapFallbacks;
            state.lastSwapMissChunkPtr = chunkPtr;
            state.lastSwapMissReason = "current-chunk-outside-stack-tile";
            if (request)
            {
                state.lastSwapMissAdtTileX = request->adtTileX;
                state.lastSwapMissAdtTileY = request->adtTileY;
                state.lastSwapMissWorldChunkX = request->worldChunkX;
                state.lastSwapMissWorldChunkY = request->worldChunkY;
            }
            return false;
        }

        if (payload->worldChunkX != request->worldChunkX || payload->worldChunkY != request->worldChunkY)
        {
            ++state.swapFallbacks;
            state.lastSwapMissChunkPtr = chunkPtr;
            state.lastSwapMissReason = "stale-payload-coords";
            state.lastSwapMissAdtTileX = request->adtTileX;
            state.lastSwapMissAdtTileY = request->adtTileY;
            state.lastSwapMissWorldChunkX = request->worldChunkX;
            state.lastSwapMissWorldChunkY = request->worldChunkY;
            return false;
        }

        if (!request->cacheLookupHit || request->sourceStackHash.empty() ||
            payload->sourceStackHash != request->sourceStackHash)
        {
            ++state.swapFallbacks;
            state.lastSwapMissChunkPtr = chunkPtr;
            state.lastSwapMissReason = "stale-payload-hash";
            state.lastSwapMissAdtTileX = request->adtTileX;
            state.lastSwapMissAdtTileY = request->adtTileY;
            state.lastSwapMissWorldChunkX = request->worldChunkX;
            state.lastSwapMissWorldChunkY = request->worldChunkY;
            return false;
        }

        if (!PayloadMatchesCachedStackTile(state, *payload))
        {
            ++state.swapFallbacks;
            state.lastSwapMissChunkPtr = chunkPtr;
            state.lastSwapMissReason = "outside-stack-tile";
            state.lastSwapMissAdtTileX = payload->adtTileX;
            state.lastSwapMissAdtTileY = payload->adtTileY;
            state.lastSwapMissWorldChunkX = payload->worldChunkX;
            state.lastSwapMissWorldChunkY = payload->worldChunkY;
            return false;
        }

        restore.active = true;
        restore.targetedForce = hasForceTicket;
        restore.chunkPtr = chunkPtr;
        restore.layerCount = ReadU8At(reinterpret_cast<void*>(chunkPtr), 0x09);
        restore.slot0TextureHandle = ReadU32At(reinterpret_cast<void*>(chunkPtr), 0x38);
        restore.compositeTextureHandle = ReadU32At(reinterpret_cast<void*>(chunkPtr), 0x84);

        WriteU8At(reinterpret_cast<void*>(chunkPtr), 0x09, 1);
        WriteU32At(reinterpret_cast<void*>(chunkPtr), 0x38, static_cast<uint32_t>(payload->textureHandle));

        payload->swapped = true;
        ++state.swapActivations;
        state.lastSwapActivationChunkPtr = chunkPtr;
        state.lastSwapActivationMs = GetTickCount();
        state.lastSwapActivationLocalChunkX = request->localChunkX;
        state.lastSwapActivationLocalChunkY = request->localChunkY;
        state.lastSwapActivationWorldChunkX = request->worldChunkX;
        state.lastSwapActivationWorldChunkY = request->worldChunkY;
        state.lastSwapActivationAdtTileX = request->adtTileX;
        state.lastSwapActivationAdtTileY = request->adtTileY;
        if (hasForceTicket)
        {
            state.pendingForceReattachOwners.erase(chunkPtr);
            if (request && request->nativeChunkPtr)
                state.pendingForceReattachOwners.erase(request->nativeChunkPtr);
        }
        return true;
    }

    void EndTerrainDrawOverride(TerrainDrawOverride const& restore)
    {
        if (!restore.active || !restore.chunkPtr)
            return;

        void* chunk = reinterpret_cast<void*>(restore.chunkPtr);
        WriteU8At(chunk, 0x09, restore.layerCount);
        WriteU32At(chunk, 0x38, restore.slot0TextureHandle);
        WriteU32At(chunk, 0x84, restore.compositeTextureHandle);

    }

    bool BeginOverlayRenderState(D3DOverlayState& restore)
    {
        restore.device = D3D::GetDevice();
        if (!restore.device)
            return false;

        struct SavedRenderState
        {
            D3DRENDERSTATETYPE state;
            DWORD* value;
        };

        SavedRenderState const saved[] = {
            { D3DRS_ALPHABLENDENABLE, &restore.alphaBlendEnable },
            { D3DRS_SRCBLEND, &restore.srcBlend },
            { D3DRS_DESTBLEND, &restore.destBlend },
            { D3DRS_BLENDOP, &restore.blendOp },
            { D3DRS_ZWRITEENABLE, &restore.zWriteEnable },
            { D3DRS_ALPHATESTENABLE, &restore.alphaTestEnable },
            { D3DRS_ALPHAREF, &restore.alphaRef },
            { D3DRS_ALPHAFUNC, &restore.alphaFunc },
        };

        for (SavedRenderState const& item : saved)
        {
            if (FAILED(restore.device->GetRenderState(item.state, item.value)))
                return false;
        }
        if (FAILED(restore.device->GetPixelShader(&restore.pixelShader)))
            restore.pixelShader = nullptr;

        restore.device->GetTextureStageState(0, D3DTSS_COLOROP, &restore.colorOp0);
        restore.device->GetTextureStageState(0, D3DTSS_COLORARG1, &restore.colorArg10);
        restore.device->GetTextureStageState(0, D3DTSS_COLORARG2, &restore.colorArg20);
        restore.device->GetTextureStageState(0, D3DTSS_ALPHAOP, &restore.alphaOp0);
        restore.device->GetTextureStageState(0, D3DTSS_ALPHAARG1, &restore.alphaArg10);
        restore.device->GetTextureStageState(1, D3DTSS_COLOROP, &restore.colorOp1);
        restore.device->GetTextureStageState(1, D3DTSS_ALPHAOP, &restore.alphaOp1);

        restore.device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        restore.device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        restore.device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        restore.device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
        restore.device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        restore.device->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
        restore.device->SetRenderState(D3DRS_ALPHAREF, 1);
        restore.device->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATER);
        restore.device->SetPixelShader(nullptr);
        restore.device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        restore.device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        restore.device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        restore.device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
        restore.device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
        restore.device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
        restore.device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
        restore.active = true;
        return true;
    }

    void EndOverlayRenderState(D3DOverlayState const& restore)
    {
        if (!restore.active || !restore.device)
            return;

        restore.device->SetRenderState(D3DRS_ALPHABLENDENABLE, restore.alphaBlendEnable);
        restore.device->SetRenderState(D3DRS_SRCBLEND, restore.srcBlend);
        restore.device->SetRenderState(D3DRS_DESTBLEND, restore.destBlend);
        restore.device->SetRenderState(D3DRS_BLENDOP, restore.blendOp);
        restore.device->SetRenderState(D3DRS_ZWRITEENABLE, restore.zWriteEnable);
        restore.device->SetRenderState(D3DRS_ALPHATESTENABLE, restore.alphaTestEnable);
        restore.device->SetRenderState(D3DRS_ALPHAREF, restore.alphaRef);
        restore.device->SetRenderState(D3DRS_ALPHAFUNC, restore.alphaFunc);
        restore.device->SetTextureStageState(0, D3DTSS_COLOROP, restore.colorOp0);
        restore.device->SetTextureStageState(0, D3DTSS_COLORARG1, restore.colorArg10);
        restore.device->SetTextureStageState(0, D3DTSS_COLORARG2, restore.colorArg20);
        restore.device->SetTextureStageState(0, D3DTSS_ALPHAOP, restore.alphaOp0);
        restore.device->SetTextureStageState(0, D3DTSS_ALPHAARG1, restore.alphaArg10);
        restore.device->SetTextureStageState(1, D3DTSS_COLOROP, restore.colorOp1);
        restore.device->SetTextureStageState(1, D3DTSS_ALPHAOP, restore.alphaOp1);
        restore.device->SetPixelShader(restore.pixelShader);
        if (restore.pixelShader)
            restore.pixelShader->Release();
    }

    bool DrawExtendedPayloadOverlay(int chunk, TerrainDrawFn nativeDrawFn, uintptr_t nativeAddress)
    {
        RuntimeState& state = State();
        if (state.residentPayloadCount.load(std::memory_order_acquire) == 0)
        {
            std::unique_lock<std::mutex> lock(state.mutex, std::try_to_lock);
            if (lock.owns_lock())
            {
                ++state.drawWrapperNoResidents;
                state.lastDrawChunkPtr = static_cast<uintptr_t>(chunk);
                state.lastNativeDrawFunction = nativeAddress;
            }
            return false;
        }

        {
            std::unique_lock<std::mutex> lock(state.mutex, std::try_to_lock);
            if (lock.owns_lock())
                ++state.overlayPassAttempts;
        }

        TerrainDrawOverride textureRestore;
        if (!BeginTerrainDrawOverride(chunk, textureRestore))
            return false;

        D3DOverlayState stateRestore;
        if (!BeginOverlayRenderState(stateRestore))
        {
            EndTerrainDrawOverride(textureRestore);
            std::unique_lock<std::mutex> lock(state.mutex, std::try_to_lock);
            if (lock.owns_lock())
            {
                ++state.overlayPassStateFailures;
                RecordFallbackUnlocked(state, "overlay-render-state-failed", "d3d-state");
            }
            return false;
        }

        gTerrainOverlayPassActive = true;
        gTerrainDrawOverrideActive = true;
        nativeDrawFn(chunk);
        gTerrainDrawOverrideActive = false;
        gTerrainOverlayPassActive = false;
        EndOverlayRenderState(stateRestore);
        EndTerrainDrawOverride(textureRestore);

        {
            std::unique_lock<std::mutex> lock(state.mutex, std::try_to_lock);
            if (lock.owns_lock())
            {
                ++state.overlayPassApplied;
                state.lastNativeDrawFunction = nativeAddress;
            }
        }
        return true;
    }

    bool DrawExtendedPayloadOverlay(void* chunk, TerrainDrawThiscallFn nativeDrawFn, uintptr_t nativeAddress)
    {
        RuntimeState& state = State();
        int const chunkPtr = reinterpret_cast<int>(chunk);
        if (state.residentPayloadCount.load(std::memory_order_acquire) == 0)
        {
            std::unique_lock<std::mutex> lock(state.mutex, std::try_to_lock);
            if (lock.owns_lock())
            {
                ++state.drawWrapperNoResidents;
                state.lastDrawChunkPtr = static_cast<uintptr_t>(chunkPtr);
                state.lastNativeDrawFunction = nativeAddress;
            }
            return false;
        }

        {
            std::unique_lock<std::mutex> lock(state.mutex, std::try_to_lock);
            if (lock.owns_lock())
                ++state.overlayPassAttempts;
        }

        TerrainDrawOverride textureRestore;
        if (!BeginTerrainDrawOverride(chunkPtr, textureRestore))
            return false;

        D3DOverlayState stateRestore;
        if (!BeginOverlayRenderState(stateRestore))
        {
            EndTerrainDrawOverride(textureRestore);
            std::unique_lock<std::mutex> lock(state.mutex, std::try_to_lock);
            if (lock.owns_lock())
            {
                ++state.overlayPassStateFailures;
                RecordFallbackUnlocked(state, "overlay-render-state-failed", "d3d-state");
            }
            return false;
        }

        gTerrainOverlayPassActive = true;
        gTerrainDrawOverrideActive = true;
        nativeDrawFn(chunk);
        gTerrainDrawOverrideActive = false;
        gTerrainOverlayPassActive = false;
        EndOverlayRenderState(stateRestore);
        EndTerrainDrawOverride(textureRestore);

        {
            std::unique_lock<std::mutex> lock(state.mutex, std::try_to_lock);
            if (lock.owns_lock())
            {
                ++state.overlayPassApplied;
                state.lastNativeDrawFunction = nativeAddress;
            }
        }
        return true;
    }

    void InvokeTerrainDrawWithOverride(int chunk, TerrainDrawFn nativeDrawFn, uintptr_t nativeAddress)
    {
        RuntimeState& state = State();
        if (!nativeDrawFn)
        {
            std::unique_lock<std::mutex> lock(state.mutex, std::try_to_lock);
            if (lock.owns_lock())
                ++state.drawWrapperNoNative;
            return;
        }

        if (!state.configured || !state.config.drawReplacement || state.runtimeDisabled || state.circuitOpen)
        {
            nativeDrawFn(chunk);
            return;
        }

        if (gTerrainDrawOverrideActive)
        {
            nativeDrawFn(chunk);
            return;
        }

        {
            std::unique_lock<std::mutex> lock(state.mutex, std::try_to_lock);
            if (lock.owns_lock())
            {
                ++state.drawWrapperCalls;
                state.lastDrawChunkPtr = static_cast<uintptr_t>(chunk);
                state.lastNativeDrawFunction = nativeAddress;
            }
        }

        gTerrainDrawOverrideActive = true;
        nativeDrawFn(chunk);
        gTerrainDrawOverrideActive = false;
        DrawExtendedPayloadOverlay(chunk, nativeDrawFn, nativeAddress);
    }

    void __cdecl TerrainDrawOverrideWrapper(int chunk)
    {
        RuntimeState& state = State();
        uintptr_t const drawFn = state.nativeTerrainDrawFn.load(std::memory_order_acquire);
        InvokeTerrainDrawWithOverride(chunk, reinterpret_cast<TerrainDrawFn>(drawFn), drawFn);
    }

    void InvokeTerrainDrawThiscallWithOverride(void* chunk, TerrainDrawThiscallFn nativeDrawFn, uintptr_t nativeAddress)
    {
        RuntimeState& state = State();
        if (!nativeDrawFn)
        {
            std::unique_lock<std::mutex> lock(state.mutex, std::try_to_lock);
            if (lock.owns_lock())
                ++state.drawWrapperNoNative;
            return;
        }

        if (!state.configured || !state.config.drawReplacement || state.runtimeDisabled || state.circuitOpen)
        {
            nativeDrawFn(chunk);
            return;
        }

        if (gTerrainDrawOverrideActive)
        {
            nativeDrawFn(chunk);
            return;
        }

        int const chunkPtr = reinterpret_cast<int>(chunk);
        {
            std::unique_lock<std::mutex> lock(state.mutex, std::try_to_lock);
            if (lock.owns_lock())
            {
                ++state.drawWrapperCalls;
                state.lastDrawChunkPtr = static_cast<uintptr_t>(chunkPtr);
                state.lastNativeDrawFunction = nativeAddress;
            }
        }

        gTerrainDrawOverrideActive = true;
        nativeDrawFn(chunk);
        gTerrainDrawOverrideActive = false;
        DrawExtendedPayloadOverlay(chunk, nativeDrawFn, nativeAddress);
    }

    void RestoreTerrainDrawOverrideUnlocked(RuntimeState& state)
    {
        uintptr_t* drawFnPtr = reinterpret_cast<uintptr_t*>(kTerrainDrawFunctionPointer);
        uintptr_t const wrapper = reinterpret_cast<uintptr_t>(&TerrainDrawOverrideWrapper);
        uintptr_t const native = state.nativeTerrainDrawFn.load(std::memory_order_acquire);

        if (*drawFnPtr == wrapper && native)
        {
            *drawFnPtr = native;
            ++state.drawOverrideRestores;
            state.lastDrawFunction = native;
        }
    }

    void ApplyTerrainDrawOverrideUnlocked(RuntimeState& state)
    {
        uintptr_t* drawFnPtr = reinterpret_cast<uintptr_t*>(kTerrainDrawFunctionPointer);
        uintptr_t const current = *drawFnPtr;
        uintptr_t const wrapper = reinterpret_cast<uintptr_t>(&TerrainDrawOverrideWrapper);
        state.lastDrawFunction = current;

        if (!state.configured || !state.config.enabled || !state.config.renderSwap || !state.config.drawReplacement ||
            state.runtimeDisabled || state.circuitOpen)
        {
            RestoreTerrainDrawOverrideUnlocked(state);
            return;
        }

        if (!current || current == wrapper)
            return;

        state.nativeTerrainDrawFn.store(current, std::memory_order_release);
        state.lastNativeDrawFunction = current;
        *drawFnPtr = wrapper;
        state.lastDrawFunction = wrapper;
        ++state.drawOverrideInstalls;
    }

    void EvictPayload(RuntimeState& state, uintptr_t chunkPtr, char const* reason)
    {
        PayloadRecord* payload = FindPayload(state, chunkPtr);
        if (!payload)
            return;

        LOG_INFO << "ExtendedTerrainTextures: evict payload chunk=0x" << std::hex << chunkPtr << std::dec
                 << " reason=" << (reason ? reason : "<none>")
                 << " textureBytes=" << payload->textureBytes
                 << " materialBytes=" << payload->materialBytes
                 << " textureHandle=" << payload->textureHandle
                 << " resident=" << (payload->resident ? 1 : 0);

        ReleasePayloadHandle(state, *payload, reason);
        state.payloads.erase(std::remove_if(state.payloads.begin(), state.payloads.end(),
                                            [chunkPtr](PayloadRecord const& item) { return item.chunkPtr == chunkPtr; }),
                             state.payloads.end());
        RebuildPayloadIndex(state);
        ++state.memoryEvictions;
        ++state.budgetFallbacks;
        RecordFallbackUnlocked(state, "memory-budget-pressure", reason);
        RecordLifecycleUnlocked(state, "evict", reason);
        RecalculatePayloadCounters(state);
    }

    void EvictUntilWithinBudget(RuntimeState& state)
    {
        uint64_t const budget = MemoryBudgetBytes(state);
        while (LiveBytes(state) > budget && !state.payloads.empty())
        {
            auto itr = std::min_element(state.payloads.begin(), state.payloads.end(),
                                        [&](PayloadRecord const& left, PayloadRecord const& right) {
                                            if (left.resident != right.resident)
                                                return left.resident && !right.resident;
                                            if (state.hasFocusChunk)
                                            {
                                                int64_t const leftDx = static_cast<int64_t>(left.worldChunkX) - state.focusWorldChunkX;
                                                int64_t const leftDy = static_cast<int64_t>(left.worldChunkY) - state.focusWorldChunkY;
                                                int64_t const rightDx = static_cast<int64_t>(right.worldChunkX) - state.focusWorldChunkX;
                                                int64_t const rightDy = static_cast<int64_t>(right.worldChunkY) - state.focusWorldChunkY;
                                                int64_t const leftDist = leftDx * leftDx + leftDy * leftDy;
                                                int64_t const rightDist = rightDx * rightDx + rightDy * rightDy;
                                                if (leftDist != rightDist)
                                                    return leftDist > rightDist;
                                            }
                                            return left.lastGeneration < right.lastGeneration;
                                        });
            if (itr == state.payloads.end())
                return;

            uintptr_t const chunkPtr = itr->chunkPtr;
            EvictPayload(state, chunkPtr, "memory-budget");
        }
    }

    bool UploadBudgetAllows(RuntimeState& state, PayloadRecord const& payload, uint32_t nowMs, bool& oversized)
    {
        ResetUploadFrameIfNeeded(state, nowMs);
        oversized = false;

        uint32_t const textureBytes = static_cast<uint32_t>((std::max)(0, payload.textureBytes));
        uint32_t const budgetBytes = static_cast<uint32_t>(state.config.uploadBudgetKBPerFrame) * 1024u;
        uint32_t const costMs = EstimateUploadMs(payload.textureBytes);

        bool const firstUploadThisFrame = state.uploadFrameCount == 0 && state.uploadFrameBytes == 0;
        oversized = textureBytes > budgetBytes || costMs > static_cast<uint32_t>(state.config.uploadBudgetMsPerFrame);

        if (state.uploadFrameCount >= static_cast<uint32_t>(state.config.uploadBudgetCountPerFrame))
            return false;

        if (!firstUploadThisFrame)
        {
            if (state.uploadFrameBytes + textureBytes > budgetBytes)
                return false;
            if (state.uploadFrameMs + costMs > static_cast<uint32_t>(state.config.uploadBudgetMsPerFrame))
                return false;
        }

        return true;
    }

    void SpendUploadBudget(RuntimeState& state, PayloadRecord const& payload)
    {
        state.uploadFrameBytes += static_cast<uint32_t>((std::max)(0, payload.textureBytes));
        state.uploadFrameMs += EstimateUploadMs(payload.textureBytes);
        ++state.uploadFrameCount;
    }

    void ReleasePayloadHandle(RuntimeState& state, PayloadRecord& payload, char const* reason)
    {
        if (payload.attached)
            DetachPayloadFromTextureSet(state, payload, reason ? reason : "release-attached");

        if (!payload.textureHandle)
            return;

        int const handle = payload.textureHandle;
        payload.textureHandle = 0;
        payload.resident = false;
        payload.retiredHandle = handle;
        ++state.retiredTextureHandles;
        state.directTextureCache.erase(handle);
        HandleClose_ExtTex(handle);
        RebuildPayloadIndex(state);
        RecordLifecycleUnlocked(state, "texture-handle-release", reason ? reason : "<none>");
    }

    void DetachPayloadFromTextureSet(RuntimeState& state, PayloadRecord& payload, char const* reason)
    {
        if (!payload.attached || !payload.chunkPtr)
            return;

        void* owner = reinterpret_cast<void*>(payload.chunkPtr);
        WriteU8At(owner, 0x09, payload.savedLayerCount);
        WriteU32At(owner, 0x38, payload.savedSlot0DiffuseHandle);
        WriteU32At(owner, 0x84, payload.savedCompositeHandle);
        payload.attached = false;
        payload.swapped = false;
        state.lastCleanupReason = reason ? reason : "<none>";
        ++state.lifecycleDetachCount;
        RecordLifecycleUnlocked(state, "texture-set-detach", reason ? reason : "<none>");
    }

    bool AttachPayloadToTextureSet(RuntimeState& state, PayloadRecord& payload, ChunkRequest const& request,
                                   char const* reason)
    {
        if (payload.attached)
            return true;

        if (!payload.resident || !payload.textureHandle)
        {
            RecordAttachReject(state, "attach-not-resident", payload.quality.c_str());
            return false;
        }

        if (!request.tileLoadIdentity || !payload.tileLoadIdentity)
        {
            ++state.staleOwnerRejects;
            RecordAttachReject(state, "attach-untrusted-owner-identity", payload.texturePath.c_str());
            return false;
        }

        if (!PayloadOwnerMatchesRequest(payload, request))
        {
            ++state.staleOwnerRejects;
            ++state.generationMismatchRejects;
            RecordAttachReject(state, "attach-owner-mismatch", payload.texturePath.c_str());
            return false;
        }

        if (!RequestMatchesCachedStackTile(state, request) || !PayloadMatchesCachedStackTile(state, payload))
        {
            RecordAttachReject(state, "attach-off-target-stack-tile", payload.texturePath.c_str());
            return false;
        }

        if (!request.cacheLookupHit || request.sourceStackHash.empty() ||
            payload.sourceStackHash != request.sourceStackHash)
        {
            RecordAttachReject(state, "attach-stack-hash-mismatch", payload.texturePath.c_str());
            return false;
        }

        void* owner = reinterpret_cast<void*>(payload.chunkPtr);
        uint8_t const nativeLayerCount = ReadU8At(owner, 0x09);
        if (nativeLayerCount == 0)
        {
            RecordAttachReject(state, "attach-empty-native-layer-set", payload.texturePath.c_str());
            return false;
        }

        payload.savedLayerCount = nativeLayerCount;
        payload.savedSlot0DiffuseHandle = ReadU32At(owner, 0x38);
        payload.savedCompositeHandle = ReadU32At(owner, 0x84);
        // Present baked diffuse in slot 0, but keep native composite handle so
        // shadow/material-related native composite behavior can still participate.
        WriteU8At(owner, 0x09, 1);
        WriteU32At(owner, 0x38, static_cast<uint32_t>(payload.textureHandle));
        WriteU32At(owner, 0x84, payload.savedCompositeHandle);
        payload.attached = true;
        payload.swapped = true;

        ++state.lifecycleAttachCount;
        state.lastAttachedTextureSetPtr = payload.textureSetPtr;
        state.lastAttachedNativeChunkPtr = payload.nativeChunkPtr;
        state.lastAttachedOwnerGeneration = payload.ownerGeneration;
        state.lastAttachedChunkIndex = payload.chunkIndex;
        state.lastAttachedAdtTileX = payload.adtTileX;
        state.lastAttachedAdtTileY = payload.adtTileY;
        state.lastAttachedHandle = payload.textureHandle;

        if (state.config.trace)
        {
            LOG_INFO << "ExtendedTerrainTextures: texture-set attach owner=0x" << std::hex << payload.textureSetPtr
                     << " chunk=0x" << payload.nativeChunkPtr << std::dec
                     << " gen=" << payload.ownerGeneration
                     << " adt=(" << payload.adtTileX << "," << payload.adtTileY << ")"
                     << " chunkIndex=" << payload.chunkIndex
                     << " handle=" << payload.textureHandle
                     << " quality=" << payload.quality
                     << " reason=" << (reason ? reason : "<none>");
        }

        RecordLifecycleUnlocked(state, "texture-set-attach", reason ? reason : "<none>");
        return true;
    }

    void CloseUnownedTextureHandle(RuntimeState& state, int handle, char const* reason)
    {
        if (!handle)
            return;

        state.directTextureCache.erase(handle);
        HandleClose_ExtTex(handle);
        ++state.retiredTextureHandles;
        RecordLifecycleUnlocked(state, "texture-handle-release", reason ? reason : "<none>");
    }

    bool ReadWholeFile(std::string const& path, std::vector<uint8_t>& out)
    {
        out.clear();
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file)
            return false;

        std::streamsize const size = file.tellg();
        if (size <= 0)
            return false;

        file.seekg(0, std::ios::beg);
        out.resize(static_cast<size_t>(size));
        return file.read(reinterpret_cast<char*>(out.data()), size).good();
    }

    int CreateClientTextureDirectFromBlp(std::string const& texturePath)
    {
        std::vector<uint8_t> blpBytes;
        if (!ReadWholeFile(texturePath, blpBytes) || blpBytes.size() < 0x94)
            return 0;

        OpaqueBLPFile blp = {};
        Sub4B58D0_CBLPFileCtor_ExtTex(&blp);
        if (!CBLPFile__Source_ExtTex(&blp, blpBytes.data()))
        {
            CBLPFile__Close_ExtTex(&blp);
            return 0;
        }

        int pixelBytes = 0;
        uint32_t info[4] = {};
        if (!Sub6AF990_LoadBLP_ExtTex(&blp, 2, 0, &pixelBytes, info) || !pixelBytes)
        {
            Sub6AF6E0_FreeBLPDecode_ExtTex(&blp, 0);
            CBLPFile__Close_ExtTex(&blp);
            return 0;
        }

        uint32_t const width = *reinterpret_cast<uint32_t*>(blp.storage.data() + 0x10);
        uint32_t const height = *reinterpret_cast<uint32_t*>(blp.storage.data() + 0x14);
        if (!width || !height || width > 4096 || height > 4096)
        {
            Sub6AF6E0_FreeBLPDecode_ExtTex(&blp, 0);
            CBLPFile__Close_ExtTex(&blp);
            return 0;
        }

        auto entry = std::make_unique<DirectTextureCacheEntry>();
        entry->pixels.resize(width * height);
        entry->width = width;
        entry->height = height;
        std::copy_n(reinterpret_cast<CImVector*>(pixelBytes), entry->pixels.size(), entry->pixels.data());

        RuntimeState& state = State();
        if (state.config.debugCheckerEnabled)
        {
            int32_t alphaPct = state.config.debugCheckerAlphaPct;
            if (alphaPct < 0)
                alphaPct = 0;
            if (alphaPct > 100)
                alphaPct = 100;
            uint32_t const wBase = static_cast<uint32_t>(100 - alphaPct);
            uint32_t const wOver = static_cast<uint32_t>(alphaPct);

            for (uint32_t y = 0; y < height; ++y)
            {
                for (uint32_t x = 0; x < width; ++x)
                {
                    size_t const idx = static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
                    uint32_t& px = *reinterpret_cast<uint32_t*>(&entry->pixels[idx]);
                    uint8_t const a = static_cast<uint8_t>((px >> 24) & 0xFF);
                    uint8_t const r = static_cast<uint8_t>((px >> 16) & 0xFF);
                    uint8_t const g = static_cast<uint8_t>((px >> 8) & 0xFF);
                    uint8_t const b = static_cast<uint8_t>(px & 0xFF);

                    bool const checker = (((x / 32) + (y / 32)) & 1u) != 0;
                    uint8_t const orr = checker ? 255 : 0;
                    uint8_t const org = checker ? 0 : 255;
                    uint8_t const orb = checker ? 255 : 0;

                    uint8_t const nr = static_cast<uint8_t>((static_cast<uint32_t>(r) * wBase +
                                                             static_cast<uint32_t>(orr) * wOver) / 100u);
                    uint8_t const ng = static_cast<uint8_t>((static_cast<uint32_t>(g) * wBase +
                                                             static_cast<uint32_t>(org) * wOver) / 100u);
                    uint8_t const nb = static_cast<uint8_t>((static_cast<uint32_t>(b) * wBase +
                                                             static_cast<uint32_t>(orb) * wOver) / 100u);
                    px = (static_cast<uint32_t>(a) << 24) |
                         (static_cast<uint32_t>(nr) << 16) |
                         (static_cast<uint32_t>(ng) << 8) |
                         static_cast<uint32_t>(nb);
                }
            }
        }

        // Forced red test texture should remain semi-transparent so underlying shape is visible.
        bool const isForcedRedTestTexture = state.config.forceTestBlp && (
            texturePath.find("blastedlandsdirt01.blp") != std::string::npos ||
            texturePath.find("blastedlandsdirt01.BLP") != std::string::npos);
        if (isForcedRedTestTexture)
        {
            for (uint32_t y = 0; y < height; ++y)
            {
                for (uint32_t x = 0; x < width; ++x)
                {
                    size_t const idx = static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
                    // Terrain path here is effectively opaque for this layer, so blend RGB directly.
                    // Keep 25% red marker while preserving underlying texture shape cues.
                    uint8_t const r = entry->pixels[idx].r;
                    uint8_t const g = entry->pixels[idx].g;
                    uint8_t const b = entry->pixels[idx].b;
                    entry->pixels[idx].r = static_cast<uint8_t>((static_cast<uint16_t>(r) * 3u + 255u) / 4u);
                    entry->pixels[idx].g = static_cast<uint8_t>((static_cast<uint16_t>(g) * 3u) / 4u);
                    entry->pixels[idx].b = static_cast<uint8_t>((static_cast<uint16_t>(b) * 3u) / 4u);
                }
            }
        }

        Sub6AF6E0_FreeBLPDecode_ExtTex(&blp, 0);
        CBLPFile__Close_ExtTex(&blp);

        entry->image.m_alloc = static_cast<uint32_t>(entry->pixels.size());
        entry->image.m_count = static_cast<uint32_t>(entry->pixels.size());
        entry->image.m_data = entry->pixels.data();
        entry->image.m_chunk = 0;

        uint32_t flags = 0;
        uint32_t* builtFlags = CGxTexFlags__constructor_ExtTex(&flags, 1, 0, 0, 0, 0, 0, 1u, 0, 0, 0);
        if (!builtFlags)
            return 0;

        entry->debugName = "ExtendedTerrain:" + texturePath;
        int const handle = TextureCreateDirect_ExtTex(width, height, 2, 2, *builtFlags, &entry->image, 0x00616B90,
                                                      entry->debugName.c_str(), 0);
        if (!handle)
            return 0;

        std::unique_lock<std::mutex> lock(state.mutex, std::try_to_lock);
        if (lock.owns_lock())
        {
            state.directTextureCache.emplace(handle, std::move(entry));
            return handle;
        }

        HandleClose_ExtTex(handle);
        return 0;
    }

    int CreateClientTextureFromPath(std::string const& texturePath)
    {
        if (texturePath.empty())
            return 0;

        int const directHandle = CreateClientTextureDirectFromBlp(texturePath);
        if (directHandle)
            return directHandle;

        std::string mutablePath = texturePath;
        std::replace(mutablePath.begin(), mutablePath.end(), '/', '\\');
        std::vector<char> pathBuffer(mutablePath.begin(), mutablePath.end());
        pathBuffer.push_back('\0');
        return CMap__LoadTexture_ExtTex(pathBuffer.data());
    }

    int CreateStrictCheckerTexture()
    {
        constexpr uint32_t kW = 64;
        constexpr uint32_t kH = 64;
        auto entry = std::make_unique<DirectTextureCacheEntry>();
        entry->pixels.resize(kW * kH);
        entry->width = kW;
        entry->height = kH;
        for (uint32_t y = 0; y < kH; ++y)
        {
            for (uint32_t x = 0; x < kW; ++x)
            {
                bool const checker = (((x / 16) + (y / 16)) & 1u) != 0;
                CImVector px = {};
                px.a = 255;
                px.r = checker ? 255 : 0;
                px.g = checker ? 0 : 255;
                px.b = checker ? 255 : 0;
                entry->pixels[static_cast<size_t>(y) * kW + x] = px;
            }
        }

        entry->image.m_alloc = static_cast<uint32_t>(entry->pixels.size());
        entry->image.m_count = static_cast<uint32_t>(entry->pixels.size());
        entry->image.m_data = entry->pixels.data();
        entry->image.m_chunk = 0;

        uint32_t flags = 0;
        uint32_t* builtFlags = CGxTexFlags__constructor_ExtTex(&flags, 1, 0, 0, 0, 0, 0, 1u, 0, 0, 0);
        if (!builtFlags)
            return 0;

        int const handle = TextureCreateDirect_ExtTex(kW, kH, 2, 2, *builtFlags, &entry->image, 0x00616B90,
                                                      "ExtendedTerrain:StrictChecker", 0);
        if (!handle)
            return 0;

        RuntimeState& state = State();
        std::unique_lock<std::mutex> lock(state.mutex, std::try_to_lock);
        if (lock.owns_lock())
            state.directTextureCache.emplace(handle, std::move(entry));
        return handle;
    }

    void DropOldestRequest(RuntimeState& state)
    {
        uintptr_t oldest = 0;
        if (!state.queue.empty())
        {
            oldest = state.queue.front();
            state.queue.pop_front();
            state.queuedRequests.erase(oldest);
        }
        else if (!state.requests.empty())
        {
            auto itr = std::min_element(state.requests.begin(), state.requests.end(),
                                        [](ChunkRequest const& left, ChunkRequest const& right) {
                                            return left.lastSeenMs < right.lastSeenMs;
                                        });
            oldest = itr == state.requests.end() ? 0 : itr->chunkPtr;
        }

        if (!oldest)
            return;

        ++state.queueDrops;
        state.requests.erase(std::remove_if(state.requests.begin(), state.requests.end(),
                                            [oldest](ChunkRequest const& request) { return request.chunkPtr == oldest; }),
                             state.requests.end());
        state.queue.erase(std::remove_if(state.queue.begin(), state.queue.end(),
                                         [oldest](uintptr_t chunkPtr) { return chunkPtr == oldest; }),
                          state.queue.end());
        state.queuedRequests.erase(oldest);
        RebuildRequestIndex(state);
    }

    void SweepStaleRequests(RuntimeState& state, uint32_t nowMs)
    {
        size_t before = state.requests.size();
        state.requests.erase(
            std::remove_if(state.requests.begin(), state.requests.end(),
                           [&](ChunkRequest const& request) {
                               bool const stale = nowMs - request.lastSeenMs >
                                                  static_cast<uint32_t>(state.config.staleRequestMs);
                               if (stale)
                                   ++state.staleDrops;
                               return stale;
                           }),
            state.requests.end());
        RebuildRequestIndex(state);
        state.queue.erase(std::remove_if(state.queue.begin(), state.queue.end(),
                                         [&](uintptr_t chunkPtr) { return FindRequest(state, chunkPtr) == nullptr; }),
                          state.queue.end());
        state.queuedRequests.clear();
        for (uintptr_t chunkPtr : state.queue)
            state.queuedRequests.insert(chunkPtr);

        if (state.config.trace && before != state.requests.size())
        {
            LOG_INFO << "ExtendedTerrainTextures: swept stale requests before=" << before
                     << " after=" << state.requests.size();
        }
    }

    void ClearPending(RuntimeState& state)
    {
        for (PayloadRecord& payload : state.payloads)
            ReleasePayloadHandle(state, payload, "clear-pending");
        ClearRequestQueue(state);
        state.requests.clear();
        state.payloads.clear();
        state.requestIndex.clear();
        state.payloadIndex.clear();
        state.payloadNativeIndex.clear();
        state.payloadTileIndex.clear();
        state.textureSetNativeOffsetCache.clear();
        RecordLifecycleUnlocked(state, "clear-pending", "fallback-active");
        RecalculatePayloadCounters(state);
    }

    void OpenCircuit(RuntimeState& state, char const* operation, char const* detail)
    {
        state.circuitOpen = true;
        ++state.circuitOpens;
        RestoreTerrainDrawOverrideUnlocked(state);
        ClearPending(state);
        RecordFallbackUnlocked(state, "helper-circuit-open", detail);
        LOG_WARN << "ExtendedTerrainTextures: helper circuit opened failures=" << state.helperFailures
                 << " threshold=" << state.config.circuitBreakerFailures
                 << " op=" << (operation ? operation : "<unknown>")
                 << " detail=" << (detail ? detail : "<none>")
                 << ". Native terrain fallback remains active.";
    }

    void LogTraceSummary(RuntimeState& state, uint32_t nowMs)
    {
        if (!TelemetryEnabled(state))
            return;

        if (state.lastSummaryMs != 0 && nowMs - state.lastSummaryMs < 5000)
            return;

        state.lastSummaryMs = nowMs;
        LOG_INFO << "ExtendedTerrainTextures: summary pending=" << state.requests.size()
                 << " queued=" << state.queue.size()
                 << " uploadQueueDepth=" << state.uploadQueueDepth
                 << " uploadFrameCount=" << state.uploadFrameCount
                 << " uploadFrameBytes=" << state.uploadFrameBytes
                 << " uploadFrameMs=" << state.uploadFrameMs
                 << " uploadAccepted=" << state.uploadAccepted
                 << " uploadDeferred=" << state.uploadDeferred
                 << " uploadCompleted=" << state.uploadCompleted
                 << " uploadFailed=" << state.uploadFailed
                 << " textureCreateAttempts=" << state.textureCreateAttempts
                 << " textureCreateFailures=" << state.textureCreateFailures
                 << " retiredTextureHandles=" << state.retiredTextureHandles
                 << " deviceLossCount=" << state.deviceLossCount
                 << " deviceRestoreCount=" << state.deviceRestoreCount
                 << " deviceReuploadQueued=" << state.deviceReuploadQueued
                 << " renderSwap=" << (state.config.renderSwap ? 1 : 0)
                 << " drawSetupCalls=" << state.drawSetupCalls
                 << " drawOverrideInstalls=" << state.drawOverrideInstalls
                 << " drawWrapperCalls=" << state.drawWrapperCalls
                 << " drawWrapperNoResidents=" << state.drawWrapperNoResidents
                 << " overlayPassAttempts=" << state.overlayPassAttempts
                 << " overlayPassApplied=" << state.overlayPassApplied
                 << " overlayPassStateFailures=" << state.overlayPassStateFailures
                 << " swapActivations=" << state.swapActivations
                 << " swapFallbacks=" << state.swapFallbacks
                 << " lastSwapMiss=" << state.lastSwapMissReason
                 << " payloadValidationOk=" << state.payloadValidationOk
                 << " payloadValidationFailed=" << state.payloadValidationFailed
                 << " materialLookupAttempts=" << state.materialLookupAttempts
                 << " materialLookupHits=" << state.materialLookupHits
                 << " materialLookupFallbacks=" << state.materialLookupFallbacks
                 << " autoCacheLookups=" << state.autoCacheLookups
                 << " autoCacheHits=" << state.autoCacheHits
                 << " autoCacheMisses=" << state.autoCacheMisses
                 << " autoCacheSidecarMisses=" << state.autoCacheSidecarMisses
                 << " autoCacheInvalidIndexMisses=" << state.autoCacheInvalidIndexMisses
                 << " autoCacheHashMisses=" << state.autoCacheHashMisses
                 << " autoCachePayloadMisses=" << state.autoCachePayloadMisses
                 << " lastAutoCacheMiss=" << state.lastAutoCacheMissReason
                 << " lastAutoCacheDetail=" << state.lastAutoCacheMissDetail
                 << " autoQueueFrameCount=" << state.autoQueueFrameCount
                 << " autoQueueDeferred=" << state.autoQueueDeferred
                 << " observerFrameNewChunks=" << state.observerFrameNewChunks
                 << " observerThrottled=" << state.observerThrottled
                 << " observerRefreshSkips=" << state.observerRefreshSkips
                 << " liveTextureBytes=" << state.liveTextureBytes
                 << " liveMaterialBytes=" << state.liveMaterialBytes
                 << " pendingPayloadBytes=" << state.pendingPayloadBytes
                 << " memoryBudgetBytes=" << MemoryBudgetBytes(state)
                 << " focus=(" << state.focusWorldChunkX << "," << state.focusWorldChunkY << ")"
                 << " observations=" << state.observations
                 << " queueDrops=" << state.queueDrops
                 << " staleDrops=" << state.staleDrops
                 << " memoryEvictions=" << state.memoryEvictions
                 << " budgetFallbacks=" << state.budgetFallbacks
                 << " fallbackCount=" << state.fallbackCount
                 << " lastFallback=" << state.lastFallbackReason
                 << " lifecycleCount=" << state.lifecycleCount
                 << " lastLifecycle=" << state.lastLifecycleStage
                 << " compositeReadOverrides=" << state.compositeReadOverrides
                 << " gpuOverlayAttempts=" << state.gpuOverlayAttempts
                 << " gpuOverlayApplied=" << state.gpuOverlayApplied
                 << " gpuOverlayFallbackApplied=" << state.gpuOverlayFallbackApplied
                 << " lastOwnerHadResidentPayload=" << (state.lastOwnerHadResidentPayload ? 1 : 0)
                 << " helperSuccesses=" << state.helperSuccesses
                 << " helperFailures=" << state.helperFailures
                 << " circuitOpen=" << (state.circuitOpen ? 1 : 0)
                 << " runtimeDisabled=" << (state.runtimeDisabled ? 1 : 0)
                 << " fallbackFrames=" << state.fallbackFrames;
    }

    void PushStatusField(lua_State* L, char const* key, double value)
    {
        ClientLua::PushNumber(L, value);
        ClientLua::SetField(L, -2, key);
    }

    void PushStatusField(lua_State* L, char const* key, bool value)
    {
        ClientLua::PushBoolean(L, value ? 1 : 0);
        ClientLua::SetField(L, -2, key);
    }

    void PushStatusField(lua_State* L, char const* key, std::string const& value)
    {
        ClientLua::PushString(L, value.c_str());
        ClientLua::SetField(L, -2, key);
    }

    bool BuildGpuCompositeStage(RuntimeState& state, uintptr_t textureSetPtr, int width, int height, int outStride,
                                void* outBits, uint8_t layerCount)
    {
        ++state.gpuCompositeAttempts;
        uint32_t const startMs = GetTickCount();
        IDirect3DDevice9* const device = D3D::GetDevice();
        if (!device)
        {
            ++state.gpuCompositeNoDevice;
            ++state.gpuCompositeFailures;
            return false;
        }

        if (width <= 0 || height <= 0)
        {
            ++state.gpuCompositeFailures;
            return false;
        }

        GpuCompositeRt& slot = state.gpuCompositeRts[textureSetPtr];
        if (!slot.texture || !slot.sysmemSurface || slot.width != static_cast<uint32_t>(width) ||
            slot.height != static_cast<uint32_t>(height))
        {
            if (slot.texture)
            {
                slot.texture->Release();
                slot.texture = nullptr;
            }
            if (slot.uploadTexture)
            {
                slot.uploadTexture->Release();
                slot.uploadTexture = nullptr;
            }
            if (slot.sysmemSurface)
            {
                slot.sysmemSurface->Release();
                slot.sysmemSurface = nullptr;
            }

            HRESULT hr = device->CreateTexture(static_cast<UINT>(width), static_cast<UINT>(height), 1,
                                               D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &slot.texture,
                                               nullptr);
            if (FAILED(hr) || !slot.texture)
            {
                ++state.gpuCompositeFailures;
                return false;
            }

            hr = device->CreateTexture(static_cast<UINT>(width), static_cast<UINT>(height), 1, D3DUSAGE_DYNAMIC,
                                       D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &slot.uploadTexture, nullptr);
            if (FAILED(hr) || !slot.uploadTexture)
            {
                if (slot.texture)
                {
                    slot.texture->Release();
                    slot.texture = nullptr;
                }
                ++state.gpuCompositeFailures;
                return false;
            }

            slot.width = static_cast<uint32_t>(width);
            slot.height = static_cast<uint32_t>(height);

            hr = device->CreateOffscreenPlainSurface(static_cast<UINT>(width), static_cast<UINT>(height),
                                                     D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &slot.sysmemSurface, nullptr);
            if (FAILED(hr) || !slot.sysmemSurface)
            {
                if (slot.texture)
                {
                    slot.texture->Release();
                    slot.texture = nullptr;
                }
                if (slot.uploadTexture)
                {
                    slot.uploadTexture->Release();
                    slot.uploadTexture = nullptr;
                }
                ++state.gpuCompositeFailures;
                return false;
            }
        }

        if (!outBits || outStride <= 0)
        {
            ++state.gpuCompositeFailures;
            return false;
        }

        D3DLOCKED_RECT uploadLocked = {};
        HRESULT hr = slot.uploadTexture->LockRect(0, &uploadLocked, nullptr, D3DLOCK_DISCARD);
        if (FAILED(hr) || !uploadLocked.pBits || uploadLocked.Pitch <= 0)
        {
            ++state.gpuCompositeFailures;
            return false;
        }

        uint8_t const* srcNative = reinterpret_cast<uint8_t const*>(outBits);
        uint8_t* dstUpload = reinterpret_cast<uint8_t*>(uploadLocked.pBits);
        int const uploadRowBytes = (std::min)(outStride, static_cast<int>(slot.width * 4u));
        for (int y = 0; y < height; ++y)
            std::memcpy(dstUpload + y * uploadLocked.Pitch, srcNative + y * outStride, static_cast<size_t>(uploadRowBytes));
        slot.uploadTexture->UnlockRect(0);

        IDirect3DSurface9* uploadSurface = nullptr;
        IDirect3DSurface9* rtSurface = nullptr;
        hr = slot.uploadTexture->GetSurfaceLevel(0, &uploadSurface);
        if (FAILED(hr) || !uploadSurface)
        {
            ++state.gpuCompositeFailures;
            return false;
        }
        hr = slot.texture->GetSurfaceLevel(0, &rtSurface);
        if (FAILED(hr) || !rtSurface)
        {
            uploadSurface->Release();
            ++state.gpuCompositeFailures;
            return false;
        }

        hr = device->StretchRect(uploadSurface, nullptr, rtSurface, nullptr, D3DTEXF_NONE);
        uploadSurface->Release();
        if (FAILED(hr))
        {
            rtSurface->Release();
            ++state.gpuCompositeFailures;
            return false;
        }

        hr = device->GetRenderTargetData(rtSurface, slot.sysmemSurface);
        rtSurface->Release();
        if (FAILED(hr))
        {
            ++state.gpuCompositeFailures;
            return false;
        }

        D3DLOCKED_RECT locked = {};
        hr = slot.sysmemSurface->LockRect(&locked, nullptr, D3DLOCK_READONLY);
        if (FAILED(hr) || !locked.pBits || locked.Pitch <= 0)
        {
            ++state.gpuCompositeFailures;
            return false;
        }

        uint8_t* dst = reinterpret_cast<uint8_t*>(outBits);
        uint8_t const* src = reinterpret_cast<uint8_t const*>(locked.pBits);
        int const rowBytes = (std::min)(outStride, static_cast<int>(slot.width * 4u));
        for (int y = 0; y < height; ++y)
            std::memcpy(dst + y * outStride, src + y * locked.Pitch, static_cast<size_t>(rowBytes));
        slot.sysmemSurface->UnlockRect();

        // Blend-input seed from native slot identities; keeps deterministic marker for later shader parity work.
        uint32_t seed = layerCount;
        for (uint32_t handle : state.lastGpuNativeDiffuseHandles)
            seed ^= (handle * 2654435761u);
        seed ^= state.lastGpuNativeCompositeHandle;
        state.gpuCompositeSeed = seed;

        ++state.gpuCompositeSuccesses;
        state.gpuCompositeBuildMsTotal += (GetTickCount() - startMs);
        return true;
    }

    bool BlendResidentPayloadOverComposite(RuntimeState& state, PayloadRecord* payload, int width, int height, int outStride,
                                           void* outBits)
    {
        ++state.gpuOverlayAttempts;
        state.lastOwnerHadResidentPayload = payload != nullptr;

        if (!payload || !payload->textureHandle || !outBits || outStride <= 0)
        {
            ++state.gpuOverlaySkipNoPayload;
            return false;
        }

        auto entryIt = state.directTextureCache.find(payload->textureHandle);
        if (entryIt == state.directTextureCache.end() || !entryIt->second)
        {
            ++state.gpuOverlaySkipNoCache;
            return false;
        }

        DirectTextureCacheEntry const& source = *entryIt->second;
        if (source.pixels.empty())
            return false;

        uint32_t const srcW = source.width;
        uint32_t const srcH = source.height;
        if (srcW == 0 || srcH == 0 || static_cast<size_t>(srcW) * static_cast<size_t>(srcH) > source.pixels.size())
            return false;

        if (!payload->materialMapLoaded || payload->materialMap.empty() ||
            payload->materialWidth <= 0 || payload->materialHeight <= 0)
        {
            ++state.gpuOverlaySkipNoCache;
            return false;
        }

        int debugAlphaPct = state.config.debugCheckerAlphaPct;
        if (debugAlphaPct < 1)
            debugAlphaPct = 100;
        if (debugAlphaPct > 100)
            debugAlphaPct = 100;
        uint32_t const maxSourcePct = state.config.debugCheckerEnabled ? static_cast<uint32_t>(debugAlphaPct) : 100u;
        int32_t const effectiveLayerCap =
            (std::min)(static_cast<int32_t>(kMaxExtendedTerrainLayers), (std::max)(0, state.config.maxLayers));
        uint32_t const mapW = static_cast<uint32_t>(payload->materialWidth);
        uint32_t const mapH = static_cast<uint32_t>(payload->materialHeight);
        uint8_t* dstBits = reinterpret_cast<uint8_t*>(outBits);
        for (int y = 0; y < height; ++y)
        {
            uint32_t const sy = (static_cast<uint32_t>(y) * srcH) / static_cast<uint32_t>(height);
            uint32_t const my = (static_cast<uint32_t>(y) * mapH) / static_cast<uint32_t>(height);
            uint8_t* dstRow = dstBits + static_cast<size_t>(y) * static_cast<size_t>(outStride);
            for (int x = 0; x < width; ++x)
            {
                uint32_t const sx = (static_cast<uint32_t>(x) * srcW) / static_cast<uint32_t>(width);
                CImVector const& spx = source.pixels[static_cast<size_t>(sy) * srcW + sx];

                if (spx.a <= 8)
                    continue;

                if (state.config.debugCheckerStrict && spx.a > 8)
                {
                    uint8_t* d = dstRow + static_cast<size_t>(x) * 4u;
                    uint8_t const db = d[0];
                    uint8_t const dg = d[1];
                    uint8_t const dr = d[2];
                    constexpr uint32_t kSrc = 70;
                    constexpr uint32_t kDst = 30;
                    d[2] = static_cast<uint8_t>((static_cast<uint32_t>(dr) * kDst + 255u * kSrc) / 100u);
                    d[1] = static_cast<uint8_t>((static_cast<uint32_t>(dg) * kDst +   0u * kSrc) / 100u);
                    d[0] = static_cast<uint8_t>((static_cast<uint32_t>(db) * kDst + 255u * kSrc) / 100u);
                    continue;
                }

                uint32_t const mx = (static_cast<uint32_t>(x) * mapW) / static_cast<uint32_t>(width);
                size_t const mIdx = static_cast<size_t>(my) * mapW + mx;
                if (mIdx >= payload->materialMap.size())
                {
                    ++state.gpuOverlaySkipNoCache;
                    return false;
                }
                uint8_t const dominantLayer = payload->materialMap[mIdx];
                if (dominantLayer < kNativeTerrainLayerCount ||
                    static_cast<int32_t>(dominantLayer) >= effectiveLayerCap)
                {
                    continue;
                }

                uint8_t* d = dstRow + static_cast<size_t>(x) * 4u;
                uint8_t const db = d[0];
                uint8_t const dg = d[1];
                uint8_t const dr = d[2];
                uint32_t const wSrc = (maxSourcePct * static_cast<uint32_t>(spx.a)) / 255u;
                uint32_t const wDst = 100u - wSrc;
                d[2] = static_cast<uint8_t>((static_cast<uint32_t>(dr) * wDst + static_cast<uint32_t>(spx.r) * wSrc) / 100u);
                d[1] = static_cast<uint8_t>((static_cast<uint32_t>(dg) * wDst + static_cast<uint32_t>(spx.g) * wSrc) / 100u);
                d[0] = static_cast<uint8_t>((static_cast<uint32_t>(db) * wDst + static_cast<uint32_t>(spx.b) * wSrc) / 100u);
            }
        }
        ++state.gpuOverlayApplied;
        return true;
    }
} // namespace

CLIENT_DETOUR_THISCALL(CMapTile__LoadChunk_ExtTex, 0x7D6B30, int, (int localChunkX, int localChunkY))
{
    int const result = CMapTile__LoadChunk_ExtTex(self, localChunkX, localChunkY);
    ExtendedTerrainTextures::ObserveChunkTileLoad(self, localChunkX, localChunkY);
    return result;
}

CLIENT_DETOUR_THISCALL(CMapChunk__InitFromMcnk_ExtTex, 0x7C64B0, int, (int mcnkPayload, int flags))
{
    int const result = CMapChunk__InitFromMcnk_ExtTex(self, mcnkPayload, flags);
    ExtendedTerrainTextures::ObserveChunkIdentity(self);
    return result;
}

CLIENT_DETOUR_THISCALL_NOARGS(CMapChunk__PrepareTerrainRenderState_ExtTex, 0x7D3F70, void)
{
    ExtendedTerrainTextures::ObserveChunkForScheduling(self);
    CMapChunk__PrepareTerrainRenderState_ExtTex(self);
}

CLIENT_DETOUR_THISCALL_NOARGS(CMapChunkTextureSet__UpdateTextures_ExtTex, 0x7BA050, void)
{
    uintptr_t const prevCtx = gActiveTextureSetContext;
    bool const prevValid = gActiveTextureSetIdentityValid;
    int32_t const prevAdtX = gActiveTextureSetAdtX;
    int32_t const prevAdtY = gActiveTextureSetAdtY;
    int32_t const prevChunkIndex = gActiveTextureSetChunkIndex;
    gActiveTextureSetContext = reinterpret_cast<uintptr_t>(self);
    gActiveTextureSetIdentityValid = false;
    gActiveTextureSetAdtX = -1;
    gActiveTextureSetAdtY = -1;
    gActiveTextureSetChunkIndex = -1;
    {
        uintptr_t const nativeChunkPtr = ReadU32At(self, 0x10);
        int32_t gx = 0, gy = 0;
        if (TryReadChunkCoords(nativeChunkPtr, gx, gy))
        {
            int32_t adtX = -1, adtY = -1;
            ClientChunkToAdtTile(gx, gy, adtX, adtY);
            gActiveTextureSetIdentityValid = adtX >= 0 && adtY >= 0;
            gActiveTextureSetAdtX = adtX;
            gActiveTextureSetAdtY = adtY;
            gActiveTextureSetChunkIndex = PositiveMod16(gy) * 16 + PositiveMod16(gx);
        }
    }
    RuntimeState& state = State();
    if (state.configured && state.config.nativeCompositeGpu)
    {
        static thread_local uint32_t sGpuFrameId = 0;
        static thread_local uint32_t sGpuUpdateCallsThisFrame = 0;
        uint32_t const frameId = UploadFrameId(GetTickCount());
        if (frameId != sGpuFrameId)
        {
            sGpuFrameId = frameId;
            sGpuUpdateCallsThisFrame = 0;
            ExtendedTerrainTextures::PumpUploadBudget();
        }
        ++sGpuUpdateCallsThisFrame;

        // Load shedding: when farclip/camera movement causes callback storms, do native-only for remainder of frame.
        constexpr uint32_t kGpuExtProcessBudgetPerFrame = 40;
        bool const extOverBudget = sGpuUpdateCallsThisFrame > kGpuExtProcessBudgetPerFrame;
        if (extOverBudget)
        {
            CMapChunkTextureSet__UpdateTextures_ExtTex(self);
            gActiveTextureSetContext = prevCtx;
            gActiveTextureSetIdentityValid = prevValid;
            gActiveTextureSetAdtX = prevAdtX;
            gActiveTextureSetAdtY = prevAdtY;
            gActiveTextureSetChunkIndex = prevChunkIndex;
            return;
        }

        ExtendedTerrainTextures::PrepareTextureSetForNativeUpdate(self);
        {
            std::unique_lock<std::mutex> lock(state.mutex, std::try_to_lock);
            if (lock.owns_lock())
            {
                if (ChunkRequest* request = FindRequest(state, reinterpret_cast<uintptr_t>(self)))
                {
                    state.lastOwnerAdtX = request->adtTileX;
                    state.lastOwnerAdtY = request->adtTileY;
                    state.lastOwnerChunkIndex = request->chunkIndex;
                    state.lastOwnerWorldChunkX = request->worldChunkX;
                    state.lastOwnerWorldChunkY = request->worldChunkY;
                    state.lastOwnerHadResidentPayload =
                        FindPayloadByTileChunkIndex(state, request->adtTileX, request->adtTileY, request->chunkIndex) != nullptr;
                }
            }
        }
        CMapChunkTextureSet__UpdateTextures_ExtTex(self);
        // Preserve native base/material semantics; callback mask-aware compose handles ext contribution.
        gActiveTextureSetContext = prevCtx;
        gActiveTextureSetIdentityValid = prevValid;
        gActiveTextureSetAdtX = prevAdtX;
        gActiveTextureSetAdtY = prevAdtY;
        gActiveTextureSetChunkIndex = prevChunkIndex;
        return;
    }

    ExtendedTerrainTextures::PrepareTextureSetForNativeUpdate(self);
    CMapChunkTextureSet__UpdateTextures_ExtTex(self);
    ExtendedTerrainTextures::PumpUploadBudget();
    ExtendedTerrainTextures::AttachReadyTextureSet(self);
    gActiveTextureSetContext = prevCtx;
    gActiveTextureSetIdentityValid = prevValid;
    gActiveTextureSetAdtX = prevAdtX;
    gActiveTextureSetAdtY = prevAdtY;
    gActiveTextureSetChunkIndex = prevChunkIndex;
}

CLIENT_DETOUR_THISCALL_NOARGS(CMapChunkTextureSet__UpdateCompositeTexture_ExtTex, 0x7B9F90, void)
{
    uintptr_t const prevCtx = gActiveTextureSetContext;
    bool const prevValid = gActiveTextureSetIdentityValid;
    int32_t const prevAdtX = gActiveTextureSetAdtX;
    int32_t const prevAdtY = gActiveTextureSetAdtY;
    int32_t const prevChunkIndex = gActiveTextureSetChunkIndex;
    gActiveTextureSetContext = reinterpret_cast<uintptr_t>(self);
    gActiveTextureSetIdentityValid = false;
    gActiveTextureSetAdtX = -1;
    gActiveTextureSetAdtY = -1;
    gActiveTextureSetChunkIndex = -1;
    {
        uintptr_t const nativeChunkPtr = ReadU32At(self, 0x10);
        int32_t gx = 0, gy = 0;
        if (TryReadChunkCoords(nativeChunkPtr, gx, gy))
        {
            int32_t adtX = -1, adtY = -1;
            ClientChunkToAdtTile(gx, gy, adtX, adtY);
            gActiveTextureSetIdentityValid = adtX >= 0 && adtY >= 0;
            gActiveTextureSetAdtX = adtX;
            gActiveTextureSetAdtY = adtY;
            gActiveTextureSetChunkIndex = PositiveMod16(gy) * 16 + PositiveMod16(gx);
        }
    }
    CMapChunkTextureSet__UpdateCompositeTexture_ExtTex(self);
    RuntimeState& state = State();
    gActiveTextureSetContext = prevCtx;
    gActiveTextureSetIdentityValid = prevValid;
    gActiveTextureSetAdtX = prevAdtX;
    gActiveTextureSetAdtY = prevAdtY;
    gActiveTextureSetChunkIndex = prevChunkIndex;
    if (state.configured && state.config.nativeCompositeGpu)
        return;
    ExtendedTerrainTextures::AttachReadyTextureSet(self);
}

CLIENT_DETOUR_THISCALL_NOARGS(CMapChunkTextureSet__FreeTexturesAndResetLayers_ExtTex, 0x7B7350, int)
{
    ExtendedTerrainTextures::ReleaseChunkResources(self, "native-texture-cleanup");
    return CMapChunkTextureSet__FreeTexturesAndResetLayers_ExtTex(self);
}

CLIENT_DETOUR(CMapChunkTextureCallback_CreateCompositeTexture_ReadControl_ExtTex, 0x7B9C60, __cdecl, void*,
              (int event, int width, int height, int pitch, int format, void* textureSet, int* outStride,
               void** outBits))
{
    void* const result = CMapChunkTextureCallback_CreateCompositeTexture_ReadControl_ExtTex(
        event, width, height, pitch, format, textureSet, outStride, outBits);

    if (event != 1 || !textureSet || !outBits || !*outBits || !outStride || width <= 0 || height <= 0)
        return result;

    RuntimeState& state = State();
    std::unique_lock<std::mutex> lock(state.mutex, std::try_to_lock);
    if (lock.owns_lock() && state.config.nativeCompositeGpu)
    {
        uintptr_t resolvedTextureSetPtr = ResolveTextureSetOwnerPtr(state, reinterpret_cast<uintptr_t>(textureSet));
        if (!FindRequest(state, resolvedTextureSetPtr) && gActiveTextureSetContext && FindRequest(state, gActiveTextureSetContext))
            resolvedTextureSetPtr = gActiveTextureSetContext;

        if (TelemetryEnabled(state))
        {
            ++state.gpuCompositeCallbacks;
            state.lastGpuCompositeEvent = event;
            state.lastGpuCompositeTextureSet = resolvedTextureSetPtr;
            state.lastGpuCompositeLayerCount = textureSet ? ReadU8At(textureSet, 0x09) : 0;
            state.lastGpuNativeDiffuseHandles = { 0, 0, 0, 0 };
            state.lastGpuSlotHandleHash = 0;
            state.lastGpuSlotCountCaptured = 0;
            if (textureSet)
            {
                for (size_t i = 0; i < state.lastGpuNativeDiffuseHandles.size(); ++i)
                    state.lastGpuNativeDiffuseHandles[i] = ReadU32At(textureSet, 0x38 + i * 0x14);
                state.lastGpuNativeCompositeHandle = ReadU32At(textureSet, 0x84);

                uint8_t const captureCount = (std::min)(static_cast<uint8_t>(16), state.lastGpuCompositeLayerCount);
                uint32_t rolling = 2166136261u;
                for (uint8_t i = 0; i < captureCount; ++i)
                {
                    uint32_t const handle = ReadU32At(textureSet, 0x38 + static_cast<size_t>(i) * 0x14);
                    rolling ^= handle;
                    rolling *= 16777619u;
                }
                state.lastGpuSlotCountCaptured = captureCount;
                state.lastGpuSlotHandleHash = rolling;

                if (captureCount > state.maxGpuSlotCountCaptured)
                {
                    state.maxGpuSlotCountCaptured = captureCount;
                    uintptr_t const nativeChunkPtr = ReadU32At(textureSet, 0x10);
                    if (nativeChunkPtr && IsReadablePointer(reinterpret_cast<void const*>(nativeChunkPtr), 0x3C))
                    {
                        int32_t const worldChunkX = ReadInt32At(reinterpret_cast<void*>(nativeChunkPtr), 0x34);
                        int32_t const worldChunkY = ReadInt32At(reinterpret_cast<void*>(nativeChunkPtr), 0x38);
                        state.maxGpuSlotWorldChunkX = worldChunkX;
                        state.maxGpuSlotWorldChunkY = worldChunkY;
                        int32_t adtX = -1;
                        int32_t adtY = -1;
                        ClientChunkToAdtTile(worldChunkX, worldChunkY, adtX, adtY);
                        state.maxGpuSlotAdtX = adtX;
                        state.maxGpuSlotAdtY = adtY;
                    }
                }
            }
        }

        if (!state.configured || !state.config.enabled || !state.config.nativeUpload ||
            state.runtimeDisabled || state.circuitOpen)
        {
            if (TelemetryEnabled(state))
                ++state.compositeReadSkipGate;
            return result;
        }

        PayloadRecord* payload = FindResidentPayloadForTextureSetAggressive(state, resolvedTextureSetPtr);
        if (!payload)
            payload = FindResidentPayloadForActiveIdentity(state);

        if (!payload)
        {
            ++state.gpuOverlaySkipNoPayload;
            return result;
        }

        // Native composite bits are blend/control data for the four native terrain slots, not final diffuse color.
        // Mutating this buffer with an extended diffuse payload only stripes one native layer. Leave it untouched
        // until extended layers are rendered by a real overlay/group draw path.
        ++state.compositeReadSkipGate;
        RecordLifecycleUnlocked(state, "gpu-composite-pass-through", "control-texture-not-diffuse");
        return result;
    }

    if (lock.owns_lock() && TelemetryEnabled(state))
        ++state.compositeReadCallbackCalls;
    if (!lock.owns_lock() || !state.configured || !state.config.enabled || !state.config.nativeUpload ||
        state.runtimeDisabled || state.circuitOpen)
    {
        if (lock.owns_lock() && TelemetryEnabled(state))
            ++state.compositeReadSkipGate;
        return result;
    }

    PayloadRecord* payload = FindResidentPayloadForTextureSet(state, reinterpret_cast<uintptr_t>(textureSet));
    if (!payload)
    {
        // Fallback seam control: if this callback belongs to our configured stack tile,
        // still force group0 blend bits so native rebuild does not drift after culling.
        int32_t adtX = -1;
        int32_t adtY = -1;
        if (ChunkRequest* request = FindRequest(state, reinterpret_cast<uintptr_t>(textureSet)))
        {
            if (request->adtTileValid)
            {
                adtX = request->adtTileX;
                adtY = request->adtTileY;
            }
        }

        if (adtX < 0 || adtY < 0)
        {
            uintptr_t nativeChunkPtr = 0;
            if (ResolveNativeChunkFromTextureSet(state, textureSet, nativeChunkPtr))
            {
                int32_t const globalChunkX = ReadInt32At(reinterpret_cast<void*>(nativeChunkPtr), 0x34);
                int32_t const globalChunkY = ReadInt32At(reinterpret_cast<void*>(nativeChunkPtr), 0x38);
                ClientChunkToAdtTile(globalChunkX, globalChunkY, adtX, adtY);
            }
        }

        bool const inStackTile = state.cachedStackTileValid &&
                                 adtX == state.cachedStackTileX &&
                                 adtY == state.cachedStackTileY;
        if (!inStackTile)
        {
            ++state.compositeReadSkipNoPayload;
            if (state.config.trace)
                RecordLifecycleUnlocked(state, "composite-read-skip", "no-resident-payload");
            return result;
        }
    }

    // Do not flatten native composite bits anymore. Keep native blend/shadow output intact
    // and use baked diffuse only through controlled slot-0 attach path.
    ++state.compositeReadOverrides;
    RecordLifecycleUnlocked(state, "composite-read-override", "native-pass-through");
    return result;
}

CLIENT_DETOUR(CMapTerrainDrawSetup_ExtTex, 0x7D3E10, __cdecl, void, (int shaderMode, int alphaMode))
{
    RuntimeState& state = State();
    if (!state.configured || !state.config.nativeCompositeGpu)
    {
        // Frame-driven progress: keep upload/queue advancing even without camera movement.
        ExtendedTerrainTextures::PumpUploadBudget();
        ExtendedTerrainTextures::AttachReadyVisibleTextureSets();
    }
    else
    {
        // GPU composite mode still needs payload uploads for extended-layer source textures.
        ExtendedTerrainTextures::PumpUploadBudget();
    }
    CMapTerrainDrawSetup_ExtTex(shaderMode, alphaMode);
    if (state.configured && state.config.drawReplacement)
        ExtendedTerrainTextures::InstallTerrainDrawOverride();
}

CLIENT_DETOUR(CMapTerrainDrawVariant_7D0760_ExtTex, 0x7D0760, __cdecl, void, (int chunk))
{
    InvokeTerrainDrawWithOverride(chunk, CMapTerrainDrawVariant_7D0760_ExtTex, 0x7D0760);
}

CLIENT_DETOUR(CMapTerrainDrawVariant_7D0D70_ExtTex, 0x7D0D70, __cdecl, void, (int chunk))
{
    InvokeTerrainDrawWithOverride(chunk, CMapTerrainDrawVariant_7D0D70_ExtTex, 0x7D0D70);
}

CLIENT_DETOUR(CMapTerrainDrawVariant_7D13F0_ExtTex, 0x7D13F0, __cdecl, void, (int chunk))
{
    InvokeTerrainDrawWithOverride(chunk, CMapTerrainDrawVariant_7D13F0_ExtTex, 0x7D13F0);
}

CLIENT_DETOUR(CMapTerrainDrawVariant_7D1AD0_ExtTex, 0x7D1AD0, __cdecl, void, (int chunk))
{
    InvokeTerrainDrawWithOverride(chunk, CMapTerrainDrawVariant_7D1AD0_ExtTex, 0x7D1AD0);
}

CLIENT_DETOUR(CMapTerrainDrawVariant_7D20A0_ExtTex, 0x7D20A0, __cdecl, void, (int chunk))
{
    InvokeTerrainDrawWithOverride(chunk, CMapTerrainDrawVariant_7D20A0_ExtTex, 0x7D20A0);
}

CLIENT_DETOUR(CMapTerrainDrawVariant_7D2520_ExtTex, 0x7D2520, __cdecl, void, (int chunk))
{
    InvokeTerrainDrawWithOverride(chunk, CMapTerrainDrawVariant_7D2520_ExtTex, 0x7D2520);
}

CLIENT_DETOUR_THISCALL_NOARGS(CMapTerrainDrawShaderHelper_7D28B0_ExtTex, 0x7D28B0, void)
{
    InvokeTerrainDrawThiscallWithOverride(self, CMapTerrainDrawShaderHelper_7D28B0_ExtTex, 0x7D28B0);
}

CLIENT_DETOUR_THISCALL_NOARGS(CMapTerrainDrawShaderHelper_7D2D70_ExtTex, 0x7D2D70, void)
{
    InvokeTerrainDrawThiscallWithOverride(self, CMapTerrainDrawShaderHelper_7D2D70_ExtTex, 0x7D2D70);
}

void ExtendedTerrainTextures::Apply()
{
    ExtendedTerrainTextureSettings config = DefaultExtendedTerrainTextureSettings();
    config.quality = ToLowerCopy(config.quality);
    if (config.quality != "quick" && config.quality != "final")
        config.quality = "quick";

    LogConfig(config);

    {
        std::lock_guard<std::mutex> lock(State().mutex);
        State().config     = config;
        State().configured = true;
        State().runtimeDisabled = false;
        State().disabledReason.clear();
        PrewarmSidecarPayloadMetadata(State());
    }

    if (!config.enabled)
        return;

    LOG_INFO << "ExtendedTerrainTextures: render-prep observer active at CMapChunk__PrepareTerrainRenderState";
    (void)CMapTile__LoadChunk_ExtTex__Result;
    (void)CMapChunk__InitFromMcnk_ExtTex__Result;
    (void)CMapChunk__PrepareTerrainRenderState_ExtTex__Result;
    (void)CMapChunkTextureSet__UpdateTextures_ExtTex__Result;
    (void)CMapChunkTextureSet__UpdateCompositeTexture_ExtTex__Result;
    (void)CMapChunkTextureSet__FreeTexturesAndResetLayers_ExtTex__Result;
    (void)CMapChunkTextureCallback_CreateCompositeTexture_ReadControl_ExtTex__Result;
    if (config.drawReplacement)
    {
        LOG_INFO << "ExtendedTerrainTextures: extended overlay draw hooks enabled";
        (void)CMapTerrainDrawSetup_ExtTex__Result;
        (void)CMapTerrainDrawVariant_7D0760_ExtTex__Result;
        (void)CMapTerrainDrawVariant_7D0D70_ExtTex__Result;
        (void)CMapTerrainDrawVariant_7D13F0_ExtTex__Result;
        (void)CMapTerrainDrawVariant_7D1AD0_ExtTex__Result;
        (void)CMapTerrainDrawVariant_7D20A0_ExtTex__Result;
        (void)CMapTerrainDrawVariant_7D2520_ExtTex__Result;
        (void)CMapTerrainDrawShaderHelper_7D28B0_ExtTex__Result;
        (void)CMapTerrainDrawShaderHelper_7D2D70_ExtTex__Result;
    }
    else
    {
        LOG_INFO << "ExtendedTerrainTextures: draw replacement hooks disabled";
    }
    D3D::RegisterOnRelease([]() { ExtendedTerrainTextures::OnDeviceLost(); });
    D3D::RegisterOnRestore([]() { ExtendedTerrainTextures::OnDeviceRestored(); });
    D3D::RegisterDrawIndexedPrimitiveCallback([](IDirect3DDevice9* device, D3DPRIMITIVETYPE, INT, UINT, UINT, UINT, UINT) {
        if (gTerrainOverlayPassActive && device)
        {
            device->SetPixelShader(nullptr);
            device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
            device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
            device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
            device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
        }
    });
    D3D::RegisterDrawPrimitiveCallback([](IDirect3DDevice9* device, D3DPRIMITIVETYPE, UINT, UINT) {
        if (gTerrainOverlayPassActive && device)
        {
            device->SetPixelShader(nullptr);
            device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
            device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
            device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
            device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
        }
    });
}

void ExtendedTerrainTextures::InstallTerrainDrawOverride()
{
    RuntimeState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    ++state.drawSetupCalls;
    ApplyTerrainDrawOverrideUnlocked(state);
}

void ExtendedTerrainTextures::ObserveChunkTileLoad(void* tile, int32_t localChunkX, int32_t localChunkY)
{
    RuntimeState& state = State();
    if (!state.configured || !tile || !IsLocalChunkCoord(localChunkX) || !IsLocalChunkCoord(localChunkY))
        return;

    std::unique_lock<std::mutex> lock(state.mutex, std::try_to_lock);
    if (!lock.owns_lock())
        return;

    int32_t* tileWords = reinterpret_cast<int32_t*>(tile);
    uintptr_t const chunkPtr = static_cast<uintptr_t>(tileWords[47 + localChunkX + 16 * localChunkY]);
    if (!chunkPtr)
        return;

    uint32_t const nowMs = GetTickCount();
    ChunkRequest* request = FindRequest(state, chunkPtr);
    if (!request)
    {
        ChunkRequest next;
        next.chunkPtr = chunkPtr;
        next.firstSeenMs = nowMs;
        next.lastSeenMs = nowMs;
        next.generation = state.generation;
        state.requests.push_back(next);
        state.requestIndex[chunkPtr] = state.requests.size() - 1;
        request = &state.requests.back();
    }

    int32_t const globalChunkX = localChunkX + tileWords[20];
    int32_t const globalChunkY = localChunkY + tileWords[21];
    int32_t adtX = -1;
    int32_t adtY = -1;
    ClientChunkToAdtTile(globalChunkX, globalChunkY, adtX, adtY);

    bool const coordsChanged = request->coordsValid &&
                               (request->worldChunkX != globalChunkX || request->worldChunkY != globalChunkY);
    request->localChunkX = localChunkX;
    request->localChunkY = localChunkY;
    request->worldChunkX = globalChunkX;
    request->worldChunkY = globalChunkY;
    request->adtTileX = adtX;
    request->adtTileY = adtY;
    request->adtTileValid = adtX >= 0 && adtY >= 0;
    request->coordsValid = IsValidGlobalChunkPair(globalChunkX, globalChunkY);
    request->tileLoadIdentity = request->coordsValid && request->adtTileValid;
    request->nativeLayerCount = ReadU8At(reinterpret_cast<void*>(chunkPtr), 0x09);
    request->lastSeenMs = nowMs;
    request->generation = state.generation;
    RefreshOwnerIdentity(state, *request, reinterpret_cast<void*>(chunkPtr), coordsChanged);

    if (coordsChanged)
    {
        request->cacheLookupAttempted = false;
        request->cacheLookupHit = false;
        request->sourceStackHash.clear();
        if (PayloadRecord* payload = FindPayload(state, chunkPtr))
            ReleasePayloadHandle(state, *payload, "tile-load-identity-changed");
        state.payloads.erase(std::remove_if(state.payloads.begin(), state.payloads.end(),
                                            [chunkPtr](PayloadRecord const& item) {
                                                return item.chunkPtr == chunkPtr;
                                            }),
                             state.payloads.end());
        RebuildPayloadIndex(state);
        RecalculatePayloadCounters(state);
    }

    if (state.config.autoQueueCache && !request->cacheLookupAttempted)
        EnqueueRequest(state, chunkPtr);

    state.focusWorldChunkX = request->worldChunkX;
    state.focusWorldChunkY = request->worldChunkY;
    state.lastObservedLocalChunkX = request->localChunkX;
    state.lastObservedLocalChunkY = request->localChunkY;
    state.lastObservedWorldChunkX = request->worldChunkX;
    state.lastObservedWorldChunkY = request->worldChunkY;
    state.hasFocusChunk = request->coordsValid;
}

void ExtendedTerrainTextures::ObserveChunkIdentity(void* chunk)
{
    RuntimeState& state = State();
    if (!state.configured || !chunk)
        return;

    std::unique_lock<std::mutex> lock(state.mutex, std::try_to_lock);
    if (!lock.owns_lock())
        return;

    uintptr_t const chunkPtr = reinterpret_cast<uintptr_t>(chunk);
    uint32_t const nowMs = GetTickCount();
    ChunkRequest* request = FindRequest(state, chunkPtr);
    if (!request)
    {
        ChunkRequest next;
        next.chunkPtr = chunkPtr;
        next.firstSeenMs = nowMs;
        next.lastSeenMs = nowMs;
        next.generation = state.generation;
        next.nativeLayerCount = ReadU8At(chunk, 0x09);
        bool const trustedLoadIdentity = FillChunkRequestCoordsFromLoadFields(next, chunk);
        if (trustedLoadIdentity)
            next.tileLoadIdentity = true;
        else
            FillChunkRequestCoordsFromIdentity(next, chunk);
        if (!next.coordsValid)
            FillChunkRequestCoordsFromRenderOrigin(next, chunk);
        RefreshOwnerIdentity(state, next, chunk, true);
        state.requests.push_back(next);
        state.requestIndex[chunkPtr] = state.requests.size() - 1;
        if (state.config.autoQueueCache)
            EnqueueRequest(state, chunkPtr);
        request = &state.requests.back();
    }
    else
    {
        if (request->tileLoadIdentity)
        {
            request->lastSeenMs = nowMs;
            request->generation = state.generation;
            request->nativeLayerCount = ReadU8At(chunk, 0x09);
            RefreshOwnerIdentity(state, *request, chunk, false);
            state.focusWorldChunkX = request->worldChunkX;
            state.focusWorldChunkY = request->worldChunkY;
            state.lastObservedLocalChunkX = request->localChunkX;
            state.lastObservedLocalChunkY = request->localChunkY;
            state.lastObservedWorldChunkX = request->worldChunkX;
            state.lastObservedWorldChunkY = request->worldChunkY;
            state.hasFocusChunk = request->coordsValid;
            return;
        }

        bool const hadCoords = request->coordsValid;
        int32_t const oldWorldX = request->worldChunkX;
        int32_t const oldWorldY = request->worldChunkY;
        request->lastSeenMs = nowMs;
        request->generation = state.generation;
        request->nativeLayerCount = ReadU8At(chunk, 0x09);
        bool const trustedLoadIdentity = FillChunkRequestCoordsFromLoadFields(*request, chunk);
        if (trustedLoadIdentity)
            request->tileLoadIdentity = true;
        if (!trustedLoadIdentity)
            FillChunkRequestCoordsFromIdentity(*request, chunk);
        if (!request->coordsValid)
            FillChunkRequestCoordsFromRenderOrigin(*request, chunk);

        bool const coordsChanged = hadCoords && request->coordsValid &&
                                   (oldWorldX != request->worldChunkX || oldWorldY != request->worldChunkY);
        bool const preserveTileLoadIdentity = coordsChanged && request->tileLoadIdentity;
        if (preserveTileLoadIdentity)
        {
            request->worldChunkX = oldWorldX;
            request->worldChunkY = oldWorldY;
            ClientChunkToAdtTile(request->worldChunkX, request->worldChunkY, request->adtTileX, request->adtTileY);
            request->adtTileValid = request->adtTileX >= 0 && request->adtTileY >= 0;
            request->coordsValid = true;
            RecordFallbackUnlocked(state, "render-origin-tileload-mismatch", "preserve-tile-load-identity");
        }
        RefreshOwnerIdentity(state, *request, chunk, coordsChanged && !preserveTileLoadIdentity);
        if (coordsChanged && !request->tileLoadIdentity)
        {
            request->cacheLookupAttempted = false;
            request->cacheLookupHit = false;
            request->sourceStackHash.clear();

            if (PayloadRecord* payload = FindPayload(state, chunkPtr))
                ReleasePayloadHandle(state, *payload, "chunk-identity-changed");

            state.payloads.erase(std::remove_if(state.payloads.begin(), state.payloads.end(),
                                                [chunkPtr](PayloadRecord const& item) {
                                                    return item.chunkPtr == chunkPtr;
                                                }),
                                 state.payloads.end());
            RebuildPayloadIndex(state);
            RecalculatePayloadCounters(state);
            if (state.config.autoQueueCache)
                EnqueueRequest(state, chunkPtr);
        }
    }

    state.focusWorldChunkX = request->worldChunkX;
    state.focusWorldChunkY = request->worldChunkY;
    state.lastObservedLocalChunkX = request->localChunkX;
    state.lastObservedLocalChunkY = request->localChunkY;
    state.lastObservedWorldChunkX = request->worldChunkX;
    state.lastObservedWorldChunkY = request->worldChunkY;
    state.hasFocusChunk = request->coordsValid;
}

void ExtendedTerrainTextures::ObserveChunkForScheduling(void* chunk)
{
    RuntimeState& state = State();
    if (!state.configured || !chunk)
        return;

    std::unique_lock<std::mutex> lock(state.mutex, std::try_to_lock);
    if (!lock.owns_lock())
        return;

    uint32_t const nowMs = GetTickCount();
    if (!state.config.enabled || state.runtimeDisabled || state.circuitOpen)
    {
        ++state.fallbackFrames;
        LogTraceSummary(state, nowMs);
        return;
    }

    if (!state.config.autoQueueCache)
    {
        ++state.observations;
        if ((state.observations & 0x3FF) == 0)
            LogTraceSummary(state, nowMs);
        return;
    }

    ++state.generation;
    ++state.observations;
    ResetObserverFrameIfNeeded(state, nowMs);

    uintptr_t const chunkPtr = reinterpret_cast<uintptr_t>(chunk);
    ChunkRequest* request    = FindRequest(state, chunkPtr);
    if (!request)
    {
        uint32_t const maxNewChunks = static_cast<uint32_t>((std::max)(0, state.config.observerNewChunksPerFrame));
        if (maxNewChunks == 0 || state.observerFrameNewChunks >= maxNewChunks)
        {
            ++state.observerThrottled;
            return;
        }

        ++state.observerFrameNewChunks;
        while (state.requests.size() >= static_cast<size_t>(state.config.maxPendingRequests))
            DropOldestRequest(state);

        ChunkRequest next;
        next.chunkPtr          = chunkPtr;
        FillChunkRequestCoordsFromIdentity(next, chunk);
        if (!next.coordsValid)
            FillChunkRequestCoordsFromRenderOrigin(next, chunk);
        RefreshOwnerIdentity(state, next, chunk, true);
        state.focusWorldChunkX = next.worldChunkX;
        state.focusWorldChunkY = next.worldChunkY;
        state.lastObservedLocalChunkX = next.localChunkX;
        state.lastObservedLocalChunkY = next.localChunkY;
        state.lastObservedWorldChunkX = next.worldChunkX;
        state.lastObservedWorldChunkY = next.worldChunkY;
        state.hasFocusChunk    = true;
        next.nativeLayerCount  = ReadU8At(chunk, 0x09);
        next.firstSeenMs       = nowMs;
        next.lastSeenMs        = nowMs;
        next.generation        = state.generation;
        state.requests.push_back(next);
        state.requestIndex[chunkPtr] = state.requests.size() - 1;
        EnqueueRequest(state, chunkPtr);
    }
    else
    {
        uint32_t const refreshMs = static_cast<uint32_t>((std::max)(0, state.config.observerRefreshMs));
        if (refreshMs > 0 && nowMs - request->lastSeenMs < refreshMs)
        {
            ++state.observerRefreshSkips;
            return;
        }

        request->lastSeenMs       = nowMs;
        request->generation       = state.generation;
        request->nativeLayerCount = ReadU8At(chunk, 0x09);
        if (!request->coordsValid)
        {
            FillChunkRequestCoordsFromIdentity(*request, chunk);
            if (!request->coordsValid)
                FillChunkRequestCoordsFromRenderOrigin(*request, chunk);
        }
        RefreshOwnerIdentity(state, *request, chunk, false);
        state.focusWorldChunkX    = request->worldChunkX;
        state.focusWorldChunkY    = request->worldChunkY;
        state.lastObservedLocalChunkX = request->localChunkX;
        state.lastObservedLocalChunkY = request->localChunkY;
        state.lastObservedWorldChunkX = request->worldChunkX;
        state.lastObservedWorldChunkY = request->worldChunkY;
        state.hasFocusChunk       = true;
    }

    if ((state.observations & 0x3F) == 0)
    {
        SweepStaleRequests(state, nowMs);
        LogTraceSummary(state, nowMs);
    }
}

void ExtendedTerrainTextures::RecordHelperResult(bool ok, char const* operation, char const* detail)
{
    RuntimeState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);

    if (ok)
    {
        ++state.helperSuccesses;
        state.helperFailures = 0;
        if (state.circuitOpen)
            RestoreTerrainDrawOverrideUnlocked(state);
        RecordLifecycleUnlocked(state, operation ? operation : "helper", "ok");
        if (state.config.trace)
            LOG_INFO << "ExtendedTerrainTextures: helper ok op=" << (operation ? operation : "<unknown>");
        return;
    }

    ++state.helperFailures;
    RecordLifecycleUnlocked(state, operation ? operation : "helper", "failure");
    RecordFallbackUnlocked(state, "helper-failure", detail);
    LOG_WARN << "ExtendedTerrainTextures: helper failure op=" << (operation ? operation : "<unknown>")
             << " failures=" << state.helperFailures
             << " detail=" << (detail ? detail : "<none>");

    if (!state.circuitOpen && state.helperFailures >= state.config.circuitBreakerFailures)
        OpenCircuit(state, operation, detail);
}

void ExtendedTerrainTextures::RecordPayloadValidation(bool ok, char const* detail)
{
    RuntimeState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);

    if (ok)
    {
        ++state.payloadValidationOk;
        RecordLifecycleUnlocked(state, "validation-ok", detail);
        return;
    }

    ++state.payloadValidationFailed;
    RecordFallbackUnlocked(state, "validation-failed", detail);
    RecordLifecycleUnlocked(state, "validation-failed", detail);
}

void ExtendedTerrainTextures::RecordPayloadBytes(void* chunk, int32_t textureBytes, int32_t materialBytes, bool resident)
{
    RuntimeState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    uintptr_t const chunkPtr = reinterpret_cast<uintptr_t>(chunk);
    uint32_t const nowMs     = GetTickCount();

    PayloadRecord* payload = FindOrCreatePayload(state, chunkPtr, nowMs);

    int32_t const storedTextureBytes = (std::max)(0, textureBytes);
    int32_t const storedMaterialBytes = (std::max)(0, materialBytes);
    bool const storedResident = resident;

    payload->textureBytes    = storedTextureBytes;
    payload->materialBytes   = storedMaterialBytes;
    payload->resident        = resident;
    payload->pendingUpload   = !resident;
    payload->uploadFailed    = false;
    payload->lastSeenMs      = nowMs;
    payload->lastGeneration  = state.generation;

    AttachChunkCoordsFromRequest(state, *payload);
    RebuildPayloadIndex(state);

    RecalculatePayloadCounters(state);
    RecordLifecycleUnlocked(state, resident ? "payload-resident" : "payload-pending", "record-bytes");
    EvictUntilWithinBudget(state);

    if (state.config.trace)
    {
        LOG_INFO << "ExtendedTerrainTextures: payload bytes chunk=0x" << std::hex << chunkPtr << std::dec
                 << " textureBytes=" << storedTextureBytes
                 << " materialBytes=" << storedMaterialBytes
                 << " resident=" << (storedResident ? 1 : 0)
                 << " liveBytes=" << LiveBytes(state)
                 << " pendingPayloadBytes=" << state.pendingPayloadBytes;
    }
}

bool ExtendedTerrainTextures::SampleMaterial(void* chunk, float u, float v, int32_t& layerIndex, int32_t& materialId)
{
    layerIndex = -1;
    materialId = 0;

    RuntimeState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    ++state.materialLookupAttempts;

    if (!state.config.materialLookup || !state.config.enabled || state.runtimeDisabled || state.circuitOpen)
    {
        ++state.materialLookupFallbacks;
        RecordFallbackUnlocked(state, "material-lookup-disabled", "native-material");
        return false;
    }

    PayloadRecord* payload = FindPayload(state, reinterpret_cast<uintptr_t>(chunk));
    if (!payload || !payload->materialMapLoaded)
    {
        ++state.materialLookupFallbacks;
        RecordFallbackUnlocked(state, "material-map-missing", "native-material");
        return false;
    }

    float const clampedU = (std::min)((std::max)(u, 0.0f), 1.0f);
    float const clampedV = (std::min)((std::max)(v, 0.0f), 1.0f);
    int32_t const x = (std::min)(payload->materialWidth - 1,
                                 static_cast<int32_t>(clampedU * static_cast<float>(payload->materialWidth)));
    int32_t const y = (std::min)(payload->materialHeight - 1,
                                 static_cast<int32_t>(clampedV * static_cast<float>(payload->materialHeight)));
    size_t const offset = static_cast<size_t>(y * payload->materialWidth + x);
    if (offset >= payload->materialMap.size())
    {
        ++state.materialLookupFallbacks;
        RecordFallbackUnlocked(state, "material-map-bounds", "native-material");
        return false;
    }

    layerIndex = payload->materialMap[offset];
    if (layerIndex >= 0 && layerIndex < static_cast<int32_t>(payload->layerMaterialIds.size()))
        materialId = payload->layerMaterialIds[layerIndex];

    ++state.materialLookupHits;
    return true;
}

void ExtendedTerrainTextures::QueuePayloadForUpload(void* chunk, char const* texturePath, char const* manifestPath,
                                                    char const* quality, int32_t textureBytes, int32_t materialBytes)
{
    RuntimeState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    uint32_t const nowMs = GetTickCount();
    uintptr_t const chunkPtr = reinterpret_cast<uintptr_t>(chunk);
    int32_t const storedTextureBytes = (std::max)(0, textureBytes);
    int32_t const storedMaterialBytes = (std::max)(0, materialBytes);

    if (!texturePath || !*texturePath || storedTextureBytes <= 0)
    {
        RecordFallbackUnlocked(state, "invalid-upload-payload", "missing-path-or-bytes");
        return;
    }

    uint64_t const queuedBytes = static_cast<uint64_t>(storedTextureBytes) + static_cast<uint64_t>(storedMaterialBytes);
    if (queuedBytes > MemoryBudgetBytes(state))
    {
        ++state.budgetFallbacks;
        RecordFallbackUnlocked(state, "memory-budget-pressure", "payload-larger-than-budget");
        return;
    }

    PayloadRecord* payload = FindOrCreatePayload(state, chunkPtr, nowMs);
    ++state.payloadValidationOk;
    RecordLifecycleUnlocked(state, "validation-ok", manifestPath ? manifestPath : "");
    payload->textureBytes = storedTextureBytes;
    payload->materialBytes = storedMaterialBytes;
    payload->resident = false;
    payload->pendingUpload = true;
    payload->uploadFailed = false;
    payload->lastSeenMs = nowMs;
    payload->lastGeneration = state.generation;
    payload->texturePath = texturePath;
    payload->manifestPath = manifestPath ? manifestPath : "";
    payload->quality = ToLowerCopy(quality ? quality : state.config.quality);
    if (payload->quality != "quick" && payload->quality != "final")
        payload->quality = state.config.quality;

    AttachChunkCoordsFromRequest(state, *payload);
    ++state.uploadAccepted;
    RebuildPayloadIndex(state);
    RecalculatePayloadCounters(state);
    RecordLifecycleUnlocked(state, "payload-transfer", payload->quality.c_str());

    if (state.config.trace)
    {
        LOG_INFO << "ExtendedTerrainTextures: queued upload chunk=0x" << std::hex << chunkPtr << std::dec
                 << " path=" << payload->texturePath
                 << " quality=" << payload->quality
                 << " textureBytes=" << payload->textureBytes
                 << " materialBytes=" << payload->materialBytes
                 << " uploadQueueDepth=" << state.uploadQueueDepth;
    }
}

void ExtendedTerrainTextures::PrepareTextureSetForNativeUpdate(void* textureSet)
{
    RuntimeState& state = State();
    if (!textureSet)
        return;

    std::unique_lock<std::mutex> lock(state.mutex, std::try_to_lock);
    if (!lock.owns_lock())
        return;

    SyncTextureSetIdentityFromNativeChunk(state, textureSet, GetTickCount());
}

void ExtendedTerrainTextures::AttachReadyTextureSet(void* textureSet)
{
    RuntimeState& state = State();
    if (!textureSet)
        return;

    std::unique_lock<std::mutex> lock(state.mutex, std::try_to_lock);
    if (!lock.owns_lock())
        return;

    if (!state.configured || !state.config.enabled || !state.config.nativeUpload ||
        state.runtimeDisabled || state.circuitOpen)
        return;

    SyncTextureSetIdentityFromNativeChunk(state, textureSet, GetTickCount());

    uintptr_t const textureSetPtr = reinterpret_cast<uintptr_t>(textureSet);
    ChunkRequest* request = FindRequest(state, textureSetPtr);
    PayloadRecord* payload = FindPayload(state, textureSetPtr);
    if (!request)
        return;

    bool forceReattach = state.pendingForceReattachOwners.erase(textureSetPtr) > 0;
    if (request->nativeChunkPtr)
        forceReattach = (state.pendingForceReattachOwners.erase(request->nativeChunkPtr) > 0) || forceReattach;

    if (!payload)
    {
        payload = FindPayloadByNativeChunk(state, request->nativeChunkPtr);
        if (payload && payload->textureHandle && payload->resident)
        {
            uintptr_t const oldChunkPtr = payload->chunkPtr;
            payload->chunkPtr = textureSetPtr;
            payload->textureSetPtr = textureSetPtr;
            payload->nativeChunkPtr = request->nativeChunkPtr;
            payload->ownerGeneration = request->ownerGeneration;
            payload->lastSeenMs = GetTickCount();
            RebuildPayloadIndex(state);

            if (!request->cacheLookupHit && !payload->sourceStackHash.empty())
            {
                request->cacheLookupHit = true;
                request->sourceStackHash = payload->sourceStackHash;
            }

            RecordLifecycleUnlocked(state, "payload-rebind-native-owner",
                                    oldChunkPtr != textureSetPtr ? "owner-pointer-changed" : "owner-pointer-match");
        }
    }

    if (!payload)
    {
        if (forceReattach)
        {
            state.pendingForceReattachOwners.insert(textureSetPtr);
            if (request->nativeChunkPtr)
                state.pendingForceReattachOwners.insert(request->nativeChunkPtr);
            RecordFallbackUnlocked(state, "force-reattach-payload-missing", "retry");
        }
        return;
    }

    if (!payload->resident && payload->pendingUpload && !payload->uploadFailed &&
        !payload->texturePath.empty() && state.config.nativeUpload)
    {
        ++state.uploadDeferred;
        return;
    }

    if (forceReattach && payload->tileLoadIdentity)
    {
        request->tileLoadIdentity = true;
        request->coordsValid = payload->worldChunkX >= 0 && payload->worldChunkY >= 0;
        request->worldChunkX = payload->worldChunkX;
        request->worldChunkY = payload->worldChunkY;
        request->adtTileX = payload->adtTileX;
        request->adtTileY = payload->adtTileY;
        request->adtTileValid = payload->adtTileX >= 0 && payload->adtTileY >= 0;
        request->chunkIndex = payload->chunkIndex;
        request->nativeChunkPtr = payload->nativeChunkPtr;
        request->textureSetPtr = textureSetPtr;
        request->ownerGeneration = payload->ownerGeneration;
        if (!request->cacheLookupHit && !payload->sourceStackHash.empty())
        {
            request->cacheLookupHit = true;
            request->sourceStackHash = payload->sourceStackHash;
        }
    }

    bool const attached = AttachPayloadToTextureSet(state, *payload, *request,
                                                    forceReattach ? "native-update-force-reattach" : "native-update");
    if (attached && payload->textureHandle)
    {
        // Hard-stick after native update: if native path rewrote either field later in the same cycle,
        // pin both diffuse and composite handles again for this owner.
        void* owner = reinterpret_cast<void*>(payload->chunkPtr);
        WriteU8At(owner, 0x09, 1);
        WriteU32At(owner, 0x38, static_cast<uint32_t>(payload->textureHandle));
        WriteU32At(owner, 0x84, payload->savedCompositeHandle);
    }
    if (forceReattach && !attached)
    {
        state.pendingForceReattachOwners.insert(textureSetPtr);
        if (request->nativeChunkPtr)
            state.pendingForceReattachOwners.insert(request->nativeChunkPtr);
        RecordFallbackUnlocked(state, "force-reattach-attach-failed", "retry");
    }
}

void ExtendedTerrainTextures::AttachReadyVisibleTextureSets()
{
    RuntimeState& state = State();
    std::vector<uintptr_t> owners;
    owners.reserve(64);

    {
        std::unique_lock<std::mutex> lock(state.mutex, std::try_to_lock);
        if (!lock.owns_lock())
            return;
        if (!state.configured || !state.config.enabled || !state.config.nativeUpload ||
            state.runtimeDisabled || state.circuitOpen)
            return;

        for (ChunkRequest& request : state.requests)
        {
            if (!request.textureSetPtr || !request.coordsValid)
                continue;
            if (state.cachedStackTileValid &&
                (request.adtTileX != state.cachedStackTileX || request.adtTileY != state.cachedStackTileY))
            {
                continue;
            }

            // Static-camera prewarm: ensure stack-tile requests are lookup-queued even when
            // observer identity churn is not producing fresh queue events.
            if (state.config.autoQueueCache && !request.cacheLookupAttempted)
                EnqueueRequest(state, request.chunkPtr);

            owners.push_back(request.textureSetPtr);
            if (owners.size() >= 96)
                break;
        }
    }

    for (uintptr_t owner : owners)
        AttachReadyTextureSet(reinterpret_cast<void*>(owner));

}

void ExtendedTerrainTextures::PumpUploadBudget()
{
    RuntimeState& state = State();
    std::unique_lock<std::mutex> lock(state.mutex, std::try_to_lock);
    if (!lock.owns_lock())
        return;

    uint32_t const nowMs = GetTickCount();
    ResetUploadFrameIfNeeded(state, nowMs);

    if (!state.configured || !state.config.enabled || state.runtimeDisabled || state.circuitOpen)
        return;

    PumpAutoQueueBudgetUnlocked(state, nowMs);

    PayloadRecord* selected = nullptr;
    bool selectedOversized = false;
    for (PayloadRecord& payload : state.payloads)
    {
        if (!payload.pendingUpload || payload.uploadFailed || payload.texturePath.empty())
            continue;

        bool oversized = false;
        if (!UploadBudgetAllows(state, payload, nowMs, oversized))
        {
            ++state.uploadDeferred;
            ++state.budgetFallbacks;
            RecordFallbackUnlocked(state, "upload-budget-pressure", payload.quality.c_str());
            return;
        }

        selected = &payload;
        selectedOversized = oversized;
        break;
    }

    if (!selected)
        return;

    if (!state.config.nativeUpload)
    {
        ++state.uploadDeferred;
        selected->pendingUpload = false;
        RecordFallbackUnlocked(state, "native-upload-disabled", selected->quality.c_str());
        RecordLifecycleUnlocked(state, "upload-skipped", "native-upload-disabled");
        RecalculatePayloadCounters(state);
        return;
    }

    uintptr_t const chunkPtr = selected->chunkPtr;
    std::string const texturePath = (state.config.forceTestBlp ? std::string(kForcedExtensionBlpPath)
                                                               : selected->texturePath);
    std::string const quality = selected->quality;
    int32_t const oldHandle = selected->textureHandle;
    ++selected->uploadAttempts;
    ++state.textureCreateAttempts;
    SpendUploadBudget(state, *selected);
    if (selectedOversized)
        RecordFallbackUnlocked(state, "upload-budget-oversized-single", quality.c_str());
    RecordLifecycleUnlocked(state, "upload-start", quality.c_str());

    lock.unlock();
    int handle = 0;
    if (state.config.debugCheckerEnabled && state.config.debugCheckerStrict)
    {
        handle = CreateClientTextureDirectFromBlp(texturePath);
        if (!handle)
            handle = CreateStrictCheckerTexture();
    }
    else
    {
        handle = CreateClientTextureFromPath(texturePath);
    }
    lock.lock();

    PayloadRecord* payload = FindUploadPayload(state, chunkPtr, texturePath);
    if (!payload)
    {
        CloseUnownedTextureHandle(state, handle, "stale-upload-payload-missing");
        return;
    }

    payload->lastUploadMs = nowMs;
    ChunkRequest* currentRequest = FindRequest(state, chunkPtr);
    if (handle && (!currentRequest || !PayloadOwnerMatchesRequest(*payload, *currentRequest)))
    {
        CloseUnownedTextureHandle(state, handle, "stale-upload-owner");
        payload->resident = false;
        payload->pendingUpload = false;
        payload->uploadFailed = true;
        ++state.uploadFailed;
        ++state.staleOwnerRejects;
        if (currentRequest)
            ++state.generationMismatchRejects;
        RecordFallbackUnlocked(state, currentRequest ? "owner-generation-mismatch" : "owner-missing", texturePath.c_str());
        RecordLifecycleUnlocked(state, "upload-stale-owner", quality.c_str());
        RecalculatePayloadCounters(state);
        return;
    }

    if (handle)
    {
        if (oldHandle && oldHandle != handle)
            ReleasePayloadHandle(state, *payload, "quick-final-replace");

        payload->textureHandle = handle;
        payload->resident = true;
        payload->pendingUpload = false;
        payload->uploadFailed = false;
        ++state.uploadCompleted;
        if (state.config.nativeCompositeGpu)
        {
            if (MarkNativeTextureSetDirtyForComposite(*payload))
                RecordLifecycleUnlocked(state, "native-composite-dirty", quality.c_str());
            else
                RecordFallbackUnlocked(state, "native-composite-dirty-failed", quality.c_str());
        }
        RecordLifecycleUnlocked(state, oldHandle && oldHandle != handle ? "upload-replace" : "upload-complete",
                                quality.c_str());
        RebuildPayloadIndex(state);
        RecalculatePayloadCounters(state);
        EvictUntilWithinBudget(state);
        return;
    }

    payload->resident = false;
    payload->pendingUpload = false;
    payload->uploadFailed = true;
    ++state.uploadFailed;
    ++state.textureCreateFailures;
    RecordFallbackUnlocked(state, "texture-create-failed", texturePath.c_str());
    RecordLifecycleUnlocked(state, "upload-failed", quality.c_str());
    RecalculatePayloadCounters(state);
}

void ExtendedTerrainTextures::ReleaseChunkResources(void* chunk, char const* reason)
{
    RuntimeState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    uintptr_t const chunkPtr = reinterpret_cast<uintptr_t>(chunk);
    std::string const cleanupReason = reason ? reason : "";
    bool const preserveForOcclusion = cleanupReason == "native-texture-cleanup";
    ++state.ownerGeneration;
    ++state.nativeCleanupInvalidations;

    PayloadRecord* payload = FindPayload(state, chunkPtr);
    if (payload)
    {
        if (preserveForOcclusion)
        {
            if (payload->attached)
                DetachPayloadFromTextureSet(state, *payload, "native-cleanup-preserve");
            payload->swapped = false;
            payload->attached = false;
            payload->lastSeenMs = GetTickCount();
        }
        else
        {
            ReleasePayloadHandle(state, *payload, reason);
            payload->swapped = false;
        }
    }

    if (preserveForOcclusion)
    {
        if (ChunkRequest* request = FindRequest(state, chunkPtr))
        {
            request->lastSeenMs = GetTickCount();
            request->stale = false;
        }
        state.pendingForceReattachOwners.insert(chunkPtr);
        if (payload && payload->nativeChunkPtr)
            state.pendingForceReattachOwners.insert(payload->nativeChunkPtr);

        RecordLifecycleUnlocked(state, "chunk-release-preserve", reason);
        RecalculatePayloadCounters(state);
        return;
    }

    state.payloads.erase(std::remove_if(state.payloads.begin(), state.payloads.end(),
                                        [chunkPtr](PayloadRecord const& item) { return item.chunkPtr == chunkPtr; }),
                         state.payloads.end());
    state.requests.erase(std::remove_if(state.requests.begin(), state.requests.end(),
                                        [chunkPtr](ChunkRequest const& request) { return request.chunkPtr == chunkPtr; }),
                         state.requests.end());
    state.queue.erase(std::remove_if(state.queue.begin(), state.queue.end(),
                                     [chunkPtr](uintptr_t queued) { return queued == chunkPtr; }),
                      state.queue.end());
    state.queuedRequests.erase(chunkPtr);
    RebuildPayloadIndex(state);
    RebuildRequestIndex(state);

    RecordLifecycleUnlocked(state, "chunk-release", reason);
    RecalculatePayloadCounters(state);
}

void ExtendedTerrainTextures::OnDeviceLost()
{
    RuntimeState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    ++state.deviceLossCount;
    ++state.ownerGeneration;

    for (auto& pair : state.gpuCompositeRts)
    {
        if (pair.second.texture)
            pair.second.texture->Release();
        if (pair.second.uploadTexture)
            pair.second.uploadTexture->Release();
        if (pair.second.sysmemSurface)
            pair.second.sysmemSurface->Release();
    }
    state.gpuCompositeRts.clear();

    for (PayloadRecord& payload : state.payloads)
    {
        bool const hadTexturePath = !payload.texturePath.empty();
        if (payload.textureHandle)
            ReleasePayloadHandle(state, payload, "device-lost");
        payload.swapped = false;
        if (hadTexturePath && !payload.uploadFailed)
        {
            payload.pendingUpload = true;
            payload.resident = false;
            ++state.deviceReuploadQueued;
        }
    }

    RecordLifecycleUnlocked(state, "device-lost", "requeue-resident-payloads");
    RecalculatePayloadCounters(state);
}

void ExtendedTerrainTextures::OnDeviceRestored()
{
    RuntimeState& state = State();
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        ++state.deviceRestoreCount;
        RecordLifecycleUnlocked(state, "device-restored", "upload-pump-ready");
    }

    PumpUploadBudget();
}

void ExtendedTerrainTextures::RecordFallback(char const* reason, char const* detail)
{
    RuntimeState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    RecordFallbackUnlocked(state, reason, detail);
}

void ExtendedTerrainTextures::RecordLifecycle(char const* stage, char const* detail)
{
    RuntimeState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    RecordLifecycleUnlocked(state, stage, detail);
}

void ExtendedTerrainTextures::SetRuntimeEnabled(bool enabled, char const* reason)
{
    RuntimeState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    state.runtimeDisabled = !enabled;
    state.disabledReason  = reason ? reason : "";
    if (!enabled)
    {
        RestoreTerrainDrawOverrideUnlocked(state);
        RecordFallbackUnlocked(state, "runtime-disabled", reason);
        ClearPending(state);
    }
    else
    {
        if (state.config.drawReplacement)
            ApplyTerrainDrawOverrideUnlocked(state);
        RecordLifecycleUnlocked(state, "runtime-enabled", reason);
    }

    LOG_INFO << "ExtendedTerrainTextures: runtime " << (enabled ? "enabled" : "disabled")
             << " reason=" << (reason ? reason : "<none>");
}

bool ExtendedTerrainTextures::SetRuntimeOption(char const* option, bool enabled, char const* reason)
{
    std::string const name = ToLowerCopy(option ? option : "");
    RuntimeState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);

    if (name == "enabled")
    {
        state.runtimeDisabled = !enabled;
        state.disabledReason  = reason ? reason : "";
        if (!enabled)
        {
            RestoreTerrainDrawOverrideUnlocked(state);
            RecordFallbackUnlocked(state, "runtime-disabled", reason);
            ClearPending(state);
        }
        else
        {
            if (state.config.drawReplacement)
                ApplyTerrainDrawOverrideUnlocked(state);
            RecordLifecycleUnlocked(state, "runtime-enabled", reason);
        }
    }
    else if (name == "trace")
    {
        state.config.trace = enabled;
    }
    else if (name == "telemetry")
    {
        state.config.telemetry = enabled;
    }
    else if (name == "nativeupload" || name == "native-upload")
    {
        state.config.nativeUpload = enabled;
        if (!enabled)
            ClearPending(state);
    }
    else if (name == "renderswap" || name == "render-swap")
    {
        state.config.renderSwap = enabled;
        if (enabled && state.config.drawReplacement)
            ApplyTerrainDrawOverrideUnlocked(state);
        else
            RestoreTerrainDrawOverrideUnlocked(state);
    }
    else if (name == "drawreplacement" || name == "draw-replacement")
    {
        state.config.drawReplacement = enabled;
        if (enabled)
            ApplyTerrainDrawOverrideUnlocked(state);
        else
            RestoreTerrainDrawOverrideUnlocked(state);
    }
    else if (name == "autoqueuecache" || name == "auto-queue-cache")
    {
        state.config.autoQueueCache = enabled;
        if (enabled)
        {
            ClearRequestQueue(state);
            for (ChunkRequest& request : state.requests)
            {
                request.cacheLookupAttempted = false;
                EnqueueRequest(state, request.chunkPtr);
            }
        }
        else
        {
            ClearRequestQueue(state);
        }
    }
    else if (name == "materiallookup" || name == "material-lookup")
    {
        state.config.materialLookup = enabled;
    }
    else if (name == "nativecompositegpu" || name == "native-composite-gpu" || name == "gpucomposite" ||
             name == "gpu-composite")
    {
        state.config.nativeCompositeGpu = enabled;
        if (enabled)
        {
            // GPU composite mode still needs payload upload/cache flow for ext layer sources.
            state.config.nativeUpload = true;
            state.config.renderSwap = true;
            state.config.autoQueueCache = true;
            if (state.config.drawReplacement)
                ApplyTerrainDrawOverrideUnlocked(state);
            else
                RestoreTerrainDrawOverrideUnlocked(state);
            RecordLifecycleUnlocked(state, "gpu-composite-mode", "enabled");
        }
        else
        {
            for (auto& pair : state.gpuCompositeRts)
            {
                if (pair.second.texture)
                    pair.second.texture->Release();
                if (pair.second.uploadTexture)
                    pair.second.uploadTexture->Release();
                if (pair.second.sysmemSurface)
                    pair.second.sysmemSurface->Release();
            }
            state.gpuCompositeRts.clear();
            RecordLifecycleUnlocked(state, "gpu-composite-mode", "disabled");
        }
    }
    else if (name == "debugchecker" || name == "debug-checker")
    {
        state.config.debugCheckerEnabled = enabled;
    }
    else if (name == "debugcheckerstrict" || name == "debug-checker-strict")
    {
        state.config.debugCheckerStrict = enabled;
    }
    else if (name == "forcetestblp" || name == "force-test-blp")
    {
        state.config.forceTestBlp = enabled;
        // Force immediate visual refresh since source texture selection changes.
        for (PayloadRecord& payload : state.payloads)
        {
            if (!payload.texturePath.empty())
            {
                if (payload.textureHandle)
                    ReleasePayloadHandle(state, payload, "force-test-blp-toggled");
                payload.resident = false;
                payload.pendingUpload = true;
                payload.uploadFailed = false;
                if (payload.chunkPtr)
                    state.pendingForceReattachOwners.insert(payload.chunkPtr);
                if (payload.nativeChunkPtr)
                    state.pendingForceReattachOwners.insert(payload.nativeChunkPtr);
            }
        }
        RecalculatePayloadCounters(state);
    }
    else
    {
        LOG_WARN << "ExtendedTerrainTextures: unknown runtime option=" << (option ? option : "<null>");
        return false;
    }

    RecordLifecycleUnlocked(state, "runtime-option", name.c_str());
    LOG_INFO << "ExtendedTerrainTextures: runtime option " << name << "=" << (enabled ? 1 : 0)
             << " reason=" << (reason ? reason : "<none>");
    return true;
}

bool ExtendedTerrainTextures::SetRuntimeIntOption(char const* option, int32_t value, char const* reason)
{
    std::string const name = ToLowerCopy(option ? option : "");
    RuntimeState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);

    int32_t const safeValue = (std::max)(0, value);
    if (name == "autoqueuelookupsperframe" || name == "auto-queue-lookups-per-frame")
    {
        state.config.autoQueueLookupsPerFrame = safeValue;
    }
    else if (name == "autoqueuepopsperframe" || name == "auto-queue-pops-per-frame")
    {
        state.config.autoQueuePopsPerFrame = safeValue;
    }
    else if (name == "observernewchunksperframe" || name == "observer-new-chunks-per-frame")
    {
        state.config.observerNewChunksPerFrame = safeValue;
    }
    else if (name == "observerrefreshms" || name == "observer-refresh-ms")
    {
        state.config.observerRefreshMs = safeValue;
    }
    else if (name == "uploadbudgetkbperframe" || name == "upload-budget-kb-per-frame")
    {
        state.config.uploadBudgetKBPerFrame = safeValue;
    }
    else if (name == "uploadbudgetcountperframe" || name == "upload-budget-count-per-frame")
    {
        state.config.uploadBudgetCountPerFrame = safeValue;
    }
    else if (name == "uploadbudgetmsperframe" || name == "upload-budget-ms-per-frame")
    {
        state.config.uploadBudgetMsPerFrame = safeValue;
    }
    else if (name == "debugcheckeralphapct" || name == "debug-checker-alpha-pct")
    {
        state.config.debugCheckerAlphaPct = (std::min)(100, safeValue);
        // Force immediate visual refresh: checker strength is baked into texture pixels.
        for (PayloadRecord& payload : state.payloads)
        {
            if (!payload.texturePath.empty())
            {
                if (payload.textureHandle)
                    ReleasePayloadHandle(state, payload, "debug-checker-alpha-changed");
                payload.resident = false;
                payload.pendingUpload = true;
                payload.uploadFailed = false;
                if (payload.chunkPtr)
                    state.pendingForceReattachOwners.insert(payload.chunkPtr);
                if (payload.nativeChunkPtr)
                    state.pendingForceReattachOwners.insert(payload.nativeChunkPtr);
            }
        }
        RecalculatePayloadCounters(state);
    }
    else
    {
        LOG_WARN << "ExtendedTerrainTextures: unknown runtime int option=" << (option ? option : "<null>");
        return false;
    }

    RecordLifecycleUnlocked(state, "runtime-int-option", name.c_str());
    LOG_INFO << "ExtendedTerrainTextures: runtime int option " << name << "=" << safeValue
             << " reason=" << (reason ? reason : "<none>");
    return true;
}

void ExtendedTerrainTextures::ResetCircuitBreaker()
{
    RuntimeState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    state.circuitOpen    = false;
    state.helperFailures = 0;
    if (state.config.drawReplacement)
        ApplyTerrainDrawOverrideUnlocked(state);
    LOG_INFO << "ExtendedTerrainTextures: helper circuit reset";
}

LUA_FUNCTION(ExtendedTerrainTexturesSetEnabled, (lua_State * L))
{
    bool const enabled = ClientLua::GetNumber(L, 1, 1) != 0;
    ExtendedTerrainTextures::SetRuntimeEnabled(enabled, ClientLua::GetString(L, 2, "lua").c_str());
    return 0;
}

LUA_FUNCTION(ExtendedTerrainTexturesSetRuntimeOption, (lua_State * L))
{
    std::string option = ClientLua::GetString(L, 1, "").c_str();
    bool const enabled = ClientLua::GetNumber(L, 2, 1) != 0;
    bool const ok = ExtendedTerrainTextures::SetRuntimeOption(option.c_str(), enabled,
                                                              ClientLua::GetString(L, 3, "lua").c_str());
    ClientLua::PushBoolean(L, ok ? 1 : 0);
    return 1;
}

LUA_FUNCTION(ExtendedTerrainTexturesSetRuntimeIntOption, (lua_State * L))
{
    std::string option = ClientLua::GetString(L, 1, "").c_str();
    int32_t const value = static_cast<int32_t>(ClientLua::GetNumber(L, 2, 0));
    bool const ok = ExtendedTerrainTextures::SetRuntimeIntOption(option.c_str(), value,
                                                                 ClientLua::GetString(L, 3, "lua").c_str());
    ClientLua::PushBoolean(L, ok ? 1 : 0);
    return 1;
}

LUA_FUNCTION(ExtendedTerrainTexturesResetCircuit, (lua_State *))
{
    ExtendedTerrainTextures::ResetCircuitBreaker();
    return 0;
}

LUA_FUNCTION(ExtendedTerrainTexturesDebugHelperResult, (lua_State * L))
{
    bool const ok = ClientLua::GetNumber(L, 1, 1) != 0;
    ExtendedTerrainTextures::RecordHelperResult(ok, ClientLua::GetString(L, 2, "lua").c_str(),
                                                ClientLua::GetString(L, 3, "").c_str());
    return 0;
}

LUA_FUNCTION(ExtendedTerrainTexturesDebugPayloadValidation, (lua_State * L))
{
    bool const ok = ClientLua::GetNumber(L, 1, 1) != 0;
    ExtendedTerrainTextures::RecordPayloadValidation(ok, ClientLua::GetString(L, 2, "").c_str());
    return 0;
}

LUA_FUNCTION(ExtendedTerrainTexturesDebugPayloadBytes, (lua_State * L))
{
    void* chunk = reinterpret_cast<void*>(static_cast<uintptr_t>(ClientLua::GetNumber(L, 1, 0)));
    int32_t textureBytes = static_cast<int32_t>(ClientLua::GetNumber(L, 2, 0));
    int32_t materialBytes = static_cast<int32_t>(ClientLua::GetNumber(L, 3, 0));
    bool const resident = ClientLua::GetNumber(L, 4, 0) != 0;
    ExtendedTerrainTextures::RecordPayloadBytes(chunk, textureBytes, materialBytes, resident);
    return 0;
}

LUA_FUNCTION(ExtendedTerrainTexturesDebugSampleMaterial, (lua_State * L))
{
    void* chunk = reinterpret_cast<void*>(static_cast<uintptr_t>(ClientLua::GetNumber(L, 1, 0)));
    float const u = static_cast<float>(ClientLua::GetNumber(L, 2, 0));
    float const v = static_cast<float>(ClientLua::GetNumber(L, 3, 0));
    int32_t layerIndex = -1;
    int32_t materialId = 0;
    bool const ok = ExtendedTerrainTextures::SampleMaterial(chunk, u, v, layerIndex, materialId);

    ClientLua::CreateTable(L, 0, 3);
    PushStatusField(L, "ok", ok);
    PushStatusField(L, "layerIndex", static_cast<double>(layerIndex));
    PushStatusField(L, "materialId", static_cast<double>(materialId));
    return 1;
}

LUA_FUNCTION(ExtendedTerrainTexturesDebugQueueUpload, (lua_State * L))
{
    void* chunk = reinterpret_cast<void*>(static_cast<uintptr_t>(ClientLua::GetNumber(L, 1, 0)));
    std::string texturePath = ClientLua::GetString(L, 2, "").c_str();
    std::string manifestPath = ClientLua::GetString(L, 3, "").c_str();
    std::string quality = ClientLua::GetString(L, 4, "quick").c_str();
    int32_t textureBytes = static_cast<int32_t>(ClientLua::GetNumber(L, 5, 0));
    int32_t materialBytes = static_cast<int32_t>(ClientLua::GetNumber(L, 6, 0));
    ExtendedTerrainTextures::QueuePayloadForUpload(chunk, texturePath.c_str(), manifestPath.c_str(), quality.c_str(),
                                                   textureBytes, materialBytes);
    return 0;
}

LUA_FUNCTION(ExtendedTerrainTexturesDebugPumpUpload, (lua_State *))
{
    ExtendedTerrainTextures::PumpUploadBudget();
    return 0;
}

LUA_FUNCTION(ExtendedTerrainTexturesDebugFallback, (lua_State * L))
{
    ExtendedTerrainTextures::RecordFallback(ClientLua::GetString(L, 1, "lua").c_str(),
                                            ClientLua::GetString(L, 2, "").c_str());
    return 0;
}

LUA_FUNCTION(ExtendedTerrainTexturesDebugLifecycle, (lua_State * L))
{
    ExtendedTerrainTextures::RecordLifecycle(ClientLua::GetString(L, 1, "lua").c_str(),
                                             ClientLua::GetString(L, 2, "").c_str());
    return 0;
}

LUA_FUNCTION(ExtendedTerrainTexturesStatus, (lua_State * L))
{
    RuntimeState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    uint32_t const nowMs = GetTickCount();

    ClientLua::CreateTable(L, 0, 48);
    PushStatusField(L, "configured", state.configured);
    PushStatusField(L, "enabled", state.config.enabled && !state.runtimeDisabled && !state.circuitOpen);
    PushStatusField(L, "configEnabled", state.config.enabled);
    PushStatusField(L, "trace", state.config.trace);
    PushStatusField(L, "telemetry", state.config.telemetry);
    PushStatusField(L, "nativeUpload", state.config.nativeUpload);
    PushStatusField(L, "nativeCompositeGpu", state.config.nativeCompositeGpu);
    PushStatusField(L, "drawReplacement", state.config.drawReplacement);
    PushStatusField(L, "runtimeDisabled", state.runtimeDisabled);
    PushStatusField(L, "disabledReason", state.disabledReason);
    PushStatusField(L, "circuitOpen", state.circuitOpen);
    PushStatusField(L, "pending", static_cast<double>(state.requests.size()));
    PushStatusField(L, "queued", static_cast<double>(state.queue.size()));
    PushStatusField(L, "payloadRecords", static_cast<double>(state.payloads.size()));
    PushStatusField(L, "uploadQueueDepth", static_cast<double>(state.uploadQueueDepth));
    PushStatusField(L, "uploadBudgetKBPerFrame", static_cast<double>(state.config.uploadBudgetKBPerFrame));
    PushStatusField(L, "uploadBudgetCountPerFrame", static_cast<double>(state.config.uploadBudgetCountPerFrame));
    PushStatusField(L, "uploadBudgetMsPerFrame", static_cast<double>(state.config.uploadBudgetMsPerFrame));
    PushStatusField(L, "uploadFrameCount", static_cast<double>(state.uploadFrameCount));
    PushStatusField(L, "uploadFrameBytes", static_cast<double>(state.uploadFrameBytes));
    PushStatusField(L, "uploadFrameMs", static_cast<double>(state.uploadFrameMs));
    PushStatusField(L, "uploadAccepted", static_cast<double>(state.uploadAccepted));
    PushStatusField(L, "uploadDeferred", static_cast<double>(state.uploadDeferred));
    PushStatusField(L, "uploadCompleted", static_cast<double>(state.uploadCompleted));
    PushStatusField(L, "uploadFailed", static_cast<double>(state.uploadFailed));
    PushStatusField(L, "textureCreateAttempts", static_cast<double>(state.textureCreateAttempts));
    PushStatusField(L, "textureCreateFailures", static_cast<double>(state.textureCreateFailures));
    PushStatusField(L, "retiredTextureHandles", static_cast<double>(state.retiredTextureHandles));
    PushStatusField(L, "deviceLossCount", static_cast<double>(state.deviceLossCount));
    PushStatusField(L, "deviceRestoreCount", static_cast<double>(state.deviceRestoreCount));
    PushStatusField(L, "deviceReuploadQueued", static_cast<double>(state.deviceReuploadQueued));
    PushStatusField(L, "renderSwap", state.config.renderSwap);
    PushStatusField(L, "drawSetupCalls", static_cast<double>(state.drawSetupCalls));
    PushStatusField(L, "drawOverrideInstalls", static_cast<double>(state.drawOverrideInstalls));
    PushStatusField(L, "drawOverrideRestores", static_cast<double>(state.drawOverrideRestores));
    PushStatusField(L, "drawWrapperCalls", static_cast<double>(state.drawWrapperCalls));
    PushStatusField(L, "drawWrapperNoNative", static_cast<double>(state.drawWrapperNoNative));
    PushStatusField(L, "drawWrapperNoResidents", static_cast<double>(state.drawWrapperNoResidents));
    PushStatusField(L, "overlayPassAttempts", static_cast<double>(state.overlayPassAttempts));
    PushStatusField(L, "overlayPassApplied", static_cast<double>(state.overlayPassApplied));
    PushStatusField(L, "overlayPassStateFailures", static_cast<double>(state.overlayPassStateFailures));
    PushStatusField(L, "lastDrawFunction", static_cast<double>(state.lastDrawFunction));
    PushStatusField(L, "lastNativeDrawFunction", static_cast<double>(state.lastNativeDrawFunction));
    PushStatusField(L, "lastDrawChunkPtr", static_cast<double>(state.lastDrawChunkPtr));
    PushStatusField(L, "lastSwapMissChunkPtr", static_cast<double>(state.lastSwapMissChunkPtr));
    PushStatusField(L, "lastSwapMissReason", state.lastSwapMissReason);
    PushStatusField(L, "lastSwapMissAdtX", static_cast<double>(state.lastSwapMissAdtTileX));
    PushStatusField(L, "lastSwapMissAdtY", static_cast<double>(state.lastSwapMissAdtTileY));
    PushStatusField(L, "lastSwapMissWorldChunkX", static_cast<double>(state.lastSwapMissWorldChunkX));
    PushStatusField(L, "lastSwapMissWorldChunkY", static_cast<double>(state.lastSwapMissWorldChunkY));
    PushStatusField(L, "swapActivations", static_cast<double>(state.swapActivations));
    PushStatusField(L, "swapFallbacks", static_cast<double>(state.swapFallbacks));
    PushStatusField(L, "staleOwnerRejects", static_cast<double>(state.staleOwnerRejects));
    PushStatusField(L, "generationMismatchRejects", static_cast<double>(state.generationMismatchRejects));
    PushStatusField(L, "offTargetRejects", static_cast<double>(state.offTargetRejects));
    PushStatusField(L, "nativeCleanupInvalidations", static_cast<double>(state.nativeCleanupInvalidations));
    PushStatusField(L, "lifecycleAttachCount", static_cast<double>(state.lifecycleAttachCount));
    PushStatusField(L, "lifecycleDetachCount", static_cast<double>(state.lifecycleDetachCount));
    PushStatusField(L, "lastAttachedTextureSetPtr", static_cast<double>(state.lastAttachedTextureSetPtr));
    PushStatusField(L, "lastAttachedNativeChunkPtr", static_cast<double>(state.lastAttachedNativeChunkPtr));
    PushStatusField(L, "lastAttachedOwnerGeneration", static_cast<double>(state.lastAttachedOwnerGeneration));
    PushStatusField(L, "lastAttachedChunkIndex", static_cast<double>(state.lastAttachedChunkIndex));
    PushStatusField(L, "lastAttachedAdtX", static_cast<double>(state.lastAttachedAdtTileX));
    PushStatusField(L, "lastAttachedAdtY", static_cast<double>(state.lastAttachedAdtTileY));
    PushStatusField(L, "lastAttachedHandle", static_cast<double>(state.lastAttachedHandle));
    PushStatusField(L, "lastAttachRejectReason", state.lastAttachRejectReason);
    PushStatusField(L, "lastCleanupReason", state.lastCleanupReason);
    PushStatusField(L, "lastSwapActivationChunkPtr", static_cast<double>(state.lastSwapActivationChunkPtr));
    PushStatusField(L, "lastSwapActivationAgeMs",
                    static_cast<double>(state.lastSwapActivationMs ? nowMs - state.lastSwapActivationMs : -1));
    PushStatusField(L, "lastSwapActivationLocalChunkX", static_cast<double>(state.lastSwapActivationLocalChunkX));
    PushStatusField(L, "lastSwapActivationLocalChunkY", static_cast<double>(state.lastSwapActivationLocalChunkY));
    PushStatusField(L, "lastSwapActivationWorldChunkX", static_cast<double>(state.lastSwapActivationWorldChunkX));
    PushStatusField(L, "lastSwapActivationWorldChunkY", static_cast<double>(state.lastSwapActivationWorldChunkY));
    int32_t swapAdtX = state.lastSwapActivationAdtTileX;
    int32_t swapAdtY = state.lastSwapActivationAdtTileY;
    int32_t swapCellX = 0;
    int32_t swapCellY = 0;
    if (swapAdtX < 0 || swapAdtY < 0)
        ClientChunkToAdtTile(state.lastSwapActivationWorldChunkX, state.lastSwapActivationWorldChunkY, swapAdtX, swapAdtY);
    AdtTileToServerCell(swapAdtX, swapAdtY, swapCellX, swapCellY);
    PushStatusField(L, "lastSwapActivationAdtX", static_cast<double>(swapAdtX));
    PushStatusField(L, "lastSwapActivationAdtY", static_cast<double>(swapAdtY));
    PushStatusField(L, "lastSwapActivationCellX", static_cast<double>(swapCellX));
    PushStatusField(L, "lastSwapActivationCellY", static_cast<double>(swapCellY));
    PushStatusField(L, "materialLookup", state.config.materialLookup);
    PushStatusField(L, "materialLookupAttempts", static_cast<double>(state.materialLookupAttempts));
    PushStatusField(L, "materialLookupHits", static_cast<double>(state.materialLookupHits));
    PushStatusField(L, "materialLookupFallbacks", static_cast<double>(state.materialLookupFallbacks));
    PushStatusField(L, "payloadValidationOk", static_cast<double>(state.payloadValidationOk));
    PushStatusField(L, "payloadValidationFailed", static_cast<double>(state.payloadValidationFailed));
    PushStatusField(L, "autoQueueCache", state.config.autoQueueCache);
    PushStatusField(L, "autoQueueLookupsPerFrame", static_cast<double>(state.config.autoQueueLookupsPerFrame));
    PushStatusField(L, "autoQueuePopsPerFrame", static_cast<double>(state.config.autoQueuePopsPerFrame));
    PushStatusField(L, "autoQueueFrameCount", static_cast<double>(state.autoQueueFrameCount));
    PushStatusField(L, "autoQueueDeferred", static_cast<double>(state.autoQueueDeferred));
    PushStatusField(L, "observerNewChunksPerFrame", static_cast<double>(state.config.observerNewChunksPerFrame));
    PushStatusField(L, "observerRefreshMs", static_cast<double>(state.config.observerRefreshMs));
    PushStatusField(L, "observerFrameNewChunks", static_cast<double>(state.observerFrameNewChunks));
    PushStatusField(L, "observerThrottled", static_cast<double>(state.observerThrottled));
    PushStatusField(L, "observerRefreshSkips", static_cast<double>(state.observerRefreshSkips));
    PushStatusField(L, "autoCacheLookups", static_cast<double>(state.autoCacheLookups));
    PushStatusField(L, "autoCacheHits", static_cast<double>(state.autoCacheHits));
    PushStatusField(L, "autoCacheMisses", static_cast<double>(state.autoCacheMisses));
    PushStatusField(L, "autoCacheSidecarMisses", static_cast<double>(state.autoCacheSidecarMisses));
    PushStatusField(L, "autoCacheInvalidIndexMisses", static_cast<double>(state.autoCacheInvalidIndexMisses));
    PushStatusField(L, "autoCacheHashMisses", static_cast<double>(state.autoCacheHashMisses));
    PushStatusField(L, "autoCachePayloadMisses", static_cast<double>(state.autoCachePayloadMisses));
    PushStatusField(L, "lastAutoCacheMissReason", state.lastAutoCacheMissReason);
    PushStatusField(L, "lastAutoCacheMissDetail", state.lastAutoCacheMissDetail);
    PushStatusField(L, "lastAutoCacheChunkIndex", static_cast<double>(state.lastAutoCacheChunkIndex));
    PushStatusField(L, "liveTextureBytes", static_cast<double>(state.liveTextureBytes));
    PushStatusField(L, "liveMaterialBytes", static_cast<double>(state.liveMaterialBytes));
    PushStatusField(L, "liveBytes", static_cast<double>(LiveBytes(state)));
    PushStatusField(L, "pendingPayloadBytes", static_cast<double>(state.pendingPayloadBytes));
    PushStatusField(L, "memoryBudgetBytes", static_cast<double>(MemoryBudgetBytes(state)));
    PushStatusField(L, "memoryEvictions", static_cast<double>(state.memoryEvictions));
    PushStatusField(L, "budgetFallbacks", static_cast<double>(state.budgetFallbacks));
    PushStatusField(L, "fallbackCount", static_cast<double>(state.fallbackCount));
    PushStatusField(L, "lastFallbackReason", state.lastFallbackReason);
    PushStatusField(L, "lastFallbackDetail", state.lastFallbackDetail);
    PushStatusField(L, "lifecycleCount", static_cast<double>(state.lifecycleCount));
    PushStatusField(L, "lastLifecycleStage", state.lastLifecycleStage);
    PushStatusField(L, "lastLifecycleDetail", state.lastLifecycleDetail);
    PushStatusField(L, "focusWorldChunkX", static_cast<double>(state.focusWorldChunkX));
    PushStatusField(L, "focusWorldChunkY", static_cast<double>(state.focusWorldChunkY));
    PushStatusField(L, "lastObservedLocalChunkX", static_cast<double>(state.lastObservedLocalChunkX));
    PushStatusField(L, "lastObservedLocalChunkY", static_cast<double>(state.lastObservedLocalChunkY));
    PushStatusField(L, "lastObservedWorldChunkX", static_cast<double>(state.lastObservedWorldChunkX));
    PushStatusField(L, "lastObservedWorldChunkY", static_cast<double>(state.lastObservedWorldChunkY));
    PushStatusField(L, "lastObservedChunkIndex", static_cast<double>(state.lastObservedChunkIndex));
    PushStatusField(L, "lastObservedTextureSetPtr", static_cast<double>(state.lastObservedTextureSetPtr));
    PushStatusField(L, "lastObservedNativeChunkPtr", static_cast<double>(state.lastObservedNativeChunkPtr));
    PushStatusField(L, "lastObservedOwnerGeneration", static_cast<double>(state.lastObservedOwnerGeneration));
    bool lastObservedTileLoadIdentity = false;
    if (ChunkRequest* observedRequest = FindRequest(state, state.lastObservedTextureSetPtr))
        lastObservedTileLoadIdentity = observedRequest->tileLoadIdentity;
    PushStatusField(L, "lastObservedTileLoadIdentity", lastObservedTileLoadIdentity);
    int32_t observedAdtX = -1;
    int32_t observedAdtY = -1;
    int32_t observedCellX = 0;
    int32_t observedCellY = 0;
    if (ChunkRequest* request = FindRequest(state, state.lastDrawChunkPtr))
    {
        observedAdtX = request->adtTileX;
        observedAdtY = request->adtTileY;
    }
    if (observedAdtX < 0 || observedAdtY < 0)
        ClientChunkToAdtTile(state.lastObservedWorldChunkX, state.lastObservedWorldChunkY, observedAdtX, observedAdtY);
    AdtTileToServerCell(observedAdtX, observedAdtY, observedCellX, observedCellY);
    PushStatusField(L, "lastObservedAdtX", static_cast<double>(observedAdtX));
    PushStatusField(L, "lastObservedAdtY", static_cast<double>(observedAdtY));
    PushStatusField(L, "lastObservedCellX", static_cast<double>(observedCellX));
    PushStatusField(L, "lastObservedCellY", static_cast<double>(observedCellY));
    PushStatusField(L, "stackTileX", static_cast<double>(state.cachedStackTileX));
    PushStatusField(L, "stackTileY", static_cast<double>(state.cachedStackTileY));
    PushStatusField(L, "stackTileValid", state.cachedStackTileValid);
    PushStatusField(L, "forceTestBlp", state.config.forceTestBlp);
    bool const focusedCoordsValid = state.hasFocusChunk &&
                                    IsValidGlobalChunkPair(state.lastObservedWorldChunkX, state.lastObservedWorldChunkY) &&
                                    IsLocalChunkCoord(state.lastObservedLocalChunkX) &&
                                    IsLocalChunkCoord(state.lastObservedLocalChunkY);
    PushStatusField(L, "lastObservedCoordsValid", focusedCoordsValid);
    PushStatusField(L, "hasFocusChunk", state.hasFocusChunk);
    PushStatusField(L, "observations", static_cast<double>(state.observations));
    PushStatusField(L, "queueDrops", static_cast<double>(state.queueDrops));
    PushStatusField(L, "staleDrops", static_cast<double>(state.staleDrops));
    PushStatusField(L, "fallbackFrames", static_cast<double>(state.fallbackFrames));
    PushStatusField(L, "helperFailures", static_cast<double>(state.helperFailures));
    PushStatusField(L, "helperSuccesses", static_cast<double>(state.helperSuccesses));
    PushStatusField(L, "compositeReadCallbackCalls", static_cast<double>(state.compositeReadCallbackCalls));
    PushStatusField(L, "compositeReadOverrides", static_cast<double>(state.compositeReadOverrides));
    PushStatusField(L, "compositeReadSkipNoPayload", static_cast<double>(state.compositeReadSkipNoPayload));
    PushStatusField(L, "compositeReadSkipGate", static_cast<double>(state.compositeReadSkipGate));
    PushStatusField(L, "gpuCompositeCallbacks", static_cast<double>(state.gpuCompositeCallbacks));
    PushStatusField(L, "gpuCompositeAttempts", static_cast<double>(state.gpuCompositeAttempts));
    PushStatusField(L, "gpuCompositeSuccesses", static_cast<double>(state.gpuCompositeSuccesses));
    PushStatusField(L, "gpuCompositeFailures", static_cast<double>(state.gpuCompositeFailures));
    PushStatusField(L, "gpuCompositeNoDevice", static_cast<double>(state.gpuCompositeNoDevice));
    PushStatusField(L, "gpuCompositeBuildMsTotal", static_cast<double>(state.gpuCompositeBuildMsTotal));
    PushStatusField(L, "gpuOverlayAttempts", static_cast<double>(state.gpuOverlayAttempts));
    PushStatusField(L, "gpuOverlayApplied", static_cast<double>(state.gpuOverlayApplied));
    PushStatusField(L, "gpuOverlaySkipNoPayload", static_cast<double>(state.gpuOverlaySkipNoPayload));
    PushStatusField(L, "gpuOverlaySkipNoCache", static_cast<double>(state.gpuOverlaySkipNoCache));
    PushStatusField(L, "gpuOverlayFallbackApplied", static_cast<double>(state.gpuOverlayFallbackApplied));
    PushStatusField(L, "lastOwnerWorldChunkX", static_cast<double>(state.lastOwnerWorldChunkX));
    PushStatusField(L, "lastOwnerWorldChunkY", static_cast<double>(state.lastOwnerWorldChunkY));
    PushStatusField(L, "lastOwnerAdtX", static_cast<double>(state.lastOwnerAdtX));
    PushStatusField(L, "lastOwnerAdtY", static_cast<double>(state.lastOwnerAdtY));
    PushStatusField(L, "lastOwnerChunkIndex", static_cast<double>(state.lastOwnerChunkIndex));
    PushStatusField(L, "lastOwnerHadResidentPayload", state.lastOwnerHadResidentPayload);
    PushStatusField(L, "lastGpuCompositeEvent", static_cast<double>(state.lastGpuCompositeEvent));
    PushStatusField(L, "lastGpuCompositeLayerCount", static_cast<double>(state.lastGpuCompositeLayerCount));
    PushStatusField(L, "lastGpuCompositeTextureSet", static_cast<double>(state.lastGpuCompositeTextureSet));
    PushStatusField(L, "lastGpuNativeDiffuse0", static_cast<double>(state.lastGpuNativeDiffuseHandles[0]));
    PushStatusField(L, "lastGpuNativeDiffuse1", static_cast<double>(state.lastGpuNativeDiffuseHandles[1]));
    PushStatusField(L, "lastGpuNativeDiffuse2", static_cast<double>(state.lastGpuNativeDiffuseHandles[2]));
    PushStatusField(L, "lastGpuNativeDiffuse3", static_cast<double>(state.lastGpuNativeDiffuseHandles[3]));
    PushStatusField(L, "lastGpuNativeComposite", static_cast<double>(state.lastGpuNativeCompositeHandle));
    PushStatusField(L, "gpuCompositeSeed", static_cast<double>(state.gpuCompositeSeed));
    PushStatusField(L, "lastGpuSlotCountCaptured", static_cast<double>(state.lastGpuSlotCountCaptured));
    PushStatusField(L, "lastGpuSlotHandleHash", static_cast<double>(state.lastGpuSlotHandleHash));
    PushStatusField(L, "maxGpuSlotCountCaptured", static_cast<double>(state.maxGpuSlotCountCaptured));
    PushStatusField(L, "maxGpuSlotAdtX", static_cast<double>(state.maxGpuSlotAdtX));
    PushStatusField(L, "maxGpuSlotAdtY", static_cast<double>(state.maxGpuSlotAdtY));
    PushStatusField(L, "maxGpuSlotWorldChunkX", static_cast<double>(state.maxGpuSlotWorldChunkX));
    PushStatusField(L, "maxGpuSlotWorldChunkY", static_cast<double>(state.maxGpuSlotWorldChunkY));
    PushStatusField(L, "disabledReason", state.disabledReason);
    return 1;
}
