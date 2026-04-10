#include <Windows.h>
#include <detours.h>
#include "scripts.generated.h"

#include <ClientArguments.h>
#include <ClientDetours.h>
#include <FileIntegrity/FileIntegrity.h>
#include <ClientExtensions.h>
#include <Clientlua.h>
#include <ClientNetwork.h>
#include <FrameXMLExtensions.h>
#include <Logger.h>
#include "IPC/IPCTest.h"

#include <vector>

class Main
{
  public:
    static inline PROCESS_INFORMATION pi = {0};
    static inline HANDLE hJob            = NULL;
    static void startup()
    {
        LOG_INFO << "Client starting up";
        // gets this from scripts.generated.ih
        MiscFixes::SetYearOffsetMultiplier();
        LOG_INFO << "Time offset set.";
        __init_scripts();
        LOG_INFO << "Client init scripts";
        ClientLua::allowOutOfBoundsPointer();
        LOG_INFO << "Client pointer extension applied";
        ClientNetwork::initialize();
        LOG_INFO << "Client network initialized";
        // some people get windows crashes, idk
        ClientArguments::initialize();
        LOG_INFO << "Client arguments initialized";
        ClientExtensions::initialize();
        LOG_INFO << "Client extensions initialized";
        ClientDetours::Apply();
        LOG_INFO << "Client detours applied";
        FileIntegrity::RunStartupScan();
        LOG_INFO << "FileIntegrity initalized";
        FrameXMLExtensions::Apply();
        LOG_INFO << "FrameXMLExtensions applied";
    }

    static void StartDHV() {
        STARTUPINFOA si;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        // Command line construction
        const char* executable = "./ClientExtensions64.exe";
        const char* args       = ""; // e.g. "-debug -port 8080"

        char cmd[4096];
        if (strlen(args) > 0)
            snprintf(cmd, sizeof(cmd), "%s %s", executable, args);
        else
            snprintf(cmd, sizeof(cmd), "%s", executable);

        // Create a job object to ensure the child process is killed when the parent is killed
        hJob = CreateJobObjectA(NULL, NULL);
        if (hJob)
        {
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = {0};
            jeli.BasicLimitInformation.LimitFlags     = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
            if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
            {
                LOG_INFO << "Started " << executable;
                if (hJob)
                {
                    AssignProcessToJobObject(hJob, pi.hProcess);
                }
            }
        }
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
        CreateThread(nullptr, 0, [](LPVOID) -> DWORD {
            LOG_INFO << "Thread Made";
            Main::startup();
            LOG_INFO << "Main Done";
            Main::StartDHV();
            LOG_INFO << "StartDHV Done";
            return 0;
        }, nullptr, 0, nullptr);
    }
    return TRUE;
}
