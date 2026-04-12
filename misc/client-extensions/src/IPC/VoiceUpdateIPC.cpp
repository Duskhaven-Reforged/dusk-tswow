#include "VoiceUpdateIPC.h"

#include "ClientLua.h"
#include "PacketBuilder.h"

#include <atomic>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace
{
constexpr wchar_t kVoiceUpdatePipeName[] = L"duskhaven_social_sdk_updates_pipe";
constexpr size_t kMaxQueuedUpdates = 4096;

struct QueuedVoiceUpdate
{
    Opcode opcode;
    std::vector<uint8_t> payload;
};

std::atomic<bool> g_started{false};
std::mutex g_queueMutex;
std::deque<QueuedVoiceUpdate> g_updates;

void QueueUpdate(Opcode opcode, std::vector<uint8_t> payload)
{
    std::lock_guard<std::mutex> lock(g_queueMutex);
    if (g_updates.size() >= kMaxQueuedUpdates)
    {
        g_updates.pop_front();
    }
    g_updates.push_back({opcode, std::move(payload)});
}

std::optional<QueuedVoiceUpdate> PopUpdate()
{
    std::lock_guard<std::mutex> lock(g_queueMutex);
    if (g_updates.empty())
    {
        return std::nullopt;
    }

    QueuedVoiceUpdate update = std::move(g_updates.front());
    g_updates.pop_front();
    return update;
}

class PayloadReader
{
  public:
    PayloadReader(const uint8_t* data, size_t size) : data_(data), size_(size) {}

    bool readUInt32(uint32_t& value)
    {
        if (!require(4))
        {
            return false;
        }

        value = read_u32_le(data_ + cursor_);
        cursor_ += 4;
        return true;
    }

    bool readUInt64(uint64_t& value)
    {
        if (!require(8))
        {
            return false;
        }

        value = read_u64_le(data_ + cursor_);
        cursor_ += 8;
        return true;
    }

    bool readBool(bool& value)
    {
        if (!require(1))
        {
            return false;
        }

        value = read_bool(data_ + cursor_);
        cursor_ += 1;
        return true;
    }

    bool readFloat(float& value)
    {
        if (!require(4))
        {
            return false;
        }

        value = read_f32_le(data_ + cursor_);
        cursor_ += 4;
        return true;
    }

    bool readString(std::string& value)
    {
        uint32_t length = 0;
        if (!readUInt32(length) || !require(length))
        {
            return false;
        }

        value.assign(reinterpret_cast<const char*>(data_ + cursor_), length);
        cursor_ += length;
        return true;
    }

  private:
    bool require(size_t bytes) const
    {
        return cursor_ + bytes <= size_;
    }

    const uint8_t* data_;
    size_t size_;
    size_t cursor_ = 0;
};

void PushUInt64String(lua_State* L, uint64_t value)
{
    const std::string asString = std::to_string(value);
    ClientLua::PushString(L, asString.c_str());
}

int PushMalformedUpdate(lua_State* L, Opcode opcode)
{
    ClientLua::PushString(L, "malformed_update");
    ClientLua::PushNumber(L, opcode.raw());
    return 2;
}

int PushCallStatus(lua_State* L, Opcode opcode, const std::vector<uint8_t>& payload)
{
    PayloadReader reader(payload.data(), payload.size());
    uint64_t lobbyId = 0;
    uint32_t status = 0;
    uint32_t errorCode = 0;
    uint32_t errorDetail = 0;

    if (!reader.readUInt64(lobbyId) ||
        !reader.readUInt32(status) ||
        !reader.readUInt32(errorCode) ||
        !reader.readUInt32(errorDetail))
    {
        return PushMalformedUpdate(L, opcode);
    }

    ClientLua::PushString(L, "call_status");
    PushUInt64String(L, lobbyId);
    ClientLua::PushNumber(L, status);
    ClientLua::PushNumber(L, errorCode);
    ClientLua::PushNumber(L, errorDetail);
    return 5;
}

int PushParticipantsClear(lua_State* L, Opcode opcode, const std::vector<uint8_t>& payload)
{
    PayloadReader reader(payload.data(), payload.size());
    uint64_t lobbyId = 0;
    if (!reader.readUInt64(lobbyId))
    {
        return PushMalformedUpdate(L, opcode);
    }

    ClientLua::PushString(L, "participants_clear");
    PushUInt64String(L, lobbyId);
    return 2;
}

int PushSelfState(lua_State* L, Opcode opcode, const std::vector<uint8_t>& payload)
{
    PayloadReader reader(payload.data(), payload.size());
    uint64_t lobbyId = 0;
    bool muted = false;
    bool deafened = false;
    float inputVolume = 0.0f;
    float outputVolume = 0.0f;
    uint32_t audioMode = 0;
    uint32_t pttReleaseDelay = 0;
    bool vadAutomatic = false;
    float vadThreshold = 0.0f;

    if (!reader.readUInt64(lobbyId) ||
        !reader.readBool(muted) ||
        !reader.readBool(deafened) ||
        !reader.readFloat(inputVolume) ||
        !reader.readFloat(outputVolume) ||
        !reader.readUInt32(audioMode) ||
        !reader.readUInt32(pttReleaseDelay) ||
        !reader.readBool(vadAutomatic) ||
        !reader.readFloat(vadThreshold))
    {
        return PushMalformedUpdate(L, opcode);
    }

    ClientLua::PushString(L, "self_state");
    PushUInt64String(L, lobbyId);
    ClientLua::PushBoolean(L, muted);
    ClientLua::PushBoolean(L, deafened);
    ClientLua::PushNumber(L, inputVolume);
    ClientLua::PushNumber(L, outputVolume);
    ClientLua::PushNumber(L, audioMode);
    ClientLua::PushNumber(L, pttReleaseDelay);
    ClientLua::PushBoolean(L, vadAutomatic);
    ClientLua::PushNumber(L, vadThreshold);
    return 10;
}

int PushSpeakingState(lua_State* L, Opcode opcode, const std::vector<uint8_t>& payload)
{
    PayloadReader reader(payload.data(), payload.size());
    uint64_t lobbyId = 0;
    uint64_t userId = 0;
    bool speaking = false;

    if (!reader.readUInt64(lobbyId) ||
        !reader.readUInt64(userId) ||
        !reader.readBool(speaking))
    {
        return PushMalformedUpdate(L, opcode);
    }

    ClientLua::PushString(L, "speaking");
    PushUInt64String(L, lobbyId);
    PushUInt64String(L, userId);
    ClientLua::PushBoolean(L, speaking);
    return 4;
}

int PushParticipantState(lua_State* L, Opcode opcode, const std::vector<uint8_t>& payload)
{
    PayloadReader reader(payload.data(), payload.size());
    uint64_t lobbyId = 0;
    uint64_t userId = 0;
    bool speaking = false;
    bool selfMuted = false;
    bool selfDeafened = false;
    bool localMute = false;
    float volume = 0.0f;

    if (!reader.readUInt64(lobbyId) ||
        !reader.readUInt64(userId) ||
        !reader.readBool(speaking) ||
        !reader.readBool(selfMuted) ||
        !reader.readBool(selfDeafened) ||
        !reader.readBool(localMute) ||
        !reader.readFloat(volume))
    {
        return PushMalformedUpdate(L, opcode);
    }

    ClientLua::PushString(L, "participant_state");
    PushUInt64String(L, lobbyId);
    PushUInt64String(L, userId);
    ClientLua::PushBoolean(L, speaking);
    ClientLua::PushBoolean(L, selfMuted);
    ClientLua::PushBoolean(L, selfDeafened);
    ClientLua::PushBoolean(L, localMute);
    ClientLua::PushNumber(L, volume);
    return 8;
}

int PushDevicesClear(lua_State* L, Opcode opcode, const std::vector<uint8_t>& payload)
{
    PayloadReader reader(payload.data(), payload.size());
    bool inputDevices = false;
    if (!reader.readBool(inputDevices))
    {
        return PushMalformedUpdate(L, opcode);
    }

    ClientLua::PushString(L, "devices_clear");
    ClientLua::PushString(L, inputDevices ? "input" : "output");
    return 2;
}

int PushDeviceState(lua_State* L, Opcode opcode, const std::vector<uint8_t>& payload)
{
    PayloadReader reader(payload.data(), payload.size());
    bool inputDevices = false;
    std::string deviceId;
    std::string deviceName;
    bool isDefault = false;
    bool isCurrent = false;

    if (!reader.readBool(inputDevices) ||
        !reader.readString(deviceId) ||
        !reader.readString(deviceName) ||
        !reader.readBool(isDefault) ||
        !reader.readBool(isCurrent))
    {
        return PushMalformedUpdate(L, opcode);
    }

    ClientLua::PushString(L, "device");
    ClientLua::PushString(L, inputDevices ? "input" : "output");
    ClientLua::PushString(L, deviceId.c_str());
    ClientLua::PushString(L, deviceName.c_str());
    ClientLua::PushBoolean(L, isDefault);
    ClientLua::PushBoolean(L, isCurrent);
    return 6;
}

int PushVoiceError(lua_State* L, Opcode opcode, const std::vector<uint8_t>& payload)
{
    PayloadReader reader(payload.data(), payload.size());
    uint32_t sourceOpcode = 0;
    uint32_t errorCode = 0;
    std::string message;

    if (!reader.readUInt32(sourceOpcode) ||
        !reader.readUInt32(errorCode) ||
        !reader.readString(message))
    {
        return PushMalformedUpdate(L, opcode);
    }

    ClientLua::PushString(L, "error");
    ClientLua::PushNumber(L, sourceOpcode);
    ClientLua::PushNumber(L, errorCode);
    ClientLua::PushString(L, message.c_str());
    return 4;
}

#ifdef _WIN32
bool ReadExact(HANDLE handle, void* dst, uint32_t bytes)
{
    uint8_t* out = reinterpret_cast<uint8_t*>(dst);
    uint32_t received = 0;
    while (received < bytes)
    {
        DWORD read = 0;
        if (!ReadFile(handle, out + received, bytes - received, &read, nullptr) || read == 0)
        {
            return false;
        }
        received += read;
    }
    return true;
}

void ThreadMain()
{
    const std::wstring fullPipeName = L"\\\\.\\pipe\\" + std::wstring(kVoiceUpdatePipeName);

    while (true)
    {
        HANDLE pipe = CreateNamedPipeW(
            fullPipeName.c_str(),
            PIPE_ACCESS_INBOUND,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
            1,
            64 * 1024,
            64 * 1024,
            0,
            nullptr);

        if (pipe == INVALID_HANDLE_VALUE)
        {
            g_started = false;
            return;
        }

        BOOL connected = ConnectNamedPipe(pipe, nullptr) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
        if (!connected)
        {
            CloseHandle(pipe);
            continue;
        }

        while (true)
        {
            MsgHeader header{};
            if (!ReadExact(pipe, &header, sizeof(header)))
            {
                break;
            }

            std::vector<uint8_t> payload(header.length);
            if (header.length > 0 && !ReadExact(pipe, payload.data(), header.length))
            {
                break;
            }

            QueueUpdate(header.opcode, std::move(payload));
        }

        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
    }
}
#endif
}

namespace VoiceUpdateIPC
{
void Start()
{
#ifdef _WIN32
    if (g_started.exchange(true))
    {
        return;
    }

    std::thread(ThreadMain).detach();
#endif
}
}

LUA_FUNCTION(getVoiceUpdateState, (lua_State* L))
{
    std::optional<QueuedVoiceUpdate> update = PopUpdate();
    if (!update.has_value())
    {
        ClientLua::PushNil(L);
        return 1;
    }

    switch (update->opcode.raw())
    {
        case Opcode::SMSG_VOICE_CALL_STATUS:
            return PushCallStatus(L, update->opcode, update->payload);
        case Opcode::SMSG_VOICE_PARTICIPANTS:
            return PushParticipantsClear(L, update->opcode, update->payload);
        case Opcode::SMSG_VOICE_SELF_STATE:
            return PushSelfState(L, update->opcode, update->payload);
        case Opcode::SMSG_VOICE_SPEAKING:
            return PushSpeakingState(L, update->opcode, update->payload);
        case Opcode::SMSG_VOICE_PARTICIPANT_STATE:
            return PushParticipantState(L, update->opcode, update->payload);
        case Opcode::SMSG_VOICE_DEVICES:
            return PushDevicesClear(L, update->opcode, update->payload);
        case Opcode::SMSG_VOICE_DEVICE_STATE:
            return PushDeviceState(L, update->opcode, update->payload);
        case Opcode::SMSG_VOICE_ERROR:
            return PushVoiceError(L, update->opcode, update->payload);
        default:
            ClientLua::PushString(L, "unknown");
            ClientLua::PushNumber(L, update->opcode.raw());
            return 2;
    }
}
