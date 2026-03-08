#include "DiscordManager/DiscordManager.h"
#include "IPC/IPCFrame.h"
#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>

// Replace appID/pipe name
const uint64_t APPLICATION_ID = 1457900736120754371;
const std::wstring PIPE_NAME = L"duskhaven_social_sdk_pipe";
//

std::atomic<bool> running = true;
void signalHandler(int signum) {
    running.store(false);
}

int main() {
    std::signal(SIGINT, signalHandler);
    std::cout << "🚀 Host Application Starting...\n";

    DiscordManager discordManager;
    if (!discordManager.Start(APPLICATION_ID)) {
        std::cerr << "❌ Failed to start Discord Manager\n";
        return 1;
    }

    IPCFrame ipcFrame;
    if (!ipcFrame.Start(PIPE_NAME)) {
        std::cerr << "⚠️ Failed to start IPC Server (is it already running?)\n";
    }

    std::cout << "✨ Services running. Press Ctrl+C to exit.\n";

    // Keep application running
    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "🛑 Stopping services...\n";
    ipcFrame.Stop();
    discordManager.Stop();

    return 0;
}
