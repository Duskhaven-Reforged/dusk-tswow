#include "x64FileStartup.h"
#include <Logger.h>
#include <SavedConfigs/DHConfig.h>
#include <Windows.h>
#include <cstdio>

namespace X64FileStartup
{
    PROCESS_INFORMATION g_processInfo = {0};
    HANDLE g_job                      = NULL;

    void StartDHV()
    {

        STARTUPINFOA si = {};
        si.cb           = sizeof(si);
        si.dwFlags      = STARTF_USESHOWWINDOW;
        si.wShowWindow  = SW_HIDE;
        ZeroMemory(&g_processInfo, sizeof(g_processInfo));

        const char* executable = "./ClientExtensions64.exe";
        char cmd[4096]         = {};
        std::snprintf(cmd, sizeof(cmd), "\"%s\" --allowDiscord=%d", executable, DHConfig::ReadInt("allowDiscord", 1));

        g_job = CreateJobObjectA(NULL, NULL);
        if (!g_job)
            return;

        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = {0};
        jeli.BasicLimitInformation.LimitFlags     = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(g_job, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
        if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &g_processInfo))
        {
            LOG_INFO << "Started " << executable;
            AssignProcessToJobObject(g_job, g_processInfo.hProcess);
        }
    }
}