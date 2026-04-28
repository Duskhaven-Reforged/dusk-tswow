#include <Character/CharacterFixes.h>

#include <ClientDetours.h>
#include <ClientData/SharedDefines.h>

#include <Windows.h>
#include <vector>
#include <ClientData/Spell.h>
#include <ClientData/ObjectFields.h>

using namespace ClientData;

namespace
{
    constexpr uint32_t SPELL_AURA_MONK_UNARMED = 355;
    constexpr uintptr_t ANIMATION_DATA_DB = 0x00AD30C8;
    constexpr uint32_t SHEATH_STATE_UNARMED = 0;
    constexpr uint32_t SHEATH_STATE_MELEE = 1;
    constexpr uintptr_t UNIT_PREVIOUS_SHEATH_STATE_OFFSET = 0xB58;
    constexpr uintptr_t UNIT_CURRENT_SHEATH_STATE_OFFSET = 0xB5C;
    constexpr int VISIBLE_ITEM_MAINHAND = 0;
    constexpr int VISIBLE_ITEM_OFFHAND = 1;
    constexpr int MOVE_HAND_ITEM_TO_HAND = 0;
    constexpr int HAND_ITEM_BONE_SEQUENCE_OFFHAND = 2;
    constexpr int HAND_ITEM_BONE_SEQUENCE_MAINHAND = 3;
    constexpr int HAND_ITEM_SEQUENCE_READY = 89;
    constexpr int HAND_ITEM_SEQUENCE_READY_SPECIAL = 90;
    constexpr uintptr_t UNIT_HAND_ITEM_FLAGS_OFFSET = 0xA38;
    constexpr uint32_t UNIT_HAND_ITEM_MAINHAND_ACTIVE = 0x100000;
    constexpr uint32_t UNIT_HAND_ITEM_OFFHAND_ACTIVE = 0x200000;
    constexpr int ANIMATION_ATTACK_UNARMED = 16;
    constexpr int ANIMATION_ATTACK_1H = 17;
    constexpr int ANIMATION_ATTACK_2H = 18;
    constexpr int ANIMATION_ATTACK_2H_LOOSE = 19;
    constexpr int ANIMATION_SPECIAL_1H = 57;
    constexpr int ANIMATION_SPECIAL_2H = 58;
    constexpr int ANIMATION_ATTACK_1H_PIERCE = 85;
    constexpr int ANIMATION_ATTACK_2H_LOOSE_PIERCE = 86;
    constexpr int ANIMATION_ATTACK_OFFHAND = 87;
    constexpr int ANIMATION_ATTACK_OFFHAND_PIERCE = 88;
    constexpr int ANIMATION_ATTACK_UNARMED_OFFHAND = 117;
    constexpr int ANIMATION_SPECIAL_UNARMED = 118;

    struct AnimationDataRow
    {
        uint32_t id;
        const char* name;
        uint32_t weaponFlags;
        uint32_t bodyFlags;
        uint32_t flags;
        uint32_t fallback;
        uint32_t behaviorId;
        uint32_t behaviorTier;
    };

    CLIENT_FUNCTION(CGUnit_C__IsSpellKnown_MonkUnarmed, 0x7260E0, __thiscall, bool, (CGUnit*, uint32_t))

    bool SpellHasAuraType(SpellRow const& row, uint32_t auraType)
    {
        for (size_t effectIndex = 0; effectIndex < 3; ++effectIndex)
            if (row.m_effectAura[effectIndex] == auraType)
                return true;

        return false;
    }

    bool UnitHasAuraType(CGUnit* unit, uint32_t auraType)
    {
        if (!unit)
            return false;

        WoWClientDB* spellDB = reinterpret_cast<WoWClientDB*>(0x00AD49D0);
        if (!spellDB || !spellDB->isLoaded)
            return false;

        SpellRow row{};
        const int auraCount = CGUnit_C::GetAuraCount(unit);

        for (int auraIndex = 0; auraIndex < auraCount; ++auraIndex)
        {
            AuraData* aura = CGUnit_C::GetAura(unit, auraIndex);
            if (!aura)
                continue;

            if (!ClientDB::GetLocalizedRow(spellDB, aura->spellId, &row))
                continue;

            if (SpellHasAuraType(row, auraType))
                return true;
        }

        return false;
    }

    bool UnitKnowsAuraTypeSpell(CGUnit* unit, uint32_t auraType)
    {
        static bool scanned = false;
        static std::vector<uint32_t> spellIds;

        if (!unit)
            return false;

        if (!scanned)
        {
            WoWClientDB* spellDB = reinterpret_cast<WoWClientDB*>(0x00AD49D0);
            if (!spellDB || !spellDB->isLoaded)
                return false;

            SpellRow row{};

            for (int spellId = spellDB->minIndex; spellId <= spellDB->maxIndex; ++spellId)
                if (ClientDB::GetLocalizedRow(spellDB, spellId, &row) && SpellHasAuraType(row, auraType))
                    spellIds.push_back(row.m_ID);

            scanned = true;
        }

        for (uint32_t spellId : spellIds)
            if (CGUnit_C__IsSpellKnown_MonkUnarmed(unit, spellId))
                return true;

        return false;
    }

    bool ShouldPreventMeleeUnsheath(CGUnit* unit)
    {
        return UnitHasAuraType(unit, SPELL_AURA_MONK_UNARMED)
            || UnitKnowsAuraTypeSpell(unit, SPELL_AURA_MONK_UNARMED);
    }

    bool IsHandItemAttachSequence(int boneSeqSlot, int sequence)
    {
        return (boneSeqSlot == HAND_ITEM_BONE_SEQUENCE_OFFHAND || boneSeqSlot == HAND_ITEM_BONE_SEQUENCE_MAINHAND)
            && (sequence == HAND_ITEM_SEQUENCE_READY || sequence == HAND_ITEM_SEQUENCE_READY_SPECIAL);
    }

    bool IsMonkUnarmedAttackBehavior(uint32_t behaviorId)
    {
        switch (behaviorId)
        {
            case ANIMATION_ATTACK_1H:
            case ANIMATION_ATTACK_2H:
            case ANIMATION_ATTACK_2H_LOOSE:
            case ANIMATION_SPECIAL_1H:
            case ANIMATION_SPECIAL_2H:
            case ANIMATION_ATTACK_1H_PIERCE:
            case ANIMATION_ATTACK_2H_LOOSE_PIERCE:
            case ANIMATION_ATTACK_OFFHAND:
            case ANIMATION_ATTACK_OFFHAND_PIERCE:
            case ANIMATION_ATTACK_UNARMED_OFFHAND:
            case ANIMATION_SPECIAL_UNARMED:
                return true;
            default:
                return false;
        }
    }

    bool IsMonkUnarmedMeleeAttackAnimation(int animationId)
    {
        if (animationId < 0 || animationId == ANIMATION_ATTACK_UNARMED)
            return false;

        if (IsMonkUnarmedAttackBehavior(static_cast<uint32_t>(animationId)))
            return true;

        auto* row = reinterpret_cast<AnimationDataRow*>(ClientDB::GetRow(reinterpret_cast<void*>(ANIMATION_DATA_DB), animationId));
        return row && IsMonkUnarmedAttackBehavior(row->behaviorId);
    }

    uint32_t& UnitSheatheState(uintptr_t unit, uintptr_t offset)
    {
        return *reinterpret_cast<uint32_t*>(unit + offset);
    }

    uint32_t& UnitHandItemFlags(uintptr_t unit)
    {
        return *reinterpret_cast<uint32_t*>(unit + UNIT_HAND_ITEM_FLAGS_OFFSET);
    }

    void ClearHandItemActiveFlag(uintptr_t unit, int slot)
    {
        if (slot == VISIBLE_ITEM_MAINHAND || slot == HAND_ITEM_BONE_SEQUENCE_MAINHAND)
            UnitHandItemFlags(unit) &= ~UNIT_HAND_ITEM_MAINHAND_ACTIVE;
        else if (slot == VISIBLE_ITEM_OFFHAND || slot == HAND_ITEM_BONE_SEQUENCE_OFFHAND)
            UnitHandItemFlags(unit) &= ~UNIT_HAND_ITEM_OFFHAND_ACTIVE;
    }

    void ForceUnitUnarmedSheatheState(uintptr_t unit)
    {
        UnitSheatheState(unit, UNIT_PREVIOUS_SHEATH_STATE_OFFSET) = SHEATH_STATE_UNARMED;
        UnitSheatheState(unit, UNIT_CURRENT_SHEATH_STATE_OFFSET) = SHEATH_STATE_UNARMED;
        UnitHandItemFlags(unit) &= ~(UNIT_HAND_ITEM_MAINHAND_ACTIVE | UNIT_HAND_ITEM_OFFHAND_ACTIVE);
    }

    CLIENT_DETOUR_THISCALL(CGUnit_C__SetSheatheState_MonkUnarmed, 0x736D30, uintptr_t, (uintptr_t sheatheState, int animate, int force))
    {
        if (sheatheState == SHEATH_STATE_MELEE && ShouldPreventMeleeUnsheath(reinterpret_cast<CGUnit*>(self)))
        {
            ForceUnitUnarmedSheatheState(reinterpret_cast<uintptr_t>(self));
            return 0;
        }

        return CGUnit_C__SetSheatheState_MonkUnarmed(self, sheatheState, animate, force);
    }

    CLIENT_DETOUR_THISCALL(CGUnit_C__AnimationData_MonkUnarmed, 0x7385C0, void, (int animationId, char flags))
    {
        if (IsMonkUnarmedMeleeAttackAnimation(animationId) && ShouldPreventMeleeUnsheath(reinterpret_cast<CGUnit*>(self)))
            animationId = ANIMATION_ATTACK_UNARMED;

        CGUnit_C__AnimationData_MonkUnarmed(self, animationId, flags);
    }

    CLIENT_DETOUR_THISCALL(CGUnit_C__HandleModelSequenceCallback_MonkUnarmed, 0x73BBD0, void, (void* model, int boneSeqSlot, int animationId, int a5, int a6))
    {
        if (IsMonkUnarmedMeleeAttackAnimation(animationId) && ShouldPreventMeleeUnsheath(reinterpret_cast<CGUnit*>(self)))
            animationId = ANIMATION_ATTACK_UNARMED;

        CGUnit_C__HandleModelSequenceCallback_MonkUnarmed(self, model, boneSeqSlot, animationId, a5, a6);
    }

    CLIENT_DETOUR_THISCALL(CGUnit_C__ResolveModelAnimationId_MonkUnarmed, 0x7176F0, int, (int animationId, void* model))
    {
        if (IsMonkUnarmedMeleeAttackAnimation(animationId) && ShouldPreventMeleeUnsheath(reinterpret_cast<CGUnit*>(self)))
            animationId = ANIMATION_ATTACK_UNARMED;

        return CGUnit_C__ResolveModelAnimationId_MonkUnarmed(self, animationId, model);
    }

    CLIENT_DETOUR_THISCALL(CGUnit_C__ApplyModelAnimationSequence_MonkUnarmed, 0x737BD0, void, (void* model, int boneSeqSlot, unsigned int sequence))
    {
        if (ShouldPreventMeleeUnsheath(reinterpret_cast<CGUnit*>(self)))
        {
            if (IsHandItemAttachSequence(boneSeqSlot, sequence))
            {
                ClearHandItemActiveFlag(reinterpret_cast<uintptr_t>(self), boneSeqSlot);
                return;
            }

            if (IsMonkUnarmedMeleeAttackAnimation(sequence))
                sequence = ANIMATION_ATTACK_UNARMED;
        }

        CGUnit_C__ApplyModelAnimationSequence_MonkUnarmed(self, model, boneSeqSlot, sequence);
    }

    CLIENT_DETOUR_THISCALL(CGUnit_C__UpdateSheatheForAnimation_MonkUnarmed, 0x738180, void, (char a2))
    {
        CGUnit_C__UpdateSheatheForAnimation_MonkUnarmed(self, a2);

        if (ShouldPreventMeleeUnsheath(reinterpret_cast<CGUnit*>(self)))
            ForceUnitUnarmedSheatheState(reinterpret_cast<uintptr_t>(self));
    }

    CLIENT_DETOUR_THISCALL(CGUnit_C__UpdateHandItemVisual_MonkUnarmed, 0x72DBC0, void, (int slot))
    {
        if ((slot == VISIBLE_ITEM_MAINHAND || slot == VISIBLE_ITEM_OFFHAND)
            && ShouldPreventMeleeUnsheath(reinterpret_cast<CGUnit*>(self)))
        {
            const uintptr_t unitAddress = reinterpret_cast<uintptr_t>(self);
            uint32_t& previousSheatheState = UnitSheatheState(unitAddress, UNIT_PREVIOUS_SHEATH_STATE_OFFSET);
            uint32_t& currentSheatheState = UnitSheatheState(unitAddress, UNIT_CURRENT_SHEATH_STATE_OFFSET);
            const uint32_t savedPreviousSheatheState = previousSheatheState;
            const uint32_t savedCurrentSheatheState = currentSheatheState;

            previousSheatheState = SHEATH_STATE_UNARMED;
            currentSheatheState = SHEATH_STATE_UNARMED;

            CGUnit_C__UpdateHandItemVisual_MonkUnarmed(self, slot);

            previousSheatheState = savedPreviousSheatheState;
            currentSheatheState = savedCurrentSheatheState;
            return;
        }

        CGUnit_C__UpdateHandItemVisual_MonkUnarmed(self, slot);
    }

    CLIENT_DETOUR_THISCALL(CGUnit_C__HandleHandItemAnimEvent_MonkUnarmed, 0x732500, void, (int position, int slot, int slotFlag))
    {
        if ((slot == VISIBLE_ITEM_MAINHAND || slot == VISIBLE_ITEM_OFFHAND)
            && ShouldPreventMeleeUnsheath(reinterpret_cast<CGUnit*>(self)))
        {
            ClearHandItemActiveFlag(reinterpret_cast<uintptr_t>(self), slot);
            ForceUnitUnarmedSheatheState(reinterpret_cast<uintptr_t>(self));
            return;
        }

        CGUnit_C__HandleHandItemAnimEvent_MonkUnarmed(self, position, slot, slotFlag);
    }

    CLIENT_DETOUR_THISCALL(CGUnit_C__MoveHandItemAttachment_MonkUnarmed, 0x7310A0, int, (int slot, int moveToSheath))
    {
        if ((slot == VISIBLE_ITEM_MAINHAND || slot == VISIBLE_ITEM_OFFHAND)
            && moveToSheath == MOVE_HAND_ITEM_TO_HAND
            && ShouldPreventMeleeUnsheath(reinterpret_cast<CGUnit*>(self)))
        {
            ClearHandItemActiveFlag(reinterpret_cast<uintptr_t>(self), slot);
            return 0;
        }

        return CGUnit_C__MoveHandItemAttachment_MonkUnarmed(self, slot, moveToSheath);
    }

    CLIENT_DETOUR_THISCALL_NOARGS(CGUnit_C__AttachMainHandItem_MonkUnarmed, 0x7367B0, int)
    {
        if (ShouldPreventMeleeUnsheath(reinterpret_cast<CGUnit*>(self)))
        {
            ClearHandItemActiveFlag(reinterpret_cast<uintptr_t>(self), HAND_ITEM_BONE_SEQUENCE_MAINHAND);
            return 1;
        }

        return CGUnit_C__AttachMainHandItem_MonkUnarmed(self);
    }

    CLIENT_DETOUR_THISCALL_NOARGS(CGUnit_C__AttachOffHandItem_MonkUnarmed, 0x7368B0, int)
    {
        if (ShouldPreventMeleeUnsheath(reinterpret_cast<CGUnit*>(self)))
        {
            ClearHandItemActiveFlag(reinterpret_cast<uintptr_t>(self), HAND_ITEM_BONE_SEQUENCE_OFFHAND);
            return 1;
        }

        return CGUnit_C__AttachOffHandItem_MonkUnarmed(self);
    }

    CLIENT_DETOUR_THISCALL(CGUnit_C__SetHandItemBoneSequence_MonkUnarmed, 0x735820, void, (uintptr_t model, int boneSeqSlot, int sequence, float sequenceTime, int a6, float speed, int a8, int a9, int a10))
    {
        if (ShouldPreventMeleeUnsheath(reinterpret_cast<CGUnit*>(self)))
        {
            if (IsHandItemAttachSequence(boneSeqSlot, sequence))
            {
                ClearHandItemActiveFlag(reinterpret_cast<uintptr_t>(self), boneSeqSlot);
                return;
            }

            if (IsMonkUnarmedMeleeAttackAnimation(sequence))
                sequence = ANIMATION_ATTACK_UNARMED;
        }

        CGUnit_C__SetHandItemBoneSequence_MonkUnarmed(self, model, boneSeqSlot, sequence, sequenceTime, a6, speed, a8, a9, a10);
    }
}

void CharacterFixes::CharacterCreationFixes() {
    DWORD flOldProtect = 0;
    // addresses pointing to, uh, some sort of shared memory storage
    // needs to be bigger to not cause crashes with our dbcs so I assigned to it 512 bytes (original table is 176 bytes iirc? cba to look in IDA), should be enough
    std::vector<uint32_t> patchedAddresses = { 0x4E157D, 0x4E16A3, 0x4E15B5, 0x4E20EE, 0x4E222A, 0x4E2127, 0x4E1E94, 0x4E1C3A };

    for (uint8_t i = 0; i < patchedAddresses.size(); i++)
        Util::OverwriteUInt32AtAddress(patchedAddresses[i], reinterpret_cast<uint32_t>(&memoryTable));

    // Name table
    // 0x4CDA43 - address of table where pointers to race name strings are stored
    SetNewRaceNamePointerTable();
    Util::OverwriteUInt32AtAddress(0x4CDA43, reinterpret_cast<uint32_t>(&raceNameTable));
    MonkUnarmedSheathFix();
}

void CharacterFixes::MonkUnarmedSheathFix() {
    (void)CGUnit_C__SetSheatheState_MonkUnarmed__Result;
    (void)CGUnit_C__AnimationData_MonkUnarmed__Result;
    (void)CGUnit_C__HandleModelSequenceCallback_MonkUnarmed__Result;
    (void)CGUnit_C__ResolveModelAnimationId_MonkUnarmed__Result;
    (void)CGUnit_C__ApplyModelAnimationSequence_MonkUnarmed__Result;
    (void)CGUnit_C__UpdateSheatheForAnimation_MonkUnarmed__Result;
    (void)CGUnit_C__UpdateHandItemVisual_MonkUnarmed__Result;
    (void)CGUnit_C__HandleHandItemAnimEvent_MonkUnarmed__Result;
    (void)CGUnit_C__MoveHandItemAttachment_MonkUnarmed__Result;
    (void)CGUnit_C__AttachMainHandItem_MonkUnarmed__Result;
    (void)CGUnit_C__AttachOffHandItem_MonkUnarmed__Result;
    (void)CGUnit_C__SetHandItemBoneSequence_MonkUnarmed__Result;
}

void CharacterFixes::SetNewRaceNamePointerTable() {
    const char* raceStrings[14] = {
        "Worgen", "Naga", "Pandaren_Alliance", "Queldo", "Pandaren_Horde ",
        "Nightborne", "VoidElf", "Vulpera_Alliance", "Vulpera_Horde",
        "Vulpera_Neutral", "Pandaren_Neutral", "ZandalariTroll", "Lightforged",
        "Eredar"
    };

    memcpy(&raceNameTable, (const void*)0xB24180, 0x30);

    for (size_t i = 0; i < sizeof(raceStrings) / sizeof(raceStrings[0]); i++)
        raceNameTable[12 + i] = reinterpret_cast<uint32_t>(raceStrings[i]);

    for (uint8_t i = 26; i < 32; i++)
        raceNameTable[i] = reinterpret_cast<uint32_t>(&dummy);
}
