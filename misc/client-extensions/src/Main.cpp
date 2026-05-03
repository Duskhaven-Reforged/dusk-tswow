#include <Rendering/MSDF/MSDFBootstrap.h>
#include <ClientArguments.h>
#include <ClientDetours.h>
#include <ClientExtensions.h>
#include <ClientNetwork.h>
#include <Clientlua.h>
#include <FileIntegrity/FileIntegrity.h>
#include <FrameXMLExtensions.h>
#include <Logger.h>
#include <SavedConfigs/DHConfig.h>
#include <Windows.h>
#include <detours.h>
#include "IPC/VoiceUpdateIPC.h"
#include "IPC/x64FileStartup.h"
#include "scripts.generated.h"

class Main
{
  public:
    static void startup()
    {
        LOG_INFO << "Client starting up";
        ClientLua::allowOutOfBoundsPointer();
        LOG_INFO << "Client pointer extension applied";
        MSDFBootstrap::initialize();
        LOG_INFO << "MSDF font rendering initialized";
        // gets this from scripts.generated.ih
        MiscFixes::SetYearOffsetMultiplier();
        LOG_INFO << "Time offset set.";
        __init_scripts();
        LOG_INFO << "Client init scripts";
        DHConfig::Initialize();
        LOG_INFO << "Save file initialized";
        ClientNetwork::initialize();
        LOG_INFO << "Client network initialized";
        // some people get windows crashes, idk
        ClientArguments::initialize();
        LOG_INFO << "Client arguments initialized";
        ClientExtensions::initialize();
        LOG_INFO << "Client extensions initialized";
        VoiceUpdateIPC::Start();
        LOG_INFO << "Voice update IPC initialized";
        ClientDetours::Apply();
        LOG_INFO << "Client detours applied";
        FileIntegrity::RunStartupScan();
        LOG_INFO << "FileIntegrity initalized";
        FrameXMLExtensions::Apply();
        LOG_INFO << "FrameXMLExtensions applied";
    }
};

extern "C"
{
    // The function we register in the exe to load this dll
    __declspec(dllexport) void ClientExtensionsDummy() {}
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        LOG_INFO << "Attach";
        DisableThreadLibraryCalls(hinstDLL);
        LOG_INFO << "Pass DisableThreadLibraryCalls";
        CreateThread(
            nullptr, 0,
            [](LPVOID) -> DWORD
            {
                LOG_INFO << "Thread Made";
                Main::startup();
                LOG_INFO << "Main Done";
                X64FileStartup::StartDHV();
                LOG_INFO << "StartDHV Done";
                return 0;
            },
            nullptr, 0, nullptr);
    }
    return TRUE;
}
