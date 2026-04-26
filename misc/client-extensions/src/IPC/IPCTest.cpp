#include "IPCTest.h"
#include "ClientLua.h"
#include <cstdlib>
#include <iostream>
#include <string>

namespace
{
uint64_t GetUInt64Arg(lua_State* L, int offset)
{
    std::string value = ClientLua::GetString(L, offset);
    if (!value.empty())
    {
        return std::strtoull(value.c_str(), nullptr, 10);
    }

    return static_cast<uint64_t>(ClientLua::GetNumber(L, offset));
}
}

int IPCTest::attemptConnect() {
    return 0;
}

LUA_FUNCTION(VoiceStartCall, (lua_State* L)) {
    std::string lobbyID = ClientLua::GetString(L, 1);
    auto packet = PacketBuilder::CreatePacket(Opcode::CMSG_VOICE_START_CALL);
    packet.writeString(lobbyID);
    packet.Send();
   LOG_INFO <<  "Sent CMSG_VOICE_START_CALL\n";
    return 0;
}

LUA_FUNCTION(VoiceLeaveCall, (lua_State* L)) {
    auto packet = PacketBuilder::CreatePacket(Opcode::CMSG_VOICE_LEAVE_CALL);
    packet.Send();
    LOG_INFO << "Sent CMSG_VOICE_LEAVE_CALL\n";
    return 0;
}

LUA_FUNCTION(VoiceEndAllCalls, (lua_State* L)) {
    auto packet = PacketBuilder::CreatePacket(Opcode::CMSG_VOICE_END_ALL_CALLS);
    packet.Send();
    LOG_INFO << "Sent CMSG_VOICE_END_ALL_CALLS\n";
    return 0;
}

LUA_FUNCTION(VoiceSetSelfMute, (lua_State* L)) {
    bool muted = ClientLua::GetNumber(L, 1);
    auto packet = PacketBuilder::CreatePacket(Opcode::CMSG_VOICE_SET_SELF_MUTE);
    packet.writeBool(muted);
    packet.Send();
    LOG_INFO << "Sent CMSG_VOICE_SET_SELF_MUTE\n";
    return 0;
}

LUA_FUNCTION(VoiceSetSelfDeaf, (lua_State* L)) {
    bool deaf = ClientLua::GetNumber(L, 1);
    auto packet = PacketBuilder::CreatePacket(Opcode::CMSG_VOICE_SET_SELF_DEAF);
    packet.writeBool(deaf);
    packet.Send();
    LOG_INFO << "Sent CMSG_VOICE_SET_SELF_DEAF\n";
    return 0;
}

LUA_FUNCTION(VoiceSetInputVolume, (lua_State* L)) {
    float v = ClientLua::GetNumber(L, 1);
    auto packet = PacketBuilder::CreatePacket(Opcode::CMSG_VOICE_SET_INPUT_VOLUME);
    packet.writeFloat(v);
    packet.Send();
    LOG_INFO << "Sent CMSG_VOICE_SET_INPUT_VOLUME\n";
    return 0;
}

LUA_FUNCTION(VoiceSetOutputVolume, (lua_State* L)) {
    float v = ClientLua::GetNumber(L, 1);
    auto packet = PacketBuilder::CreatePacket(Opcode::CMSG_VOICE_SET_OUTPUT_VOLUME);
    packet.writeFloat(v);
    packet.Send();
    LOG_INFO << "Sent CMSG_VOICE_SET_OUTPUT_VOLUME\n";
    return 0;
}

LUA_FUNCTION(VoiceSetLocalMute, (lua_State* L)) {
    uint64_t userId = ClientLua::GetNumber(L, 1);
    bool muted = ClientLua::GetNumber(L, 2);

    auto packet = PacketBuilder::CreatePacket(Opcode::CMSG_VOICE_SET_LOCAL_MUTE);
    packet.writeUInt64(userId);
    packet.writeBool(muted);
    packet.Send();
    LOG_INFO << "Sent CMSG_VOICE_SET_LOCAL_MUTE\n";
    return 0;
}

LUA_FUNCTION(VoiceSetUserVolume, (lua_State* L)) {
    uint64_t userId = ClientLua::GetNumber(L, 1);
    float v = ClientLua::GetNumber(L, 2);

    auto packet = PacketBuilder::CreatePacket(Opcode::CMSG_VOICE_SET_USER_VOLUME);
    packet.writeUInt64(userId);
    packet.writeFloat(v);
    packet.Send();
    LOG_INFO << "Sent CMSG_VOICE_SET_USER_VOLUME\n";
    return 0;
}

LUA_FUNCTION(VoiceSetPTTActive, (lua_State* L)) {
    bool active = ClientLua::GetNumber(L, 1);
    auto packet = PacketBuilder::CreatePacket(Opcode::CMSG_VOICE_SET_PTT_ACTIVE);
    packet.writeBool(active);
    packet.Send();
    LOG_INFO << "Sent CMSG_VOICE_SET_PTT_ACTIVE\n";
    return 0;
}

LUA_FUNCTION(VoiceSetPTTReleaseDelay, (lua_State* L)) {
    uint32_t ms = ClientLua::GetNumber(L, 1);
    auto packet = PacketBuilder::CreatePacket(Opcode::CMSG_VOICE_SET_PTT_RELEASE_DELAY);
    packet.writeUInt32(ms);
    packet.Send();
    LOG_INFO << "Sent CMSG_VOICE_SET_PTT_RELEASE_DELAY\n";
    return 0;
}

LUA_FUNCTION(VoiceSetVADThreshold, (lua_State* L)) {
    bool automatic = ClientLua::GetNumber(L, 1);
    float threshold = ClientLua::GetNumber(L, 2);

    auto packet = PacketBuilder::CreatePacket(Opcode::CMSG_VOICE_SET_VAD_THRESHOLD);
    packet.writeBool(automatic);
    packet.writeFloat(threshold);
    packet.Send();
    LOG_INFO << "Sent CMSG_VOICE_SET_VAD_THRESHOLD\n";
    return 0;
}

LUA_FUNCTION(VoiceSetAudioMode, (lua_State* L)) {
    uint32_t mode = static_cast<uint32_t>(ClientLua::GetNumber(L, 1));

    auto packet = PacketBuilder::CreatePacket(Opcode::CMSG_VOICE_SET_AUDIO_MODE);
    packet.writeUInt32(mode);
    packet.Send();
    LOG_INFO << "Sent CMSG_VOICE_SET_AUDIO_MODE\n";
    return 0;
}

LUA_FUNCTION(VoiceSetInputDevice, (lua_State* L)) {
    std::string deviceId = ClientLua::GetString(L, 1);

    auto packet = PacketBuilder::CreatePacket(Opcode::CMSG_VOICE_SET_INPUT_DEVICE);
    packet.writeString(deviceId);
    packet.Send();
    LOG_INFO << "Sent CMSG_VOICE_SET_INPUT_DEVICE\n";
    return 0;
}

LUA_FUNCTION(VoiceSetOutputDevice, (lua_State* L)) {
    std::string deviceId = ClientLua::GetString(L, 1);

    auto packet = PacketBuilder::CreatePacket(Opcode::CMSG_VOICE_SET_OUTPUT_DEVICE);
    packet.writeString(deviceId);
    packet.Send();
    LOG_INFO << "Sent CMSG_VOICE_SET_OUTPUT_DEVICE\n";
    return 0;
}

LUA_FUNCTION(VoiceSetAutomaticGainControl, (lua_State* L)) {
    bool enabled = ClientLua::GetNumber(L, 1);

    auto packet = PacketBuilder::CreatePacket(Opcode::CMSG_VOICE_SET_AUTOMATIC_GAIN_CONTROL);
    packet.writeBool(enabled);
    packet.Send();
    LOG_INFO << "Sent CMSG_VOICE_SET_AUTOMATIC_GAIN_CONTROL\n";
    return 0;
}

LUA_FUNCTION(VoiceSetEchoCancellation, (lua_State* L)) {
    bool enabled = ClientLua::GetNumber(L, 1);

    auto packet = PacketBuilder::CreatePacket(Opcode::CMSG_VOICE_SET_ECHO_CANCELLATION);
    packet.writeBool(enabled);
    packet.Send();
    LOG_INFO << "Sent CMSG_VOICE_SET_ECHO_CANCELLATION\n";
    return 0;
}

LUA_FUNCTION(VoiceSetNoiseSuppression, (lua_State* L)) {
    bool enabled = ClientLua::GetNumber(L, 1);

    auto packet = PacketBuilder::CreatePacket(Opcode::CMSG_VOICE_SET_NOISE_SUPPRESSION);
    packet.writeBool(enabled);
    packet.Send();
    LOG_INFO << "Sent CMSG_VOICE_SET_NOISE_SUPPRESSION\n";
    return 0;
}

LUA_FUNCTION(VoiceSetListenerPosition, (lua_State* L)) {
    float x = static_cast<float>(ClientLua::GetNumber(L, 1));
    float y = static_cast<float>(ClientLua::GetNumber(L, 2));
    float z = static_cast<float>(ClientLua::GetNumber(L, 3));
    float forwardX = static_cast<float>(ClientLua::GetNumber(L, 4, 0.0));
    float forwardY = static_cast<float>(ClientLua::GetNumber(L, 5, 1.0));
    float forwardZ = static_cast<float>(ClientLua::GetNumber(L, 6, 0.0));

    auto packet = PacketBuilder::CreatePacket(Opcode::CMSG_VOICE_SET_LISTENER_POSITION);
    packet.writeFloat(x);
    packet.writeFloat(y);
    packet.writeFloat(z);
    packet.writeFloat(forwardX);
    packet.writeFloat(forwardY);
    packet.writeFloat(forwardZ);
    packet.Send();
    LOG_INFO << "Sent CMSG_VOICE_SET_LISTENER_POSITION\n";
    return 0;
}

LUA_FUNCTION(VoiceMapDiscordPlayer, (lua_State* L)) {
    uint64_t discordUserId = GetUInt64Arg(L, 1);
    uint64_t playerId = GetUInt64Arg(L, 2);

    auto packet = PacketBuilder::CreatePacket(Opcode::CMSG_VOICE_SET_PLAYER_MAPPING);
    packet.writeUInt64(discordUserId);
    packet.writeUInt64(playerId);
    packet.Send();
    LOG_INFO << "Sent CMSG_VOICE_SET_PLAYER_MAPPING\n";
    return 0;
}

LUA_FUNCTION(VoiceRemoveDiscordPlayer, (lua_State* L)) {
    uint64_t discordUserId = GetUInt64Arg(L, 1);

    auto packet = PacketBuilder::CreatePacket(Opcode::CMSG_VOICE_REMOVE_PLAYER_MAPPING);
    packet.writeUInt64(discordUserId);
    packet.Send();
    LOG_INFO << "Sent CMSG_VOICE_REMOVE_PLAYER_MAPPING\n";
    return 0;
}

LUA_FUNCTION(VoiceSetPlayerPosition, (lua_State* L)) {
    uint64_t playerId = GetUInt64Arg(L, 1);
    float x = static_cast<float>(ClientLua::GetNumber(L, 2));
    float y = static_cast<float>(ClientLua::GetNumber(L, 3));
    float z = static_cast<float>(ClientLua::GetNumber(L, 4));

    auto packet = PacketBuilder::CreatePacket(Opcode::CMSG_VOICE_SET_PLAYER_POSITION);
    packet.writeUInt64(playerId);
    packet.writeFloat(x);
    packet.writeFloat(y);
    packet.writeFloat(z);
    packet.Send();
    LOG_INFO << "Sent CMSG_VOICE_SET_PLAYER_POSITION\n";
    return 0;
}

LUA_FUNCTION(VoiceRemovePlayerPosition, (lua_State* L)) {
    uint64_t playerId = GetUInt64Arg(L, 1);

    auto packet = PacketBuilder::CreatePacket(Opcode::CMSG_VOICE_REMOVE_PLAYER_POSITION);
    packet.writeUInt64(playerId);
    packet.Send();
    LOG_INFO << "Sent CMSG_VOICE_REMOVE_PLAYER_POSITION\n";
    return 0;
}
