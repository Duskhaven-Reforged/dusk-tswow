#include "IPCClient.h"

#ifdef _WIN32
#include <iostream>

bool IPCClient::Connect(const std::wstring& pipeName) {
    Disconnect();

    const std::wstring full =  L"\\\\.\\pipe\\" + pipeName;
    // Try to connect; if busy, wait a bit.
    while (true) {
        pipe_ = CreateFileW(
            full.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,              // no sharing
            NULL,
            OPEN_EXISTING,
            0,
            NULL
        );

        if (pipe_ != INVALID_HANDLE_VALUE) break;

        DWORD err = GetLastError();
        if (err != ERROR_PIPE_BUSY) {
            LOG_INFO << L"CreateFileW failed for pipe: "
                       << L" error=" << err;
            return false;
        }

        // Wait for server to become available
        if (!WaitNamedPipeW(full.c_str(), 2000)) {
             LOG_INFO << L"WaitNamedPipeW timed out for: " ;
            return false;
        }
    }

    // Optional: ensure byte mode (server uses PIPE_TYPE_BYTE/READMODE_BYTE)
    DWORD mode = PIPE_READMODE_BYTE;
    SetNamedPipeHandleState(pipe_, &mode, nullptr, nullptr);

    return true;
}

void IPCClient::Disconnect() {
    if (pipe_ != INVALID_HANDLE_VALUE) {
        CloseHandle(pipe_);
        pipe_ = INVALID_HANDLE_VALUE;
    }
}

bool IPCClient::IsConnected() const {
    return pipe_ != INVALID_HANDLE_VALUE;
}

bool IPCClient::WriteExact(const void* src, uint32_t bytes) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(src);
    uint32_t sent = 0;

    while (sent < bytes) {
        DWORD w = 0;
        if (!WriteFile(pipe_, p + sent, bytes - sent, &w, nullptr) || w == 0) {
            LOG_INFO << "WriteFile failed, gle=" << GetLastError();
            return false;
        }
        sent += w;
    }
    return true;
}

bool IPCClient::Send(const std::vector<uint8_t>& payload) {
    if (!IsConnected()) return false;
    
    if (payload.size() > 0 && !WriteExact(payload.data(), payload.size())) return false;
    return true;
}

#endif // _WIN32
