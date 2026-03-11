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

static std::string GetExeDirectory() {
    char path[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, path, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return "";
    for (DWORD i = len; i > 0; --i) {
        if (path[i - 1] == '\\' || path[i - 1] == '/') {
            path[i] = '\0';
            return std::string(path);
        }
    }
    return "";
}

static std::string LightweightHashFile(const std::string& fullPath, uint64_t fileSize) {
    const size_t chunk = 65536;
    HANDLE h = CreateFileA(fullPath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return "";

    uint64_t hash = FNV_OFFSET;
    uint8_t buf[chunk];
    DWORD read = 0;

    auto mix = [&hash](const uint8_t* p, size_t n) {
        for (size_t i = 0; i < n; ++i) {
            hash ^= static_cast<uint64_t>(p[i]);
            hash *= FNV_PRIME;
        }
    };

    if (ReadFile(h, buf, (DWORD)chunk, &read, NULL) && read > 0)
        mix(buf, read);

    if (fileSize > chunk) {
        LARGE_INTEGER off;
        off.QuadPart = static_cast<LONGLONG>(fileSize - (read = 0));
        if (SetFilePointerEx(h, off, NULL, FILE_BEGIN) && ReadFile(h, buf, (DWORD)chunk, &read, NULL) && read > 0)
            mix(buf, read);
    }

    for (int i = 0; i < 8; ++i) {
        hash ^= static_cast<uint64_t>((fileSize >> (i * 8)) & 0xffu);
        hash *= FNV_PRIME;
    }

    CloseHandle(h);
    return ToHex(hash);
}

static bool AlreadyRecorded(const std::string& key) {
    for (const auto& e : s_entries)
        if (e.filename == key) return true;
    return false;
}

static void ScanDirForMpqs(const std::string& fullDir, const std::string& relativePrefix) {
    WIN32_FIND_DATAA fd = {};
    std::string pattern = fullDir + "*.mpq";
    HANDLE hFind = FindFirstFileA(pattern.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) continue;
        const char* name = fd.cFileName;
        size_t len = strlen(name);
        if (len < 4 || _strnicmp(name + len - 4, ".mpq", 4) != 0) continue;

        std::string entryName = relativePrefix.empty() ? std::string(name) : relativePrefix + name;
        std::lock_guard<std::mutex> lock(s_mutex);
        if (AlreadyRecorded(entryName)) continue;

        ULARGE_INTEGER uli;
        uli.HighPart = fd.nFileSizeHigh;
        uli.LowPart = fd.nFileSizeLow;
        uint64_t sz = uli.QuadPart;
        std::string fullPath = fullDir + name;
        std::string hashHex = LightweightHashFile(fullPath, sz);
        s_entries.push_back({ entryName, sz, std::move(hashHex) });
        auto const& e = s_entries.back();
        LOG_INFO << "FileIntegrity: " << e.filename << " size=" << e.size << (e.hashHex.empty() ? "" : " hash=") << e.hashHex;
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);
}

static void ScanDataDirectoryRecursive(const std::string& fullDir, const std::string& relativePrefix) {
    ScanDirForMpqs(fullDir, relativePrefix);

    WIN32_FIND_DATAA fd = {};
    HANDLE hFind = FindFirstFileA((fullDir + "*").c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) continue;
        const char* name = fd.cFileName;
        if (name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'))) continue;

        std::string subFull = fullDir + name + "\\";
        std::string subRel = relativePrefix.empty() ? std::string(name) + "\\" : relativePrefix + name + "\\";
        ScanDataDirectoryRecursive(subFull, subRel);
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);
}

static void ScanDataDirectory() {
    std::string dataDir = GetExeDirectory();
    if (dataDir.empty()) return;
    if (dataDir.back() != '\\' && dataDir.back() != '/') dataDir += '\\';
    dataDir += "Data\\";
    ScanDataDirectoryRecursive(dataDir, "");
}

void RunStartupScan() {
    ScanDataDirectory();
}

std::vector<MpqEntry> const& GetLoadedMpqEntries() {
    return s_entries;
}

}
