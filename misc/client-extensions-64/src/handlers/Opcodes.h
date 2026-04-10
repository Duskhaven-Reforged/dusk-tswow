#pragma once
#include <cstdint>
#include <cstddef>
#include <functional>
class Opcode
{
public:
    // CMSG = client request to server
    // SMSG = server/DiscordSDK push to client
    enum Value : uint32_t
    {
        DEFAULT_VALUE = 0x0,

        // Voice / Call control (new)
        CMSG_VOICE_START_CALL = 0x1101,    // lobbyId
        CMSG_VOICE_LEAVE_CALL = 0x1102,    // (no payload) or lobbyId optional
        CMSG_VOICE_END_ALL_CALLS = 0x1103, // (no payload)

        // Self controls
        CMSG_VOICE_SET_SELF_MUTE = 0x1110,     // bool
        CMSG_VOICE_SET_SELF_DEAF = 0x1111,     // bool
        CMSG_VOICE_SET_INPUT_VOLUME = 0x1112,  // float
        CMSG_VOICE_SET_OUTPUT_VOLUME = 0x1113, // float

        // Per-user controls
        CMSG_VOICE_SET_LOCAL_MUTE = 0x1120,  // userId, bool
        CMSG_VOICE_SET_USER_VOLUME = 0x1121, // userId, float

        // Input modes / tuning
        CMSG_VOICE_SET_PTT_ACTIVE = 0x1130,        // bool
        CMSG_VOICE_SET_PTT_RELEASE_DELAY = 0x1131, // uint32 ms
        CMSG_VOICE_SET_VAD_THRESHOLD = 0x1132,     // bool automatic, float threshold
        CMSG_VOICE_SET_AUDIO_MODE = 0x1133,        // uint32 mode enum (your own)

        // Device / processing controls
        CMSG_VOICE_SET_INPUT_DEVICE = 0x1140,           // string deviceId, empty = default
        CMSG_VOICE_SET_OUTPUT_DEVICE = 0x1141,          // string deviceId, empty = default
        CMSG_VOICE_SET_AUTOMATIC_GAIN_CONTROL = 0x1142, // bool
        CMSG_VOICE_SET_ECHO_CANCELLATION = 0x1143,      // bool
        CMSG_VOICE_SET_NOISE_SUPPRESSION = 0x1144,      // bool

        // Rich presence
        CMSG_DISCORD_SET_GAME_PRESENCE = 0x1200,   // characterName, level, className, zoneName
        CMSG_DISCORD_CLEAR_GAME_PRESENCE = 0x1201, // (no payload)

        // ---------------------------------------------------------
        // Server/SDK -> Client push (new)
        // ---------------------------------------------------------
        SMSG_LOBBY_JOINED = 0x2001,  // lobbyId, secret?
        SMSG_LOBBY_LEFT = 0x2002,    // lobbyId
        SMSG_LOBBY_UPDATED = 0x2003, // lobbyId + fields, or "changed" flags

        SMSG_VOICE_CALL_STATUS = 0x2100,  // lobbyId, status enum, errorCode?
        SMSG_VOICE_PARTICIPANTS = 0x2101, // lobbyId, list(userId, flags...)
        SMSG_VOICE_SPEAKING = 0x2102,     // lobbyId, userId, bool speaking
        SMSG_VOICE_SELF_STATE = 0x2103,   // muted, deafened, inputVol, outputVol
        SMSG_VOICE_ERROR = 0x21FF         // op, errorCode, message(optional)
    };

    constexpr Opcode() : value_(DEFAULT_VALUE) {}
    constexpr Opcode(Value v) : value_(v) {}
    constexpr explicit Opcode(uint32_t v) : value_(static_cast<Value>(v)) {}
    constexpr uint32_t raw() const { return value_; }

    // Comparisons
    constexpr bool operator==(Opcode other) const { return value_ == other.value_; }
    constexpr bool operator!=(Opcode other) const { return value_ != other.value_; }
    constexpr bool operator==(uint32_t other) const { return value_ == other; }
    constexpr bool operator!=(uint32_t other) const { return value_ != other; }

private:
    Value value_;
};

struct OpcodeHash
{
    size_t operator()(const Opcode &op) const noexcept
    {
        return std::hash<uint32_t>{}(op.raw());
    }
};
