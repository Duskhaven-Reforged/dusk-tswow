#pragma once

#ifdef _WIN32
  #include <windows.h>
#endif

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>
#include "SharedMemoryRingBuffer.h"
#include "Logger.h"

class IPCClient {
public:
    IPCClient() = default;
    ~IPCClient() { Disconnect(); }
    bool Connect(const std::wstring& pipeName);
    void Disconnect();

    bool IsConnected() const;
    bool Send(const std::vector<uint8_t>& payload = {});
    bool Send(const Duskhaven::IPC::SharedRingSegment* segments, uint32_t segmentCount);

private:
#ifdef _WIN32
    Duskhaven::IPC::SharedMemoryRingBuffer channel_;
#endif

    mutable std::mutex mutex_;
};
