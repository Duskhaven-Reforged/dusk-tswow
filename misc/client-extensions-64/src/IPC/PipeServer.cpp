#include "PipeServer.h"
#include <vector>

#ifdef _WIN32

bool PipeServer::Start(const std::wstring& pipeName) {
    if (running_.exchange(true)) return false;
    th_ = std::thread([this, pipeName] { ThreadMain(pipeName); });
    return true;
}

void PipeServer::Stop() {
    if (!running_.exchange(false)) return;
    if (th_.joinable()) th_.join();
}

bool PipeServer::ReadExact(HANDLE h, void* dst, uint32_t bytes) {
    uint8_t* p = reinterpret_cast<uint8_t*>(dst);
    uint32_t got = 0;
    while (got < bytes) {
        DWORD r = 0;
        if (!ReadFile(h, p + got, bytes - got, &r, nullptr) || r == 0) {
            std::cout << "ReadFile failed/closed, gle=" << GetLastError() << " r=" << r << "\n";
            return false;
        }
        got += r;
    }
    return true;
}

bool PipeServer::WriteExact(HANDLE h, const void* src, uint32_t bytes) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(src);
    uint32_t sent = 0;
    while (sent < bytes) {
        DWORD w = 0;
        if (!WriteFile(h, p + sent, bytes - sent, &w, nullptr) || w == 0) return false;
        sent += w;
    }
    return true;
}

void PipeServer::ThreadMain(std::wstring pipeName) {
    const std::wstring full = L"\\\\.\\pipe\\" + pipeName;

    while (running_) {
        HANDLE pipe = CreateNamedPipeW(
            full.c_str(),
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
            1,              // max instances = 1
            64 * 1024,
            64 * 1024,
            0,
            nullptr
        );

        if (pipe == INVALID_HANDLE_VALUE) return;

        BOOL ok = ConnectNamedPipe(pipe, nullptr) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
        if (!ok) { CloseHandle(pipe); continue; }

        clientConnected_ = true;

        while (running_) {
            MsgHeader hdr{};
            if (!ReadExact(pipe, &hdr, sizeof(hdr))) break;
            std::cout << "READ HEADER: " << " " << hdr.opcode.raw() << " " << hdr.length << "\n";
            std::vector<uint8_t> payload;
            payload.resize(hdr.length);
            if (hdr.length > 0 && !ReadExact(pipe, payload.data(), hdr.length)) break;

             PacketReader reader(hdr, payload.data(), payload.size());
            std::cout << "Opcode: " << reader.opcode().raw() << "\n";
            std::cout << "Payload length: " << reader.payloadLength() << "\n";

            // Dispatch to opcode handler
            bool handled = dispatcher_.Dispatch(hdr.opcode, reader);
        }

        FlushFileBuffers(pipe);
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
        clientConnected_ = false;
    }
}

#endif
