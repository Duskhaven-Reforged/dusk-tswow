#include <Spells/SpellCache/SpellCacheAuthority.h>

#include <CDBCMgr/CDBCMgr.h>
#include <CDBCMgr/CDBCDefs/Spell.h>
#include <Logger.h>

#include <algorithm>
#include <deque>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{
    constexpr uint32_t SPELL_CACHE_STREAM_VERSION_AUTHORITY = 4;
    constexpr uint32_t SPELL_CACHE_MAX_REASONABLE_SPELL_ID_AUTHORITY = 1000000;

    struct SpellPromise
    {
        SpellCacheAuthority::SpellState state = SpellCacheAuthority::SpellState::Missing;
        uint32_t requestedHash = 0;
        uint32_t readyHash = 0;
        uint32_t requestGeneration = 0;
        std::vector<SpellCacheAuthority::Waiter> waiters;
    };

    std::unordered_map<uint32_t, SpellPromise> Promises;
    std::deque<uint32_t> QueuedRequests;
    std::unordered_set<uint32_t> QueuedRequestIds;
    std::unordered_map<uint32_t, uint32_t> QueuedRequestHashes;
    std::deque<std::pair<uint32_t, SpellCacheAuthority::Waiter>> ReadyWaiters;
    std::deque<uint32_t> RecentMisses;
    uint32_t SpellRevision = 1;
    uint32_t SpellbookRevision = 1;
    uint32_t ActionbarRevision = 1;
    uint32_t TooltipRevision = 1;

    bool IsReasonableSpellId(uint32_t spellId)
    {
        return spellId > 0 && spellId <= SPELL_CACHE_MAX_REASONABLE_SPELL_ID_AUTHORITY;
    }

    uint32_t BumpRevision(uint32_t value)
    {
        return value == 0xFFFFFFFFu ? 1u : value + 1u;
    }

    void BumpForWaiter(SpellCacheAuthority::WaiterKind kind)
    {
        switch (kind)
        {
            case SpellCacheAuthority::WaiterKind::Spellbook:
                SpellbookRevision = BumpRevision(SpellbookRevision);
                break;
            case SpellCacheAuthority::WaiterKind::ActionbarSlot:
            case SpellCacheAuthority::WaiterKind::MacroIcon:
            case SpellCacheAuthority::WaiterKind::Cooldown:
            case SpellCacheAuthority::WaiterKind::Range:
                ActionbarRevision = BumpRevision(ActionbarRevision);
                break;
            case SpellCacheAuthority::WaiterKind::Tooltip:
                TooltipRevision = BumpRevision(TooltipRevision);
                break;
            default:
                break;
        }
    }

    SpellCacheRow* TryGetReadyRow(uint32_t spellId, uint32_t spellDataHash)
    {
        if (!IsReasonableSpellId(spellId))
            return nullptr;

        SpellCacheRow* spell = GlobalCDBCMap.getRow<SpellCacheRow>("Spell", static_cast<int>(spellId));
        if (!spell || spell->cacheVersion != SPELL_CACHE_STREAM_VERSION_AUTHORITY)
            return nullptr;

        if (spellDataHash && spell->spellDataHash != spellDataHash)
            return nullptr;

        if (!spellDataHash && spell->spellDataHash == 0)
            return nullptr;

        return spell;
    }

    void RecordMiss(uint32_t spellId)
    {
        RecentMisses.push_back(spellId);
        while (RecentMisses.size() > 64)
            RecentMisses.pop_front();
    }

    char const* StateName(SpellCacheAuthority::SpellState state)
    {
        switch (state)
        {
            case SpellCacheAuthority::SpellState::Missing: return "missing";
            case SpellCacheAuthority::SpellState::Requested: return "requested";
            case SpellCacheAuthority::SpellState::Ready: return "ready";
            case SpellCacheAuthority::SpellState::Failed: return "failed";
            default: return "unknown";
        }
    }

    bool SameWaiter(SpellCacheAuthority::Waiter const& left, SpellCacheAuthority::Waiter const& right)
    {
        return left.kind == right.kind
            && left.target == right.target
            && left.aux == right.aux;
    }

    void AttachWaiterUnique(SpellPromise& promise, SpellCacheAuthority::Waiter waiter)
    {
        auto const existing = std::find_if(
            promise.waiters.begin(),
            promise.waiters.end(),
            [waiter](SpellCacheAuthority::Waiter const& queued)
            {
                return SameWaiter(queued, waiter);
            });

        if (existing == promise.waiters.end())
            promise.waiters.push_back(waiter);
    }
}

namespace SpellCacheAuthority
{
    bool HasReadySpell(uint32_t spellId, uint32_t spellDataHash)
    {
        if (TryGetReadyRow(spellId, spellDataHash))
        {
            auto& promise = Promises[spellId];
            promise.state = SpellState::Ready;
            promise.readyHash = spellDataHash ? spellDataHash : TryGetReadyRow(spellId, 0)->spellDataHash;
            return true;
        }

        return false;
    }

    bool IsRequestPending(uint32_t spellId)
    {
        auto const it = Promises.find(spellId);
        return it != Promises.end()
            && (it->second.state == SpellState::Requested || QueuedRequestIds.find(spellId) != QueuedRequestIds.end());
    }

    ResolveResult Resolve(uint32_t spellId, RequestPolicy policy, Waiter waiter)
    {
        ResolveResult result{};
        result.spell = TryGetReadyRow(spellId, 0);
        if (result.spell)
        {
            auto& promise = Promises[spellId];
            promise.state = SpellState::Ready;
            promise.readyHash = result.spell->spellDataHash;
            result.state = SpellState::Ready;
            return result;
        }

        if (!IsReasonableSpellId(spellId))
        {
            result.state = SpellState::Failed;
            return result;
        }

        auto& promise = Promises[spellId];
        if (waiter.kind != WaiterKind::None)
            AttachWaiterUnique(promise, waiter);

        if (promise.state == SpellState::Failed)
        {
            result.state = SpellState::Failed;
            return result;
        }

        RecordMiss(spellId);
        if (policy == RequestPolicy::RequestOnMiss)
            result.requested = QueueRequest(spellId, 0);

        result.state = promise.state == SpellState::Requested ? SpellState::Requested : SpellState::Missing;
        return result;
    }

    void AttachWaiter(uint32_t spellId, Waiter waiter)
    {
        if (!IsReasonableSpellId(spellId) || waiter.kind == WaiterKind::None)
            return;

        AttachWaiterUnique(Promises[spellId], waiter);
    }

    bool QueueRequest(uint32_t spellId, uint32_t spellDataHash)
    {
        if (!IsReasonableSpellId(spellId) || HasReadySpell(spellId, spellDataHash))
            return false;

        auto& promise = Promises[spellId];
        if (promise.state == SpellState::Failed)
            promise.state = SpellState::Missing;

        bool queued = QueuedRequestIds.insert(spellId).second;
        if (queued)
            QueuedRequests.push_back(spellId);

        if (spellDataHash)
        {
            promise.requestedHash = spellDataHash;
            QueuedRequestHashes[spellId] = spellDataHash;
        }

        promise.state = SpellState::Requested;
        promise.requestGeneration += queued ? 1u : 0u;
        return queued;
    }

    void MarkRequestSent(uint32_t spellId)
    {
        if (!IsReasonableSpellId(spellId))
            return;

        QueuedRequestIds.erase(spellId);
        QueuedRequestHashes.erase(spellId);
        Promises[spellId].state = SpellState::Requested;
    }

    void MarkReady(uint32_t spellId, uint32_t spellDataHash)
    {
        if (!IsReasonableSpellId(spellId))
            return;

        auto& promise = Promises[spellId];
        promise.state = SpellState::Ready;
        promise.readyHash = spellDataHash;
        promise.requestedHash = 0;

        SpellRevision = BumpRevision(SpellRevision);
        for (Waiter const& waiter : promise.waiters)
        {
            BumpForWaiter(waiter.kind);
            ReadyWaiters.push_back({ spellId, waiter });
        }
        promise.waiters.clear();
    }

    void MarkFailed(uint32_t spellId)
    {
        if (!IsReasonableSpellId(spellId))
            return;

        auto& promise = Promises[spellId];
        promise.state = SpellState::Failed;
        promise.requestedHash = 0;
        promise.waiters.clear();
    }

    void MarkNativeReady(uint32_t spellId)
    {
        if (!IsReasonableSpellId(spellId))
            return;

        auto& promise = Promises[spellId];
        if (promise.state != SpellState::Ready)
            promise.state = SpellState::Ready;
    }

    uint32_t PumpReadyWaiters(uint32_t maxWaiters, const std::function<void(uint32_t, const Waiter&)>& visitor)
    {
        uint32_t pumped = 0;
        while (pumped < maxWaiters && !ReadyWaiters.empty())
        {
            auto item = ReadyWaiters.front();
            ReadyWaiters.pop_front();
            if (visitor)
                visitor(item.first, item.second);
            ++pumped;
        }

        return pumped;
    }

    Stats GetStats()
    {
        Stats stats{};
        stats.trackedSpells = static_cast<uint32_t>(Promises.size());
        stats.queuedRequests = static_cast<uint32_t>(QueuedRequestIds.size());
        stats.queuedWaiters = static_cast<uint32_t>(ReadyWaiters.size());
        stats.recentMisses = static_cast<uint32_t>(RecentMisses.size());
        stats.spellRevision = SpellRevision;
        stats.spellbookRevision = SpellbookRevision;
        stats.actionbarRevision = ActionbarRevision;
        stats.tooltipRevision = TooltipRevision;

        for (auto const& entry : Promises)
        {
            switch (entry.second.state)
            {
                case SpellState::Requested:
                    ++stats.pendingRequests;
                    break;
                case SpellState::Ready:
                    ++stats.readySpells;
                    break;
                case SpellState::Failed:
                    ++stats.failedSpells;
                    break;
                default:
                    break;
            }
        }

        return stats;
    }

    std::string BuildDebugSummary(uint32_t spellId)
    {
        std::ostringstream out;
        auto const it = Promises.find(spellId);
        out << "spell=" << spellId;
        if (it == Promises.end())
        {
            out << " state=untracked";
            return out.str();
        }

        SpellPromise const& promise = it->second;
        out << " state=" << StateName(promise.state)
            << " requestedHash=" << promise.requestedHash
            << " readyHash=" << promise.readyHash
            << " waiters=" << static_cast<uint32_t>(promise.waiters.size())
            << " queued=" << (QueuedRequestIds.find(spellId) != QueuedRequestIds.end() ? 1 : 0)
            << " hasRow=" << (TryGetReadyRow(spellId, 0) ? 1 : 0);
        return out.str();
    }
}
