#include "ClientLua.h"
#include "ClientDetours.h"
#include "SharedDefines.h"
#include "Spells/Spells.h"
#include "Misc/MiscFixes.h"

enum CVarFlags : uint32_t {
    CVarFlags_ReadOnly = 0x4,
    CVarFlags_CheckTaint = 0x8,
    CVarFlags_HideFromUser = 0x40,
    CVarFlags_ReadOnlyForUser = 0x100,

};

inline CVar* GetCVar(const char* name) { return ((decltype(&GetCVar))0x00767460)(name); }
inline CVar* FindCVar(const char* name) { return ((decltype(&FindCVar))0x00767440)(name); }

CLIENT_DETOUR(Initialize, 0x00768340, __cdecl, void, (char* file))
{
    Initialize(file);

    Spells::g_spell_min_clip_distance_percentage_cvar = CVar_C::Register(
        "spellMinClipDistancePercentage", "Sets the minimum distance the clipping needs to be to activate", 1, "0.0",
        Spells::SpellMinClipDistancePercentage_CVarCallback, 5, 0, 0, 0);
    MiscFixes::g_models_alpha_cvar = CVar_C::Register("modelFadeAlpha", "Alpha value for objects in camera LOS", 1, "0.6", MiscFixes::ModelsAlpha_CVarCallback, 5, 0, 0, 0);
}
