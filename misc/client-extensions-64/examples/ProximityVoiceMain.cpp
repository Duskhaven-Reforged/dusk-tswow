#include "../src/DiscordManager/DiscordManager.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <thread>

namespace
{
std::atomic<bool> running = true;

void SignalHandler(int)
{
    running.store(false);
}
}

int main()
{
    constexpr uint64_t applicationId = 1457900736120754371;

    std::signal(SIGINT, SignalHandler);

    DiscordManager discord;
    if (!discord.Start(applicationId))
    {
        std::cerr << "Failed to start Discord manager\n";
        return 1;
    }

    while (running.load())
    {
        discord.SetVoiceListenerPosition(0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
        discord.SetVoicePlayerMapping(123456789012345678ull, 1);
        discord.SetVoicePlayerPosition(1, 5.0f, 0.0f, 0.0f);

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    discord.Stop();
    return 0;
}
