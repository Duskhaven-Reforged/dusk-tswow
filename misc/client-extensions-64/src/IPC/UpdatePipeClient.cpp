#include "UpdatePipeClient.h"

#include <utility>
#include <vector>

UpdatePipeClient::UpdatePipeClient(std::wstring pipeName)
    : pipeName_(std::move(pipeName))
{
}

void UpdatePipeClient::SetPipeName(std::wstring pipeName)
{
    std::lock_guard<std::mutex> lock(mutex_);
    pipeName_ = std::move(pipeName);
    DisconnectLocked();
}

void UpdatePipeClient::Disconnect()
{
    std::lock_guard<std::mutex> lock(mutex_);
    DisconnectLocked();
}

bool UpdatePipeClient::Send(const PacketBuilder& packet)
{
#ifdef _WIN32
    std::lock_guard<std::mutex> lock(mutex_);
    if (!ConnectLocked())
    {
        return false;
    }

    const MsgHeader header = packet.header();
    const std::vector<uint8_t>& payload = packet.bytes();

    if (WriteExactLocked(&header, sizeof(header)) &&
        (payload.empty() || WriteExactLocked(payload.data(), static_cast<uint32_t>(payload.size()))))
    {
        return true;
    }

    DisconnectLocked();
    if (!ConnectLocked())
    {
        return false;
    }

    return WriteExactLocked(&header, sizeof(header)) &&
           (payload.empty() || WriteExactLocked(payload.data(), static_cast<uint32_t>(payload.size())));
#else
    (void)packet;
    return false;
#endif
}

#ifdef _WIN32
bool UpdatePipeClient::ConnectLocked()
{
    if (pipe_ != INVALID_HANDLE_VALUE)
    {
        return true;
    }

    if (pipeName_.empty())
    {
        return false;
    }

    const std::wstring fullPipeName = L"\\\\.\\pipe\\" + pipeName_;
    while (true)
    {
        pipe_ = CreateFileW(
            fullPipeName.c_str(),
            GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr);

        if (pipe_ != INVALID_HANDLE_VALUE)
        {
            return true;
        }

        const DWORD error = GetLastError();
        if (error != ERROR_PIPE_BUSY)
        {
            return false;
        }

        if (!WaitNamedPipeW(fullPipeName.c_str(), 2000))
        {
            return false;
        }
    }
}

void UpdatePipeClient::DisconnectLocked()
{
    if (pipe_ != INVALID_HANDLE_VALUE)
    {
        CloseHandle(pipe_);
        pipe_ = INVALID_HANDLE_VALUE;
    }
}

bool UpdatePipeClient::WriteExactLocked(const void* src, uint32_t bytes)
{
    const uint8_t* data = reinterpret_cast<const uint8_t*>(src);
    uint32_t written = 0;
    while (written < bytes)
    {
        DWORD chunkWritten = 0;
        if (!WriteFile(pipe_, data + written, bytes - written, &chunkWritten, nullptr) || chunkWritten == 0)
        {
            return false;
        }
        written += chunkWritten;
    }
    return true;
}

#endif
