#pragma once
#include <Tooltip/TooltipDefines.h>

namespace CFormula {
    CLIENT_FUNCTION(GetVariableValue, 0x5782D0, __thiscall, int, (void*, uint32_t, uint32_t, SpellRow*, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t))
}

namespace CGTooltip {
    CLIENT_FUNCTION(AddLine, 0x61FEC0, __thiscall, void, (void*, char*, char*, void*, void*, uint32_t))
    CLIENT_FUNCTION(GetDurationString, 0x61A9E0, __cdecl, void, (char*, uint32_t, uint64_t, char*, uint32_t, uint32_t, uint32_t))
}

class TooltipExtensions {
private:
    static void Apply();
    static void SetNewVariablePointers();
    static void SetPowerCostTooltip(char* dest, SpellRow* spell, uint32_t powerCost, uint32_t powerCostPerSec, char* powerString, PowerDisplayRow* powerDisplayRow);
    static void SetSpellCooldownTooltip(char* dest, SpellRow* spell, uintptr_t* a7, uint32_t a6, uint32_t a8, char* src, void* _this, uint32_t powerCost);
    static void SetSpellRemainingCooldownTooltip(char* dest, SpellRow* spell, void* _this, uint32_t currentCooldown);
    static void SpellTooltipVariableExtension();
    static void SpellTooltipPowerCostExtension();
    static void SpellTooltipCooldownExtension();
    static void SpellTooltipRemainingCooldownExtension();

    static int __fastcall GetVariableValueEx(void* _this, uint32_t edx, uint32_t spellVariable, uint32_t a3, SpellRow* spell, uint32_t a5, uint32_t a6, uint32_t a7, uint32_t a8, uint32_t a9);

    // Full replacement for CGTooltip__SetSpell implemented in the DLL.
    // Matches the original __thiscall layout via a __fastcall wrapper.
    static int __fastcall SetSpellTooltipHook(
        void* thisPtr,
        void* edx,
        int spellId,
        int a3,
        int a4,
        int a5,
        int a6,
        int a7,
        int a8,
        uint32_t* a9,
        int a10,
        int a11,
        int a12,
        int a13,
        int a14,
        int a15,
        int a16);

    // Internal helper that operates on strongly-typed data.
    static int SetSpellTooltipImpl(
        void* tooltip,
        int spellId,
        int a3,
        int a4,
        int a5,
        int a6,
        int a7,
        int a8,
        uint32_t* a9,
        int a10,
        int a11,
        int a12,
        int a13,
        int a14,
        int a15,
        int a16);

    static void SpellTooltipSetSpellExtension();
    friend class ClientExtensions;
};
