#pragma once

#ifdef _WIN32
#include <windows.h>
#endif

#include <mutex>
#include <string>

#include "SharedMemoryRingBuffer.h"
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
    bool WritePacketLocked(const PacketBuilder& packet);

    Duskhaven::IPC::SharedMemoryRingBuffer channel_;
#endif

    std::wstring pipeName_;
    std::mutex mutex_;
};
