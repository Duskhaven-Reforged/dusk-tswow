#pragma once
#ifdef _WIN32
#include <windows.h>
#endif
#include <atomic>
#include <cstdint>
#include <thread>
#include <string>
#include "SharedMemoryRingBuffer.h"
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
	bool DispatchFrame(const uint8_t* frame, uint32_t frameBytes);
#endif

	OpcodeDispatcher& dispatcher_;
	std::atomic<bool> running_{ false };
	std::thread th_;

#ifdef _WIN32
	std::atomic<bool> clientConnected_{ false };
	Duskhaven::IPC::SharedMemoryRingBuffer channel_;
#endif
};
