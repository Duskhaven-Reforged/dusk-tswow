#include "LobbyHandler.h"

REGISTER_IPC_HANDLER(LobbyHandler)

void LobbyHandler::Register(OpcodeDispatcher &dispatcher)
{
    dispatcher.Register(Opcode::CMSG_VOICE_START_CALL, [this](Opcode op, PacketReader reader)
                        {
        std::string secret = reader.readString();
        DiscordManager::Get()->JoinLobbyAndCall(secret); });

    dispatcher.Register(Opcode::CMSG_VOICE_LEAVE_CALL, [this](Opcode op, PacketReader reader)
                        { DiscordManager::Get()->LeaveCall(); });

    dispatcher.Register(Opcode::CMSG_VOICE_END_ALL_CALLS, [this](Opcode op, PacketReader reader)
                        { DiscordManager::Get()->EndAllCalls(); });

    dispatcher.Register(Opcode::CMSG_VOICE_SET_SELF_MUTE, [this](Opcode op, PacketReader reader)
                        {
        bool muted = reader.readBool();
        DiscordManager::Get()->SetSelfMute(muted); });

    dispatcher.Register(Opcode::CMSG_VOICE_SET_SELF_DEAF, [this](Opcode op, PacketReader reader)
                        {
        bool deaf = reader.readBool();
        DiscordManager::Get()->SetSelfDeaf(deaf); });

    dispatcher.Register(Opcode::CMSG_VOICE_SET_INPUT_VOLUME, [this](Opcode op, PacketReader reader)
                        {
        float v = reader.readFloat();
        DiscordManager::Get()->SetInputVolume(v); });

    dispatcher.Register(Opcode::CMSG_VOICE_SET_OUTPUT_VOLUME, [this](Opcode op, PacketReader reader)
                        {
        float v = reader.readFloat();
        DiscordManager::Get()->SetOutputVolume(v); });

    dispatcher.Register(Opcode::CMSG_VOICE_SET_LOCAL_MUTE, [this](Opcode op, PacketReader reader)
                        {
        uint64_t userId = reader.readUInt64();
        bool muted = reader.readBool();
        DiscordManager::Get()->SetLocalMute(userId, muted); });

    dispatcher.Register(Opcode::CMSG_VOICE_SET_USER_VOLUME, [this](Opcode op, PacketReader reader)
                        {
        uint64_t userId = reader.readUInt64();
        float v = reader.readFloat();
        DiscordManager::Get()->SetParticipantVolume(userId, v); });

    dispatcher.Register(Opcode::CMSG_VOICE_SET_PTT_ACTIVE, [this](Opcode op, PacketReader reader)
                        {
        bool active = reader.readBool();
        DiscordManager::Get()->SetPTTActive(active); });

    dispatcher.Register(Opcode::CMSG_VOICE_SET_PTT_RELEASE_DELAY, [this](Opcode op, PacketReader reader)
                        {
        uint32_t ms = reader.readUInt32();
        DiscordManager::Get()->SetPTTReleaseDelay(ms); });

    dispatcher.Register(Opcode::CMSG_VOICE_SET_VAD_THRESHOLD, [this](Opcode op, PacketReader reader)
                        {
        bool automatic = reader.readBool();
        float threshold = std::round(reader.readFloat() * 100.0f - 100.0f);
        DiscordManager::Get()->SetVADThreshold(automatic, threshold); });

    dispatcher.Register(Opcode::CMSG_VOICE_SET_AUDIO_MODE, [this](Opcode op, PacketReader reader)
                        {
        uint32_t mode = reader.readUInt32();
        DiscordManager::Get()->SetAudioMode(static_cast<discordpp::AudioModeType>(mode)); });

    dispatcher.Register(Opcode::CMSG_VOICE_SET_INPUT_DEVICE, [this](Opcode op, PacketReader reader)
                        {
        std::string deviceId = reader.readString();
        DiscordManager::Get()->SetInputDevice(deviceId); });

    dispatcher.Register(Opcode::CMSG_VOICE_SET_OUTPUT_DEVICE, [this](Opcode op, PacketReader reader)
                        {
        std::string deviceId = reader.readString();
        DiscordManager::Get()->SetOutputDevice(deviceId); });

    dispatcher.Register(Opcode::CMSG_VOICE_SET_AUTOMATIC_GAIN_CONTROL, [this](Opcode op, PacketReader reader)
                        {
        bool enabled = reader.readBool();
        DiscordManager::Get()->SetAutomaticGainControl(enabled); });

    dispatcher.Register(Opcode::CMSG_VOICE_SET_ECHO_CANCELLATION, [this](Opcode op, PacketReader reader)
                        {
        bool enabled = reader.readBool();
        DiscordManager::Get()->SetEchoCancellation(enabled); });

    dispatcher.Register(Opcode::CMSG_VOICE_SET_NOISE_SUPPRESSION, [this](Opcode op, PacketReader reader)
                        {
        bool enabled = reader.readBool();
        DiscordManager::Get()->SetNoiseSuppression(enabled); });

    dispatcher.Register(Opcode::CMSG_DISCORD_SET_GAME_PRESENCE, [this](Opcode op, PacketReader reader)
                        {
        std::string characterName = reader.readString();
        uint32_t characterLevel = reader.readUInt32();
        std::string className = reader.readString();
        std::string zoneName = reader.readString();
        DiscordManager::Get()->SetGamePresence(characterName, characterLevel, className, zoneName); });

    dispatcher.Register(Opcode::CMSG_DISCORD_CLEAR_GAME_PRESENCE, [this](Opcode op, PacketReader reader)
                        { DiscordManager::Get()->ClearGamePresence(); });
}

void LobbyHandler::Unregister(OpcodeDispatcher &dispatcher)
{

    dispatcher.Unregister(Opcode::CMSG_VOICE_START_CALL);
    dispatcher.Unregister(Opcode::CMSG_VOICE_LEAVE_CALL);
    dispatcher.Unregister(Opcode::CMSG_VOICE_END_ALL_CALLS);
    dispatcher.Unregister(Opcode::CMSG_VOICE_SET_SELF_MUTE);
    dispatcher.Unregister(Opcode::CMSG_VOICE_SET_SELF_DEAF);
    dispatcher.Unregister(Opcode::CMSG_VOICE_SET_INPUT_VOLUME);
    dispatcher.Unregister(Opcode::CMSG_VOICE_SET_OUTPUT_VOLUME);
    dispatcher.Unregister(Opcode::CMSG_VOICE_SET_LOCAL_MUTE);
    dispatcher.Unregister(Opcode::CMSG_VOICE_SET_USER_VOLUME);
    dispatcher.Unregister(Opcode::CMSG_VOICE_SET_PTT_ACTIVE);
    dispatcher.Unregister(Opcode::CMSG_VOICE_SET_PTT_RELEASE_DELAY);
    dispatcher.Unregister(Opcode::CMSG_VOICE_SET_VAD_THRESHOLD);
    dispatcher.Unregister(Opcode::CMSG_VOICE_SET_AUDIO_MODE);
    dispatcher.Unregister(Opcode::CMSG_VOICE_SET_INPUT_DEVICE);
    dispatcher.Unregister(Opcode::CMSG_VOICE_SET_OUTPUT_DEVICE);
    dispatcher.Unregister(Opcode::CMSG_VOICE_SET_AUTOMATIC_GAIN_CONTROL);
    dispatcher.Unregister(Opcode::CMSG_VOICE_SET_ECHO_CANCELLATION);
    dispatcher.Unregister(Opcode::CMSG_VOICE_SET_NOISE_SUPPRESSION);
    dispatcher.Unregister(Opcode::CMSG_DISCORD_SET_GAME_PRESENCE);
    dispatcher.Unregister(Opcode::CMSG_DISCORD_CLEAR_GAME_PRESENCE);
}
