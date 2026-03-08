#define DISCORDPP_IMPLEMENTATION
#include "discordpp.h"
#include "DiscordManager.h"
#include <iostream>
#include <chrono>

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

    // TODO: eventually write this somewhere so i dont ask every time
    auto codeVerifier = client_->CreateAuthorizationCodeVerifier();
    discordpp::AuthorizationArgs args{};
    args.SetClientId(appId);
    args.SetScopes(discordpp::Client::GetDefaultPresenceScopes());
    args.SetScopes(discordpp::Client::GetDefaultCommunicationScopes());
    args.SetCodeChallenge(codeVerifier.Challenge());

    client_->Authorize(args, [this, codeVerifier, appId](auto result, auto code, auto redirectUri)
                       {
        if (!result.Successful()) {
            std::cerr << "❌ Authentication Error: " << result.Error() << std::endl;
            return;
        }
        std::cout << "✅ Authorization successful! Getting access token...\n";
        client_->GetToken(appId, code, codeVerifier.Verifier(), redirectUri,
            [this](discordpp::ClientResult result,
                std::string accessToken,
                std::string refreshToken,
                discordpp::AuthorizationTokenType tokenType,
                int32_t expiresIn,
                std::string scope) {
                    std::cout << "🔓 Access token received! Establishing connection...\n";
                    client_->UpdateToken(discordpp::AuthorizationTokenType::Bearer, accessToken, [this](discordpp::ClientResult result) {
                        if (result.Successful()) {
                             std::cout << "🔑 Token updated, connecting to Discord...\n";
                             client_->Connect();
                        }
                    });
            }); });

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
}

void DiscordManager::UpdateActivity()
{
    if (!isReady_ || !client_)
        return;

    discordpp::Activity activity;
    activity.SetType(discordpp::ActivityTypes::Playing);
    activity.SetState("In Competitive Match");
    activity.SetDetails("Rank: Diamond II");

    client_->UpdateRichPresence(activity, [](discordpp::ClientResult result)
                                {
        if (result.Successful()) {
            std::cout << "🎮 Rich Presence updated successfully!\n";
        } else {
            std::cerr << "❌ Rich Presence update failed\n";
        } });
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
        activeLobbyId_ = 0;
        inCall_ = false;
        ClearCallCaches();
        call_.reset();
        return;
    }

    inCall_ = true;
    std::cout << "🎤 Voice call started (channel/lobby " << lobbyId << ")\n";

    // Apply cached settings immediately.
    call_->SetSelfMute(selfMuted_.load());
    call_->SetSelfDeaf(selfDeafened_.load());
    call_->SetPTTActive(false);

    HookCallCallbacks();
    {
        std::lock_guard<std::mutex> lock(voiceCacheMutex_);
        participantsCache_ = call_->GetParticipants();
        speakingCache_.clear();
    }
}

void DiscordManager::LeaveCall_Internal()
{
    if (!isReady_ || !client_)
        return;

    if (!call_ || !(*call_))
    {
        inCall_ = false;
        activeLobbyId_ = 0;
        ClearCallCaches();
        call_.reset();
        return;
    }

    const uint64_t channelId = activeLobbyId_.load();
    client_->EndCall(channelId, [this]()
                     { std::cout << "🔇 Call ended" << std::endl; });

    inCall_ = false;
    activeLobbyId_ = 0;
    ClearCallCaches();
    call_.reset();
}

void DiscordManager::EndAllCalls_Internal()
{
    if (!isReady_ || !client_)
        return;

    client_->EndCalls([]()
                      { std::cout << "🔇 All calls ended successfully" << std::endl; });
    inCall_ = false;
    activeLobbyId_ = 0;
    ClearCallCaches();
    call_.reset();
}

void DiscordManager::SetInputVolume_Internal(float volume01)
{
    if (!isReady_ || !client_)
        return;
    inputVolume_ = volume01;
    client_->SetInputVolume(volume01);
}

void DiscordManager::SetOutputVolume_Internal(float volume01)
{
    if (!isReady_ || !client_)
        return;
    outputVolume_ = volume01;
    client_->SetOutputVolume(volume01);
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
}

void DiscordManager::SetLocalMute_Internal(uint64_t userId, bool muted)
{
    if (!isReady_ || !client_)
        return;
    if (call_ && *call_)
    {
        call_->SetLocalMute(userId, muted);
    }
}

void DiscordManager::SetParticipantVolume_Internal(uint64_t userId, float volume01)
{
    if (!isReady_ || !client_)
        return;
    if (call_ && *call_)
    {
        call_->SetParticipantVolume(userId, volume01);
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
    if (!isReady_ || !client_)
        return;
    if (call_ && *call_)
    {
        call_->SetPTTReleaseDelay(releaseDelayMs);
    }
}

void DiscordManager::SetVADThreshold_Internal(bool automatic, float threshold)
{
    if (!isReady_ || !client_)
        return;
    if (call_ && *call_)
    {
        call_->SetVADThreshold(automatic, threshold);
    }
}

void DiscordManager::SetAudioMode_Internal(discordpp::AudioModeType mode)
{
    if (!isReady_ || !client_)
        return;
    if (call_ && *call_)
    {
        call_->SetAudioMode(mode);
    }
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
		std::lock_guard<std::mutex> lock(voiceCacheMutex_);
		participantsCache_ = call_->GetParticipants(); });

    // Cache speaking flags for UI polling
    call_->SetSpeakingStatusChangedCallback([this](uint64_t userId, bool isSpeaking)
                                            {
		std::lock_guard<std::mutex> lock(voiceCacheMutex_);
		speakingCache_[userId] = isSpeaking; });

    // Keep our self mute/deaf mirrors synced if user toggles via Discord overlay
    call_->SetOnVoiceStateChangedCallback([this](uint64_t userId)
                                          {
		if (!call_ || !(*call_))
			return;
		// The call only exposes a VoiceStateHandle per user. For self, we can read from the call.
		(void)userId;
		selfMuted_ = call_->GetSelfMute();
		selfDeafened_ = call_->GetSelfDeaf(); });

    call_->SetStatusChangedCallback([this](discordpp::Call::Status status, discordpp::Call::Error error, int32_t errorDetail)
                                    {
		std::cout << "🎧 Call status: " << discordpp::Call::StatusToString(status) << std::endl;
		if (status == discordpp::Call::Status::Disconnected) {
			inCall_ = false;
			activeLobbyId_ = 0;
			ClearCallCaches();
			call_.reset();
		}
		if (error != discordpp::Call::Error::None) {
			std::cerr << "❌ Call error: " << discordpp::Call::ErrorToString(error) << " (" << errorDetail << ")\n";
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
    isReady_ = false;
    inCall_ = false;
    activeLobbyId_ = 0;
    std::cerr << "❌ Connection Error: " << discordpp::Client::ErrorToString(error) << " - Details: " << errorDetail << std::endl;
}
