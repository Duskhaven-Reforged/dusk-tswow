#include "PipeServer.h"

#include <iostream>
#include <vector>

#ifdef _WIN32

bool PipeServer::Start(const std::wstring& pipeName)
{
    if (running_.exchange(true)) return false;
    th_ = std::thread([this, pipeName] { ThreadMain(pipeName); });
    return true;
}

void PipeServer::Stop()
{
    if (!running_.exchange(false))
    {
        return;
    }

    channel_.WakeReader();
    if (th_.joinable())
    {
        th_.join();
    }
    channel_.Close();
}

bool PipeServer::DispatchFrame(const uint8_t* frame, uint32_t frameBytes)
{
    if (frameBytes < sizeof(MsgHeader))
    {
        return false;
    }

    MsgHeader header{};
    header.opcode = Opcode(Duskhaven::IPC::LoadU32LE(frame));
    header.length = Duskhaven::IPC::LoadU32LE(frame + sizeof(uint32_t));

    if (frameBytes - sizeof(MsgHeader) < header.length)
    {
        return false;
    }

    PacketReader reader(header, frame + sizeof(MsgHeader), header.length);
    return dispatcher_.Dispatch(header.opcode, reader);
}

void PipeServer::ThreadMain(std::wstring pipeName)
{
    if (!channel_.Create(pipeName))
    {
        std::wcerr << L"Failed to create IPC shared ring: " << pipeName << std::endl;
        running_ = false;
        return;
    }

    clientConnected_ = true;

    std::vector<uint8_t> scratch;
    while (running_)
    {
        channel_.Consume(
            scratch,
            [this](const uint8_t* frame, uint32_t frameBytes)
            {
                DispatchFrame(frame, frameBytes);
            },
            &running_);
    }

    clientConnected_ = false;
}

#endif
