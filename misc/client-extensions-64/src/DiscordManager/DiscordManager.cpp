#define DISCORDPP_IMPLEMENTATION
#include "discordpp.h"
#include "DiscordManager.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <iostream>
#include <optional>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#include <mmdeviceapi.h>
#include <objbase.h>
#include <propidl.h>
#include <propsys.h>
#endif

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

template <typename Writer>
void SendUpdatePacket(UpdatePipeClient& updatePipe, Opcode opcode, Writer&& writer)
{
    auto packet = PacketBuilder::CreatePacket(opcode);
    writer(packet);
    updatePipe.Send(packet);
}

const char* DeviceTypeName(bool inputDevice)
{
    return inputDevice ? "input" : "output";
}

#ifdef _WIN32
constexpr PROPERTYKEY kDeviceFriendlyNameProperty = {
    {0xA45C254E, 0xDF1C, 0x4EFD, {0x80, 0x20, 0x67, 0xD1, 0x46, 0xA8, 0x50, 0xE0}},
    14};

std::wstring Utf8ToWide(const std::string& value)
{
    if (value.empty())
    {
        return {};
    }

    const int length = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (length <= 0)
    {
        return {};
    }

    std::wstring wide(length, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), wide.data(), length);
    return wide;
}

std::string WideToUtf8(const wchar_t* value)
{
    if (!value || value[0] == L'\0')
    {
        return {};
    }

    const int length = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
    if (length <= 1)
    {
        return {};
    }

    std::string utf8(length, '\0');
    WideCharToMultiByte(CP_UTF8, 0, value, -1, utf8.data(), length, nullptr, nullptr);
    if (!utf8.empty() && utf8.back() == '\0')
    {
        utf8.pop_back();
    }
    return utf8;
}

std::optional<std::string> ResolveWindowsAudioDeviceName(const std::string& deviceId)
{
    const std::wstring wideId = Utf8ToWide(deviceId);
    if (wideId.empty())
    {
        return std::nullopt;
    }

    const HRESULT coInit = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool shouldUninitialize = coInit == S_OK || coInit == S_FALSE;
    if (FAILED(coInit) && coInit != RPC_E_CHANGED_MODE)
    {
        return std::nullopt;
    }

    std::optional<std::string> result;
    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice* device = nullptr;
    IPropertyStore* properties = nullptr;

    if (SUCCEEDED(CoCreateInstance(
            __uuidof(MMDeviceEnumerator),
            nullptr,
            CLSCTX_ALL,
            IID_PPV_ARGS(&enumerator))) &&
        SUCCEEDED(enumerator->GetDevice(wideId.c_str(), &device)) &&
        SUCCEEDED(device->OpenPropertyStore(STGM_READ, &properties)))
    {
        PROPVARIANT friendlyName;
        PropVariantInit(&friendlyName);
        if (SUCCEEDED(properties->GetValue(kDeviceFriendlyNameProperty, &friendlyName)) &&
            friendlyName.vt == VT_LPWSTR)
        {
            std::string name = TrimWhitespace(WideToUtf8(friendlyName.pwszVal));
            if (!name.empty())
            {
                result = std::move(name);
            }
        }
        PropVariantClear(&friendlyName);
    }

    if (properties)
    {
        properties->Release();
    }
    if (device)
    {
        device->Release();
    }
    if (enumerator)
    {
        enumerator->Release();
    }
    if (shouldUninitialize)
    {
        CoUninitialize();
    }

    return result;
}
#endif

bool LooksLikeWindowsEndpointId(const std::string& value)
{
    return value.rfind("{0.0.0.", 0) == 0;
}

std::string DisplayNameForDevice(bool inputDevices, const discordpp::AudioDevice& device)
{
    const std::string deviceId = device.Id();
    std::string deviceName = TrimWhitespace(device.Name());

#ifdef _WIN32
    if (!inputDevices &&
        (deviceName.empty() || deviceName == deviceId || LooksLikeWindowsEndpointId(deviceName)))
    {
        if (std::optional<std::string> resolvedName = ResolveWindowsAudioDeviceName(deviceId))
        {
            return *resolvedName;
        }
    }
#endif

    if (!deviceName.empty())
    {
        return deviceName;
    }

    return deviceId;
}

Opcode DeviceSetOpcode(bool inputDevice)
{
    return inputDevice ? Opcode::CMSG_VOICE_SET_INPUT_DEVICE
                       : Opcode::CMSG_VOICE_SET_OUTPUT_DEVICE;
}

void PushDeviceEntries(
    UpdatePipeClient& updatePipe,
    bool inputDevices,
    const std::string& currentId,
    const std::vector<discordpp::AudioDevice>& devices)
{
    SendUpdatePacket(updatePipe, Opcode::SMSG_VOICE_DEVICES, [inputDevices, currentId](PacketBuilder& packet)
                     {
                         packet.writeBool(inputDevices);
                         packet.writeString(currentId);
                     });

    for (const auto& device : devices)
    {
        const std::string deviceId = device.Id();
        const std::string deviceName = DisplayNameForDevice(inputDevices, device);
        SendUpdatePacket(updatePipe, Opcode::SMSG_VOICE_DEVICE_STATE, [&](PacketBuilder& packet)
                         {
                             packet.writeBool(inputDevices);
                             packet.writeString(deviceId);
                             packet.writeString(deviceName);
                             packet.writeBool(device.IsDefault());
                             packet.writeBool(!currentId.empty() && deviceId == currentId);
                         });
    }
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
    PushDevicesSnapshot(true);
    PushDevicesSnapshot(false);
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

#define ENQUEUE_DISCORD_CALL0(Name, Body) \
    void DiscordManager::Name() { Enqueue([this]() { Body; }); }

#define ENQUEUE_DISCORD_CALL1(Name, Type1, Arg1, Body) \
    void DiscordManager::Name(Type1 Arg1) { Enqueue([this, Arg1]() { Body; }); }

#define ENQUEUE_DISCORD_CALL2(Name, Type1, Arg1, Type2, Arg2, Body) \
    void DiscordManager::Name(Type1 Arg1, Type2 Arg2)               \
    {                                                               \
        Enqueue([this, Arg1, Arg2]() { Body; });                    \
    }

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

ENQUEUE_DISCORD_CALL1(StartCall, uint64_t, lobbyId, StartCall_Internal(lobbyId))
ENQUEUE_DISCORD_CALL0(LeaveCall, LeaveCall_Internal())
ENQUEUE_DISCORD_CALL0(EndAllCalls, EndAllCalls_Internal())
ENQUEUE_DISCORD_CALL1(SetInputVolume, float, volume01, SetInputVolume_Internal(volume01))
ENQUEUE_DISCORD_CALL1(SetOutputVolume, float, volume01, SetOutputVolume_Internal(volume01))
ENQUEUE_DISCORD_CALL1(SetSelfMute, bool, muted, SetSelfMute_Internal(muted))
ENQUEUE_DISCORD_CALL1(SetSelfDeaf, bool, deafened, SetSelfDeaf_Internal(deafened))
ENQUEUE_DISCORD_CALL2(SetLocalMute, uint64_t, userId, bool, muted, SetLocalMute_Internal(userId, muted))
ENQUEUE_DISCORD_CALL2(SetParticipantVolume, uint64_t, userId, float, volume01, SetParticipantVolume_Internal(userId, volume01))
ENQUEUE_DISCORD_CALL1(SetPTTActive, bool, active, SetPTTActive_Internal(active))
ENQUEUE_DISCORD_CALL1(SetPTTReleaseDelay, uint32_t, releaseDelayMs, SetPTTReleaseDelay_Internal(releaseDelayMs))
ENQUEUE_DISCORD_CALL2(SetVADThreshold, bool, automatic, float, threshold, SetVADThreshold_Internal(automatic, threshold))
ENQUEUE_DISCORD_CALL1(SetAudioMode, discordpp::AudioModeType, mode, SetAudioMode_Internal(mode))
ENQUEUE_DISCORD_CALL1(SetInputDevice, const std::string&, deviceId, SetDevice_Internal(true, deviceId))
ENQUEUE_DISCORD_CALL1(SetOutputDevice, const std::string&, deviceId, SetDevice_Internal(false, deviceId))
ENQUEUE_DISCORD_CALL1(SetAutomaticGainControl, bool, enabled, SetAutomaticGainControl_Internal(enabled))
ENQUEUE_DISCORD_CALL1(SetEchoCancellation, bool, enabled, SetEchoCancellation_Internal(enabled))
ENQUEUE_DISCORD_CALL1(SetNoiseSuppression, bool, enabled, SetNoiseSuppression_Internal(enabled))

#undef ENQUEUE_DISCORD_CALL0
#undef ENQUEUE_DISCORD_CALL1
#undef ENQUEUE_DISCORD_CALL2

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
        PushDevicesSnapshot(true);
        PushDevicesSnapshot(false); });
}

void DiscordManager::PushCallStatusUpdate(
    discordpp::Call::Status status,
    discordpp::Call::Error error,
    int32_t errorDetail)
{
    SendUpdatePacket(updatePipe_, Opcode::SMSG_VOICE_CALL_STATUS, [&](PacketBuilder& packet)
                     {
                         packet.writeUInt64(activeLobbyId_.load());
                         packet.writeUInt32(static_cast<uint32_t>(status));
                         packet.writeUInt32(static_cast<uint32_t>(error));
                         packet.writeUInt32(static_cast<uint32_t>(errorDetail));
                     });
}

void DiscordManager::PushVoiceErrorUpdate(Opcode sourceOpcode, uint32_t errorCode, std::string message)
{
    SendUpdatePacket(updatePipe_, Opcode::SMSG_VOICE_ERROR, [&](PacketBuilder& packet)
                     {
                         packet.writeUInt32(sourceOpcode.raw());
                         packet.writeUInt32(errorCode);
                         packet.writeString(message);
                     });
}

void DiscordManager::PushSelfStateUpdate()
{
    SendUpdatePacket(updatePipe_, Opcode::SMSG_VOICE_SELF_STATE, [&](PacketBuilder& packet)
                     {
                         packet.writeUInt64(activeLobbyId_.load());
                         packet.writeBool(selfMuted_.load());
                         packet.writeBool(selfDeafened_.load());
                         packet.writeFloat(inputVolume_.load());
                         packet.writeFloat(outputVolume_.load());
                         packet.writeUInt32(audioMode_.load());
                         packet.writeUInt32(pttReleaseDelayMs_.load());
                         packet.writeBool(vadAutomatic_.load());
                         packet.writeFloat(vadThreshold_.load());
                     });
}

void DiscordManager::PushSpeakingUpdate(uint64_t userId, bool speaking)
{
    SendUpdatePacket(updatePipe_, Opcode::SMSG_VOICE_SPEAKING, [&](PacketBuilder& packet)
                     {
                         packet.writeUInt64(activeLobbyId_.load());
                         packet.writeUInt64(userId);
                         packet.writeBool(speaking);
                     });
}

void DiscordManager::PushParticipantsClear(uint64_t lobbyId)
{
    SendUpdatePacket(updatePipe_, Opcode::SMSG_VOICE_PARTICIPANTS, [lobbyId](PacketBuilder& packet)
                     { packet.writeUInt64(lobbyId); });
}

void DiscordManager::PushParticipantsSnapshot()
{
    PushParticipantsClear(activeLobbyId_.load());

    auto* call = ActiveCall();
    if (!call)
    {
        return;
    }

    for (uint64_t userId : call->GetParticipants())
    {
        PushParticipantStateUpdate(userId);
    }
}

void DiscordManager::PushParticipantStateUpdate(uint64_t userId)
{
    auto* call = ActiveCall();
    if (!call)
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
    if (auto voiceState = call->GetVoiceStateHandle(userId); voiceState.has_value())
    {
        selfMuted = voiceState->SelfMute();
        selfDeafened = voiceState->SelfDeaf();
    }

    SendUpdatePacket(updatePipe_, Opcode::SMSG_VOICE_PARTICIPANT_STATE, [&](PacketBuilder& packet)
                     {
                         packet.writeUInt64(activeLobbyId_.load());
                         packet.writeUInt64(userId);
                         packet.writeBool(speaking);
                         packet.writeBool(selfMuted);
                         packet.writeBool(selfDeafened);
                         packet.writeBool(call->GetLocalMute(userId));
                         packet.writeFloat(call->GetParticipantVolume(userId));
                     });
}

void DiscordManager::PushDevicesSnapshot(bool inputDevices)
{
    if (!client_)
    {
        return;
    }

    auto pushDevices = [this, inputDevices](discordpp::AudioDevice currentDevice, std::vector<discordpp::AudioDevice> devices)
    {
        PushDeviceEntries(updatePipe_, inputDevices, DeviceIdOrEmpty(currentDevice), devices);
    };

    if (inputDevices)
    {
        client_->GetCurrentInputDevice([this, pushDevices = std::move(pushDevices)](discordpp::AudioDevice currentDevice) mutable
                                       {
                                           if (!client_)
                                           {
                                               return;
                                           }

                                           client_->GetInputDevices(
                                               [pushDevices = std::move(pushDevices), currentDevice](std::vector<discordpp::AudioDevice> devices) mutable
                                               { pushDevices(currentDevice, std::move(devices)); });
                                       });
        return;
    }

    client_->GetCurrentOutputDevice([this, pushDevices = std::move(pushDevices)](discordpp::AudioDevice currentDevice) mutable
                                    {
                                        if (!client_)
                                        {
                                            return;
                                        }

                                        client_->GetOutputDevices(
                                            [pushDevices = std::move(pushDevices), currentDevice](std::vector<discordpp::AudioDevice> devices) mutable
                                            { pushDevices(currentDevice, std::move(devices)); });
                                    });
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

discordpp::Call* DiscordManager::ActiveCall()
{
    return call_ && *call_ ? &call_.value() : nullptr;
}

const discordpp::Call* DiscordManager::ActiveCall() const
{
    return call_ && *call_ ? &call_.value() : nullptr;
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
    if (!HasReadyClient())
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
    if (!HasReadyClient())
        return;

    const uint64_t previousLobbyId = activeLobbyId_.load();
    if (!ActiveCall())
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
    if (!HasReadyClient())
        return;

    const uint64_t previousLobbyId = activeLobbyId_.load();
    client_->EndCalls([]()
                      { std::cout << "🔇 All calls ended successfully" << std::endl; });
    PushCallStatusUpdate(discordpp::Call::Status::Disconnected);
    ResetCallState(previousLobbyId);
}

void DiscordManager::SetInputVolume_Internal(float volume01)
{
    if (!HasReadyClient())
        return;
    inputVolume_ = volume01;
    client_->SetInputVolume(volume01);
    PushSelfStateUpdate();
}

void DiscordManager::SetOutputVolume_Internal(float volume01)
{
    if (!HasReadyClient())
        return;
    outputVolume_ = volume01;
    client_->SetOutputVolume(volume01);
    PushSelfStateUpdate();
}

void DiscordManager::SetSelfMute_Internal(bool muted)
{
    if (!HasReadyClient())
        return;

    selfMuted_ = muted;
    if (auto* call = ActiveCall())
    {
        call->SetSelfMute(muted);
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
    if (!HasReadyClient())
        return;

    selfDeafened_ = deafened;
    if (auto* call = ActiveCall())
    {
        call->SetSelfDeaf(deafened);
    }
    else
    {
        client_->SetSelfDeafAll(deafened);
    }
    PushSelfStateUpdate();
}

void DiscordManager::SetLocalMute_Internal(uint64_t userId, bool muted)
{
    if (!HasReadyClient())
        return;

    if (auto* call = ActiveCall())
    {
        call->SetLocalMute(userId, muted);
        PushParticipantStateUpdate(userId);
    }
}

void DiscordManager::SetParticipantVolume_Internal(uint64_t userId, float volume01)
{
    if (!HasReadyClient())
        return;

    if (auto* call = ActiveCall())
    {
        call->SetParticipantVolume(userId, volume01);
        PushParticipantStateUpdate(userId);
    }
}

void DiscordManager::SetPTTActive_Internal(bool active)
{
    if (!HasReadyClient())
        return;

    if (auto* call = ActiveCall())
    {
        call->SetPTTActive(active);
    }
}

void DiscordManager::SetPTTReleaseDelay_Internal(uint32_t releaseDelayMs)
{
    pttReleaseDelayMs_ = releaseDelayMs;
    if (!HasReadyClient())
        return;

    if (auto* call = ActiveCall())
    {
        call->SetPTTReleaseDelay(releaseDelayMs);
    }
    PushSelfStateUpdate();
}

void DiscordManager::SetVADThreshold_Internal(bool automatic, float threshold)
{
    vadAutomatic_ = automatic;
    vadThreshold_ = threshold;
    if (!HasReadyClient())
        return;

    if (auto* call = ActiveCall())
    {
        call->SetVADThreshold(automatic, threshold);
    }
    PushSelfStateUpdate();
}

void DiscordManager::SetAudioMode_Internal(discordpp::AudioModeType mode)
{
    audioMode_ = static_cast<uint32_t>(mode);
    if (!HasReadyClient())
        return;

    if (auto* call = ActiveCall())
    {
        call->SetAudioMode(mode);
    }
    PushSelfStateUpdate();
}

void DiscordManager::SetDevice_Internal(bool inputDevice, std::string deviceId)
{
    if (!HasReadyClient())
        return;

    if (deviceId.empty())
    {
        deviceId = discordpp::Client::GetDefaultAudioDeviceId();
    }

    auto onComplete = [this, inputDevice, deviceId](discordpp::ClientResult result)
    {
        if (result.Successful())
        {
            std::cout << DeviceTypeName(inputDevice) << " device updated: " << deviceId << std::endl;
            PushDevicesSnapshot(inputDevice);
            return;
        }

        std::cerr << "Failed to set " << DeviceTypeName(inputDevice) << " device: " << result.Error() << std::endl;
        PushVoiceErrorUpdate(
            DeviceSetOpcode(inputDevice),
            ClientResultErrorCode(result),
            ClientResultMessage(result));
    };

    if (inputDevice)
    {
        client_->SetInputDevice(deviceId, onComplete);
        return;
    }

    client_->SetOutputDevice(deviceId, onComplete);
}

void DiscordManager::SetAutomaticGainControl_Internal(bool enabled)
{
    if (!HasReadyClient())
        return;
    client_->SetAutomaticGainControl(enabled);
}

void DiscordManager::SetEchoCancellation_Internal(bool enabled)
{
    if (!HasReadyClient())
        return;
    client_->SetEchoCancellation(enabled);
}

void DiscordManager::SetNoiseSuppression_Internal(bool enabled)
{
    if (!HasReadyClient())
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
