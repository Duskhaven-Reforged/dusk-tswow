#include <ClientDetours.h>
#include <Logger.h>
#include <SharedDefines.h>
#include <World/WMOLogging.h>

#include <Windows.h>
#include <cstdint>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace WMOLogging
{
    constexpr uintptr_t WMOROOT_LOAD_START = 0x007D7470;
    constexpr uintptr_t WMOROOT_LOAD_END   = 0x007D7EFF;
    constexpr size_t MAX_RECENT_EVENTS         = 48;

    struct RecentFileEvent
    {
        uint64_t tickMs = 0;
        std::string action;
        std::string path;
        uint64_t size    = 0;
        uintptr_t handle = 0;
    };

    struct RootParseContext
    {
        uintptr_t objectPtr = 0;
        uintptr_t bufferPtr = 0;
        uint32_t bufferSize = 0;
        uint64_t bufferHash = 0;
        std::string candidatePath;
    };

    static std::mutex s_mutex;
    static std::deque<RecentFileEvent> s_recentEvents;
    static std::unordered_map<uintptr_t, std::string> s_handleToPath;
    static std::unordered_map<uint64_t, std::string> s_lastWmoPathBySize;
    static std::string s_currentMapDirectory;
    static uint32_t s_currentMapId = 0;
    static C3Vector s_currentMapPosition = {};
    static RootParseContext s_lastRootParseContext;
    static PVOID s_vectoredHandler = nullptr;

    bool EndsWithInsensitive(const char* value, const char* suffix)
    {
        if (!value || !suffix)
            return false;

        const size_t valueLen  = strlen(value);
        const size_t suffixLen = strlen(suffix);
        if (valueLen < suffixLen)
            return false;

        return _stricmp(value + valueLen - suffixLen, suffix) == 0;
    }

    bool IsTrackedAssetPath(const char* path)
    {
        return EndsWithInsensitive(path, ".wmo") || EndsWithInsensitive(path, ".adt")
            || EndsWithInsensitive(path, ".wdt");
    }

    void PushRecentEventLocked(const char* action, const std::string& path, uint64_t size, uintptr_t handle)
    {
        RecentFileEvent event;
        event.tickMs = OsGetAsyncTimeMs();
        event.action = action ? action : "";
        event.path   = path;
        event.size   = size;
        event.handle = handle;

        s_recentEvents.push_back(std::move(event));
        while (s_recentEvents.size() > MAX_RECENT_EVENTS)
            s_recentEvents.pop_front();
    }

    uint64_t GetHandleSize(HANDLE handle)
    {
        if (!handle)
            return 0;

        DWORD high = 0;
        DWORD low  = SFile::GetFileSize(handle, &high);
        if (low == INVALID_FILE_SIZE && GetLastError() != NO_ERROR)
            return 0;

        return (static_cast<uint64_t>(high) << 32) | low;
    }

    uint64_t ComputeFnv1a64(const uint8_t* data, size_t size)
    {
        uint64_t hash = 1469598103934665603ull;
        for (size_t i = 0; i < size; ++i)
        {
            hash ^= static_cast<uint64_t>(data[i]);
            hash *= 1099511628211ull;
        }
        return hash;
    }

    void RecordSuccessfulOpen(const char* action, const char* path, HANDLE handle)
    {
        if (!path || !IsTrackedAssetPath(path))
            return;

        const uint64_t size = GetHandleSize(handle);
        const uintptr_t key = reinterpret_cast<uintptr_t>(handle);

        {
            std::lock_guard<std::mutex> lock(s_mutex);
            s_handleToPath[key] = path;
            if (EndsWithInsensitive(path, ".wmo"))
                s_lastWmoPathBySize[size] = path;
            PushRecentEventLocked(action, path, size, key);
        }

        LOG_INFO << "WMOLogging: " << action << " path=" << path << " handle=0x" << std::hex << key << std::dec
                 << " size=" << size;
    }

    void RecordClose(HANDLE handle)
    {
        const uintptr_t key = reinterpret_cast<uintptr_t>(handle);
        std::string path;

        {
            std::lock_guard<std::mutex> lock(s_mutex);
            auto itr = s_handleToPath.find(key);
            if (itr == s_handleToPath.end())
                return;

            path = itr->second;
            PushRecentEventLocked("close", path, 0, key);
            s_handleToPath.erase(itr);
        }

        LOG_INFO << "WMOLogging: close path=" << path << " handle=0x" << std::hex << key << std::dec;
    }

    void DumpRecentState(const EXCEPTION_POINTERS* exceptionInfo)
    {
        std::vector<RecentFileEvent> snapshot;
        std::string mapDirectory;
        uint32_t mapId = 0;
        C3Vector mapPosition = {};
        RootParseContext rootParseContext = {};

        {
            std::lock_guard<std::mutex> lock(s_mutex);
            snapshot.assign(s_recentEvents.begin(), s_recentEvents.end());
            mapDirectory = s_currentMapDirectory;
            mapId = s_currentMapId;
            mapPosition = s_currentMapPosition;
            rootParseContext = s_lastRootParseContext;
        }

        LOG_ERROR << "WMOLogging: detected WMORoot parse crash. mapId=" << mapId << " mapDir=" << mapDirectory
                  << " pos=(" << mapPosition.x << "," << mapPosition.y << "," << mapPosition.z << ")";

        if (exceptionInfo && exceptionInfo->ContextRecord)
        {
            const CONTEXT* context = exceptionInfo->ContextRecord;
            LOG_ERROR << "WMOLogging: crash context EIP=0x" << std::hex << context->Eip << " ECX=0x" << context->Ecx
                      << " ESI=0x" << context->Esi << " EDI=0x" << context->Edi << " EAX=0x" << context->Eax
                      << " EDX=0x" << context->Edx << std::dec;
        }

        if (rootParseContext.objectPtr || rootParseContext.bufferPtr || !rootParseContext.candidatePath.empty())
        {
            LOG_ERROR << "WMOLogging: last root-parse object=0x" << std::hex << rootParseContext.objectPtr
                      << " buffer=0x" << rootParseContext.bufferPtr
                      << " hash=0x" << rootParseContext.bufferHash << std::dec
                      << " size=" << rootParseContext.bufferSize
                      << " candidatePath="
                      << (rootParseContext.candidatePath.empty() ? "<unknown>" : rootParseContext.candidatePath);
        }

        for (const RecentFileEvent& event : snapshot)
        {
            LOG_ERROR << "WMOLogging: recent tick=" << event.tickMs << " action=" << event.action << " handle=0x"
                      << std::hex << event.handle << std::dec << " size=" << event.size
                      << " path=" << event.path;
        }
    }

    LONG CALLBACK VectoredCrashHandler(EXCEPTION_POINTERS* exceptionInfo)
    {
        if (!exceptionInfo || !exceptionInfo->ExceptionRecord)
            return EXCEPTION_CONTINUE_SEARCH;

        const uintptr_t faultAddress =
            reinterpret_cast<uintptr_t>(exceptionInfo->ExceptionRecord->ExceptionAddress);

        if (faultAddress >= WMOROOT_LOAD_START && faultAddress <= WMOROOT_LOAD_END)
            DumpRecentState(exceptionInfo);

        return EXCEPTION_CONTINUE_SEARCH;
    }

    void RecordMapAssetOpen(const char* path, HANDLE handle)
    {
        RecordSuccessfulOpen("map-open", path, handle);
    }

    void RecordMapAssetMissing(const char* path)
    {
        if (!path || !IsTrackedAssetPath(path))
            return;

        {
            std::lock_guard<std::mutex> lock(s_mutex);
            PushRecentEventLocked("map-miss", path, 0, 0);
        }

        LOG_WARN << "WMOLogging: map-miss path=" << path;
    }

    void Apply()
    {
        if (!s_vectoredHandler)
        {
            s_vectoredHandler = AddVectoredExceptionHandler(1, &VectoredCrashHandler);
            LOG_INFO << "WMOLogging: vectored crash handler registered";
        }
    }

    void RecordWmoRootParseBegin(void* objectPtr, const char* candidatePath, uintptr_t bufferPtr, uint32_t bufferSize)
    {
        const uintptr_t objectKey = reinterpret_cast<uintptr_t>(objectPtr);
        std::string resolvedCandidate = candidatePath ? candidatePath : "";
        uint64_t bufferHash = 0;

        if (bufferPtr && bufferSize > 0 && bufferSize <= (64u * 1024u * 1024u))
        {
            bufferHash = ComputeFnv1a64(reinterpret_cast<const uint8_t*>(bufferPtr), bufferSize);
        }

        {
            std::lock_guard<std::mutex> lock(s_mutex);
            if (resolvedCandidate.empty())
            {
                auto bySize = s_lastWmoPathBySize.find(bufferSize);
                if (bufferSize > 0 && bySize != s_lastWmoPathBySize.end())
                    resolvedCandidate = bySize->second;
            }

            if (resolvedCandidate.empty())
            {
                for (auto itr = s_recentEvents.rbegin(); itr != s_recentEvents.rend(); ++itr)
                {
                    if (itr->action == "map-open" && EndsWithInsensitive(itr->path.c_str(), ".wmo"))
                    {
                        resolvedCandidate = itr->path;
                        break;
                    }
                }
            }

            s_lastRootParseContext.objectPtr = objectKey;
            s_lastRootParseContext.bufferPtr = bufferPtr;
            s_lastRootParseContext.bufferSize = bufferSize;
            s_lastRootParseContext.bufferHash = bufferHash;
            s_lastRootParseContext.candidatePath = resolvedCandidate;

            PushRecentEventLocked(
                "root-parse-begin",
                resolvedCandidate.empty() ? "<unknown>" : resolvedCandidate,
                0,
                objectKey
            );
        }

        LOG_INFO << "WMOLogging: root-parse-begin object=0x" << std::hex << objectKey
                 << " buffer=0x" << bufferPtr
                 << " hash=0x" << bufferHash << std::dec
                 << " size=" << bufferSize
                 << " candidatePath=" << (resolvedCandidate.empty() ? "<unknown>" : resolvedCandidate);
    }
}

CLIENT_DETOUR(World__LoadMapTrace, 0x781430, __cdecl, void, (char* directory, C3Vector* position, uint32_t mapId))
{
    {
        std::lock_guard<std::mutex> lock(WMOLogging::s_mutex);
        WMOLogging::s_currentMapDirectory = directory ? directory : "";
        WMOLogging::s_currentMapId = mapId;
        WMOLogging::s_currentMapPosition = position ? *position : C3Vector{};
    }

    LOG_INFO << "WMOLogging: LoadMap mapId=" << mapId << " dir=" << (directory ? directory : "<null>");
    if (position)
        LOG_INFO << "WMOLogging: LoadMap position=(" << position->x << "," << position->y << "," << position->z
                 << ")";

    World__LoadMapTrace(directory, position, mapId);
}

CLIENT_DETOUR_THISCALL_NOARGS(WMORoot__PostloadCallbackTrace, 0x007D7EB0, int)
{
    auto* a1 = reinterpret_cast<uint32_t*>(self);
    uintptr_t bufferPtr = 0;
    uint32_t bufferSize = 0;
    if (a1)
    {
        bufferPtr = static_cast<uintptr_t>(a1[115]);
        bufferSize = a1[116];
    }

    WMOLogging::RecordWmoRootParseBegin(a1, nullptr, bufferPtr, bufferSize);
    return WMORoot__PostloadCallbackTrace(self);
}
