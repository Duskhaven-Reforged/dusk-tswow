#define DISCORDPP_IMPLEMENTATION
#include "discordpp.h"
#include "DiscordManager.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <iostream>
#include <optional>
#include <utility>

namespace
{
constexpr uint64_t kAccessTokenRefreshLeadTimeMs = 24ull * 60ull * 60ull * 1000ull;

std::string TrimWhitespace(std::string value)
{
    auto isSpace = [](unsigned char ch) { return std::isspace(ch) != 0; };
    auto begin = std::find_if_not(value.begin(), value.end(), isSpace);
    if (begin == value.end())
    {
        return {};
    }

    auto end = std::find_if_not(value.rbegin(), value.rend(), isSpace).base();
    return std::string(begin, end);
}

std::optional<std::string> ClampActivityField(std::string value, size_t maxLength = 128)
{
    value = TrimWhitespace(std::move(value));
    if (value.size() < 2)
    {
        return std::nullopt;
    }

    if (value.size() > maxLength)
    {
        value.resize(maxLength);
    }

    return value;
}

std::optional<std::string> BuildActivityDetails(
    std::string const& characterName,
    uint32_t characterLevel,
    std::string const& className)
{
    std::string details;
    if (!characterName.empty())
    {
        details = characterName;
    }

    if (characterLevel > 0)
    {
        if (!details.empty())
        {
            details += ", Level";
        }
        details += std::to_string(characterLevel);
    }

    if (!className.empty())
    {
        if (!details.empty())
        {
            details += " ";
        }
        details += className;
    }

    return ClampActivityField(std::move(details));
}

std::optional<std::string> BuildActivityState(std::string const& zoneName)
{
    if (zoneName.empty())
    {
        return std::nullopt;
    }

    return ClampActivityField("In " + zoneName);
}

uint64_t CurrentUnixMilliseconds()
{
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
}

bool ShouldRefreshCachedAccessToken(DiscordTokenStore::CachedToken const& token)
{
    if (token.accessToken.empty() || token.accessTokenExpiresAtMs == 0)
    {
        return true;
    }

    uint64_t now = CurrentUnixMilliseconds();
    return now + kAccessTokenRefreshLeadTimeMs >= token.accessTokenExpiresAtMs;
}

bool HasUnexpiredCachedAccessToken(DiscordTokenStore::CachedToken const& token)
{
    if (token.accessToken.empty() || token.accessTokenExpiresAtMs == 0)
    {
        return false;
    }

    return CurrentUnixMilliseconds() < token.accessTokenExpiresAtMs;
}

std::string BuildRequestedScopes()
{
    return discordpp::Client::GetDefaultPresenceScopes() + " " +
           discordpp::Client::GetDefaultCommunicationScopes();
}

std::string DeviceIdOrEmpty(const discordpp::AudioDevice& device)
{
    return device ? device.Id() : std::string{};
}

uint32_t ClientResultErrorCode(const discordpp::ClientResult& result)
{
    const int32_t errorCode = result.ErrorCode();
    return errorCode > 0 ? static_cast<uint32_t>(errorCode)
                         : static_cast<uint32_t>(result.Type());
}

std::string ClientResultMessage(const discordpp::ClientResult& result)
{
    std::string message = TrimWhitespace(result.Error());
    return message.empty() ? result.ToString() : message;
}
}

DiscordManager *DiscordManager::instance_ = nullptr;

DiscordManager *DiscordManager::Get()
{
    return instance_;
}

bool DiscordManager::Start(uint64_t appId)
{
    if (running_.exchange(true))
        return false;
    instance_ = this;
    th_ = std::thread([this, appId]
                      { ThreadMain(appId); });
    return true;
}

void DiscordManager::Stop()
{
    if (!running_.exchange(false))
        return;
    if (instance_ == this)
        instance_ = nullptr;
    if (th_.joinable())
        th_.join();
}

void DiscordManager::ThreadMain(uint64_t appId)
{
    std::cout << "🚀 Initializing Discord SDK in DiscordManager...\n";
    applicationId_ = appId;
    usingCachedAccessToken_ = false;
    refreshAttempted_ = false;
    interactiveAuthAttempted_ = false;
    interactiveAuthInProgress_ = false;
    cachedToken_.reset();

    // Create client
    client_ = std::make_shared<discordpp::Client>();

    // Set up logging callback
    // TODO: make this write to a file or something else.
    client_->AddLogCallback([](auto message, auto severity)
                            { std::cout << "[" << EnumToString(severity) << "] " << message << std::endl; }, discordpp::LoggingSeverity::Info);

    // Set up status callback
    // NOTE: We capturing 'this' safely because ThreadMain is a member, but be careful if *this is destroyed.
    // However, Stop() joins the thread, so *this should be valid while thread runs.
    client_->SetStatusChangedCallback([this](discordpp::Client::Status status, discordpp::Client::Error error, int32_t errorDetail)
                                      {
        std::cout << "🔄 Status changed: " << discordpp::Client::StatusToString(status) << std::endl;
        if (status == discordpp::Client::Status::Ready) {
            OnConnect();
        } else if (error != discordpp::Client::Error::None) {
            OnDisconnect(error, errorDetail);
        } });

    BeginAuthentication(appId);

    // Main loop
    while (running_)
    {
        std::function<void()> fn;
        while (cmds_.TryPop(fn))
        {
            try
            {
                fn();
            }
            catch (...)
            {
            }
        }
        discordpp::RunCallbacks();
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 10ms is what discord had in docs.
    }

    // Cleanup
    isReady_ = false;
    inCall_ = false;
    activeLobbyId_ = 0;
    selfMuted_ = false;
    selfDeafened_ = false;
    applicationId_ = 0;
    usingCachedAccessToken_ = false;
    refreshAttempted_ = false;
    interactiveAuthAttempted_ = false;
    interactiveAuthInProgress_ = false;
    cachedToken_.reset();
    ClearCallCaches();
    call_.reset();
    client_.reset();
}

void DiscordManager::OnConnect()
{
    std::cout << "✅ Client is ready! You can now call SDK functions.\n";
    if (client_)
    {
        std::cout << "👥 Friends Count: " << client_->GetRelationships().size() << std::endl;
    }
    isReady_ = true;

    UpdateActivity();
    ConfigureVoiceUpdateCallbacks();
    PushCallStatusUpdate(call_ && *call_ ? call_->GetStatus() : discordpp::Call::Status::Disconnected);
    PushSelfStateUpdate();
    PushParticipantsSnapshot();
    PushInputDevicesSnapshot();
    PushOutputDevicesSnapshot();
}

void DiscordManager::BeginAuthentication(uint64_t appId)
{
    if (!client_)
    {
        return;
    }

    auto cachedToken = DiscordTokenStore::Load(appId);
    if (cachedToken.has_value())
    {
        cachedToken_ = std::move(cachedToken);

        if (!ShouldRefreshCachedAccessToken(*cachedToken_))
        {
            usingCachedAccessToken_ = true;
            std::cout << "[Auth] Using cached Discord access token\n";
            ApplyAccessToken(cachedToken_->tokenType, cachedToken_->accessToken);
            return;
        }

        if (!cachedToken_->refreshToken.empty())
        {
            refreshAttempted_ = true;
            std::cout << "[Auth] Refreshing cached Discord token\n";
            RefreshCachedToken(appId, cachedToken_->refreshToken);
            return;
        }
    }

    BeginInteractiveAuthorization(appId);
}

void DiscordManager::BeginInteractiveAuthorization(uint64_t appId)
{
    if (!client_ || interactiveAuthInProgress_)
    {
        return;
    }

    interactiveAuthAttempted_ = true;
    interactiveAuthInProgress_ = true;

    auto codeVerifier = client_->CreateAuthorizationCodeVerifier();
    discordpp::AuthorizationArgs args{};
    args.SetClientId(appId);
    args.SetScopes(BuildRequestedScopes());
    args.SetCodeChallenge(codeVerifier.Challenge());

    std::cout << "[Auth] Starting interactive Discord authorization\n";
    client_->Authorize(args, [this, codeVerifier, appId](auto result, auto code, auto redirectUri)
                       {
        if (!result.Successful()) {
            interactiveAuthInProgress_ = false;
            std::cerr << "[Auth] Authentication error: " << result.Error() << std::endl;
            return;
        }
        std::cout << "[Auth] Authorization successful, getting access token...\n";
        client_->GetToken(appId, code, codeVerifier.Verifier(), redirectUri,
            [this](discordpp::ClientResult result,
                std::string accessToken,
                std::string refreshToken,
                discordpp::AuthorizationTokenType tokenType,
                int32_t expiresIn,
                std::string scopes) {
                    interactiveAuthInProgress_ = false;
                    if (!result.Successful()) {
                        std::cerr << "[Auth] Failed to exchange authorization code: " << result.Error() << std::endl;
                        return;
                    }

                    std::cout << "[Auth] Access token received from interactive authorization\n";
                    SaveCachedToken(tokenType, accessToken, refreshToken, expiresIn, scopes);
                    usingCachedAccessToken_ = false;
                    refreshAttempted_ = true;
                    ApplyAccessToken(tokenType, accessToken);
            }); });
}

void DiscordManager::RefreshCachedToken(uint64_t appId, std::string refreshToken)
{
    if (!client_)
    {
        return;
    }

    client_->RefreshToken(
        appId,
        refreshToken,
        [this](discordpp::ClientResult result,
               std::string accessToken,
               std::string newRefreshToken,
               discordpp::AuthorizationTokenType tokenType,
               int32_t expiresIn,
               std::string scopes)
        {
            if (!result.Successful())
            {
                std::cerr << "[Auth] Failed to refresh cached Discord token: " << result.Error() << std::endl;
                if (cachedToken_.has_value() && HasUnexpiredCachedAccessToken(*cachedToken_))
                {
                    usingCachedAccessToken_ = true;
                    std::cout << "[Auth] Falling back to still-valid cached Discord access token\n";
                    ApplyAccessToken(cachedToken_->tokenType, cachedToken_->accessToken);
                    return;
                }

                if (!interactiveAuthAttempted_)
                {
                    ClearCachedToken();
                    BeginInteractiveAuthorization(applicationId_);
                }
                return;
            }

            std::cout << "[Auth] Cached Discord token refreshed successfully\n";
            SaveCachedToken(tokenType, accessToken, newRefreshToken, expiresIn, scopes);
            usingCachedAccessToken_ = false;
            ApplyAccessToken(tokenType, accessToken);
        });
}

void DiscordManager::ApplyAccessToken(discordpp::AuthorizationTokenType tokenType, std::string accessToken)
{
    if (!client_)
    {
        return;
    }

    client_->UpdateToken(tokenType, accessToken, [this](discordpp::ClientResult result)
                         {
        if (result.Successful()) {
            std::cout << "[Auth] Token updated, connecting to Discord...\n";
            client_->Connect();
            return;
        }

        std::cerr << "[Auth] Failed to update Discord token: " << result.Error() << std::endl;
        if (usingCachedAccessToken_ &&
            cachedToken_.has_value() &&
            !refreshAttempted_ &&
            !cachedToken_->refreshToken.empty()) {
            usingCachedAccessToken_ = false;
            refreshAttempted_ = true;
            std::cout << "[Auth] Cached access token rejected, attempting refresh\n";
            RefreshCachedToken(applicationId_, cachedToken_->refreshToken);
            return;
        }

        ClearCachedToken();
        if (!interactiveAuthAttempted_) {
            BeginInteractiveAuthorization(applicationId_);
        } });
}

void DiscordManager::SaveCachedToken(
    discordpp::AuthorizationTokenType tokenType,
    std::string accessToken,
    std::string refreshToken,
    int32_t expiresIn,
    std::string scopes)
{
    DiscordTokenStore::CachedToken token{};
    token.tokenType = tokenType;
    token.accessToken = std::move(accessToken);
    token.refreshToken = std::move(refreshToken);
    token.scopes = std::move(scopes);
    token.accessTokenExpiresAtMs = expiresIn > 0
        ? CurrentUnixMilliseconds() + static_cast<uint64_t>(expiresIn) * 1000ull
        : 0;

    cachedToken_ = token;
    if (!DiscordTokenStore::Save(applicationId_, token))
    {
        std::cerr << "[Auth] Failed to persist Discord token cache\n";
    }
}

void DiscordManager::ClearCachedToken()
{
    cachedToken_.reset();
    usingCachedAccessToken_ = false;
    if (applicationId_ != 0)
    {
        DiscordTokenStore::Clear(applicationId_);
    }
}

void DiscordManager::UpdateActivity()
{
    if (!isReady_ || !client_)
        return;

    auto details = BuildActivityDetails(activityCharacterName_, activityCharacterLevel_, activityClassName_);
    auto state = BuildActivityState(activityZoneName_);
    if (!details.has_value() && !state.has_value())
    {
        client_->ClearRichPresence();
        std::cout << "🎮 Rich Presence cleared\n";
        return;
    }

    discordpp::Activity activity;
    activity.SetType(discordpp::ActivityTypes::Playing);
    if (details.has_value())
    {
        activity.SetDetails(*details);
        activity.SetStatusDisplayType(
            std::optional<discordpp::StatusDisplayTypes>(discordpp::StatusDisplayTypes::Details));
    }
    else if (state.has_value())
    {
        activity.SetStatusDisplayType(
            std::optional<discordpp::StatusDisplayTypes>(discordpp::StatusDisplayTypes::State));
    }

    if (state.has_value())
    {
        activity.SetState(*state);
    }

    if (activityStartMs_.has_value())
    {
        discordpp::ActivityTimestamps timestamps;
        timestamps.SetStart(*activityStartMs_);
        activity.SetTimestamps(std::optional<discordpp::ActivityTimestamps>(timestamps));
    }

    client_->UpdateRichPresence(activity, [](discordpp::ClientResult result)
                                {
        if (result.Successful()) {
            std::cout << "🎮 Rich Presence updated successfully!\n";
        } else {
            std::cerr << "❌ Rich Presence update failed\n";
        } });
}

void DiscordManager::SetGamePresence(
    std::string characterName,
    uint32_t characterLevel,
    std::string className,
    std::string zoneName)
{
    Enqueue(
        [this,
         characterName = std::move(characterName),
         characterLevel,
         className = std::move(className),
         zoneName = std::move(zoneName)]() mutable
        {
            SetGamePresence_Internal(
                std::move(characterName),
                characterLevel,
                std::move(className),
                std::move(zoneName));
        });
}

void DiscordManager::ClearGamePresence()
{
    Enqueue([this]() { ClearGamePresence_Internal(); });
}

// -----------------------------------------------------------------------------
// Voice (public thread-safe wrappers)
// -----------------------------------------------------------------------------

void DiscordManager::JoinLobbyAndCall(const std::string &lobbySecret)
{
    Enqueue([this, lobbySecret]()
            {
		if (!isReady_ || !client_)
			return;

		client_->CreateOrJoinLobby(lobbySecret, [this](const discordpp::ClientResult& result, uint64_t lobbyId) {
			if (!result.Successful()) {
				std::cerr << "❌ Failed to join lobby: " << result.Error() << std::endl;
				PushVoiceErrorUpdate(Opcode::CMSG_VOICE_START_CALL,
					ClientResultErrorCode(result),
					ClientResultMessage(result));
				return;
			}
			std::cout << "🎮 Successfully joined lobby! (" << lobbyId << ")\n";
			StartCall_Internal(lobbyId);
		}); });
}

void DiscordManager::StartCall(uint64_t lobbyId)
{
    Enqueue([this, lobbyId]()
            { StartCall_Internal(lobbyId); });
}

void DiscordManager::LeaveCall()
{
    Enqueue([this]()
            { LeaveCall_Internal(); });
}

void DiscordManager::EndAllCalls()
{
    Enqueue([this]()
            { EndAllCalls_Internal(); });
}

void DiscordManager::SetInputVolume(float volume01)
{
    Enqueue([this, volume01]()
            { SetInputVolume_Internal(volume01); });
}

void DiscordManager::SetOutputVolume(float volume01)
{
    Enqueue([this, volume01]()
            { SetOutputVolume_Internal(volume01); });
}

void DiscordManager::SetSelfMute(bool muted)
{
    Enqueue([this, muted]()
            { SetSelfMute_Internal(muted); });
}

void DiscordManager::SetSelfDeaf(bool deafened)
{
    Enqueue([this, deafened]()
            { SetSelfDeaf_Internal(deafened); });
}

void DiscordManager::SetLocalMute(uint64_t userId, bool muted)
{
    Enqueue([this, userId, muted]()
            { SetLocalMute_Internal(userId, muted); });
}

void DiscordManager::SetParticipantVolume(uint64_t userId, float volume01)
{
    Enqueue([this, userId, volume01]()
            { SetParticipantVolume_Internal(userId, volume01); });
}

void DiscordManager::SetPTTActive(bool active)
{
    Enqueue([this, active]()
            { SetPTTActive_Internal(active); });
}

void DiscordManager::SetPTTReleaseDelay(uint32_t releaseDelayMs)
{
    Enqueue([this, releaseDelayMs]()
            { SetPTTReleaseDelay_Internal(releaseDelayMs); });
}

void DiscordManager::SetVADThreshold(bool automatic, float threshold)
{
    Enqueue([this, automatic, threshold]()
            { SetVADThreshold_Internal(automatic, threshold); });
}

void DiscordManager::SetAudioMode(discordpp::AudioModeType mode)
{
    Enqueue([this, mode]()
            { SetAudioMode_Internal(mode); });
}

void DiscordManager::SetInputDevice(const std::string &deviceId)
{
    Enqueue([this, deviceId]()
            { SetInputDevice_Internal(deviceId); });
}

void DiscordManager::SetOutputDevice(const std::string &deviceId)
{
    Enqueue([this, deviceId]()
            { SetOutputDevice_Internal(deviceId); });
}

void DiscordManager::SetAutomaticGainControl(bool enabled)
{
    Enqueue([this, enabled]()
            { SetAutomaticGainControl_Internal(enabled); });
}

void DiscordManager::SetEchoCancellation(bool enabled)
{
    Enqueue([this, enabled]()
            { SetEchoCancellation_Internal(enabled); });
}

void DiscordManager::SetNoiseSuppression(bool enabled)
{
    Enqueue([this, enabled]()
            { SetNoiseSuppression_Internal(enabled); });
}

void DiscordManager::ConfigureVoiceUpdateCallbacks()
{
    if (!client_)
    {
        return;
    }

    client_->SetDeviceChangeCallback([this](std::vector<discordpp::AudioDevice> inputDevices,
                                            std::vector<discordpp::AudioDevice> outputDevices)
                                     {
        (void)inputDevices;
        (void)outputDevices;
        PushInputDevicesSnapshot();
        PushOutputDevicesSnapshot(); });
}

void DiscordManager::PushCallStatusUpdate(
    discordpp::Call::Status status,
    discordpp::Call::Error error,
    int32_t errorDetail)
{
    auto packet = PacketBuilder::CreatePacket(Opcode::SMSG_VOICE_CALL_STATUS);
    packet.writeUInt64(activeLobbyId_.load());
    packet.writeUInt32(static_cast<uint32_t>(status));
    packet.writeUInt32(static_cast<uint32_t>(error));
    packet.writeUInt32(static_cast<uint32_t>(errorDetail));
    updatePipe_.Send(packet);
}

void DiscordManager::PushVoiceErrorUpdate(Opcode sourceOpcode, uint32_t errorCode, std::string message)
{
    auto packet = PacketBuilder::CreatePacket(Opcode::SMSG_VOICE_ERROR);
    packet.writeUInt32(sourceOpcode.raw());
    packet.writeUInt32(errorCode);
    packet.writeString(message);
    updatePipe_.Send(packet);
}

void DiscordManager::PushSelfStateUpdate()
{
    auto packet = PacketBuilder::CreatePacket(Opcode::SMSG_VOICE_SELF_STATE);
    packet.writeUInt64(activeLobbyId_.load());
    packet.writeBool(selfMuted_.load());
    packet.writeBool(selfDeafened_.load());
    packet.writeFloat(inputVolume_.load());
    packet.writeFloat(outputVolume_.load());
    packet.writeUInt32(audioMode_.load());
    packet.writeUInt32(pttReleaseDelayMs_.load());
    packet.writeBool(vadAutomatic_.load());
    packet.writeFloat(vadThreshold_.load());
    updatePipe_.Send(packet);
}

void DiscordManager::PushSpeakingUpdate(uint64_t userId, bool speaking)
{
    auto packet = PacketBuilder::CreatePacket(Opcode::SMSG_VOICE_SPEAKING);
    packet.writeUInt64(activeLobbyId_.load());
    packet.writeUInt64(userId);
    packet.writeBool(speaking);
    updatePipe_.Send(packet);
}

void DiscordManager::PushParticipantsClear(uint64_t lobbyId)
{
    auto clearPacket = PacketBuilder::CreatePacket(Opcode::SMSG_VOICE_PARTICIPANTS);
    clearPacket.writeUInt64(lobbyId);
    updatePipe_.Send(clearPacket);
}

void DiscordManager::PushParticipantsSnapshot()
{
    PushParticipantsClear(activeLobbyId_.load());

    if (!call_ || !(*call_))
    {
        return;
    }

    for (uint64_t userId : call_->GetParticipants())
    {
        PushParticipantStateUpdate(userId);
    }
}

void DiscordManager::PushParticipantStateUpdate(uint64_t userId)
{
    if (!call_ || !(*call_))
    {
        return;
    }

    bool speaking = false;
    {
        std::lock_guard<std::mutex> lock(voiceCacheMutex_);
        auto itr = speakingCache_.find(userId);
        speaking = itr != speakingCache_.end() ? itr->second : false;
    }

    bool selfMuted = false;
    bool selfDeafened = false;
    if (auto voiceState = call_->GetVoiceStateHandle(userId); voiceState.has_value())
    {
        selfMuted = voiceState->SelfMute();
        selfDeafened = voiceState->SelfDeaf();
    }

    auto packet = PacketBuilder::CreatePacket(Opcode::SMSG_VOICE_PARTICIPANT_STATE);
    packet.writeUInt64(activeLobbyId_.load());
    packet.writeUInt64(userId);
    packet.writeBool(speaking);
    packet.writeBool(selfMuted);
    packet.writeBool(selfDeafened);
    packet.writeBool(call_->GetLocalMute(userId));
    packet.writeFloat(call_->GetParticipantVolume(userId));
    updatePipe_.Send(packet);
}

void DiscordManager::PushInputDevicesSnapshot()
{
    if (!client_)
    {
        return;
    }

    client_->GetCurrentInputDevice([this](discordpp::AudioDevice currentDevice)
                                   {
        if (!client_) {
            return;
        }

        client_->GetInputDevices([this, currentId = DeviceIdOrEmpty(currentDevice)](
                                     std::vector<discordpp::AudioDevice> devices)
                                 {
            auto clearPacket = PacketBuilder::CreatePacket(Opcode::SMSG_VOICE_DEVICES);
            clearPacket.writeBool(true);
            updatePipe_.Send(clearPacket);

            for (const discordpp::AudioDevice& device : devices) {
                const std::string deviceId = device.Id();
                auto packet = PacketBuilder::CreatePacket(Opcode::SMSG_VOICE_DEVICE_STATE);
                packet.writeBool(true);
                packet.writeString(deviceId);
                packet.writeString(device.Name());
                packet.writeBool(device.IsDefault());
                packet.writeBool(!currentId.empty() && deviceId == currentId);
                updatePipe_.Send(packet);
            } }); });
}

void DiscordManager::PushOutputDevicesSnapshot()
{
    if (!client_)
    {
        return;
    }

    client_->GetCurrentOutputDevice([this](discordpp::AudioDevice currentDevice)
                                    {
        if (!client_) {
            return;
        }

        client_->GetOutputDevices([this, currentId = DeviceIdOrEmpty(currentDevice)](
                                      std::vector<discordpp::AudioDevice> devices)
                                  {
            auto clearPacket = PacketBuilder::CreatePacket(Opcode::SMSG_VOICE_DEVICES);
            clearPacket.writeBool(false);
            updatePipe_.Send(clearPacket);

            for (const discordpp::AudioDevice& device : devices) {
                const std::string deviceId = device.Id();
                auto packet = PacketBuilder::CreatePacket(Opcode::SMSG_VOICE_DEVICE_STATE);
                packet.writeBool(false);
                packet.writeString(deviceId);
                packet.writeString(device.Name());
                packet.writeBool(device.IsDefault());
                packet.writeBool(!currentId.empty() && deviceId == currentId);
                updatePipe_.Send(packet);
            } }); });
}

void DiscordManager::SetGamePresence_Internal(
    std::string characterName,
    uint32_t characterLevel,
    std::string className,
    std::string zoneName)
{
    characterName = TrimWhitespace(std::move(characterName));
    className = TrimWhitespace(std::move(className));
    zoneName = TrimWhitespace(std::move(zoneName));

    const bool changed = characterName != activityCharacterName_ ||
                         characterLevel != activityCharacterLevel_ ||
                         className != activityClassName_ ||
                         zoneName != activityZoneName_;
    if (!changed)
    {
        return;
    }

    activityCharacterName_ = std::move(characterName);
    activityCharacterLevel_ = characterLevel;
    activityClassName_ = std::move(className);
    activityZoneName_ = std::move(zoneName);

    if (activityCharacterName_.empty() &&
        activityCharacterLevel_ == 0 &&
        activityClassName_.empty() &&
        activityZoneName_.empty())
    {
        activityStartMs_.reset();
    }
    else if (!activityStartMs_.has_value())
    {
        activityStartMs_ = CurrentUnixMilliseconds();
    }

    UpdateActivity();
}

void DiscordManager::ClearGamePresence_Internal()
{
    if (activityCharacterName_.empty() &&
        activityCharacterLevel_ == 0 &&
        activityClassName_.empty() &&
        activityZoneName_.empty() &&
        !activityStartMs_.has_value())
    {
        return;
    }

    activityCharacterName_.clear();
    activityCharacterLevel_ = 0;
    activityClassName_.clear();
    activityZoneName_.clear();
    activityStartMs_.reset();

    UpdateActivity();
}

std::vector<uint64_t> DiscordManager::GetParticipants() const
{
    std::lock_guard<std::mutex> lock(voiceCacheMutex_);
    return participantsCache_;
}

bool DiscordManager::IsUserSpeaking(uint64_t userId) const
{
    std::lock_guard<std::mutex> lock(voiceCacheMutex_);
    auto it = speakingCache_.find(userId);
    return it != speakingCache_.end() ? it->second : false;
}

void DiscordManager::ResetCallState(uint64_t previousLobbyId)
{
    inCall_ = false;
    activeLobbyId_ = 0;
    ClearCallCaches();
    call_.reset();
    PushParticipantsClear(previousLobbyId);
    PushSelfStateUpdate();
}

// -----------------------------------------------------------------------------
// Voice (internal SDK-thread implementations)
// -----------------------------------------------------------------------------

void DiscordManager::StartCall_Internal(uint64_t lobbyId)
{
    if (!isReady_ || !client_)
        return;

    activeLobbyId_ = lobbyId;

    // StartCall returns a Call handle (invalid if it fails).
    call_ = client_->StartCall(lobbyId);
    if (!call_ || !(*call_))
    {
        std::cerr << "❌ StartCall failed for lobby/channel " << lobbyId << "\n";
        PushCallStatusUpdate(discordpp::Call::Status::Disconnected);
        PushVoiceErrorUpdate(Opcode::CMSG_VOICE_START_CALL,
                             0,
                             "StartCall returned an invalid Discord call handle");
        ResetCallState(lobbyId);
        return;
    }

    inCall_ = true;
    std::cout << "🎤 Voice call started (channel/lobby " << lobbyId << ")\n";

    // Apply cached settings immediately.
    call_->SetSelfMute(selfMuted_.load());
    call_->SetSelfDeaf(selfDeafened_.load());
    call_->SetAudioMode(static_cast<discordpp::AudioModeType>(audioMode_.load()));
    call_->SetPTTReleaseDelay(pttReleaseDelayMs_.load());
    call_->SetVADThreshold(vadAutomatic_.load(), vadThreshold_.load());
    call_->SetPTTActive(false);

    HookCallCallbacks();
    {
        std::lock_guard<std::mutex> lock(voiceCacheMutex_);
        participantsCache_ = call_->GetParticipants();
        speakingCache_.clear();
    }

    PushCallStatusUpdate(call_->GetStatus());
    PushSelfStateUpdate();
    PushParticipantsSnapshot();
}

void DiscordManager::LeaveCall_Internal()
{
    if (!isReady_ || !client_)
        return;

    const uint64_t previousLobbyId = activeLobbyId_.load();
    if (!call_ || !(*call_))
    {
        ResetCallState(previousLobbyId);
        return;
    }

    client_->EndCall(previousLobbyId, [this]()
                     { std::cout << "🔇 Call ended" << std::endl; });

    PushCallStatusUpdate(discordpp::Call::Status::Disconnected);
    ResetCallState(previousLobbyId);
}

void DiscordManager::EndAllCalls_Internal()
{
    if (!isReady_ || !client_)
        return;

    const uint64_t previousLobbyId = activeLobbyId_.load();
    client_->EndCalls([]()
                      { std::cout << "🔇 All calls ended successfully" << std::endl; });
    PushCallStatusUpdate(discordpp::Call::Status::Disconnected);
    ResetCallState(previousLobbyId);
}

void DiscordManager::SetInputVolume_Internal(float volume01)
{
    if (!isReady_ || !client_)
        return;
    inputVolume_ = volume01;
    client_->SetInputVolume(volume01);
    PushSelfStateUpdate();
}

void DiscordManager::SetOutputVolume_Internal(float volume01)
{
    if (!isReady_ || !client_)
        return;
    outputVolume_ = volume01;
    client_->SetOutputVolume(volume01);
    PushSelfStateUpdate();
}

void DiscordManager::SetSelfMute_Internal(bool muted)
{
    if (!isReady_ || !client_)
        return;

    selfMuted_ = muted;
    if (call_ && *call_)
    {
        call_->SetSelfMute(muted);
    }
    else
    {
        // Applies to any active calls.
        client_->SetSelfMuteAll(muted);
    }
    PushSelfStateUpdate();
}

void DiscordManager::SetSelfDeaf_Internal(bool deafened)
{
    if (!isReady_ || !client_)
        return;

    selfDeafened_ = deafened;
    if (call_ && *call_)
    {
        call_->SetSelfDeaf(deafened);
    }
    else
    {
        client_->SetSelfDeafAll(deafened);
    }
    PushSelfStateUpdate();
}

void DiscordManager::SetLocalMute_Internal(uint64_t userId, bool muted)
{
    if (!isReady_ || !client_)
        return;
    if (call_ && *call_)
    {
        call_->SetLocalMute(userId, muted);
        PushParticipantStateUpdate(userId);
    }
}

void DiscordManager::SetParticipantVolume_Internal(uint64_t userId, float volume01)
{
    if (!isReady_ || !client_)
        return;
    if (call_ && *call_)
    {
        call_->SetParticipantVolume(userId, volume01);
        PushParticipantStateUpdate(userId);
    }
}

void DiscordManager::SetPTTActive_Internal(bool active)
{
    if (!isReady_ || !client_)
        return;
    if (call_ && *call_)
    {
        call_->SetPTTActive(active);
    }
}

void DiscordManager::SetPTTReleaseDelay_Internal(uint32_t releaseDelayMs)
{
    pttReleaseDelayMs_ = releaseDelayMs;
    if (!isReady_ || !client_)
        return;
    if (call_ && *call_)
    {
        call_->SetPTTReleaseDelay(releaseDelayMs);
    }
    PushSelfStateUpdate();
}

void DiscordManager::SetVADThreshold_Internal(bool automatic, float threshold)
{
    vadAutomatic_ = automatic;
    vadThreshold_ = threshold;
    if (!isReady_ || !client_)
        return;
    if (call_ && *call_)
    {
        call_->SetVADThreshold(automatic, threshold);
    }
    PushSelfStateUpdate();
}

void DiscordManager::SetAudioMode_Internal(discordpp::AudioModeType mode)
{
    audioMode_ = static_cast<uint32_t>(mode);
    if (!isReady_ || !client_)
        return;
    if (call_ && *call_)
    {
        call_->SetAudioMode(mode);
    }
    PushSelfStateUpdate();
}

void DiscordManager::SetInputDevice_Internal(std::string deviceId)
{
    if (!isReady_ || !client_)
        return;

    if (deviceId.empty())
    {
        deviceId = discordpp::Client::GetDefaultAudioDeviceId();
    }

    client_->SetInputDevice(deviceId, [this, deviceId](discordpp::ClientResult result)
                            {
        if (result.Successful()) {
            std::cout << "Input device updated: " << deviceId << std::endl;
            PushInputDevicesSnapshot();
        } else {
            std::cerr << "Failed to set input device: " << result.Error() << std::endl;
            PushVoiceErrorUpdate(Opcode::CMSG_VOICE_SET_INPUT_DEVICE,
                                 ClientResultErrorCode(result),
                                 ClientResultMessage(result));
        } });
}

void DiscordManager::SetOutputDevice_Internal(std::string deviceId)
{
    if (!isReady_ || !client_)
        return;

    if (deviceId.empty())
    {
        deviceId = discordpp::Client::GetDefaultAudioDeviceId();
    }

    client_->SetOutputDevice(deviceId, [this, deviceId](discordpp::ClientResult result)
                             {
        if (result.Successful()) {
            std::cout << "Output device updated: " << deviceId << std::endl;
            PushOutputDevicesSnapshot();
        } else {
            std::cerr << "Failed to set output device: " << result.Error() << std::endl;
            PushVoiceErrorUpdate(Opcode::CMSG_VOICE_SET_OUTPUT_DEVICE,
                                 ClientResultErrorCode(result),
                                 ClientResultMessage(result));
        } });
}

void DiscordManager::SetAutomaticGainControl_Internal(bool enabled)
{
    if (!isReady_ || !client_)
        return;
    client_->SetAutomaticGainControl(enabled);
}

void DiscordManager::SetEchoCancellation_Internal(bool enabled)
{
    if (!isReady_ || !client_)
        return;
    client_->SetEchoCancellation(enabled);
}

void DiscordManager::SetNoiseSuppression_Internal(bool enabled)
{
    if (!isReady_ || !client_)
        return;
    client_->SetNoiseSuppression(enabled);
}

void DiscordManager::HookCallCallbacks()
{
    if (!call_ || !(*call_))
        return;

    // Cache participant list for UI polling
    call_->SetParticipantChangedCallback([this](uint64_t userId, bool added)
                                         {
		(void)userId;
		(void)added;
		if (!call_ || !(*call_))
			return;
		{
			std::lock_guard<std::mutex> lock(voiceCacheMutex_);
			participantsCache_ = call_->GetParticipants();
		}
		PushParticipantsSnapshot(); });

    // Cache speaking flags for UI polling
    call_->SetSpeakingStatusChangedCallback([this](uint64_t userId, bool isSpeaking)
                                            {
		{
			std::lock_guard<std::mutex> lock(voiceCacheMutex_);
			speakingCache_[userId] = isSpeaking;
		}
		PushSpeakingUpdate(userId, isSpeaking);
		PushParticipantStateUpdate(userId); });

    // Keep our self mute/deaf mirrors synced if user toggles via Discord overlay
    call_->SetOnVoiceStateChangedCallback([this](uint64_t userId)
                                          {
		if (!call_ || !(*call_))
			return;
		// The call only exposes a VoiceStateHandle per user. For self, we can read from the call.
		(void)userId;
		selfMuted_ = call_->GetSelfMute();
		selfDeafened_ = call_->GetSelfDeaf();
		PushSelfStateUpdate();
		PushParticipantStateUpdate(userId); });

    call_->SetStatusChangedCallback([this](discordpp::Call::Status status, discordpp::Call::Error error, int32_t errorDetail)
                                    {
		std::cout << "🎧 Call status: " << discordpp::Call::StatusToString(status) << std::endl;
		PushCallStatusUpdate(status, error, errorDetail);
		if (status == discordpp::Call::Status::Disconnected) {
			ResetCallState(activeLobbyId_.load());
		}
		if (error != discordpp::Call::Error::None) {
			std::cerr << "❌ Call error: " << discordpp::Call::ErrorToString(error) << " (" << errorDetail << ")\n";
			PushVoiceErrorUpdate(Opcode::SMSG_VOICE_CALL_STATUS,
				static_cast<uint32_t>(error),
				discordpp::Call::ErrorToString(error));
		} });
}

void DiscordManager::ClearCallCaches()
{
    std::lock_guard<std::mutex> lock(voiceCacheMutex_);
    participantsCache_.clear();
    speakingCache_.clear();
}

void DiscordManager::OnDisconnect(discordpp::Client::Error error, int32_t errorDetail)
{
    const uint64_t previousLobbyId = activeLobbyId_.load();
    isReady_ = false;
    PushCallStatusUpdate(discordpp::Call::Status::Disconnected,
                         discordpp::Call::Error::None,
                         errorDetail);
    PushVoiceErrorUpdate(Opcode::SMSG_VOICE_CALL_STATUS,
                         static_cast<uint32_t>(error),
                         discordpp::Client::ErrorToString(error));
    ResetCallState(previousLobbyId);
    std::cerr << "❌ Connection Error: " << discordpp::Client::ErrorToString(error) << " - Details: " << errorDetail << std::endl;
}
