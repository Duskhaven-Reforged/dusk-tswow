#pragma once

#include <cstdint>
#include <functional>

namespace ClientData
{
    struct SpellRow;
}

struct SpellEffectCacheRow;

namespace SpellCacheStreaming
{
    void Apply();
    bool HasSpell(uint32_t spellId);
    bool HasSpell(uint32_t spellId, uint32_t spellDataHash);
    bool IsRequestPending(uint32_t spellId);
    uint32_t ForEachSpellEffect(uint32_t spellId, const std::function<bool(const SpellEffectCacheRow&)>& visitor);
    uint32_t GetSpellEffectCount(uint32_t spellId);
    bool TryGetSpellEffect(uint32_t spellId, uint32_t effectIndex, const SpellEffectCacheRow*& out);
    bool HasSpellEffect(uint32_t spellId, uint32_t effectId);
    bool HasSpellAura(uint32_t spellId, uint32_t auraId);
    bool HasAnySpellAura(uint32_t spellId, const uint32_t* auraIds, uint32_t auraIdCount);
    bool TryFindSpellEffectByEffect(uint32_t spellId, uint32_t effectId, const SpellEffectCacheRow*& out);
    bool TryFindSpellEffectByAura(uint32_t spellId, uint32_t auraId, const SpellEffectCacheRow*& out);
    bool TryBuildSpellRow(uint32_t spellId, ClientData::SpellRow& out);
    bool TryGetSpellRow(uint32_t spellId, ClientData::SpellRow& out, bool requestOnMiss = true);
    void RequestSpell(uint32_t spellId, uint32_t spellDataHash = 0);
}
