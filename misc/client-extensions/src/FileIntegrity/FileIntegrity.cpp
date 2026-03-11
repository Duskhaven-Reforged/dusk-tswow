#include "FileIntegrity.h"
#include <Logger.h>

#include <Windows.h>
#include <cstdint>
#include <cstring>
#include <mutex>

namespace FileIntegrity {

static std::vector<MpqEntry> s_entries;
static std::mutex s_mutex;

constexpr uint64_t FNV_OFFSET = 14695981039346656037ULL;
constexpr uint64_t FNV_PRIME  = 1099511628211ULL;

static std::string ToHex(uint64_t v) {
    char buf[17];
    snprintf(buf, sizeof(buf), "%016llX", (unsigned long long)v);
    return std::string(buf);
}

// One path: exe directory. Call once per scan.
static std::string GetDataRoot() {
    char path[MAX_PATH];
    if (GetModuleFileNameA(NULL, path, MAX_PATH) == 0) return "";
    for (int i = (int)strlen(path); i > 0; --i) {
        if (path[i - 1] == '\\' || path[i - 1] == '/') {
            path[i] = '\0';
            return std::string(path) + "Data\\";
        }
    }
    return "";
}

// Metadata: size from find data. Hash: single read of first 64k + size mixed in.
static std::string HashFileHead(const std::string& path, uint64_t size) {
    HANDLE h = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return "";

    uint64_t hash = FNV_OFFSET;
    uint8_t buf[65536];
    DWORD nr = 0;
    if (ReadFile(h, buf, sizeof(buf), &nr, NULL) && nr > 0) {
        for (DWORD i = 0; i < nr; ++i) {
            hash ^= (uint64_t)buf[i];
            hash *= FNV_PRIME;
        }
    }
    for (int i = 0; i < 8; ++i) {
        hash ^= (size >> (i * 8)) & 0xff;
        hash *= FNV_PRIME;
    }
    CloseHandle(h);
    return ToHex(hash);
}

// Folders under Data to scan (empty = Data root).
static const char* const s_dataSubdirs[] = { "", "enUS", "enGB" };

static void ScanOneDir(const std::string& dataRoot, const char* subdir) {
    std::string dir = dataRoot;
    if (subdir[0]) { dir += subdir; dir += '\\'; }

    WIN32_FIND_DATAA fd = {};
    HANDLE h = FindFirstFileA((dir + "*.mpq").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;

    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        const char* name = fd.cFileName;
        size_t nlen = strlen(name);
        if (nlen < 4 || _strnicmp(name + nlen - 4, ".mpq", 4) != 0) continue;

        std::string entryName = subdir[0] ? std::string(subdir) + "\\" + name : name;
        ULARGE_INTEGER uli;
        uli.HighPart = fd.nFileSizeHigh;
        uli.LowPart = fd.nFileSizeLow;
        uint64_t sz = uli.QuadPart;
        std::string fullPath = dir + name;
        std::string hashHex = HashFileHead(fullPath, sz);

        std::lock_guard<std::mutex> lock(s_mutex);
        s_entries.push_back({ entryName, sz, std::move(hashHex) });
        const auto& e = s_entries.back();
        LOG_INFO << "FileIntegrity: " << e.filename << " size=" << e.size << (e.hashHex.empty() ? "" : " hash=") << e.hashHex;
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

void RunStartupScan() {
    std::string dataRoot = GetDataRoot();
    if (dataRoot.empty()) return;
    for (const char* subdir : s_dataSubdirs)
        ScanOneDir(dataRoot, subdir);
}

std::vector<MpqEntry> const& GetLoadedMpqEntries() {
    return s_entries;
}

}
