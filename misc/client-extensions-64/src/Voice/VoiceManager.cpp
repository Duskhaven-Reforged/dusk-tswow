#include "VoiceManager.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <vector>

namespace
{
constexpr int32_t kVoiceSampleRate = 48000;
constexpr uint64_t kVoiceChannels = 1;
constexpr uint32_t kVoiceBytesPerSample = sizeof(int16_t);
constexpr uint32_t kOneSecondBytes =
    static_cast<uint32_t>(kVoiceSampleRate * kVoiceChannels * kVoiceBytesPerSample);
}

VoiceManager::~VoiceManager()
{
    Shutdown();
}

bool VoiceManager::InitFMOD()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (system_)
    {
        return true;
    }

    FMOD_RESULT result = FMOD::System_Create(&system_);
    if (result != FMOD_OK)
    {
        system_ = nullptr;
        LogFMODError("System_Create", result);
        return false;
    }

    result = system_->init(512, FMOD_INIT_3D_RIGHTHANDED, nullptr);
    if (result != FMOD_OK)
    {
        LogFMODError("System::init", result);
        system_->release();
        system_ = nullptr;
        return false;
    }

    result = system_->set3DSettings(1.0f, 1.0f, 1.0f);
    if (result != FMOD_OK)
    {
        LogFMODError("System::set3DSettings", result);
    }

    return true;
}

void VoiceManager::Shutdown()
{
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& entry : voiceStreams)
    {
        VoiceStream& stream = entry.second;
        if (stream.channel)
        {
            stream.channel->stop();
            stream.channel = nullptr;
        }
        if (stream.sound)
        {
            stream.sound->release();
            stream.sound = nullptr;
        }
    }

    voiceStreams.clear();
    discordToPlayer.clear();
    playerPositions.clear();

    if (system_)
    {
        system_->close();
        system_->release();
        system_ = nullptr;
    }
}

VoiceStream* VoiceManager::CreateVoiceStream(uint64_t userId)
{
    if (!system_)
    {
        return nullptr;
    }

    auto existing = voiceStreams.find(userId);
    if (existing != voiceStreams.end())
    {
        return &existing->second;
    }

    FMOD_CREATESOUNDEXINFO info = {};
    info.cbsize = sizeof(info);
    info.length = kOneSecondBytes;
    info.numchannels = static_cast<int>(kVoiceChannels);
    info.defaultfrequency = kVoiceSampleRate;
    info.format = FMOD_SOUND_FORMAT_PCM16;

    FMOD::Sound* sound = nullptr;
    FMOD_RESULT result = system_->createSound(
        nullptr,
        FMOD_OPENUSER | FMOD_OPENRAW | FMOD_LOOP_NORMAL | FMOD_3D | FMOD_CREATESAMPLE,
        &info,
        &sound);
    if (result != FMOD_OK)
    {
        LogFMODError("System::createSound", result);
        return nullptr;
    }

    sound->set3DMinMaxDistance(kMinDistance, kMaxDistance);

    void* lockA = nullptr;
    void* lockB = nullptr;
    unsigned int lenA = 0;
    unsigned int lenB = 0;
    result = sound->lock(0, kOneSecondBytes, &lockA, &lockB, &lenA, &lenB);
    if (result == FMOD_OK)
    {
        if (lockA && lenA > 0)
        {
            std::memset(lockA, 0, lenA);
        }
        if (lockB && lenB > 0)
        {
            std::memset(lockB, 0, lenB);
        }
        sound->unlock(lockA, lockB, lenA, lenB);
    }

    FMOD::Channel* channel = nullptr;
    result = system_->playSound(sound, nullptr, false, &channel);
    if (result != FMOD_OK)
    {
        LogFMODError("System::playSound", result);
        sound->release();
        return nullptr;
    }

    if (channel)
    {
        channel->setMode(FMOD_3D | FMOD_LOOP_NORMAL);
    }

    VoiceStream stream;
    stream.sound = sound;
    stream.channel = channel;
    stream.writePos = 0;
    stream.bufferSize = kOneSecondBytes;

    auto [inserted, _] = voiceStreams.emplace(userId, stream);
    std::cout << "Created FMOD voice stream for Discord user " << userId << std::endl;
    return &inserted->second;
}

void VoiceManager::OnUserAudio(uint64_t userId,
                               int16_t* pcm,
                               uint64_t samplesPerChannel,
                               int32_t sampleRate,
                               uint64_t channels,
                               bool& outShouldMuteData)
{
    outShouldMuteData = true;
    if (!pcm || samplesPerChannel == 0)
    {
        return;
    }

    if (sampleRate != kVoiceSampleRate)
    {
        std::cerr << "Skipping voice frame from " << userId << ": unsupported sample rate " << sampleRate << std::endl;
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    VoiceStream* stream = CreateVoiceStream(userId);
    if (!stream)
    {
        return;
    }

    if (channels == kVoiceChannels)
    {
        const uint32_t byteCount = static_cast<uint32_t>(
            std::min<uint64_t>(samplesPerChannel * kVoiceBytesPerSample, stream->bufferSize));
        WritePcm(*stream, pcm, byteCount);
        return;
    }

    std::vector<int16_t> mono;
    mono.resize(static_cast<size_t>(samplesPerChannel));
    for (uint64_t sample = 0; sample < samplesPerChannel; ++sample)
    {
        int32_t mixed = 0;
        for (uint64_t channel = 0; channel < channels; ++channel)
        {
            mixed += pcm[sample * channels + channel];
        }
        mono[static_cast<size_t>(sample)] = static_cast<int16_t>(mixed / static_cast<int32_t>(channels));
    }

    const uint32_t byteCount = static_cast<uint32_t>(
        std::min<uint64_t>(mono.size() * kVoiceBytesPerSample, stream->bufferSize));
    WritePcm(*stream, mono.data(), byteCount);
}

void VoiceManager::SetListenerPosition(Vec3 position, Vec3 forward, Vec3 up)
{
    std::lock_guard<std::mutex> lock(mutex_);
    listenerPosition_ = position;
    listenerForward_ = forward;
    listenerUp_ = up;
}

void VoiceManager::SetPlayerMapping(uint64_t discordUserId, PlayerId playerId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    discordToPlayer[discordUserId] = playerId;
}

void VoiceManager::RemovePlayerMapping(uint64_t discordUserId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    discordToPlayer.erase(discordUserId);
}

void VoiceManager::SetPlayerPosition(PlayerId playerId, Vec3 position)
{
    std::lock_guard<std::mutex> lock(mutex_);
    playerPositions[playerId] = position;
}

void VoiceManager::RemovePlayerPosition(PlayerId playerId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    playerPositions.erase(playerId);
}

void VoiceManager::UpdateVoicePositions()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!system_)
    {
        return;
    }

    FMOD_VECTOR listener = ToFMOD(listenerPosition_);
    FMOD_VECTOR velocity{0.0f, 0.0f, 0.0f};
    FMOD_VECTOR forward = ToFMOD(listenerForward_);
    FMOD_VECTOR up = ToFMOD(listenerUp_);
    system_->set3DListenerAttributes(0, &listener, &velocity, &forward, &up);

    for (auto& entry : voiceStreams)
    {
        auto playerIt = discordToPlayer.find(entry.first);
        if (playerIt == discordToPlayer.end())
        {
            continue;
        }

        auto positionIt = playerPositions.find(playerIt->second);
        if (positionIt == playerPositions.end())
        {
            continue;
        }

        FMOD_VECTOR speaker = ToFMOD(positionIt->second);
        VoiceStream& stream = entry.second;
        if (stream.channel)
        {
            stream.channel->set3DAttributes(&speaker, &velocity);
        }
    }
}

void VoiceManager::Update()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (system_)
    {
        system_->update();
    }
}

FMOD_VECTOR VoiceManager::ToFMOD(Vec3 value) const
{
    return FMOD_VECTOR{value.x, value.y, value.z};
}

void VoiceManager::WritePcm(VoiceStream& stream, const int16_t* pcm, uint32_t byteCount)
{
    if (!stream.sound || byteCount == 0)
    {
        return;
    }

    byteCount = std::min(byteCount, stream.bufferSize);

    void* lockA = nullptr;
    void* lockB = nullptr;
    unsigned int lenA = 0;
    unsigned int lenB = 0;

    FMOD_RESULT result = stream.sound->lock(stream.writePos, byteCount, &lockA, &lockB, &lenA, &lenB);
    if (result != FMOD_OK)
    {
        LogFMODError("Sound::lock", result);
        return;
    }

    const uint8_t* source = reinterpret_cast<const uint8_t*>(pcm);
    if (lockA && lenA > 0)
    {
        std::memcpy(lockA, source, lenA);
        source += lenA;
    }
    if (lockB && lenB > 0)
    {
        std::memcpy(lockB, source, lenB);
    }

    stream.sound->unlock(lockA, lockB, lenA, lenB);
    stream.writePos = (stream.writePos + byteCount) % stream.bufferSize;
}

void VoiceManager::LogFMODError(const char* action, FMOD_RESULT result) const
{
    std::cerr << "FMOD " << action << " failed: " << result << std::endl;
}
