#pragma once

#ifdef _WIN32
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace Duskhaven::IPC
{
constexpr uint32_t kSharedRingMagic = 0x50494844; // DHIP
constexpr uint32_t kSharedRingVersion = 1;
constexpr uint32_t kDefaultRingCapacity = 16u * 1024u * 1024u;
constexpr uint32_t kMaxRingCapacity = 64u * 1024u * 1024u;

inline void StoreU32LE(uint8_t* out, uint32_t value)
{
    out[0] = static_cast<uint8_t>(value & 0xFF);
    out[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    out[2] = static_cast<uint8_t>((value >> 16) & 0xFF);
    out[3] = static_cast<uint8_t>((value >> 24) & 0xFF);
}

inline uint32_t LoadU32LE(const uint8_t* in)
{
    return static_cast<uint32_t>(in[0]) |
           (static_cast<uint32_t>(in[1]) << 8) |
           (static_cast<uint32_t>(in[2]) << 16) |
           (static_cast<uint32_t>(in[3]) << 24);
}

struct SharedRingSegment
{
    const void* data = nullptr;
    uint32_t bytes = 0;
};

struct alignas(64) SharedRingHeader
{
    uint32_t magic = 0;
    uint32_t version = 0;
    uint32_t capacity = 0;
    uint32_t reserved = 0;

    alignas(64) volatile LONG64 writeCursor = 0;
    alignas(64) volatile LONG64 readCursor = 0;
};

class SharedMemoryRingBuffer
{
public:
    SharedMemoryRingBuffer() = default;
    ~SharedMemoryRingBuffer() { Close(); }

    SharedMemoryRingBuffer(const SharedMemoryRingBuffer&) = delete;
    SharedMemoryRingBuffer& operator=(const SharedMemoryRingBuffer&) = delete;

    bool Create(const std::wstring& channelName, uint32_t capacity = kDefaultRingCapacity)
    {
        Close();

        if (capacity < 4096 || capacity > kMaxRingCapacity)
        {
            return false;
        }

        channelName_ = channelName;
        mapName_ = MakeKernelObjectName(channelName, L"map");
        dataEventName_ = MakeKernelObjectName(channelName, L"data");
        spaceEventName_ = MakeKernelObjectName(channelName, L"space");

        const uint64_t totalBytes = static_cast<uint64_t>(sizeof(SharedRingHeader)) + capacity;
        if (totalBytes > (std::numeric_limits<DWORD>::max)())
        {
            return false;
        }

        mapping_ = CreateFileMappingW(
            INVALID_HANDLE_VALUE,
            nullptr,
            PAGE_READWRITE,
            static_cast<DWORD>(totalBytes >> 32),
            static_cast<DWORD>(totalBytes & 0xFFFFFFFFu),
            mapName_.c_str());
        if (!mapping_)
        {
            Close();
            return false;
        }

        if (!Map())
        {
            Close();
            return false;
        }

        if (mappedBytes_ < totalBytes)
        {
            Close();
            return false;
        }

        Initialize(capacity);

        dataEvent_ = CreateEventW(nullptr, FALSE, FALSE, dataEventName_.c_str());
        spaceEvent_ = CreateEventW(nullptr, FALSE, TRUE, spaceEventName_.c_str());
        if (!dataEvent_ || !spaceEvent_)
        {
            Close();
            return false;
        }

        SetEvent(spaceEvent_);
        return true;
    }

    bool Open(const std::wstring& channelName, DWORD timeoutMs)
    {
        Close();

        channelName_ = channelName;
        mapName_ = MakeKernelObjectName(channelName, L"map");
        dataEventName_ = MakeKernelObjectName(channelName, L"data");
        spaceEventName_ = MakeKernelObjectName(channelName, L"space");

        const DWORD start = GetTickCount();
        while (true)
        {
            mapping_ = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, mapName_.c_str());
            if (mapping_)
            {
                break;
            }

            if (timeoutMs == 0)
            {
                Close();
                return false;
            }

            if (timeoutMs != INFINITE && GetTickCount() - start >= timeoutMs)
            {
                Close();
                return false;
            }

            Sleep(1);
        }

        while (true)
        {
            if (Map() && Validate())
            {
                dataEvent_ = CreateEventW(nullptr, FALSE, FALSE, dataEventName_.c_str());
                spaceEvent_ = CreateEventW(nullptr, FALSE, TRUE, spaceEventName_.c_str());
                if (dataEvent_ && spaceEvent_)
                {
                    return true;
                }
            }

            Close();

            if (timeoutMs == 0)
            {
                return false;
            }

            if (timeoutMs != INFINITE && GetTickCount() - start >= timeoutMs)
            {
                return false;
            }

            Sleep(1);
            mapping_ = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, mapName_.c_str());
            if (!mapping_)
            {
                if (timeoutMs != INFINITE && GetTickCount() - start >= timeoutMs)
                {
                    return false;
                }
                Sleep(1);
            }
        }
    }

    void Close()
    {
        if (view_)
        {
            UnmapViewOfFile(view_);
            view_ = nullptr;
        }

        if (mapping_)
        {
            CloseHandle(mapping_);
            mapping_ = nullptr;
        }

        if (dataEvent_)
        {
            CloseHandle(dataEvent_);
            dataEvent_ = nullptr;
        }

        if (spaceEvent_)
        {
            CloseHandle(spaceEvent_);
            spaceEvent_ = nullptr;
        }

        header_ = nullptr;
        data_ = nullptr;
        capacity_ = 0;
        mappedBytes_ = 0;
    }

    bool IsOpen() const
    {
        return header_ != nullptr && data_ != nullptr && dataEvent_ != nullptr && spaceEvent_ != nullptr;
    }

    bool Write(const void* data, uint32_t bytes, DWORD timeoutMs = INFINITE)
    {
        SharedRingSegment segment{data, bytes};
        return Write(&segment, 1, timeoutMs);
    }

    bool Write(const SharedRingSegment* segments, uint32_t segmentCount, DWORD timeoutMs = INFINITE)
    {
        if (!IsOpen() || !segments)
        {
            return false;
        }

        uint32_t payloadBytes = 0;
        for (uint32_t i = 0; i < segmentCount; ++i)
        {
            if (segments[i].bytes > 0 && !segments[i].data)
            {
                return false;
            }

            if (segments[i].bytes > (std::numeric_limits<uint32_t>::max)() - payloadBytes)
            {
                return false;
            }
            payloadBytes += segments[i].bytes;
        }

        const uint32_t frameBytes = payloadBytes + sizeof(uint32_t);
        if (payloadBytes > capacity_ - sizeof(uint32_t))
        {
            return false;
        }

        if (!WaitForSpace(frameBytes, timeoutMs))
        {
            return false;
        }

        uint64_t cursor = LoadCursor(&header_->writeCursor);
        const bool wasEmpty = cursor == LoadCursor(&header_->readCursor);
        uint8_t lengthBytes[sizeof(uint32_t)];
        StoreU32LE(lengthBytes, payloadBytes);

        CopyToRing(cursor, lengthBytes, sizeof(lengthBytes));
        cursor += sizeof(lengthBytes);

        for (uint32_t i = 0; i < segmentCount; ++i)
        {
            if (segments[i].bytes == 0)
            {
                continue;
            }

            CopyToRing(cursor, segments[i].data, segments[i].bytes);
            cursor += segments[i].bytes;
        }

        StoreCursor(&header_->writeCursor, cursor);
        if (wasEmpty)
        {
            SetEvent(dataEvent_);
        }
        return true;
    }

    bool TryRead(std::vector<uint8_t>& out)
    {
        if (!IsOpen())
        {
            return false;
        }

        const uint64_t readCursor = LoadCursor(&header_->readCursor);
        const uint64_t writeCursor = LoadCursor(&header_->writeCursor);
        const uint64_t available = writeCursor - readCursor;
        if (available < sizeof(uint32_t))
        {
            return false;
        }

        uint8_t lengthBytes[sizeof(uint32_t)];
        CopyFromRing(readCursor, lengthBytes, sizeof(lengthBytes));

        const uint32_t payloadBytes = LoadU32LE(lengthBytes);
        if (payloadBytes > capacity_ - sizeof(uint32_t))
        {
            StoreCursor(&header_->readCursor, writeCursor);
            SetEvent(spaceEvent_);
            return false;
        }

        const uint64_t frameBytes = static_cast<uint64_t>(sizeof(uint32_t)) + payloadBytes;
        if (available < frameBytes)
        {
            return false;
        }

        out.resize(payloadBytes);
        if (payloadBytes > 0)
        {
            CopyFromRing(readCursor + sizeof(uint32_t), out.data(), payloadBytes);
        }

        StoreCursor(&header_->readCursor, readCursor + frameBytes);
        SetEvent(spaceEvent_);
        return true;
    }

    template <typename Consumer>
    bool TryConsume(std::vector<uint8_t>& scratch, Consumer&& consume)
    {
        if (!IsOpen())
        {
            return false;
        }

        const uint64_t readCursor = LoadCursor(&header_->readCursor);
        const uint64_t writeCursor = LoadCursor(&header_->writeCursor);
        const uint64_t available = writeCursor - readCursor;
        if (available < sizeof(uint32_t))
        {
            return false;
        }

        uint8_t lengthBytes[sizeof(uint32_t)];
        CopyFromRing(readCursor, lengthBytes, sizeof(lengthBytes));

        const uint32_t payloadBytes = LoadU32LE(lengthBytes);
        if (payloadBytes > capacity_ - sizeof(uint32_t))
        {
            StoreCursor(&header_->readCursor, writeCursor);
            SetEvent(spaceEvent_);
            return false;
        }

        const uint64_t frameBytes = static_cast<uint64_t>(sizeof(uint32_t)) + payloadBytes;
        if (available < frameBytes)
        {
            return false;
        }

        const uint64_t payloadCursor = readCursor + sizeof(uint32_t);
        const uint32_t payloadOffset = static_cast<uint32_t>(payloadCursor % capacity_);
        const uint32_t contiguousBytes = capacity_ - payloadOffset;

        if (payloadBytes == 0)
        {
            consume(nullptr, 0);
        }
        else if (payloadBytes <= contiguousBytes)
        {
            consume(data_ + payloadOffset, payloadBytes);
        }
        else
        {
            scratch.resize(payloadBytes);
            CopyFromRing(payloadCursor, scratch.data(), payloadBytes);
            consume(scratch.data(), payloadBytes);
        }

        StoreCursor(&header_->readCursor, readCursor + frameBytes);
        SetEvent(spaceEvent_);
        return true;
    }

    bool Read(std::vector<uint8_t>& out, const std::atomic<bool>* running = nullptr, DWORD waitSliceMs = INFINITE)
    {
        while (!running || running->load(std::memory_order_acquire))
        {
            if (TryRead(out))
            {
                return true;
            }

            const DWORD waitMs = running && (waitSliceMs == INFINITE || waitSliceMs > 50)
                ? 50
                : waitSliceMs;
            WaitForSingleObject(dataEvent_, waitMs);
        }
        return false;
    }

    template <typename Consumer>
    bool Consume(std::vector<uint8_t>& scratch, Consumer&& consume, const std::atomic<bool>* running = nullptr, DWORD waitSliceMs = INFINITE)
    {
        while (!running || running->load(std::memory_order_acquire))
        {
            if (TryConsume(scratch, consume))
            {
                return true;
            }

            const DWORD waitMs = running && (waitSliceMs == INFINITE || waitSliceMs > 50)
                ? 50
                : waitSliceMs;
            WaitForSingleObject(dataEvent_, waitMs);
        }
        return false;
    }

    void WakeReader()
    {
        if (dataEvent_)
        {
            SetEvent(dataEvent_);
        }
    }

    void WakeWriter()
    {
        if (spaceEvent_)
        {
            SetEvent(spaceEvent_);
        }
    }

private:
    static std::wstring MakeKernelObjectName(const std::wstring& channelName, const wchar_t* suffix)
    {
        std::wstring name = L"Local\\DuskhavenIPC_";
        for (wchar_t ch : channelName)
        {
            const bool keep =
                (ch >= L'0' && ch <= L'9') ||
                (ch >= L'A' && ch <= L'Z') ||
                (ch >= L'a' && ch <= L'z');
            name.push_back(keep ? ch : L'_');
        }
        name.push_back(L'_');
        name.append(suffix);
        return name;
    }

    static uint64_t LoadCursor(volatile LONG64* cursor)
    {
        const LONG64 value = InterlockedCompareExchange64(cursor, 0, 0);
        MemoryBarrier();
        return static_cast<uint64_t>(value);
    }

    static void StoreCursor(volatile LONG64* cursor, uint64_t value)
    {
        MemoryBarrier();
        InterlockedExchange64(cursor, static_cast<LONG64>(value));
    }

    bool Map()
    {
        view_ = MapViewOfFile(mapping_, FILE_MAP_ALL_ACCESS, 0, 0, 0);
        if (!view_)
        {
            return false;
        }

        MEMORY_BASIC_INFORMATION info{};
        if (VirtualQuery(view_, &info, sizeof(info)) == 0)
        {
            return false;
        }

        mappedBytes_ = info.RegionSize;
        header_ = reinterpret_cast<SharedRingHeader*>(view_);
        data_ = reinterpret_cast<uint8_t*>(view_) + sizeof(SharedRingHeader);
        return true;
    }

    void Initialize(uint32_t capacity)
    {
        std::memset(view_, 0, sizeof(SharedRingHeader) + capacity);
        header_->magic = kSharedRingMagic;
        header_->version = kSharedRingVersion;
        header_->capacity = capacity;
        StoreCursor(&header_->readCursor, 0);
        StoreCursor(&header_->writeCursor, 0);
        capacity_ = capacity;
    }

    bool Validate()
    {
        if (!header_ ||
            header_->magic != kSharedRingMagic ||
            header_->version != kSharedRingVersion ||
            header_->capacity < 4096 ||
            header_->capacity > kMaxRingCapacity)
        {
            return false;
        }

        const uint64_t requiredBytes = static_cast<uint64_t>(sizeof(SharedRingHeader)) + header_->capacity;
        if (mappedBytes_ < requiredBytes)
        {
            return false;
        }

        capacity_ = header_->capacity;
        data_ = reinterpret_cast<uint8_t*>(view_) + sizeof(SharedRingHeader);
        return true;
    }

    uint64_t UsedBytes() const
    {
        const uint64_t writeCursor = LoadCursor(&header_->writeCursor);
        const uint64_t readCursor = LoadCursor(&header_->readCursor);
        const uint64_t used = writeCursor - readCursor;
        return used <= capacity_ ? used : capacity_;
    }

    bool WaitForSpace(uint32_t bytes, DWORD timeoutMs)
    {
        constexpr uint32_t kSpinCount = 1024;
        for (uint32_t i = 0; i < kSpinCount; ++i)
        {
            if (capacity_ - UsedBytes() >= bytes)
            {
                return true;
            }
            YieldProcessor();
        }

        if (timeoutMs == 0)
        {
            return false;
        }

        const DWORD start = GetTickCount();
        while (true)
        {
            if (capacity_ - UsedBytes() >= bytes)
            {
                return true;
            }

            if (timeoutMs != INFINITE)
            {
                const DWORD elapsed = GetTickCount() - start;
                if (elapsed >= timeoutMs)
                {
                    return false;
                }

                WaitForSingleObject(spaceEvent_, std::min<DWORD>(timeoutMs - elapsed, 1));
            }
            else
            {
                WaitForSingleObject(spaceEvent_, INFINITE);
            }
        }
    }

    void CopyToRing(uint64_t cursor, const void* src, uint32_t bytes)
    {
        if (bytes == 0)
        {
            return;
        }

        const uint8_t* in = reinterpret_cast<const uint8_t*>(src);
        const uint32_t offset = static_cast<uint32_t>(cursor % capacity_);
        const uint32_t first = std::min<uint32_t>(bytes, capacity_ - offset);
        std::memcpy(data_ + offset, in, first);
        if (bytes > first)
        {
            std::memcpy(data_, in + first, bytes - first);
        }
    }

    void CopyFromRing(uint64_t cursor, void* dst, uint32_t bytes) const
    {
        if (bytes == 0)
        {
            return;
        }

        uint8_t* out = reinterpret_cast<uint8_t*>(dst);
        const uint32_t offset = static_cast<uint32_t>(cursor % capacity_);
        const uint32_t first = std::min<uint32_t>(bytes, capacity_ - offset);
        std::memcpy(out, data_ + offset, first);
        if (bytes > first)
        {
            std::memcpy(out + first, data_, bytes - first);
        }
    }

    std::wstring channelName_;
    std::wstring mapName_;
    std::wstring dataEventName_;
    std::wstring spaceEventName_;

    HANDLE mapping_ = nullptr;
    HANDLE dataEvent_ = nullptr;
    HANDLE spaceEvent_ = nullptr;
    void* view_ = nullptr;
    SharedRingHeader* header_ = nullptr;
    uint8_t* data_ = nullptr;
    uint32_t capacity_ = 0;
    SIZE_T mappedBytes_ = 0;
};
}

#else
#include <cstdint>

namespace Duskhaven::IPC
{
inline void StoreU32LE(uint8_t* out, uint32_t value)
{
    out[0] = static_cast<uint8_t>(value & 0xFF);
    out[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    out[2] = static_cast<uint8_t>((value >> 16) & 0xFF);
    out[3] = static_cast<uint8_t>((value >> 24) & 0xFF);
}

inline uint32_t LoadU32LE(const uint8_t* in)
{
    return static_cast<uint32_t>(in[0]) |
           (static_cast<uint32_t>(in[1]) << 8) |
           (static_cast<uint32_t>(in[2]) << 16) |
           (static_cast<uint32_t>(in[3]) << 24);
}

struct SharedRingSegment
{
    const void* data = nullptr;
    uint32_t bytes = 0;
};
}
#endif
