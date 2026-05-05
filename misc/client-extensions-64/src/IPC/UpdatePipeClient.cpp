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
    std::lock_guard<std::mutex> lock(mutex_);
    if (!ConnectLocked())
    {
        return false;
    }

    if (WritePacketLocked(packet))
    {
        return true;
    }

    DisconnectLocked();
    return ConnectLocked() && WritePacketLocked(packet);
}

#ifdef _WIN32
bool UpdatePipeClient::ConnectLocked()
{
    if (channel_.IsOpen())
    {
        return true;
    }

    if (pipeName_.empty())
    {
        return false;
    }

    return channel_.Open(pipeName_, 0);
}

void UpdatePipeClient::DisconnectLocked()
{
    channel_.Close();
}

bool UpdatePipeClient::WritePacketLocked(const PacketBuilder& packet)
{
    const MsgHeader header = packet.header();
    const std::vector<uint8_t>& payload = packet.bytes();

    uint8_t headerBytes[sizeof(MsgHeader)]{};
    Duskhaven::IPC::StoreU32LE(headerBytes, header.opcode.raw());
    Duskhaven::IPC::StoreU32LE(headerBytes + sizeof(uint32_t), header.length);

    Duskhaven::IPC::SharedRingSegment segments[2] = {
        {headerBytes, static_cast<uint32_t>(sizeof(headerBytes))},
        {payload.empty() ? nullptr : payload.data(), static_cast<uint32_t>(payload.size())}
    };
    const uint32_t segmentCount = payload.empty() ? 1u : 2u;

    return channel_.Write(segments, segmentCount, 50);
}

#endif
