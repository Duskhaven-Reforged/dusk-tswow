#pragma once
#include <atomic>
#include <thread>
#include <cstdint>
#include <functional>
#include <string>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <optional>
#include "DiscordTokenStore.h"
#include "../discordpp.h"
#include "../IPC/CommandQueue.h"
#include "../IPC/UpdatePipeClient.h"

class DiscordManager {
public:

	void ThreadMain(uint64_t appId, bool allowDiscord);

	std::atomic<bool> running_{ false };
	std::atomic<bool> isReady_{ false };
	std::thread th_;
	CommandQueue cmds_;
	std::shared_ptr<discordpp::Client> client_;
	static DiscordManager* instance_;

	void OnConnect();

	bool Start(uint64_t appId, bool allowDiscord);
	void Stop();

	// Called from any thread (enqueues onto SDK thread)
	void Enqueue(std::function<void()> fn) { cmds_.Push(std::move(fn)); }

	static DiscordManager* Get();
	std::shared_ptr<discordpp::Client> GetClient() { return client_; }
	bool IsReady() const { return isReady_; }


	// Helper methods
	void UpdateActivity();
	void SetGamePresence(std::string characterName, uint32_t characterLevel, std::string className, std::string zoneName);
	void ClearGamePresence();

	// ---- Voice helpers (thread-safe wrappers; enqueue onto SDK thread) ----
	// Lobby + Call
	void JoinLobbyAndCall(const std::string& lobbySecret);
	void StartCall(uint64_t lobbyId);
	void LeaveCall();
	void EndAllCalls();

	// Audio controls
	void SetInputVolume(float volume01);
	void SetOutputVolume(float volume01);
	void SetSelfMute(bool muted);
	void SetSelfDeaf(bool deafened);
	void SetLocalMute(uint64_t userId, bool muted);
	void SetParticipantVolume(uint64_t userId, float volume01);
	void SetPTTActive(bool active);
	void SetPTTReleaseDelay(uint32_t releaseDelayMs);
	void SetVADThreshold(bool automatic, float threshold);
	void SetAudioMode(discordpp::AudioModeType mode);
	void SetInputDevice(const std::string& deviceId);
	void SetOutputDevice(const std::string& deviceId);
	void SetAutomaticGainControl(bool enabled);
	void SetEchoCancellation(bool enabled);
	void SetNoiseSuppression(bool enabled);

	// Convenience / state accessors (best-effort; mirrors last values we set)
	bool IsInCall() const { return inCall_; }
	uint64_t ActiveLobbyId() const { return activeLobbyId_; }
	bool SelfMuted() const { return selfMuted_; }
	bool SelfDeafened() const { return selfDeafened_; }
	float InputVolume() const { return inputVolume_; }
	float OutputVolume() const { return outputVolume_; }

	// Cached call state (updated by call callbacks)
	std::vector<uint64_t> GetParticipants() const;
	bool IsUserSpeaking(uint64_t userId) const;

	void OnDisconnect(discordpp::Client::Error error, int32_t errorDetail);

private:
	void BeginAuthentication(uint64_t appId);
	void BeginInteractiveAuthorization(uint64_t appId);
	void RefreshCachedToken(uint64_t appId, std::string refreshToken);
	void ApplyAccessToken(discordpp::AuthorizationTokenType tokenType, std::string accessToken);
	void SaveCachedToken(
		discordpp::AuthorizationTokenType tokenType,
		std::string accessToken,
		std::string refreshToken,
		int32_t expiresIn,
		std::string scopes);
	void ClearCachedToken();
	bool HasReadyClient() const { return isReady_.load() && static_cast<bool>(client_); }
	discordpp::Call* ActiveCall();
	const discordpp::Call* ActiveCall() const;

	// Voice state we track locally (not authoritative, but useful for UI)
	std::atomic<uint64_t> activeLobbyId_{ 0 };
	std::atomic<bool> inCall_{ false };
	std::atomic<bool> selfMuted_{ false };
	std::atomic<bool> selfDeafened_{ false };
	std::atomic<float> inputVolume_{ 1.0f };
	std::atomic<float> outputVolume_{ 1.0f };
	std::atomic<uint32_t> audioMode_{ static_cast<uint32_t>(discordpp::AudioModeType::MODE_VAD) };
	std::atomic<uint32_t> pttReleaseDelayMs_{ 0 };
	std::atomic<bool> vadAutomatic_{ true };
	std::atomic<float> vadThreshold_{ 0.0f };

	// Current active call (lives on SDK thread; guarded access via Enqueue/callback updates)
	std::optional<discordpp::Call> call_;

	// Cached call info for UI / polling from any thread
	mutable std::mutex voiceCacheMutex_;
	std::vector<uint64_t> participantsCache_;
	std::unordered_map<uint64_t, bool> speakingCache_;

	// Internal helpers (run on SDK thread)
	void StartCall_Internal(uint64_t lobbyId);
	void LeaveCall_Internal();
	void EndAllCalls_Internal();
	void SetInputVolume_Internal(float volume01);
	void SetOutputVolume_Internal(float volume01);
	void SetSelfMute_Internal(bool muted);
	void SetSelfDeaf_Internal(bool deafened);
	void SetLocalMute_Internal(uint64_t userId, bool muted);
	void SetParticipantVolume_Internal(uint64_t userId, float volume01);
	void SetPTTActive_Internal(bool active);
	void SetPTTReleaseDelay_Internal(uint32_t releaseDelayMs);
	void SetVADThreshold_Internal(bool automatic, float threshold);
	void SetAudioMode_Internal(discordpp::AudioModeType mode);
	void SetDevice_Internal(bool inputDevice, std::string deviceId);
	void SetAutomaticGainControl_Internal(bool enabled);
	void SetEchoCancellation_Internal(bool enabled);
	void SetNoiseSuppression_Internal(bool enabled);
	void SetGamePresence_Internal(std::string characterName, uint32_t characterLevel, std::string className, std::string zoneName);
	void ClearGamePresence_Internal();

	void HookCallCallbacks();
	void ClearCallCaches();
	void ResetCallState(uint64_t previousLobbyId);
	void ConfigureVoiceUpdateCallbacks();
	void PushCallStatusUpdate(discordpp::Call::Status status,
		discordpp::Call::Error error = discordpp::Call::Error::None,
		int32_t errorDetail = 0);
	void PushVoiceErrorUpdate(Opcode sourceOpcode, uint32_t errorCode, std::string message);
	void PushSelfStateUpdate();
	void PushSpeakingUpdate(uint64_t userId, bool speaking);
	void PushParticipantsClear(uint64_t lobbyId);
	void PushParticipantsSnapshot();
	void PushParticipantStateUpdate(uint64_t userId);
	void PushDevicesSnapshot(bool inputDevices);

	std::string activityCharacterName_;
	std::string activityClassName_;
	uint32_t activityCharacterLevel_ = 0;
	std::string activityZoneName_;
	std::optional<uint64_t> activityStartMs_;
	UpdatePipeClient updatePipe_{ L"duskhaven_social_sdk_updates_pipe" };

	uint64_t applicationId_ = 0;
	bool usingCachedAccessToken_ = false;
	bool refreshAttempted_ = false;
	bool interactiveAuthAttempted_ = false;
	bool interactiveAuthInProgress_ = false;
	std::optional<DiscordTokenStore::CachedToken> cachedToken_;
};
