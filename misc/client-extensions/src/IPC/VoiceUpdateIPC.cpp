#include "VoiceUpdateIPC.h"

#include "ClientLua.h"
#include "PacketBuilder.h"

#include <atomic>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
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

struct UInt64String
{
    uint64_t value;
};

bool ReadValue(PayloadReader& reader, uint32_t& value) { return reader.readUInt32(value); }
bool ReadValue(PayloadReader& reader, uint64_t& value) { return reader.readUInt64(value); }
bool ReadValue(PayloadReader& reader, bool& value) { return reader.readBool(value); }
bool ReadValue(PayloadReader& reader, float& value) { return reader.readFloat(value); }
bool ReadValue(PayloadReader& reader, std::string& value) { return reader.readString(value); }

template <typename... Args>
bool ReadValues(PayloadReader& reader, Args&... args)
{
    return (ReadValue(reader, args) && ...);
}

void PushLuaValue(lua_State* L, const char* value) { ClientLua::PushString(L, value); }
void PushLuaValue(lua_State* L, const std::string& value) { ClientLua::PushString(L, value.c_str()); }
void PushLuaValue(lua_State* L, bool value) { ClientLua::PushBoolean(L, value); }
void PushLuaValue(lua_State* L, uint32_t value) { ClientLua::PushNumber(L, value); }
void PushLuaValue(lua_State* L, float value) { ClientLua::PushNumber(L, value); }
void PushLuaValue(lua_State* L, UInt64String value)
{
    const std::string asString = std::to_string(value.value);
    ClientLua::PushString(L, asString.c_str());
}

template <typename... Args>
int PushUpdate(lua_State* L, const char* kind, Args&&... args)
{
    ClientLua::PushString(L, kind);
    (PushLuaValue(L, std::forward<Args>(args)), ...);
    return 1 + static_cast<int>(sizeof...(Args));
}

const char* DeviceDirection(bool inputDevices)
{
    return inputDevices ? "input" : "output";
}

int PushMalformedUpdate(lua_State* L, Opcode opcode)
{
    return PushUpdate(L, "malformed_update", opcode.raw());
}

int PushCallStatus(lua_State* L, Opcode opcode, const std::vector<uint8_t>& payload)
{
    PayloadReader reader(payload.data(), payload.size());
    uint64_t lobbyId = 0;
    uint32_t status = 0;
    uint32_t errorCode = 0;
    uint32_t errorDetail = 0;

    if (!ReadValues(reader, lobbyId, status, errorCode, errorDetail))
    {
        return PushMalformedUpdate(L, opcode);
    }

    return PushUpdate(L, "call_status", UInt64String{lobbyId}, status, errorCode, errorDetail);
}

int PushParticipantsClear(lua_State* L, Opcode opcode, const std::vector<uint8_t>& payload)
{
    PayloadReader reader(payload.data(), payload.size());
    uint64_t lobbyId = 0;
    if (!ReadValues(reader, lobbyId))
    {
        return PushMalformedUpdate(L, opcode);
    }

    return PushUpdate(L, "participants_clear", UInt64String{lobbyId});
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

    if (!ReadValues(
            reader,
            lobbyId,
            muted,
            deafened,
            inputVolume,
            outputVolume,
            audioMode,
            pttReleaseDelay,
            vadAutomatic,
            vadThreshold))
    {
        return PushMalformedUpdate(L, opcode);
    }

    return PushUpdate(
        L,
        "self_state",
        UInt64String{lobbyId},
        muted,
        deafened,
        inputVolume,
        outputVolume,
        audioMode,
        pttReleaseDelay,
        vadAutomatic,
        vadThreshold);
}

int PushSpeakingState(lua_State* L, Opcode opcode, const std::vector<uint8_t>& payload)
{
    PayloadReader reader(payload.data(), payload.size());
    uint64_t lobbyId = 0;
    uint64_t userId = 0;
    bool speaking = false;

    if (!ReadValues(reader, lobbyId, userId, speaking))
    {
        return PushMalformedUpdate(L, opcode);
    }

    return PushUpdate(L, "speaking", UInt64String{lobbyId}, UInt64String{userId}, speaking);
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

    if (!ReadValues(reader, lobbyId, userId, speaking, selfMuted, selfDeafened, localMute, volume))
    {
        return PushMalformedUpdate(L, opcode);
    }

    return PushUpdate(
        L,
        "participant_state",
        UInt64String{lobbyId},
        UInt64String{userId},
        speaking,
        selfMuted,
        selfDeafened,
        localMute,
        volume);
}

int PushDevicesClear(lua_State* L, Opcode opcode, const std::vector<uint8_t>& payload)
{
    PayloadReader reader(payload.data(), payload.size());
    bool inputDevices = false;
    if (!ReadValues(reader, inputDevices))
    {
        return PushMalformedUpdate(L, opcode);
    }

    return PushUpdate(L, "devices_clear", DeviceDirection(inputDevices));
}

int PushDeviceState(lua_State* L, Opcode opcode, const std::vector<uint8_t>& payload)
{
    PayloadReader reader(payload.data(), payload.size());
    bool inputDevices = false;
    std::string deviceId;
    std::string deviceName;
    bool isDefault = false;
    bool isCurrent = false;

    if (!ReadValues(reader, inputDevices, deviceId, deviceName, isDefault, isCurrent))
    {
        return PushMalformedUpdate(L, opcode);
    }

    return PushUpdate(L, "device", DeviceDirection(inputDevices), deviceId, deviceName, isDefault, isCurrent);
}

int PushVoiceError(lua_State* L, Opcode opcode, const std::vector<uint8_t>& payload)
{
    PayloadReader reader(payload.data(), payload.size());
    uint32_t sourceOpcode = 0;
    uint32_t errorCode = 0;
    std::string message;

    if (!ReadValues(reader, sourceOpcode, errorCode, message))
    {
        return PushMalformedUpdate(L, opcode);
    }

    return PushUpdate(L, "error", sourceOpcode, errorCode, message);
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
            return PushUpdate(L, "unknown", update->opcode.raw());
    }
}
