#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace FileIntegrity {

struct MpqEntry {
    std::string filename;
    uint64_t size = 0;   // file size in bytes (no full read)
    std::string hashHex; // optional: lightweight hash (e.g. first+last bytes); empty if not computed
};

std::vector<MpqEntry> const& GetLoadedMpqEntries();
void RunStartupScan();

}
