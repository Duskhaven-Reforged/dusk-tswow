#pragma once

#include <ClientData/Spell.h>

#include <cstdint>
#include <functional>

struct SpellEffectCacheRow;

namespace SpellDbProvider
{
    enum class RequestPolicy : uint8_t
    {
        NoRequest,
        RequestOnMiss
    };

    bool HasReady(uint32_t spellId, uint32_t spellDataHash = 0);
    bool TryGetRowCopy(uint32_t spellId, ClientData::SpellRow& out);
    ClientData::SpellRow* TryGetRowPtr(uint32_t spellId);
    bool RequestOrGet(uint32_t spellId, ClientData::SpellRow& out, RequestPolicy policy);

    uint32_t ForEachEffect(uint32_t spellId, const std::function<bool(const SpellEffectCacheRow&)>& visitor);
    uint32_t GetEffectCount(uint32_t spellId);
    bool TryGetEffect(uint32_t spellId, uint32_t effectIndex, const SpellEffectCacheRow*& out);

    void Invalidate(uint32_t spellId);
}
