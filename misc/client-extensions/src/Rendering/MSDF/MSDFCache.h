#pragma once
#include "MSDFCacheTypes.h"
#include "MSDFUtils.h"
#include "ankerl/unordered_dense.h"
#include <filesystem>
#include <deque>

class MSDFManager;
class MSDFPregen;
class MSDFFont;

class MSDFCache {
    friend class MSDFFont;
    friend class MSDFPregen;
    friend class MSDFManager;

public:
    MSDFCache(const FT_Byte* fontData, FT_Long dataSize,
        const char* familyName, const char* styleName,
        uint32_t sdfRenderSize, uint32_t sdfSpread);
    ~MSDFCache();

    MSDFCache(const MSDFCache&) = delete;
    MSDFCache& operator=(const MSDFCache&) = delete;
    MSDFCache(MSDFCache&&) = delete;
    MSDFCache& operator=(MSDFCache&&) = delete;

private:
    static constexpr auto* CACHE_DIR = "FontCache";
    static constexpr size_t WRITE_BATCH_SIZE = 64;
    static constexpr size_t MAX_SAFE_ALLOCATION = 32 * 1024 * 1024;


    bool TryLoadGlyph(uint32_t codepoint, GlyphMetrics& outMetrics);
    bool StoreGlyph(GlyphMetricsToStore&& metrics);
    size_t GetManifestSize();

    using ManifestMap = ankerl::unordered_dense::map<uint32_t, ManifestEntry>;

    bool LoadManifest();
    bool SaveManifest(bool isLocked = false);
    bool LoadManifestFromFile(const std::filesystem::path& path, ManifestMap& outMap) const;
    static bool LoadManifestJournal(const std::filesystem::path& journalPath, ManifestMap& outMap, size_t& outEntriesApplied);
    bool AppendManifestJournal(const std::vector<ManifestEntry>& entries);

    void BuildBlockLockPath(uint32_t blockId, std::filesystem::path& outPath) const;
    void BuildBlockPath(uint32_t blockId, std::filesystem::path& outPath) const;

    bool FlushPendingWrites();
    bool WriteBlockFile(uint32_t blockId, std::vector<GlyphMetricsToStore*>& pending, std::vector<ManifestEntry>& outEntries);
    void CleanupOrphans() const;

    static uint32_t GetBlockId(uint32_t codepoint);
    static std::string GetCacheBasePath(const char* familyName, const char* styleName,
        uint32_t sdfRenderSize, uint32_t sdfSpread);
    static std::string SanitizeName(std::string_view name);

    std::filesystem::path m_cacheBasePath;
    std::filesystem::path m_cacheManifestPath;
    std::filesystem::path m_cacheManifestLockPath;
    std::filesystem::path m_cacheManifestJournalPath;

    CacheKey m_key;
    ManifestMap m_manifest;

    bool m_manifestLoaded = false;
    uint32_t m_fontID = 0xFFFFFFFF;

    std::deque<GlyphMetricsToStore> m_pendingWrites;
    
    ankerl::unordered_dense::map<uint32_t, BlockWrap> m_blockWrap;

    static MSDFManager s_manager;
};