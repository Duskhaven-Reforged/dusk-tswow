#include <Character/CharacterFixes.h>

#include <ClientDetours.h>
#include <Util.h>
#include <ClientData/SharedDefines.h>


#include <cstring>
#include <vector>
#include <ClientData/Spell.h>
#include <ClientData/ObjectFields.h>

using namespace ClientData;

namespace
{
    constexpr uint32_t SPELL_AURA_MONK_UNARMED = 355;
    constexpr uintptr_t ANIMATION_DATA_DB = 0x00AD30C8;
    constexpr uintptr_t UNIT_MOVEMENT_FLAGS_OFFSET = 0x7CC;
    constexpr uint32_t MOVE_FLAG_DIRECTIONAL = 0x0000000F;
    constexpr uint32_t MOVE_FLAG_BACKWARD = 0x00000002;
    constexpr uint32_t MOVE_FLAG_FALLING = 0x00003000;
    constexpr uint32_t MOVE_FLAG_SWIMMING = 0x00200000;
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
    constexpr uintptr_t UNIT_MODEL_OFFSET = 0xB4;
    constexpr uintptr_t UNIT_MOVEMENT_INFO_OFFSET = 0x788;
    constexpr uintptr_t UNIT_WALK_SPEED_OFFSET = 0x818;
    constexpr uintptr_t UNIT_JUMP_VELOCITY_OFFSET = 0x840;
    constexpr uintptr_t UNIT_SPECIAL_BONE_SEQUENCE_SLOT_OFFSET = 0xB84;
    constexpr uint32_t UNIT_HAND_ITEM_MAINHAND_ACTIVE = 0x100000;
    constexpr uint32_t UNIT_HAND_ITEM_OFFHAND_ACTIVE = 0x200000;
    constexpr int ANIMATION_ATTACK_UNARMED = 16;
    constexpr int ANIMATION_ATTACK_1H = 17;
    constexpr int ANIMATION_ATTACK_2H = 18;
    constexpr int ANIMATION_ATTACK_2H_LOOSE = 19;
    constexpr int ANIMATION_JUMP_START = 37;
    constexpr int ANIMATION_FALL = 40;
    constexpr int ANIMATION_SWIM_IDLE = 41;
    constexpr int ANIMATION_SWIM = 42;
    constexpr int ANIMATION_SWIM_BACKWARDS = 45;
    constexpr int ANIMATION_WALK = 4;
    constexpr int ANIMATION_RUN = 5;
    constexpr int ANIMATION_RUN_BACKWARDS = 13;
    constexpr int ANIMATION_SPECIAL_1H = 57;
    constexpr int ANIMATION_SPECIAL_2H = 58;
    constexpr int ANIMATION_SPRINT = 143;
    constexpr int ANIMATION_CURRENT_OR_NONE = 506;
    constexpr int ANIMATION_CHANNEL_CAST_DIRECTED = 124;
    constexpr int ANIMATION_CHANNEL_CAST_OMNI = 125;
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

    struct AnimationSequenceState
    {
        int animationId;
        float sequenceTime;
        int frame;
        float speed;
    };

    CLIENT_FUNCTION(CGUnit_C__IsSpellKnown_MonkUnarmed, 0x7260E0, __thiscall, bool, (CGUnit*, uint32_t))
    CLIENT_FUNCTION(CMovement__CalcCurrentSpeed_ChannelLowerBody, 0x987570, __thiscall, float, (void*, int))
    CLIENT_FUNCTION(CGUnit_C__ResolveCurrentAnimation_ChannelLowerBody, 0x724500, __thiscall, int, (void*, int, char*, void*))
    CLIENT_FUNCTION(CGUnit_C__SetBoneSequence_ChannelLowerBody, 0x735820, __thiscall, void, (void*, uintptr_t, int, int, float, int, float, int, int, int))
    CLIENT_FUNCTION(CGUnit_C__UnsetBoneSequence_ChannelLowerBody, 0x735A60, __thiscall, int, (void*, void*, int, int, int, int))
    CLIENT_FUNCTION(CM2Model__GetBoneSequenceOriginalAnimId_ChannelLowerBody, 0x8267E0, __thiscall, int, (void*, uint32_t))

    uint32_t GetUnitCurrentChannelId(uintptr_t unit);
    uint32_t GetUnitChannelSpell(uintptr_t unit);
    bool IsUnitChanneling(uintptr_t unit);
    int GetModelBoneSequenceOriginalAnimId(void* model, int boneSeqSlot);
    bool IsAirborneAnimation(int animationId);
    bool IsJumpingUpward(uintptr_t unit);

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

    bool IsChannelCastAnimation(int animationId)
    {
        if (animationId == ANIMATION_CHANNEL_CAST_DIRECTED || animationId == ANIMATION_CHANNEL_CAST_OMNI)
            return true;

        if (animationId < 0)
            return false;

        auto* row = reinterpret_cast<AnimationDataRow*>(ClientDB::GetRow(reinterpret_cast<void*>(ANIMATION_DATA_DB), animationId));
        return row && (row->behaviorId == ANIMATION_CHANNEL_CAST_DIRECTED || row->behaviorId == ANIMATION_CHANNEL_CAST_OMNI);
    }

    bool IsMovingLowerBody(uintptr_t unit)
    {
        const uint32_t movementFlags = *reinterpret_cast<uint32_t*>(unit + UNIT_MOVEMENT_FLAGS_OFFSET);
        return (movementFlags & (MOVE_FLAG_DIRECTIONAL | MOVE_FLAG_FALLING | MOVE_FLAG_SWIMMING)) != 0;
    }

    float AnySequenceTime()
    {
        float value;
        uint32_t bits = 0xFFFFFFFF;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }

    int ResolveChannelLowerBodyAnimation(uintptr_t unit, char animationFlags, int resolveFlags, bool allowAirborne)
    {
        const uint32_t movementFlags = *reinterpret_cast<uint32_t*>(unit + UNIT_MOVEMENT_FLAGS_OFFSET);
        if ((movementFlags & MOVE_FLAG_SWIMMING) != 0)
        {
            if ((movementFlags & MOVE_FLAG_BACKWARD) != 0)
                return ANIMATION_SWIM_BACKWARDS;

            return (movementFlags & MOVE_FLAG_DIRECTIONAL) != 0 ? ANIMATION_SWIM : ANIMATION_SWIM_IDLE;
        }

        if (allowAirborne && (movementFlags & MOVE_FLAG_FALLING) != 0)
        {
            void* model = *reinterpret_cast<void**>(unit + UNIT_MODEL_OFFSET);
            const int rootAnimation = GetModelBoneSequenceOriginalAnimId(model, -1);
            if (rootAnimation == ANIMATION_JUMP_START)
            {
                if (IsJumpingUpward(unit))
                    return rootAnimation;
            }
            else if (IsAirborneAnimation(rootAnimation))
            {
                return rootAnimation;
            }

            if (IsJumpingUpward(unit))
                return ANIMATION_JUMP_START;

            char resolvedFlags = animationFlags;
            const int resolvedAnimation = CGUnit_C__ResolveCurrentAnimation_ChannelLowerBody(
                reinterpret_cast<void*>(unit),
                resolveFlags | 4,
                &resolvedFlags,
                nullptr);

            if (resolvedAnimation != ANIMATION_CURRENT_OR_NONE && !IsChannelCastAnimation(resolvedAnimation))
                return resolvedAnimation;

            return ANIMATION_FALL;
        }

        if ((movementFlags & MOVE_FLAG_DIRECTIONAL) != 0)
        {
            if ((movementFlags & MOVE_FLAG_BACKWARD) != 0)
                return ANIMATION_RUN_BACKWARDS;

            const float speed = CMovement__CalcCurrentSpeed_ChannelLowerBody(reinterpret_cast<void*>(unit + UNIT_MOVEMENT_INFO_OFFSET), 0);
            if (speed >= 11.0f)
                return ANIMATION_SPRINT;

            const float walkSpeed = *reinterpret_cast<float*>(unit + UNIT_WALK_SPEED_OFFSET);
            return speed > walkSpeed * 2.0f ? ANIMATION_RUN : ANIMATION_WALK;
        }

        return -1;
    }

    void SetUnitRootSequence(uintptr_t unit, void* model, int animationId)
    {
        CGUnit_C__SetBoneSequence_ChannelLowerBody(
            reinterpret_cast<void*>(unit),
            reinterpret_cast<uintptr_t>(model),
            -1,
            animationId,
            AnySequenceTime(),
            0,
            1.0f,
            0,
            1,
            0);
    }

    void SetUnitUpperSequence(uintptr_t unit, void* model, int animationId)
    {
        const int upperSlot = *reinterpret_cast<int*>(unit + UNIT_SPECIAL_BONE_SEQUENCE_SLOT_OFFSET);
        if (upperSlot == -1)
            return;

        CGUnit_C__SetBoneSequence_ChannelLowerBody(
            reinterpret_cast<void*>(unit),
            reinterpret_cast<uintptr_t>(model),
            upperSlot,
            animationId,
            AnySequenceTime(),
            0,
            1.0f,
            0,
            1,
            0);
    }

    int GetModelBoneSequenceOriginalAnimId(void* model, int boneSeqSlot)
    {
        if (!model)
            return 0;

        return CM2Model__GetBoneSequenceOriginalAnimId_ChannelLowerBody(model, static_cast<uint32_t>(boneSeqSlot));
    }

    bool IsAirborneAnimation(int animationId)
    {
        if (animationId == ANIMATION_JUMP_START || animationId == ANIMATION_FALL)
            return true;

        if (animationId < 0)
            return false;

        auto* row = reinterpret_cast<AnimationDataRow*>(ClientDB::GetRow(reinterpret_cast<void*>(ANIMATION_DATA_DB), animationId));
        return row && row->behaviorId >= 37 && (row->behaviorId <= 40 || row->behaviorId == 467);
    }

    bool IsJumpingUpward(uintptr_t unit)
    {
        const uint32_t movementFlags = *reinterpret_cast<uint32_t*>(unit + UNIT_MOVEMENT_FLAGS_OFFSET);
        if ((movementFlags & MOVE_FLAG_FALLING) == 0)
            return false;

        return *reinterpret_cast<float*>(unit + UNIT_JUMP_VELOCITY_OFFSET) > 0.0f;
    }

    int GetUnitUpperChannelAnimation(uintptr_t unit, void* model)
    {
        const int upperSlot = *reinterpret_cast<int*>(unit + UNIT_SPECIAL_BONE_SEQUENCE_SLOT_OFFSET);
        if (upperSlot == -1)
            return 0;

        const int upperAnimation = GetModelBoneSequenceOriginalAnimId(model, upperSlot);
        return IsChannelCastAnimation(upperAnimation) ? upperAnimation : 0;
    }

    int GetUnitRootChannelAnimation(void* model)
    {
        const int rootAnimation = GetModelBoneSequenceOriginalAnimId(model, -1);
        return IsChannelCastAnimation(rootAnimation) ? rootAnimation : 0;
    }

    void MoveRootChannelToUpperBody(uintptr_t unit, void* model)
    {
        if (GetUnitUpperChannelAnimation(unit, model) != 0)
            return;

        const int rootChannelAnimation = GetUnitRootChannelAnimation(model);
        if (rootChannelAnimation != 0)
            SetUnitUpperSequence(unit, model, rootChannelAnimation);
    }

    void RestoreRootChannelFromUpperBody(uintptr_t unit, void* model)
    {
        const int upperSlot = *reinterpret_cast<int*>(unit + UNIT_SPECIAL_BONE_SEQUENCE_SLOT_OFFSET);
        if (upperSlot == -1)
            return;

        const int upperChannelAnimation = GetUnitUpperChannelAnimation(unit, model);
        if (upperChannelAnimation == 0)
            return;

        if (GetUnitRootChannelAnimation(model) == 0)
            SetUnitRootSequence(unit, model, upperChannelAnimation);

        CGUnit_C__UnsetBoneSequence_ChannelLowerBody(reinterpret_cast<void*>(unit), model, upperSlot, 1, 0, 1);
    }

    void StabilizeChannelLowerBodyAnimation(uintptr_t unit)
    {
        if (!IsUnitChanneling(unit) || !IsMovingLowerBody(unit))
            return;

        const uint32_t movementFlags = *reinterpret_cast<uint32_t*>(unit + UNIT_MOVEMENT_FLAGS_OFFSET);
        void* model = *reinterpret_cast<void**>(unit + UNIT_MODEL_OFFSET);
        if (!model)
            return;

        const int rootAnimation = GetModelBoneSequenceOriginalAnimId(model, -1);
        if ((movementFlags & MOVE_FLAG_FALLING) != 0)
        {
            if (!IsAirborneAnimation(rootAnimation))
            {
                MoveRootChannelToUpperBody(unit, model);
                SetUnitRootSequence(unit, model, ANIMATION_JUMP_START);
            }
            return;
        }

        if (!IsAirborneAnimation(rootAnimation))
            return;

        MoveRootChannelToUpperBody(unit, model);

        const int rootMovementAnimation = ResolveChannelLowerBodyAnimation(unit, 0, 4, false);
        if (rootMovementAnimation != -1)
            SetUnitRootSequence(unit, model, rootMovementAnimation);
    }

    bool ShouldRouteMovingChannelToUpperSlot(uintptr_t unit, int animationId)
    {
        return unit
            && IsMovingLowerBody(unit)
            && IsUnitChanneling(unit)
            && IsChannelCastAnimation(animationId)
            && *reinterpret_cast<int*>(unit + UNIT_SPECIAL_BONE_SEQUENCE_SLOT_OFFSET) != -1;
    }

    uint32_t GetUnitCurrentChannelId(uintptr_t unit)
    {
        if (!unit)
            return 0;

        CGUnit* cgUnit = reinterpret_cast<CGUnit*>(unit);
        return cgUnit->currentChannelId;
    }

    uint32_t GetUnitChannelSpell(uintptr_t unit)
    {
        if (!unit)
            return 0;

        CGUnit* cgUnit = reinterpret_cast<CGUnit*>(unit);
        if (!cgUnit->unitData)
            return 0;

        return cgUnit->unitData->channelSpell;
    }

    bool IsUnitChanneling(uintptr_t unit)
    {
        return GetUnitCurrentChannelId(unit) != 0 || GetUnitChannelSpell(unit) != 0;
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

    CLIENT_DETOUR_THISCALL(CGUnit_C__ApplyAnimationSequence_ChannelLowerBody, 0x737EF0, void, (AnimationSequenceState* sequence, int previousAnimationId, int useSpecialBoneSlot, int currentAnimationId, int allowTransition, int primary))
    {
        const uintptr_t unit = reinterpret_cast<uintptr_t>(self);
        if (sequence && !useSpecialBoneSlot && ShouldRouteMovingChannelToUpperSlot(unit, sequence->animationId))
            useSpecialBoneSlot = 1;

        CGUnit_C__ApplyAnimationSequence_ChannelLowerBody(self, sequence, previousAnimationId, useSpecialBoneSlot, currentAnimationId, allowTransition, primary);
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
        const uintptr_t unit = reinterpret_cast<uintptr_t>(self);
        if (boneSeqSlot == -1 && ShouldRouteMovingChannelToUpperSlot(unit, sequence))
        {
            const int rootMovementAnimation = ResolveChannelLowerBodyAnimation(unit, 0, 4, true);
            if (rootMovementAnimation != -1)
                CGUnit_C__SetHandItemBoneSequence_MonkUnarmed(self, model, -1, rootMovementAnimation, AnySequenceTime(), a6, 1.0f, a8, a9, a10);

            boneSeqSlot = *reinterpret_cast<int*>(unit + UNIT_SPECIAL_BONE_SEQUENCE_SLOT_OFFSET);
        }

        if (ShouldPreventMeleeUnsheath(reinterpret_cast<CGUnit*>(self)))
        {
            if (IsHandItemAttachSequence(boneSeqSlot, sequence))
            {
                ClearHandItemActiveFlag(unit, boneSeqSlot);
                return;
            }

            if (IsMonkUnarmedMeleeAttackAnimation(sequence))
                sequence = ANIMATION_ATTACK_UNARMED;
        }

        CGUnit_C__SetHandItemBoneSequence_MonkUnarmed(self, model, boneSeqSlot, sequence, sequenceTime, a6, speed, a8, a9, a10);
    }

    CLIENT_DETOUR_THISCALL(CGUnit_C__UpdateCurrentAnimation_ChannelLowerBody, 0x73AC30, void, (char flags, int animationId))
    {
        const uintptr_t unit = reinterpret_cast<uintptr_t>(self);
        const bool wasChanneling = IsUnitChanneling(unit);
        void* model = wasChanneling ? *reinterpret_cast<void**>(unit + UNIT_MODEL_OFFSET) : nullptr;
        const int rootChannelBeforeUpdate = model ? GetUnitRootChannelAnimation(model) : 0;

        CGUnit_C__UpdateCurrentAnimation_ChannelLowerBody(self, flags, animationId);

        if (!IsUnitChanneling(unit))
            return;

        model = *reinterpret_cast<void**>(unit + UNIT_MODEL_OFFSET);
        if (!model)
            return;

        if (!IsMovingLowerBody(unit))
        {
            RestoreRootChannelFromUpperBody(unit, model);
            return;
        }

        MoveRootChannelToUpperBody(unit, model);
        if (rootChannelBeforeUpdate != 0 && GetUnitUpperChannelAnimation(unit, model) == 0)
            SetUnitUpperSequence(unit, model, rootChannelBeforeUpdate);

        const int rootMovementAnimation = ResolveChannelLowerBodyAnimation(unit, flags, animationId, true);
        if (rootMovementAnimation != -1)
            SetUnitRootSequence(unit, model, rootMovementAnimation);
    }

    CLIENT_DETOUR_THISCALL(CGUnit_C__PlayFallLandAnimation_ChannelLowerBody, 0x73D2B0, void, (int movementFlags, int forceLandAnimation))
    {
        CGUnit_C__PlayFallLandAnimation_ChannelLowerBody(self, movementFlags, forceLandAnimation);

        const uintptr_t unit = reinterpret_cast<uintptr_t>(self);
        if (!IsUnitChanneling(unit))
            return;

        void* model = *reinterpret_cast<void**>(unit + UNIT_MODEL_OFFSET);
        if (!model)
            return;

        MoveRootChannelToUpperBody(unit, model);

        const int rootMovementAnimation = ResolveChannelLowerBodyAnimation(unit, 0, 4, false);
        if (rootMovementAnimation != -1)
            SetUnitRootSequence(unit, model, rootMovementAnimation);
    }

    CLIENT_DETOUR_THISCALL(CGUnit_C__PreAnimate_ChannelLowerBody, 0x73DAB0, int, (int elapsed))
    {
        const int result = CGUnit_C__PreAnimate_ChannelLowerBody(self, elapsed);
        StabilizeChannelLowerBodyAnimation(reinterpret_cast<uintptr_t>(self));
        return result;
    }

}

void CharacterFixes::CharacterCreationFixes() {
    // addresses pointing to, uh, some sort of shared memory storage
    // needs to be bigger to not cause crashes with our dbcs so I assigned to it 512 bytes (original table is 176 bytes iirc? cba to look in IDA), should be enough
    std::vector<uint32_t> patchedAddresses = { 0x4E157D, 0x4E16A3, 0x4E15B5, 0x4E20EE, 0x4E222A, 0x4E2127, 0x4E1E94, 0x4E1C3A };

    for (uint8_t i = 0; i < patchedAddresses.size(); i++)
        Util::OverwriteUInt32AtAddress(patchedAddresses[i], reinterpret_cast<uint32_t>(&memoryTable));

    // Name table
    // 0x4CDA43 - address of table where pointers to race name strings are stored
    SetNewRaceNamePointerTable();
    Util::OverwriteUInt32AtAddress(0x4CDA43, reinterpret_cast<uint32_t>(&raceNameTable));
    AnimationLayeringFixes();
    MonkUnarmedSheathFix();
}

void CharacterFixes::AnimationLayeringFixes() {
    (void)CGUnit_C__ApplyAnimationSequence_ChannelLowerBody__Result;
    (void)CGUnit_C__UpdateCurrentAnimation_ChannelLowerBody__Result;
    (void)CGUnit_C__PlayFallLandAnimation_ChannelLowerBody__Result;
    (void)CGUnit_C__PreAnimate_ChannelLowerBody__Result;
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
