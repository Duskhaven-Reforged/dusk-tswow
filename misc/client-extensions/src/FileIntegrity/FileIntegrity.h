#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace FileIntegrity
{
    struct FileInfo
    {
        uint64_t size = 0;
        std::string hashHex;
    };

    std::unordered_map<std::string, FileInfo> const& GetLoadedMpqEntries();
    void RunStartupScan();

} // namespace FileIntegrity
