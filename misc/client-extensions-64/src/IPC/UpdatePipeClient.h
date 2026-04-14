#pragma once

#ifdef _WIN32
#include <windows.h>
#endif

#include <mutex>
#include <string>

#include "PacketBuilder.h"

class UpdatePipeClient
{
  public:
    explicit UpdatePipeClient(std::wstring pipeName = L"");

    void SetPipeName(std::wstring pipeName);
    void Disconnect();
    bool Send(const PacketBuilder& packet);

  private:
#ifdef _WIN32
    bool ConnectLocked();
    void DisconnectLocked();
    bool WriteExactLocked(const void* src, uint32_t bytes);

    HANDLE pipe_ = INVALID_HANDLE_VALUE;
#endif

    std::wstring pipeName_;
    std::mutex mutex_;
};
