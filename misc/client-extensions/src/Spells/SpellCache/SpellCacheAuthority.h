#pragma once

#include <cstdint>
#include <functional>
#include <string>

struct SpellCacheRow;

namespace SpellCacheAuthority
{
    enum class SpellState : uint8_t
    {
        Missing,
        Requested,
        Ready,
        Failed
    };

    enum class RequestPolicy : uint8_t
    {
        NoRequest,
        RequestOnMiss
    };

    enum class WaiterKind : uint8_t
    {
        None,
        Spellbook,
        ActionbarSlot,
        Tooltip,
        LuaApi,
        CastRetry,
        MacroIcon,
        Cooldown,
        Range,
        Preload
    };

    struct Waiter
    {
        WaiterKind kind = WaiterKind::None;
        uint32_t target = 0;
        uint32_t aux = 0;
    };

    struct ResolveResult
    {
        SpellState state = SpellState::Missing;
        SpellCacheRow* spell = nullptr;
        bool requested = false;
    };

    struct Stats
    {
        uint32_t trackedSpells = 0;
        uint32_t queuedRequests = 0;
        uint32_t pendingRequests = 0;
        uint32_t readySpells = 0;
        uint32_t failedSpells = 0;
        uint32_t queuedWaiters = 0;
        uint32_t recentMisses = 0;
        uint32_t spellRevision = 1;
        uint32_t spellbookRevision = 1;
        uint32_t actionbarRevision = 1;
        uint32_t tooltipRevision = 1;
    };

    bool HasReadySpell(uint32_t spellId, uint32_t spellDataHash = 0);
    bool IsRequestPending(uint32_t spellId);
    ResolveResult Resolve(uint32_t spellId, RequestPolicy policy, Waiter waiter = {});
    void AttachWaiter(uint32_t spellId, Waiter waiter);
    bool QueueRequest(uint32_t spellId, uint32_t spellDataHash);
    void MarkRequestSent(uint32_t spellId);
    void MarkReady(uint32_t spellId, uint32_t spellDataHash);
    void MarkFailed(uint32_t spellId);
    void MarkNativeReady(uint32_t spellId);
    uint32_t PumpReadyWaiters(uint32_t maxWaiters, const std::function<void(uint32_t, const Waiter&)>& visitor);
    Stats GetStats();
    std::string BuildDebugSummary(uint32_t spellId);
}
