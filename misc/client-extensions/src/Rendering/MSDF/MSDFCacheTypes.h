#pragma once
#include <cstdint>
#include <filesystem>
#include <ft2build.h>
#include FT_FREETYPE_H
#include "ankerl/unordered_dense.h"

namespace MSDF {
    inline constexpr size_t BLOCK_SIZE = 512;
    inline constexpr uint32_t BLOCK_MAGIC = 0x4D534442;
    inline constexpr uint32_t MANIFEST_MAGIC = 0x4D534D46;
    inline constexpr uint32_t CACHE_VERSION = 1;
}

struct BlockKey {
    uint32_t fontId;
    uint32_t blockId;

    BlockKey() : fontId(0xFFFFFFFF), blockId(0xFFFFFFFF) {}
    BlockKey(uint32_t font, uint32_t block) : fontId(font), blockId(block) {}

    bool operator==(const BlockKey& other) const {
        return fontId == other.fontId && blockId == other.blockId;
    }
    uint64_t pack() const { return (static_cast<uint64_t>(fontId) << 32) | blockId; }
};

struct BlockWrap {
    BlockKey key;
    std::filesystem::path path;
};

struct CacheKey {
    uint32_t sdfRenderSize = 0;
    uint32_t sdfSpread = 0;
    bool operator==(const CacheKey& other) const {
        return sdfRenderSize == other.sdfRenderSize &&
            sdfSpread == other.sdfSpread;
    }
};

#pragma pack(push, 1)
struct ManifestHeader {
    uint32_t magic;
    uint32_t version;
    CacheKey key;
    uint32_t entryCount;
    uint32_t pad;
};

struct ManifestEntry {
    uint32_t codepoint;
    uint32_t blockId;
};

struct alignas(64) BlockFileHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t blockId;
    uint32_t entryCount;
};

struct alignas(64) GlyphEntry {
    uint32_t codepoint;
    uint16_t width;
    uint16_t height;
    FT_Int bitmapTop;
    FT_Int bitmapLeft;
    uint32_t dataOffset;
    uint32_t dataSize;

    bool operator<(const GlyphEntry& other) const {
        return codepoint < other.codepoint;
    }
};
#pragma pack(pop)

static_assert(sizeof(ManifestHeader) == 24);
static_assert(sizeof(ManifestEntry) == 8);
static_assert(sizeof(BlockFileHeader) == 64);
static_assert(sizeof(GlyphEntry) == 64);

#include <vector>

struct GlyphMetrics {
    uint16_t width = 0;
    uint16_t height = 0;
    FT_Int bitmapTop = 0;
    FT_Int bitmapLeft = 0;
    float u0 = 0.0f, v0 = 0.0f, u1 = 0.0f, v1 = 0.0f;
    uint16_t atlasPageIndex = 0;
    const uint8_t* pixelData = nullptr;
};

struct GlyphMetricsToStore {
    uint32_t codepoint = 0;
    uint16_t width = 0;
    uint16_t height = 0;
    FT_Int bitmapTop = 0;
    FT_Int bitmapLeft = 0;
    std::vector<uint8_t> ownedPixelData;
    uint32_t dataSize = 0;
};

template<>
struct std::hash<BlockKey> {
    size_t operator()(const BlockKey& k) const noexcept {
        uint64_t packed = k.pack();
        return ankerl::unordered_dense::detail::wyhash::hash(&packed, sizeof(packed));
    }
};

template<>
struct std::hash<CacheKey> {
    size_t operator()(const CacheKey& k) const noexcept {
        uint64_t h = ankerl::unordered_dense::detail::wyhash::hash(&k.sdfRenderSize, sizeof(uint32_t));
        uint64_t h2 = ankerl::unordered_dense::detail::wyhash::hash(&k.sdfSpread, sizeof(uint32_t));
        return h ^ (h2 + 0x9e3779b9 + (h << 6) + (h >> 2));
    }
};
