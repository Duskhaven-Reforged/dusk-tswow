#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <csignal>
#include <iostream>
#include <string>
#include <thread>
#include "DiscordManager/DiscordManager.h"
#include "IPC/IPCFrame.h"

// Replace appID/pipe name
const uint64_t APPLICATION_ID = 1457900736120754371;
const std::wstring PIPE_NAME  = L"duskhaven_social_sdk_pipe";
//

std::atomic<bool> running = true;
void signalHandler(int signum)
{
    running.store(false);
}

namespace
{
    bool ParseBoolArg(std::string value, bool fallback)
    {
        std::transform(value.begin(), value.end(), value.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

        if (value == "1")
            return true;

        if (value == "0")
            return false;

        return fallback;
    }

    bool ReadAllowDiscordArg(int argc, char* argv[])
    {
        bool allowDiscord             = true;
        const std::string prefixedArg = "--allowDiscord=";

        for (int i = 1; i < argc; ++i)
        {
            std::string arg = argv[i];
            if (arg.rfind(prefixedArg, 0) == 0)
            {
                allowDiscord = ParseBoolArg(arg.substr(prefixedArg.size()), allowDiscord);
                continue;
            }
        }
        return allowDiscord;
    }
} // namespace

int main(int argc, char* argv[])
{
    std::signal(SIGINT, signalHandler);
    std::cout << "🚀 Host Application Starting...\n";

    const bool allowDiscord = ReadAllowDiscordArg(argc, argv);

    DiscordManager discordManager;
    if (!discordManager.Start(APPLICATION_ID, allowDiscord))
    {
        std::cerr << "❌ Failed to start Discord Manager\n";
    }

    IPCFrame ipcFrame;
    if (!ipcFrame.Start(PIPE_NAME))
    {
        std::cerr << "⚠️ Failed to start IPC Server (is it already running?)\n";
    }

    std::cout << "✨ Services running. Press Ctrl+C to exit.\n";

    // Keep application running
    while (running)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "🛑 Stopping services...\n";
    ipcFrame.Stop();
    discordManager.Stop();

    return 0;
}
