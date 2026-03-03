#pragma once

#ifdef _WIN32
  #include <windows.h>
#endif

#include <cstdint>
#include <string>
#include <vector>
#include "Logger.h"

class IPCClient {
public:
    IPCClient() = default;
    ~IPCClient() { Disconnect(); }
    bool Connect(const std::wstring& pipeName);
    void Disconnect();

    bool IsConnected() const;
    bool Send(const std::vector<uint8_t>& payload = {});

private:
#ifdef _WIN32
    bool WriteExact(const void* src, uint32_t bytes);

    HANDLE pipe_ = INVALID_HANDLE_VALUE;
#endif
};
