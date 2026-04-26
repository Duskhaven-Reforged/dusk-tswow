#pragma once

#include <cstdint>
#include <mutex>
#include <unordered_map>

#include <fmod.hpp>

struct Vec3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

using PlayerId = uint64_t;

struct VoiceStream
{
    FMOD::Sound* sound = nullptr;
    FMOD::Channel* channel = nullptr;
    uint32_t writePos = 0;
    uint32_t bufferSize = 0;
};

class VoiceManager
{
  public:
    VoiceManager() = default;
    ~VoiceManager();

    bool InitFMOD();
    void Shutdown();

    VoiceStream* CreateVoiceStream(uint64_t userId);
    void OnUserAudio(uint64_t userId,
                     int16_t* pcm,
                     uint64_t samplesPerChannel,
                     int32_t sampleRate,
                     uint64_t channels,
                     bool& outShouldMuteData);

    void SetListenerPosition(Vec3 position, Vec3 forward = {0.0f, 1.0f, 0.0f}, Vec3 up = {0.0f, 0.0f, 1.0f});
    void SetPlayerMapping(uint64_t discordUserId, PlayerId playerId);
    void RemovePlayerMapping(uint64_t discordUserId);
    void SetPlayerPosition(PlayerId playerId, Vec3 position);
    void RemovePlayerPosition(PlayerId playerId);

    void UpdateVoicePositions();
    void Update();

    FMOD::System* GetSystem() const { return system_; }
    bool IsInitialized() const { return system_ != nullptr; }

  private:
    FMOD_VECTOR ToFMOD(Vec3 value) const;
    void WritePcm(VoiceStream& stream, const int16_t* pcm, uint32_t byteCount);
    void LogFMODError(const char* action, FMOD_RESULT result) const;

    static constexpr float kMinDistance = 2.0f;
    static constexpr float kMaxDistance = 25.0f;

    FMOD::System* system_ = nullptr;
    Vec3 listenerPosition_{};
    Vec3 listenerForward_{0.0f, 1.0f, 0.0f};
    Vec3 listenerUp_{0.0f, 0.0f, 1.0f};
    std::mutex mutex_;

  public:
    std::unordered_map<uint64_t, PlayerId> discordToPlayer;
    std::unordered_map<PlayerId, Vec3> playerPositions;
    std::unordered_map<uint64_t, VoiceStream> voiceStreams;
};
