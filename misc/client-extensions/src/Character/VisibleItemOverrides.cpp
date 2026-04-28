#include <Character/VisibleItemOverrides.h>

#include <ClientDetours.h>
#include <ClientLua.h>
#include <ClientNetwork.h>
#include <Logger.h>
#include <SharedDefines.h>

#include <CustomPacketRead.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>
#include <unordered_map>

namespace
{
    constexpr opcode_t VISIBLE_ITEM_OVERRIDE_OPCODE = 0x7A10;
    constexpr opcode_t CHAR_SELECT_SHOULDER_OVERRIDE_OPCODE = 0x7A11;
    constexpr size_t MAX_VISIBLE_ITEM_SLOTS = 19;
    constexpr uint8_t SERVER_SHOULDER_SLOT = 2;
    constexpr uint8_t CLIENT_SHOULDER_SLOT = 1;
    constexpr bool DEBUG_VISIBLE_ITEM_OVERRIDES = true;
    constexpr uintptr_t CURRENT_CHAR_SELECT_INDEX_ADDR = 0x00AC436C;
    constexpr uintptr_t CHARACTER_DISPLAY_COMPONENT_ADDR = 0x00B6B1A0;

    struct VisibleItemOverride
    {
        uint32_t entryId = 0;
        uint32_t enchantPacked = 0;
        uint32_t transmogEntry = 0;
        int32_t leftShoulderDisplay = -1;
        int32_t rightShoulderDisplay = -1;
        uint32_t flags = 0;
        bool active = false;
    };

    using VisibleItemOverrideSlots = std::array<VisibleItemOverride, MAX_VISIBLE_ITEM_SLOTS>;

    std::unordered_map<uint64_t, VisibleItemOverrideSlots> s_visibleItemOverrides;

    struct ShoulderVisualOverride
    {
        int32_t leftShoulderDisplay = -1;
        int32_t rightShoulderDisplay = -1;
        bool active = false;
    };

    std::unordered_map<uint64_t, ShoulderVisualOverride> s_charSelectShoulderOverrides;

    struct VisibleItemBuildContext
    {
        uint64_t guid = 0;
        uint8_t slot = 0xFF;
    };

    thread_local VisibleItemBuildContext s_visibleItemBuildContext;

    CLIENT_FUNCTION(BuildShoulderPathsFromDisplay, 0x4EF4B0, __thiscall, int, (void*, int, int, char*, char*, char*, char*))
    CLIENT_FUNCTION(AttachShoulderModel, 0x4EAA70, __cdecl, void, (int, unsigned int, void*, char*, int, int))
    CLIENT_FUNCTION(WowClientDB_GetRow, 0x8B7DA0, __thiscall, void, (void*))
    CLIENT_FUNCTION(CCharacterSelection__GetCharacterDisplay, 0x4E2EF0, __cdecl, uint32_t, (unsigned int))
    CLIENT_FUNCTION(ClntObjMgrObjectPtrRaw, 0x4D4DB0, __cdecl, void*, (uint32_t, uint32_t, uint32_t, const char*, uint32_t))
    CLIENT_FUNCTION(CGGameUI__UnitModelUpdate, 0x512B50, __cdecl, void, (int, int))
    CLIENT_FUNCTION(CM2Model__IsLoaded, 0x824F00, __thiscall, int, (void*, int, int))
    CLIENT_FUNCTION(CM2Model__HasAttachment, 0x8273D0, __thiscall, int, (void*, unsigned int))
    CLIENT_FUNCTION(CM2Model__DetachAllChildrenById, 0x827560, __thiscall, void, (void*, unsigned int))

    struct ScopedVisibleItemBuildContext
    {
        explicit ScopedVisibleItemBuildContext(uint64_t guid, uint8_t slot)
            : previous(s_visibleItemBuildContext)
        {
            s_visibleItemBuildContext.guid = guid;
            s_visibleItemBuildContext.slot = slot;
        }

        ~ScopedVisibleItemBuildContext()
        {
            s_visibleItemBuildContext = previous;
        }

        VisibleItemBuildContext previous;
    };

    std::string EscapeLuaString(const std::string& input)
    {
        std::string escaped;
        escaped.reserve(input.size() + 8);
        for (char c : input)
        {
            switch (c)
            {
                case '\\':
                    escaped += "\\\\";
                    break;
                case '"':
                    escaped += "\\\"";
                    break;
                default:
                    escaped += c;
                    break;
            }
        }
        return escaped;
    }

    const VisibleItemOverride* FindVisibleItemOverride(uint64_t guid, uint8_t slot)
    {
        auto itr = s_visibleItemOverrides.find(guid);
        if (itr == s_visibleItemOverrides.end() || slot >= MAX_VISIBLE_ITEM_SLOTS)
            return nullptr;

        const VisibleItemOverride& overrideData = itr->second[slot];
        return overrideData.active ? &overrideData : nullptr;
    }

    const VisibleItemOverride* GetContextualShoulderOverride()
    {
        if (s_visibleItemBuildContext.guid == 0)
            return nullptr;

        if (s_visibleItemBuildContext.slot != SERVER_SHOULDER_SLOT && s_visibleItemBuildContext.slot != CLIENT_SHOULDER_SLOT)
            return nullptr;

        return FindVisibleItemOverride(s_visibleItemBuildContext.guid, SERVER_SHOULDER_SLOT);
    }

    ShoulderVisualOverride GetShoulderVisualOverride(uint64_t guid)
    {
        if (const VisibleItemOverride* visibleItemOverride = FindVisibleItemOverride(guid, SERVER_SHOULDER_SLOT))
        {
            ShoulderVisualOverride overrideData;
            overrideData.leftShoulderDisplay = visibleItemOverride->leftShoulderDisplay;
            overrideData.rightShoulderDisplay = visibleItemOverride->rightShoulderDisplay;
            overrideData.active = true;
            return overrideData;
        }

        auto itr = s_charSelectShoulderOverrides.find(guid);
        if (itr != s_charSelectShoulderOverrides.end())
            return itr->second;

        return {};
    }

    uint64_t GetGuidFromPackedWGuid(void* self)
    {
        if (!self)
            return false;

        uint32_t* guidParts = reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(self) + 8);
        return uint64_t(guidParts[0]) | (uint64_t(guidParts[1]) << 32);
    }

    void* FindUnitByGuid(uint64_t guid)
    {
        if (!guid)
            return nullptr;

        return ClntObjMgrObjectPtrRaw(
            uint32_t(guid & 0xFFFFFFFFull),
            uint32_t(guid >> 32),
            8,
            __FILE__,
            __LINE__);
    }

    bool BuildShoulderSidePaths(
        int32_t displayId,
        bool attachmentSix,
        char* modelPath,
        char* texturePath,
        int& itemVisual)
    {
        if (!modelPath || !texturePath || displayId <= 0)
            return false;

        uint32_t fakeDisplayOwner[268] = {};
        uint8_t rowBuffer[100] = {};
        int rowPtr = 0;
        char attachmentSixModel[260] = {};
        char attachmentFiveModel[260] = {};
        char attachmentSixTexture[260] = {};
        char attachmentFiveTexture[260] = {};

        fakeDisplayOwner[267] = static_cast<uint32_t>(displayId);
        WowClientDB_GetRow(rowBuffer);
        if (!BuildShoulderPathsFromDisplay(
                fakeDisplayOwner,
                reinterpret_cast<int>(rowBuffer),
                reinterpret_cast<int>(&rowPtr),
                attachmentSixModel,
                attachmentFiveModel,
                attachmentSixTexture,
                attachmentFiveTexture))
        {
            return false;
        }

        itemVisual = rowPtr;
        if (attachmentSix)
        {
            std::strcpy(modelPath, attachmentSixModel);
            std::strcpy(texturePath, attachmentSixTexture);
        }
        else
        {
            std::strcpy(modelPath, attachmentFiveModel);
            std::strcpy(texturePath, attachmentFiveTexture);
        }

        return modelPath[0] != '\0';
    }

    void ApplyContextualShoulderOverride(void* component, int visualArg)
    {
        if (!component || s_visibleItemBuildContext.guid == 0)
            return;

        const ShoulderVisualOverride shoulderOverride = GetShoulderVisualOverride(s_visibleItemBuildContext.guid);
        if (!shoulderOverride.active)
            return;

        const int32_t baseDisplayId = reinterpret_cast<int*>(component)[267];
        const int32_t attachmentSixDisplay =
            shoulderOverride.rightShoulderDisplay >= 0 ? shoulderOverride.rightShoulderDisplay : baseDisplayId;
        const int32_t attachmentFiveDisplay =
            shoulderOverride.leftShoulderDisplay >= 0 ? shoulderOverride.leftShoulderDisplay : baseDisplayId;

        if (attachmentSixDisplay == baseDisplayId && attachmentFiveDisplay == baseDisplayId)
            return;

        void* parentModel = reinterpret_cast<void**>(component)[14];
        if (!parentModel || !CM2Model__IsLoaded(parentModel, 0, 0))
            return;

        if (CM2Model__HasAttachment(parentModel, 6))
            CM2Model__DetachAllChildrenById(parentModel, 6);
        if (CM2Model__HasAttachment(parentModel, 5))
            CM2Model__DetachAllChildrenById(parentModel, 5);

        char attachmentSixModel[260] = {};
        char attachmentSixTexture[260] = {};
        char attachmentFiveModel[260] = {};
        char attachmentFiveTexture[260] = {};
        int attachmentSixVisual = 0;
        int attachmentFiveVisual = 0;

        if (attachmentSixDisplay > 0 &&
            BuildShoulderSidePaths(
                attachmentSixDisplay,
                true,
                attachmentSixModel,
                attachmentSixTexture,
                attachmentSixVisual))
        {
            AttachShoulderModel(reinterpret_cast<int>(parentModel), 6, attachmentSixModel, attachmentSixTexture, visualArg, attachmentSixVisual);
        }

        if (attachmentFiveDisplay > 0 &&
            BuildShoulderSidePaths(
                attachmentFiveDisplay,
                false,
                attachmentFiveModel,
                attachmentFiveTexture,
                attachmentFiveVisual))
        {
            AttachShoulderModel(reinterpret_cast<int>(parentModel), 5, attachmentFiveModel, attachmentFiveTexture, visualArg, attachmentFiveVisual);
        }
    }

    uint64_t GetCharacterDisplayGuid(void* characterDisplay)
    {
        if (!characterDisplay)
            return 0;

        uint32_t* guidParts = reinterpret_cast<uint32_t*>(characterDisplay);
        return uint64_t(guidParts[0]) | (uint64_t(guidParts[1]) << 32);
    }

    void ApplyCharacterSelectShoulderOverride(void* characterDisplay, void* component)
    {
        const uint64_t guid = GetCharacterDisplayGuid(characterDisplay);
        if (!guid || !component)
            return;

        ScopedVisibleItemBuildContext context(guid, SERVER_SHOULDER_SLOT);
        ApplyContextualShoulderOverride(component, 0);
    }

    void ApplyCurrentCharacterSelectShoulderOverride()
    {
        const int selectedIndex = *reinterpret_cast<int*>(CURRENT_CHAR_SELECT_INDEX_ADDR);
        if (selectedIndex < 0)
            return;

        void* characterDisplay = reinterpret_cast<void*>(CCharacterSelection__GetCharacterDisplay(unsigned(selectedIndex)));
        if (!characterDisplay)
            return;

        void* selectedComponent = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(characterDisplay) + 392);
        ApplyCharacterSelectShoulderOverride(characterDisplay, selectedComponent);

        void* existingCharacterComponent = *reinterpret_cast<void**>(CHARACTER_DISPLAY_COMPONENT_ADDR);
        if (existingCharacterComponent)
            ApplyCharacterSelectShoulderOverride(characterDisplay, existingCharacterComponent);
    }

    uint64_t GetPlayerGuid(void* self)
    {
        if (!self)
            return 0;

        CGPlayer* player = reinterpret_cast<CGPlayer*>(self);
        if (!player->unitBase.objectBase.ObjectData)
            return 0;

        return player->unitBase.objectBase.ObjectData->OBJECT_FIELD_GUID;
    }

    void StoreVisibleItemOverride(CustomPacketRead* packet)
    {
        if (!packet)
            return;

        uint64_t guid = packet->Read<uint64_t>(0);
        uint8_t slot = packet->Read<uint8_t>(0);
        bool hasItem = packet->Read<uint8_t>(0) != 0;

        if (!guid || slot >= MAX_VISIBLE_ITEM_SLOTS)
            return;

        uint32_t entryId = 0;
        uint32_t transmogEntry = 0;
        int32_t leftShoulderDisplay = -1;
        int32_t rightShoulderDisplay = -1;

        VisibleItemOverride& overrideData = s_visibleItemOverrides[guid][slot];
        if (!hasItem)
        {
            overrideData = {};
            overrideData.leftShoulderDisplay = -1;
            overrideData.rightShoulderDisplay = -1;
            return;
        }

        entryId = packet->Read<uint32_t>(0);
        const uint16_t permanentEnchant = packet->Read<uint16_t>(0);
        const uint16_t temporaryEnchant = packet->Read<uint16_t>(0);
        transmogEntry = packet->Read<uint32_t>(0);
        leftShoulderDisplay = packet->Read<int32_t>(-1);
        rightShoulderDisplay = packet->Read<int32_t>(-1);

        overrideData.entryId = entryId;
        overrideData.transmogEntry = transmogEntry;
        overrideData.leftShoulderDisplay = leftShoulderDisplay;
        overrideData.rightShoulderDisplay = rightShoulderDisplay;
        overrideData.flags = packet->Read<uint32_t>(0);
        overrideData.enchantPacked = uint32_t(permanentEnchant) | (uint32_t(temporaryEnchant) << 16);
        overrideData.active = true;

        if (slot == SERVER_SHOULDER_SLOT)
        {
            void* unit = FindUnitByGuid(guid);
            if (unit)
            {
                ScopedVisibleItemBuildContext context(guid, SERVER_SHOULDER_SLOT);
                void* component = reinterpret_cast<void**>(unit)[723];
                ApplyContextualShoulderOverride(component, 0);
                CGGameUI__UnitModelUpdate(reinterpret_cast<int>(reinterpret_cast<uint8_t*>(unit) + 8), 3);
            }
        }
    }

    void StoreCharacterSelectShoulderOverride(CustomPacketRead* packet)
    {
        if (!packet)
            return;

        const uint8_t count = packet->Read<uint8_t>(0);
        for (uint8_t i = 0; i < count; ++i)
        {
            const uint64_t guid = packet->Read<uint64_t>(0);
            const int32_t leftShoulderDisplay = packet->Read<int32_t>(-1);
            const int32_t rightShoulderDisplay = packet->Read<int32_t>(-1);

            if (!guid)
                continue;

            ShoulderVisualOverride overrideData;
            overrideData.leftShoulderDisplay = leftShoulderDisplay;
            overrideData.rightShoulderDisplay = rightShoulderDisplay;
            overrideData.active = leftShoulderDisplay >= 0 || rightShoulderDisplay >= 0;
            s_charSelectShoulderOverrides[guid] = overrideData;
        }

        ApplyCurrentCharacterSelectShoulderOverride();
    }

    CLIENT_DETOUR_THISCALL(CGPlayer_C__EquipVisibleItem_Context, 0x6E08C0, void, (int* visibleItem, int slot))
    {
        ScopedVisibleItemBuildContext context(GetPlayerGuid(self), static_cast<uint8_t>(slot));
        CGPlayer_C__EquipVisibleItem_Context(self, visibleItem, slot);
        if (slot == SERVER_SHOULDER_SLOT)
        {
            void* component = reinterpret_cast<void**>(self)[723];
            ApplyContextualShoulderOverride(component, 0);
        }
    }

    CLIENT_DETOUR_THISCALL_NOARGS(ApplyVisibleItemEffect_Context, 0x6F82D0, void)
    {
        const int slot = *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(self) + 0x9C);
        ScopedVisibleItemBuildContext context(GetGuidFromPackedWGuid(self), static_cast<uint8_t>(slot));
        ApplyVisibleItemEffect_Context(self);
        if (slot == CLIENT_SHOULDER_SLOT)
        {
            void* unit = FindUnitByGuid(GetGuidFromPackedWGuid(self));
            if (unit)
            {
                void* component = reinterpret_cast<void**>(unit)[723];
                ApplyContextualShoulderOverride(component, 0);
            }
        }
    }

    CLIENT_DETOUR_THISCALL(ApplyVisibleItemSlotUpdate_Context, 0x723730, void, (int slot))
    {
        ScopedVisibleItemBuildContext context(GetPlayerGuid(self), static_cast<uint8_t>(slot));
        ApplyVisibleItemSlotUpdate_Context(self, slot);
        if (slot == SERVER_SHOULDER_SLOT)
        {
            void* component = reinterpret_cast<void**>(self)[723];
            ApplyContextualShoulderOverride(component, 0);
        }
    }

    CLIENT_DETOUR_THISCALL(CGPlayer_C__GetVisibleItemEntryId_Override, 0x6DE330, uint32_t, (int slot))
    {
        if (slot >= 0 && slot < int(MAX_VISIBLE_ITEM_SLOTS))
        {
            uint64_t guid = GetPlayerGuid(self);
            auto itr = s_visibleItemOverrides.find(guid);
            if (itr != s_visibleItemOverrides.end())
            {
                VisibleItemOverride& overrideData = itr->second[slot];
                if (overrideData.active)
                    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&overrideData.entryId));
            }
        }

        return CGPlayer_C__GetVisibleItemEntryId_Override(self, slot);
    }

    CLIENT_DETOUR(CharSelectExistingCharacterBuild, 0x4E0FD0, __cdecl, void, ())
    {
        CharSelectExistingCharacterBuild();
        ApplyCurrentCharacterSelectShoulderOverride();
    }

    CLIENT_DETOUR(CharSelectScreenBuild, 0x4E3CD0, __cdecl, void, ())
    {
        CharSelectScreenBuild();
        ApplyCurrentCharacterSelectShoulderOverride();
    }

}

void VisibleItemOverrides::Apply()
{
    ClientNetwork::OnCustomPacket(VISIBLE_ITEM_OVERRIDE_OPCODE, [](CustomPacketRead* packet)
    {
        StoreVisibleItemOverride(packet);
    });
    ClientNetwork::OnCustomPacket(CHAR_SELECT_SHOULDER_OVERRIDE_OPCODE, [](CustomPacketRead* packet)
    {
        StoreCharacterSelectShoulderOverride(packet);
    });
}

int CCharacterComponent__AddItem_Override__Result = 0;
int CharSelectExistingCharacterBuild__Result = 0;
int CharSelectScreenBuild__Result = 0;
