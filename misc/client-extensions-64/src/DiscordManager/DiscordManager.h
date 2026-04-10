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

class DiscordManager {
public:

	void ThreadMain(uint64_t appId);

	std::atomic<bool> running_{ false };
	std::atomic<bool> isReady_{ false };
	std::thread th_;
	CommandQueue cmds_;
	std::shared_ptr<discordpp::Client> client_;
	static DiscordManager* instance_;

	void OnConnect();

	bool Start(uint64_t appId);
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

	// Voice state we track locally (not authoritative, but useful for UI)
	std::atomic<uint64_t> activeLobbyId_{ 0 };
	std::atomic<bool> inCall_{ false };
	std::atomic<bool> selfMuted_{ false };
	std::atomic<bool> selfDeafened_{ false };
	std::atomic<float> inputVolume_{ 1.0f };
	std::atomic<float> outputVolume_{ 1.0f };

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
	void SetGamePresence_Internal(std::string characterName, uint32_t characterLevel, std::string className, std::string zoneName);
	void ClearGamePresence_Internal();

	void HookCallCallbacks();
	void ClearCallCaches();

	std::string activityCharacterName_;
	std::string activityClassName_;
	uint32_t activityCharacterLevel_ = 0;
	std::string activityZoneName_;
	std::optional<uint64_t> activityStartMs_;

	uint64_t applicationId_ = 0;
	bool usingCachedAccessToken_ = false;
	bool refreshAttempted_ = false;
	bool interactiveAuthAttempted_ = false;
	bool interactiveAuthInProgress_ = false;
	std::optional<DiscordTokenStore::CachedToken> cachedToken_;
};
