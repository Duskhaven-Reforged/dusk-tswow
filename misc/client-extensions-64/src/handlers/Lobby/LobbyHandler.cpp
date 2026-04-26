#include "LobbyHandler.h"

#include <array>
#include <cmath>
#include <utility>

REGISTER_IPC_HANDLER(LobbyHandler)

namespace
{
template <typename Handler>
void RegisterReaderHandler(OpcodeDispatcher& dispatcher, Opcode opcode, Handler&& handler)
{
    dispatcher.Register(opcode,
                        [handler = std::forward<Handler>(handler)](Opcode, PacketReader reader) mutable
                        { handler(reader); });
}

template <typename Handler>
void RegisterSimpleHandler(OpcodeDispatcher& dispatcher, Opcode opcode, Handler&& handler)
{
    dispatcher.Register(opcode,
                        [handler = std::forward<Handler>(handler)](Opcode, PacketReader) mutable
                        { handler(); });
}

constexpr std::array<Opcode, 25> kRegisteredOpcodes = {
    Opcode::CMSG_VOICE_START_CALL,
    Opcode::CMSG_VOICE_LEAVE_CALL,
    Opcode::CMSG_VOICE_END_ALL_CALLS,
    Opcode::CMSG_VOICE_SET_SELF_MUTE,
    Opcode::CMSG_VOICE_SET_SELF_DEAF,
    Opcode::CMSG_VOICE_SET_INPUT_VOLUME,
    Opcode::CMSG_VOICE_SET_OUTPUT_VOLUME,
    Opcode::CMSG_VOICE_SET_LOCAL_MUTE,
    Opcode::CMSG_VOICE_SET_USER_VOLUME,
    Opcode::CMSG_VOICE_SET_PTT_ACTIVE,
    Opcode::CMSG_VOICE_SET_PTT_RELEASE_DELAY,
    Opcode::CMSG_VOICE_SET_VAD_THRESHOLD,
    Opcode::CMSG_VOICE_SET_AUDIO_MODE,
    Opcode::CMSG_VOICE_SET_INPUT_DEVICE,
    Opcode::CMSG_VOICE_SET_OUTPUT_DEVICE,
    Opcode::CMSG_VOICE_SET_AUTOMATIC_GAIN_CONTROL,
    Opcode::CMSG_VOICE_SET_ECHO_CANCELLATION,
    Opcode::CMSG_VOICE_SET_NOISE_SUPPRESSION,
    Opcode::CMSG_VOICE_SET_LISTENER_POSITION,
    Opcode::CMSG_VOICE_SET_PLAYER_MAPPING,
    Opcode::CMSG_VOICE_REMOVE_PLAYER_MAPPING,
    Opcode::CMSG_VOICE_SET_PLAYER_POSITION,
    Opcode::CMSG_VOICE_REMOVE_PLAYER_POSITION,
    Opcode::CMSG_DISCORD_SET_GAME_PRESENCE,
    Opcode::CMSG_DISCORD_CLEAR_GAME_PRESENCE,
};
}

void LobbyHandler::Register(OpcodeDispatcher& dispatcher)
{
    RegisterReaderHandler(dispatcher, Opcode::CMSG_VOICE_START_CALL, [](PacketReader reader)
                          { DiscordManager::Get()->JoinLobbyAndCall(reader.readString()); });
    RegisterSimpleHandler(dispatcher, Opcode::CMSG_VOICE_LEAVE_CALL, []()
                          { DiscordManager::Get()->LeaveCall(); });
    RegisterSimpleHandler(dispatcher, Opcode::CMSG_VOICE_END_ALL_CALLS, []()
                          { DiscordManager::Get()->EndAllCalls(); });
    RegisterReaderHandler(dispatcher, Opcode::CMSG_VOICE_SET_SELF_MUTE, [](PacketReader reader)
                          { DiscordManager::Get()->SetSelfMute(reader.readBool()); });
    RegisterReaderHandler(dispatcher, Opcode::CMSG_VOICE_SET_SELF_DEAF, [](PacketReader reader)
                          { DiscordManager::Get()->SetSelfDeaf(reader.readBool()); });
    RegisterReaderHandler(dispatcher, Opcode::CMSG_VOICE_SET_INPUT_VOLUME, [](PacketReader reader)
                          { DiscordManager::Get()->SetInputVolume(reader.readFloat()); });
    RegisterReaderHandler(dispatcher, Opcode::CMSG_VOICE_SET_OUTPUT_VOLUME, [](PacketReader reader)
                          { DiscordManager::Get()->SetOutputVolume(reader.readFloat()); });
    RegisterReaderHandler(dispatcher, Opcode::CMSG_VOICE_SET_LOCAL_MUTE, [](PacketReader reader)
                          { DiscordManager::Get()->SetLocalMute(reader.readUInt64(), reader.readBool()); });
    RegisterReaderHandler(dispatcher, Opcode::CMSG_VOICE_SET_USER_VOLUME, [](PacketReader reader)
                          { DiscordManager::Get()->SetParticipantVolume(reader.readUInt64(), reader.readFloat()); });
    RegisterReaderHandler(dispatcher, Opcode::CMSG_VOICE_SET_PTT_ACTIVE, [](PacketReader reader)
                          { DiscordManager::Get()->SetPTTActive(reader.readBool()); });
    RegisterReaderHandler(dispatcher, Opcode::CMSG_VOICE_SET_PTT_RELEASE_DELAY, [](PacketReader reader)
                          { DiscordManager::Get()->SetPTTReleaseDelay(reader.readUInt32()); });
    RegisterReaderHandler(dispatcher, Opcode::CMSG_VOICE_SET_VAD_THRESHOLD, [](PacketReader reader)
                          {
                              const bool automatic = reader.readBool();
                              const float threshold = std::round(reader.readFloat() * 100.0f - 100.0f);
                              DiscordManager::Get()->SetVADThreshold(automatic, threshold);
                          });
    RegisterReaderHandler(dispatcher, Opcode::CMSG_VOICE_SET_AUDIO_MODE, [](PacketReader reader)
                          { DiscordManager::Get()->SetAudioMode(static_cast<discordpp::AudioModeType>(reader.readUInt32())); });
    RegisterReaderHandler(dispatcher, Opcode::CMSG_VOICE_SET_INPUT_DEVICE, [](PacketReader reader)
                          { DiscordManager::Get()->SetInputDevice(reader.readString()); });
    RegisterReaderHandler(dispatcher, Opcode::CMSG_VOICE_SET_OUTPUT_DEVICE, [](PacketReader reader)
                          { DiscordManager::Get()->SetOutputDevice(reader.readString()); });
    RegisterReaderHandler(dispatcher, Opcode::CMSG_VOICE_SET_AUTOMATIC_GAIN_CONTROL, [](PacketReader reader)
                          { DiscordManager::Get()->SetAutomaticGainControl(reader.readBool()); });
    RegisterReaderHandler(dispatcher, Opcode::CMSG_VOICE_SET_ECHO_CANCELLATION, [](PacketReader reader)
                          { DiscordManager::Get()->SetEchoCancellation(reader.readBool()); });
    RegisterReaderHandler(dispatcher, Opcode::CMSG_VOICE_SET_NOISE_SUPPRESSION, [](PacketReader reader)
                          { DiscordManager::Get()->SetNoiseSuppression(reader.readBool()); });
    RegisterReaderHandler(dispatcher, Opcode::CMSG_VOICE_SET_LISTENER_POSITION, [](PacketReader reader)
                          {
                              const float x = reader.readFloat();
                              const float y = reader.readFloat();
                              const float z = reader.readFloat();
                              const float forwardX = reader.readFloat();
                              const float forwardY = reader.readFloat();
                              const float forwardZ = reader.readFloat();
                              DiscordManager::Get()->SetVoiceListenerPosition(x, y, z, forwardX, forwardY, forwardZ);
                          });
    RegisterReaderHandler(dispatcher, Opcode::CMSG_VOICE_SET_PLAYER_MAPPING, [](PacketReader reader)
                          { DiscordManager::Get()->SetVoicePlayerMapping(reader.readUInt64(), reader.readUInt64()); });
    RegisterReaderHandler(dispatcher, Opcode::CMSG_VOICE_REMOVE_PLAYER_MAPPING, [](PacketReader reader)
                          { DiscordManager::Get()->RemoveVoicePlayerMapping(reader.readUInt64()); });
    RegisterReaderHandler(dispatcher, Opcode::CMSG_VOICE_SET_PLAYER_POSITION, [](PacketReader reader)
                          {
                              const uint64_t playerId = reader.readUInt64();
                              const float x = reader.readFloat();
                              const float y = reader.readFloat();
                              const float z = reader.readFloat();
                              DiscordManager::Get()->SetVoicePlayerPosition(playerId, x, y, z);
                          });
    RegisterReaderHandler(dispatcher, Opcode::CMSG_VOICE_REMOVE_PLAYER_POSITION, [](PacketReader reader)
                          { DiscordManager::Get()->RemoveVoicePlayerPosition(reader.readUInt64()); });
    RegisterReaderHandler(dispatcher, Opcode::CMSG_DISCORD_SET_GAME_PRESENCE, [](PacketReader reader)
                          {
                              const std::string characterName = reader.readString();
                              const uint32_t characterLevel = reader.readUInt32();
                              const std::string className = reader.readString();
                              const std::string zoneName = reader.readString();
                              DiscordManager::Get()->SetGamePresence(characterName, characterLevel, className, zoneName);
                          });
    RegisterSimpleHandler(dispatcher, Opcode::CMSG_DISCORD_CLEAR_GAME_PRESENCE, []()
                          { DiscordManager::Get()->ClearGamePresence(); });
}

void LobbyHandler::Unregister(OpcodeDispatcher& dispatcher)
{
    for (Opcode opcode : kRegisteredOpcodes)
    {
        dispatcher.Unregister(opcode);
    }
}
