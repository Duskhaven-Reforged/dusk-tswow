#pragma once

#include <cstdint>
#include <string>

struct ExtendedTerrainTextureSettings
{
    bool enabled = true;
    bool trace = false;
    bool telemetry = true;
    bool nativeUpload = true;
    bool renderSwap = true;
    bool drawReplacement = true;
    bool autoQueueCache = true;
    bool materialLookup = true;
    bool nativeCompositeGpu = true;
    bool debugCheckerEnabled = false;
    bool debugCheckerStrict = false;
    bool forceTestBlp = false;

    int32_t maxLayers = 16;
    int32_t quickSize = 128;
    int32_t finalSize = 512;
    int32_t prefetchRadius = 1;
    int32_t memoryBudgetMB = 96;
    int32_t uploadBudgetKBPerFrame = 512;
    int32_t uploadBudgetCountPerFrame = 2;
    int32_t uploadBudgetMsPerFrame = 2;
    int32_t autoQueueLookupsPerFrame = 2;
    int32_t autoQueuePopsPerFrame = 64;
    int32_t observerNewChunksPerFrame = 8;
    int32_t observerRefreshMs = 250;
    int32_t maxPendingRequests = 4096;
    int32_t staleRequestMs = 3000;
    int32_t circuitBreakerFailures = 5;
    int32_t debugCheckerAlphaPct = 25;

    std::string helperPath = "extended-adt-texture-helper.cmd";
    std::string cacheRoot = R"(D:\.duskhaven\tmp\extended-terrain-cache-validated)";
    std::string assetRoot = R"(D:\.duskhaven\release\package\.adt-audit\client\common)";
    std::string stackPath = R"(D:\.duskhaven\release\modules\dh-core\assets\World\maps\azeroth\azeroth_32_48.exttex.json)";
    std::string quality = "final";
};

inline ExtendedTerrainTextureSettings DefaultExtendedTerrainTextureSettings()
{
    ExtendedTerrainTextureSettings settings;
    return settings;
}

class ExtendedTerrainTextures
{
  public:
    static void Apply();
    static void ObserveChunkTileLoad(void* tile, int32_t localChunkX, int32_t localChunkY);
    static void ObserveChunkIdentity(void* chunk);
    static void ObserveChunkForScheduling(void* chunk);
    static void RecordHelperResult(bool ok, char const* operation, char const* detail);
    static void RecordPayloadValidation(bool ok, char const* detail);
    static void RecordPayloadBytes(void* chunk, int32_t textureBytes, int32_t materialBytes, bool resident);
    static bool SampleMaterial(void* chunk, float u, float v, int32_t& layerIndex, int32_t& materialId);
    static void QueuePayloadForUpload(void* chunk, char const* texturePath, char const* manifestPath,
                                      char const* quality, int32_t textureBytes, int32_t materialBytes);
    static void PrepareTextureSetForNativeUpdate(void* textureSet);
    static void AttachReadyTextureSet(void* textureSet);
    static void AttachReadyVisibleTextureSets();
    static void PumpUploadBudget();
    static void InstallTerrainDrawOverride();
    static void ReleaseChunkResources(void* chunk, char const* reason);
    static void OnDeviceLost();
    static void OnDeviceRestored();
    static void RecordFallback(char const* reason, char const* detail);
    static void RecordLifecycle(char const* stage, char const* detail);
    static void SetRuntimeEnabled(bool enabled, char const* reason);
    static bool SetRuntimeOption(char const* option, bool enabled, char const* reason);
    static bool SetRuntimeIntOption(char const* option, int32_t value, char const* reason);
    static void ResetCircuitBreaker();
};
