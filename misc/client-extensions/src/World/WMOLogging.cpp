#include <ClientDetours.h>
#include <Logger.h>
#include <ClientData/SharedDefines.h>
#include <World/WMOLogging.h>

#include <Windows.h>
#include <detours.h>
#include <cstdint>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <ClientData/MathTypes.h>

using namespace ClientData;

void __fastcall CAaBsp__TraverseSafeDetour(void* self, void*, int nodeIndex, int* queryBounds, int* clipBounds);
void __fastcall CAaBsp__SplitTraverseSafeDetour(void* self, void*, int nodeIndex, int* queryBounds, int* clipBounds);

namespace WMOLogging
{
    constexpr uintptr_t WMOROOT_LOAD_START = 0x007D7470;
    constexpr uintptr_t WMOROOT_LOAD_END   = 0x007D7EFF;
    constexpr uintptr_t AABSP_TRAVERSE_START = 0x007CA920;
    constexpr uintptr_t AABSP_TRAVERSE_END   = 0x007CAACF;
    constexpr uintptr_t AABSP_SPLIT_TRAVERSE_START = 0x007CA440;
    constexpr uintptr_t AABSP_SPLIT_TRAVERSE_END   = 0x007CA5EF;
    constexpr uintptr_t TEXTURE_ASYNC_LOAD_START = 0x004B7E80;
    constexpr uintptr_t TEXTURE_ASYNC_LOAD_END   = 0x004B7F0D;
    constexpr size_t MAX_RECENT_EVENTS         = 48;
    constexpr size_t CMAPOBJGROUP_BSP_OFFSET  = 100;
    constexpr size_t AABSP_PRIMARY_NODE_OFFSET = 0;
    constexpr size_t AABSP_NODE_OFFSET        = 4;
    constexpr size_t AABSP_REFCOUNT_OFFSET    = 8;
    constexpr size_t AABSP_REF_OFFSET         = 12;
    constexpr size_t CMAPOBJGROUP_FLAGS_OFFSET = 48;
    constexpr size_t CMAPOBJGROUP_BOUNDS_OFFSET = 52;
    constexpr size_t CMAPOBJGROUP_RUNTIME_FLAGS_OFFSET = 408;
    constexpr size_t TEXTURE_LOAD_CONTEXT_OFFSET = 44;
    constexpr size_t TEXTURE_ASYNC_OBJECT_OFFSET = 64;
    constexpr size_t TEXTURE_PATH_OFFSET = 108;
    constexpr uintptr_t TEXTURE_BYTES_PENDING_ADDRESS = 0x00B49CA0;

    using TextureLoadCallback_t = int(__cdecl*)(int);
    using TextureLoadContextHasError_t = bool(__cdecl*)(int);
    using TextureLoadContextMarkError_t = void(__cdecl*)(int);
    using AsyncFileReadDestroyObject_t = int(__cdecl*)(int);

    inline TextureLoadContextHasError_t TextureLoadContextHasError =
        reinterpret_cast<TextureLoadContextHasError_t>(0x0047C0F0);
    inline TextureLoadContextMarkError_t TextureLoadContextMarkError =
        reinterpret_cast<TextureLoadContextMarkError_t>(0x004B4F90);
    inline AsyncFileReadDestroyObject_t AsyncFileReadDestroyObject =
        reinterpret_cast<AsyncFileReadDestroyObject_t>(0x004B9DE0);

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

    struct BspFailureContext
    {
        uintptr_t traversalPtr = 0;
        uintptr_t bspPtr = 0;
        uintptr_t groupPtr = 0;
        uintptr_t nodePtrPrimary = 0;
        uintptr_t nodePtr = 0;
        uintptr_t refPtr = 0;
        uint32_t refCount = 0;
        uint32_t groupFlags = 0;
        uint32_t runtimeFlags = 0;
        int nodeIndex = 0;
        uint32_t hitCount = 0;
        float groupBounds[6] = {};
        float queryBounds[6] = {};
        float clipBounds[6] = {};
        std::string candidatePath;
    };

    static std::mutex s_mutex;
    static std::deque<RecentFileEvent> s_recentEvents;
    static std::unordered_map<uintptr_t, std::string> s_handleToPath;
    static std::unordered_map<uint64_t, std::string> s_lastWmoPathBySize;
    static std::unordered_map<uintptr_t, uint32_t> s_bspFailureCounts;
    static std::string s_currentMapDirectory;
    static uint32_t s_currentMapId = 0;
    static C3Vector s_currentMapPosition = {};
    static RootParseContext s_lastRootParseContext;
    static BspFailureContext s_lastBspFailureContext;
    static PVOID s_vectoredHandler = nullptr;
    static bool s_bspDetourAttached = false;
    static bool s_bspSplitDetourAttached = false;

    using CAaBsp__TraverseSafe_t = void(__thiscall*)(void* self, int nodeIndex, int* queryBounds, int* clipBounds);
    inline CAaBsp__TraverseSafe_t CAaBsp__TraverseSafe =
        reinterpret_cast<CAaBsp__TraverseSafe_t>(AABSP_TRAVERSE_START);
    using CAaBsp__SplitTraverseSafe_t = void(__thiscall*)(void* self, int nodeIndex, int* queryBounds, int* clipBounds);
    inline CAaBsp__SplitTraverseSafe_t CAaBsp__SplitTraverseSafe =
        reinterpret_cast<CAaBsp__SplitTraverseSafe_t>(AABSP_SPLIT_TRAVERSE_START);

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

    bool IsMissingWmoPlaceholderPath(const std::string& path)
    {
        return _stricmp(path.c_str(), "world\\wmo\\Dungeon\\test\\missingwmo.wmo") == 0;
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

    bool TryReadPointer(uintptr_t address, uintptr_t& value)
    {
        __try
        {
            value = *reinterpret_cast<uintptr_t*>(address);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            value = 0;
            return false;
        }
    }

    bool TryReadUInt32(uintptr_t address, uint32_t& value)
    {
        __try
        {
            value = *reinterpret_cast<uint32_t*>(address);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            value = 0;
            return false;
        }
    }

    bool TryCopyBytes(const void* source, void* destination, size_t size)
    {
        __try
        {
            memcpy(destination, source, size);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    bool TryWriteUInt32(uintptr_t address, uint32_t value)
    {
        __try
        {
            *reinterpret_cast<uint32_t*>(address) = value;
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    int CaptureTextureLoadException(DWORD* exceptionCode, CONTEXT* contextSnapshot, EXCEPTION_POINTERS* exceptionInfo)
    {
        if (exceptionCode)
            *exceptionCode = exceptionInfo && exceptionInfo->ExceptionRecord ? exceptionInfo->ExceptionRecord->ExceptionCode : 0;

        if (contextSnapshot && exceptionInfo && exceptionInfo->ContextRecord)
            *contextSnapshot = *exceptionInfo->ContextRecord;

        return EXCEPTION_EXECUTE_HANDLER;
    }

    bool InvokeTextureLoadCallback(TextureLoadCallback_t callback, int textureObject, int* result, DWORD* exceptionCode, CONTEXT* contextSnapshot)
    {
        __try
        {
            if (result)
                *result = callback(textureObject);
            else
                callback(textureObject);

            return true;
        }
        __except (CaptureTextureLoadException(exceptionCode, contextSnapshot, GetExceptionInformation()))
        {
            return false;
        }
    }

    void AdjustTextureBytesPending(int32_t delta)
    {
        __try
        {
            *reinterpret_cast<int32_t*>(TEXTURE_BYTES_PENDING_ADDRESS) += delta;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
        }
    }

    void CallFillInSolidTexture(int textureObject)
    {
        const uintptr_t functionAddress = 0x004B6920;
        __asm
        {
            mov esi, textureObject
            mov ebx, textureObject
            add ebx, TEXTURE_LOAD_CONTEXT_OFFSET
            call functionAddress
        }
    }

    std::string ReadEmbeddedString(uintptr_t address, size_t capacity)
    {
        if (!address || !capacity)
            return {};

        std::vector<char> buffer(capacity, 0);
        if (!TryCopyBytes(reinterpret_cast<const void*>(address), buffer.data(), capacity))
            return {};

        buffer.back() = '\0';
        return std::string(buffer.data());
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

    void DumpRecentState(const EXCEPTION_POINTERS* exceptionInfo, const char* reason)
    {
        std::vector<RecentFileEvent> snapshot;
        std::string mapDirectory;
        uint32_t mapId = 0;
        C3Vector mapPosition = {};
        RootParseContext rootParseContext = {};
        BspFailureContext bspFailureContext = {};

        {
            std::lock_guard<std::mutex> lock(s_mutex);
            snapshot.assign(s_recentEvents.begin(), s_recentEvents.end());
            mapDirectory = s_currentMapDirectory;
            mapId = s_currentMapId;
            mapPosition = s_currentMapPosition;
            rootParseContext = s_lastRootParseContext;
            bspFailureContext = s_lastBspFailureContext;
        }

        LOG_ERROR << "WMOLogging: detected " << (reason ? reason : "client asset crash") << ". mapId=" << mapId
                  << " mapDir=" << mapDirectory
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

        if (bspFailureContext.traversalPtr || bspFailureContext.bspPtr || bspFailureContext.groupPtr)
        {
            LOG_ERROR << "WMOLogging: last bsp-failure traversal=0x" << std::hex << bspFailureContext.traversalPtr
                      << " bsp=0x" << bspFailureContext.bspPtr
                      << " group=0x" << bspFailureContext.groupPtr
                      << " nodePtr0=0x" << bspFailureContext.nodePtrPrimary
                      << " nodePtr=0x" << bspFailureContext.nodePtr
                      << " refPtr=0x" << bspFailureContext.refPtr
                      << std::dec
                      << " refCount=" << bspFailureContext.refCount
                      << " nodeIndex=" << bspFailureContext.nodeIndex
                      << " hitCount=" << bspFailureContext.hitCount
                      << " groupFlags=0x" << std::hex << bspFailureContext.groupFlags
                      << " runtimeFlags=0x" << bspFailureContext.runtimeFlags << std::dec
                      << " candidatePath="
                      << (bspFailureContext.candidatePath.empty() ? "<unknown>" : bspFailureContext.candidatePath);

            LOG_ERROR << "WMOLogging: last bsp-failure groupBounds=("
                      << bspFailureContext.groupBounds[0] << "," << bspFailureContext.groupBounds[1] << ","
                      << bspFailureContext.groupBounds[2] << ")->(" << bspFailureContext.groupBounds[3] << ","
                      << bspFailureContext.groupBounds[4] << "," << bspFailureContext.groupBounds[5] << ")";
            LOG_ERROR << "WMOLogging: last bsp-failure queryBounds=("
                      << bspFailureContext.queryBounds[0] << "," << bspFailureContext.queryBounds[1] << ","
                      << bspFailureContext.queryBounds[2] << ")->(" << bspFailureContext.queryBounds[3] << ","
                      << bspFailureContext.queryBounds[4] << "," << bspFailureContext.queryBounds[5] << ")";
            LOG_ERROR << "WMOLogging: last bsp-failure clipBounds=("
                      << bspFailureContext.clipBounds[0] << "," << bspFailureContext.clipBounds[1] << ","
                      << bspFailureContext.clipBounds[2] << ")->(" << bspFailureContext.clipBounds[3] << ","
                      << bspFailureContext.clipBounds[4] << "," << bspFailureContext.clipBounds[5] << ")";
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
            DumpRecentState(exceptionInfo, "WMORoot parse crash");
        else if ((faultAddress >= AABSP_TRAVERSE_START && faultAddress <= AABSP_TRAVERSE_END)
              || (faultAddress >= AABSP_SPLIT_TRAVERSE_START && faultAddress <= AABSP_SPLIT_TRAVERSE_END))
            DumpRecentState(exceptionInfo, "CMapObjGroup BSP traversal crash");
        else if (faultAddress >= TEXTURE_ASYNC_LOAD_START && faultAddress <= TEXTURE_ASYNC_LOAD_END)
            DumpRecentState(exceptionInfo, "async texture load crash");

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

        if (!s_bspDetourAttached)
        {
            DetourTransactionBegin();
            DetourUpdateThread(GetCurrentThread());
            LONG attachResult =
                DetourAttach(reinterpret_cast<PVOID*>(&CAaBsp__TraverseSafe), CAaBsp__TraverseSafeDetour);
            LONG commitResult = DetourTransactionCommit();

            if (attachResult == NO_ERROR && commitResult == NO_ERROR)
            {
                s_bspDetourAttached = true;
                LOG_INFO << "WMOLogging: direct detour attached for CAaBsp__TraverseSafe@0x"
                         << std::hex << AABSP_TRAVERSE_START << std::dec;
            }
            else
            {
                LOG_ERROR << "WMOLogging: failed direct detour for CAaBsp__TraverseSafe attachResult="
                          << attachResult << " commitResult=" << commitResult;
            }
        }

        if (!s_bspSplitDetourAttached)
        {
            DetourTransactionBegin();
            DetourUpdateThread(GetCurrentThread());
            LONG attachResult =
                DetourAttach(reinterpret_cast<PVOID*>(&CAaBsp__SplitTraverseSafe), CAaBsp__SplitTraverseSafeDetour);
            LONG commitResult = DetourTransactionCommit();

            if (attachResult == NO_ERROR && commitResult == NO_ERROR)
            {
                s_bspSplitDetourAttached = true;
                LOG_INFO << "WMOLogging: direct detour attached for CAaBsp__SplitTraverseSafe@0x"
                         << std::hex << AABSP_SPLIT_TRAVERSE_START << std::dec;
            }
            else
            {
                LOG_ERROR << "WMOLogging: failed direct detour for CAaBsp__SplitTraverseSafe attachResult="
                          << attachResult << " commitResult=" << commitResult;
            }
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

            if (IsMissingWmoPlaceholderPath(resolvedCandidate))
                resolvedCandidate.clear();

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

    void RecordMissingBspTraversal(void* traversalContext, int nodeIndex, int* queryBounds, int* clipBounds)
    {
        BspFailureContext context = {};
        context.traversalPtr = reinterpret_cast<uintptr_t>(traversalContext);

        if (traversalContext)
        {
            TryReadPointer(context.traversalPtr, context.bspPtr);
        }

        if (context.bspPtr)
        {
            TryReadPointer(context.bspPtr + AABSP_PRIMARY_NODE_OFFSET, context.nodePtrPrimary);
            TryReadPointer(context.bspPtr + AABSP_NODE_OFFSET, context.nodePtr);
            TryReadUInt32(context.bspPtr + AABSP_REFCOUNT_OFFSET, context.refCount);
            TryReadPointer(context.bspPtr + AABSP_REF_OFFSET, context.refPtr);
            context.groupPtr = context.bspPtr - CMAPOBJGROUP_BSP_OFFSET;
        }

        if (context.groupPtr)
        {
            TryReadUInt32(context.groupPtr + CMAPOBJGROUP_FLAGS_OFFSET, context.groupFlags);
            TryReadUInt32(context.groupPtr + CMAPOBJGROUP_RUNTIME_FLAGS_OFFSET, context.runtimeFlags);
            TryCopyBytes(
                reinterpret_cast<void*>(context.groupPtr + CMAPOBJGROUP_BOUNDS_OFFSET),
                context.groupBounds,
                sizeof(context.groupBounds)
            );
        }

        if (queryBounds)
            TryCopyBytes(queryBounds, context.queryBounds, sizeof(context.queryBounds));
        if (clipBounds)
            TryCopyBytes(clipBounds, context.clipBounds, sizeof(context.clipBounds));

        context.nodeIndex = nodeIndex;

        bool shouldLog = true;
        bool disabledGroup = false;
        {
            std::lock_guard<std::mutex> lock(s_mutex);
            const uintptr_t failureKey = context.groupPtr ? context.groupPtr : (context.bspPtr ? context.bspPtr : context.traversalPtr);
            context.hitCount = ++s_bspFailureCounts[failureKey];
            context.candidatePath = s_lastRootParseContext.candidatePath;
            s_lastBspFailureContext = context;

            if (context.groupPtr && context.hitCount == 1)
            {
                const uint32_t disabledRuntimeFlags = context.runtimeFlags & ~1u;
                if (TryWriteUInt32(context.groupPtr + CMAPOBJGROUP_RUNTIME_FLAGS_OFFSET, disabledRuntimeFlags))
                {
                    context.runtimeFlags = disabledRuntimeFlags;
                    s_lastBspFailureContext.runtimeFlags = disabledRuntimeFlags;
                    disabledGroup = true;
                }
            }

            shouldLog = context.hitCount <= 5 || (context.hitCount % 100) == 0;
            if (shouldLog)
            {
                PushRecentEventLocked(
                    "bsp-miss",
                    context.candidatePath.empty() ? "<unknown>" : context.candidatePath,
                    context.hitCount,
                    failureKey
                );
            }
        }

        if (!shouldLog)
            return;

        LOG_ERROR << "WMOLogging: missing BSP node data. traversal=0x" << std::hex << context.traversalPtr
                  << " bsp=0x" << context.bspPtr
                  << " group=0x" << context.groupPtr
                  << " nodePtr0=0x" << context.nodePtrPrimary
                  << " nodePtr=0x" << context.nodePtr
                  << " refPtr=0x" << context.refPtr
                  << std::dec
                  << " refCount=" << context.refCount
                  << " nodeIndex=" << context.nodeIndex
                  << " hitCount=" << context.hitCount
                  << " groupFlags=0x" << std::hex << context.groupFlags
                  << " runtimeFlags=0x" << context.runtimeFlags << std::dec
                  << " disabledGroup=" << (disabledGroup ? "true" : "false")
                  << " candidatePath=" << (context.candidatePath.empty() ? "<unknown>" : context.candidatePath);

        LOG_ERROR << "WMOLogging: missing BSP groupBounds=("
                  << context.groupBounds[0] << "," << context.groupBounds[1] << "," << context.groupBounds[2]
                  << ")->(" << context.groupBounds[3] << "," << context.groupBounds[4] << ","
                  << context.groupBounds[5] << ")";
        LOG_ERROR << "WMOLogging: missing BSP queryBounds=("
                  << context.queryBounds[0] << "," << context.queryBounds[1] << "," << context.queryBounds[2]
                  << ")->(" << context.queryBounds[3] << "," << context.queryBounds[4] << ","
                  << context.queryBounds[5] << ")";
        LOG_ERROR << "WMOLogging: missing BSP clipBounds=("
                  << context.clipBounds[0] << "," << context.clipBounds[1] << "," << context.clipBounds[2]
                  << ")->(" << context.clipBounds[3] << "," << context.clipBounds[4] << ","
                  << context.clipBounds[5] << ")";
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

CLIENT_DETOUR(TextureAsyncLoadCallbackSafe, 0x004B7E80, __cdecl, int, (int textureObject))
{
    int result = 0;
    DWORD exceptionCode = 0;
    CONTEXT contextSnapshot = {};
    if (WMOLogging::InvokeTextureLoadCallback(
            TextureAsyncLoadCallbackSafe,
            textureObject,
            &result,
            &exceptionCode,
            &contextSnapshot))
        return result;

    uintptr_t asyncObject = 0;
    uintptr_t fileHandle = 0;
    uintptr_t fileBits = 0;
    uint32_t fileSize = 0;

    WMOLogging::TryReadPointer(textureObject + WMOLogging::TEXTURE_ASYNC_OBJECT_OFFSET, asyncObject);
    if (asyncObject)
    {
        WMOLogging::TryReadPointer(asyncObject, fileHandle);
        WMOLogging::TryReadPointer(asyncObject + 4, fileBits);
        WMOLogging::TryReadUInt32(asyncObject + 8, fileSize);
    }

    const std::string texturePath =
        WMOLogging::ReadEmbeddedString(textureObject + WMOLogging::TEXTURE_PATH_OFFSET, 260);

    {
        std::lock_guard<std::mutex> lock(WMOLogging::s_mutex);
        WMOLogging::PushRecentEventLocked(
            "texture-crash",
            texturePath.empty() ? "<unknown>" : texturePath,
            fileSize,
            static_cast<uintptr_t>(textureObject)
        );
    }

    LOG_ERROR << "WMOLogging: async texture load crashed. texture=0x" << std::hex << textureObject
              << " async=0x" << asyncObject
              << " fileHandle=0x" << fileHandle
              << " fileBits=0x" << fileBits
              << std::dec
              << " fileSize=" << fileSize
              << " exception=0x" << std::hex << exceptionCode << std::dec
              << " path=" << (texturePath.empty() ? "<unknown>" : texturePath);
    LOG_ERROR << "WMOLogging: texture crash context EIP=0x" << std::hex << contextSnapshot.Eip
              << " ESI=0x" << contextSnapshot.Esi
              << " EDI=0x" << contextSnapshot.Edi
              << " EAX=0x" << contextSnapshot.Eax
              << " ECX=0x" << contextSnapshot.Ecx
              << " EDX=0x" << contextSnapshot.Edx << std::dec;

    if (!WMOLogging::TextureLoadContextHasError(textureObject + WMOLogging::TEXTURE_LOAD_CONTEXT_OFFSET))
        WMOLogging::TextureLoadContextMarkError(textureObject + WMOLogging::TEXTURE_LOAD_CONTEXT_OFFSET);

    WMOLogging::CallFillInSolidTexture(textureObject);

    result = 1;
    if (asyncObject)
    {
        if (fileHandle)
            SFile::CloseFile(reinterpret_cast<HANDLE>(fileHandle));

        WMOLogging::TryWriteUInt32(asyncObject, 0);

        if (fileSize)
            WMOLogging::AdjustTextureBytesPending(-static_cast<int32_t>(fileSize));

        if (fileBits)
            SMem::Free(reinterpret_cast<void*>(fileBits), const_cast<char*>(".\\Texture.cpp"), 2013, 0);

        result = WMOLogging::AsyncFileReadDestroyObject(static_cast<int>(asyncObject));
        WMOLogging::TryWriteUInt32(textureObject + WMOLogging::TEXTURE_ASYNC_OBJECT_OFFSET, 0);
    }

    return result;
}

void __fastcall CAaBsp__TraverseSafeDetour(void* self, void*, int nodeIndex, int* queryBounds, int* clipBounds)
{
    uintptr_t bspPtr = 0;
    uintptr_t nodePtr = 0;

    if (self)
    {
        WMOLogging::TryReadPointer(reinterpret_cast<uintptr_t>(self), bspPtr);
        if (bspPtr)
            WMOLogging::TryReadPointer(bspPtr + WMOLogging::AABSP_NODE_OFFSET, nodePtr);
    }

    if (!nodePtr)
    {
        WMOLogging::RecordMissingBspTraversal(self, nodeIndex, queryBounds, clipBounds);
        return;
    }

    return WMOLogging::CAaBsp__TraverseSafe(self, nodeIndex, queryBounds, clipBounds);
}

void __fastcall CAaBsp__SplitTraverseSafeDetour(void* self, void*, int nodeIndex, int* queryBounds, int* clipBounds)
{
    uintptr_t bspPtr = 0;
    uintptr_t nodePtr = 0;

    if (self)
    {
        WMOLogging::TryReadPointer(reinterpret_cast<uintptr_t>(self), bspPtr);
        if (bspPtr)
            WMOLogging::TryReadPointer(bspPtr + WMOLogging::AABSP_NODE_OFFSET, nodePtr);
    }

    if (!nodePtr)
    {
        WMOLogging::RecordMissingBspTraversal(self, nodeIndex, queryBounds, clipBounds);
        return;
    }

    return WMOLogging::CAaBsp__SplitTraverseSafe(self, nodeIndex, queryBounds, clipBounds);
}
