#include "FileIntegrity.h"
#include <ClientLua.h>
#include <Logger.h>
#include <Windows.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace FileIntegrity
{
    static std::unordered_map<std::string, FileInfo> s_entries;

    constexpr uint64_t FNV_OFFSET = 14695981039346656037ULL;
    constexpr uint64_t FNV_PRIME  = 1099511628211ULL;

    static std::string ToHex(uint64_t v)
    {
        char buf[17];
        snprintf(buf, sizeof(buf), "%016llX", (unsigned long long)v);
        return std::string(buf);
    }

    static std::string GetDataRoot()
    {
        char path[MAX_PATH];
        DWORD len = GetModuleFileNameA(nullptr, path, MAX_PATH);
        if (len == 0 || len >= MAX_PATH)
            return "";

        for (int i = static_cast<int>(len); i > 0; --i)
        {
            if (path[i - 1] == '\\' || path[i - 1] == '/')
            {
                path[i] = '\0';
                return std::string(path) + "Data\\";
            }
        }
        return "";
    }

    static bool EndsWithMpq(const char* name)
    {
        size_t nlen = strlen(name);
        return nlen >= 4 && _stricmp(name + nlen - 4, ".mpq") == 0;
    }

    static std::string HashFileFirstLastSize(const std::string& path, uint64_t size)
    {
        HANDLE h = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);

        if (h == INVALID_HANDLE_VALUE)
            return "";

        constexpr DWORD CHUNK_SIZE = 64 * 1024;
        uint8_t buf[CHUNK_SIZE];
        uint64_t hash = FNV_OFFSET;

        auto MixBytes = [&](const uint8_t* data, DWORD count)
        {
            for (DWORD i = 0; i < count; ++i)
            {
                hash ^= static_cast<uint64_t>(data[i]);
                hash *= FNV_PRIME;
            }
        };

        auto MixU64 = [&](uint64_t v)
        {
            for (int i = 0; i < 8; ++i)
            {
                hash ^= (v >> (i * 8)) & 0xFF;
                hash *= FNV_PRIME;
            }
        };

        DWORD nr = 0;

        // Read first chunk
        if (ReadFile(h, buf, CHUNK_SIZE, &nr, nullptr) && nr > 0)
            MixBytes(buf, nr);

        // Read last chunk only if file is larger than one chunk
        if (size > CHUNK_SIZE)
        {
            LARGE_INTEGER pos;
            pos.QuadPart = (size > CHUNK_SIZE) ? static_cast<LONGLONG>(size - CHUNK_SIZE) : 0;

            if (SetFilePointerEx(h, pos, nullptr, FILE_BEGIN))
            {
                nr = 0;
                if (ReadFile(h, buf, CHUNK_SIZE, &nr, nullptr) && nr > 0)
                    MixBytes(buf, nr);
            }
        }

        // Mix file size last
        MixU64(size);

        CloseHandle(h);
        return ToHex(hash);
    }

    static void ScanDir(const std::string& root, const std::string& rel = "")
    {
        const std::string dir = root + rel;
        WIN32_FIND_DATAA fd   = {};
        HANDLE h              = FindFirstFileA((dir + "*").c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE)
            return;

        do
        {
            const char* name = fd.cFileName;

            if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
                continue;

            const bool isDir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

            if (isDir || !EndsWithMpq(name)) // Skip Directories
                continue;

            ULARGE_INTEGER uli;
            uli.HighPart        = fd.nFileSizeHigh;
            uli.LowPart         = fd.nFileSizeLow;
            const uint64_t size = uli.QuadPart;

            const std::string relativeName = rel + name;
            const std::string fullPath     = dir + name;
            const std::string hashHex      = HashFileFirstLastSize(fullPath, size);

            s_entries[relativeName] = FileInfo{size, hashHex};

            const auto& e = s_entries[relativeName];
            LOG_INFO << "FileIntegrity: " << relativeName << " size=" << e.size << (e.hashHex.empty() ? "" : " hash=")
                     << e.hashHex;
        } while (FindNextFileA(h, &fd));

        FindClose(h);
    }

    void RunStartupScan()
    {
        const std::string dataRoot = GetDataRoot();
        if (dataRoot.empty())
            return;

        s_entries.clear();
        ScanDir(dataRoot);
    }

    const std::unordered_map<std::string, FileInfo>& GetLoadedMpqEntries()
    {
        return s_entries;
    }
} // namespace FileIntegrity
    LUA_FUNCTION(GetMPQHashResults, (lua_State * L))
    {
        const std::unordered_map<std::string, FileIntegrity::FileInfo>& fileInfo = FileIntegrity::GetLoadedMpqEntries();
        std::string result;
        bool first = true;

        for (const auto& pair : fileInfo)
        {
            const std::string& key = pair.first;
            const FileIntegrity::FileInfo& info   = pair.second;

            if (first)
                first = false;
            else
                result.push_back(',');

            result += key;
            result.push_back('|');
            result += std::to_string(info.size);
            result.push_back('|');
            result += info.hashHex;
        }

        ClientLua::PushString(L, result.c_str());
        return 1;
    }