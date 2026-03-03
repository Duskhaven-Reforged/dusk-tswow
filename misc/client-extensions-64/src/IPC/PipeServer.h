#pragma once
#ifdef _WIN32
#include <windows.h>
#endif
#include <atomic>
#include <thread>
#include <string>
#include "OpcodeDispatcher.h"
#include "../handlers/Opcodes.h"
#include "PacketBuilder.h"

class PipeServer {
public:
	explicit PipeServer(OpcodeDispatcher& dispatcher) : dispatcher_(dispatcher) {}
	~PipeServer() { Stop(); }

	bool Start(const std::wstring& pipeName);
	void Stop();

private:
#ifdef _WIN32
	void ThreadMain(std::wstring pipeName);
	bool ReadExact(HANDLE h, void* dst, uint32_t bytes);
	bool WriteExact(HANDLE h, const void* src, uint32_t bytes);
#endif

	OpcodeDispatcher& dispatcher_;
	std::atomic<bool> running_{ false };
	std::thread th_;

#ifdef _WIN32
	std::atomic<bool> clientConnected_{ false };
#endif
};
