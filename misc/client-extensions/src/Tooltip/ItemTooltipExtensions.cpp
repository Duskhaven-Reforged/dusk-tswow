#include <Tooltip/ItemTooltipExtensions.h>

#include <CDBCMgr/CDBCDefs/ItemDifficultyText.h>
#include <Tooltip/SpellTooltipExtensions.h>
#include <Util.h>

#include <algorithm>

namespace {
    constexpr uint32_t kColorWhite = 0xAD2D30;
    constexpr uint32_t kColorGrey0 = 0xAD2D38;
    constexpr uint32_t kColorGreen0 = 0xAD2D40;
    constexpr uint32_t kItemQualityColors = 0xAD2D84;
    constexpr uint32_t kWdbCacheItem = 0;

    struct ItemCacheTooltipView {
        uint32_t namePtr;
        uint32_t unk04[4];
        uint32_t quality;
        uint32_t flagsAndFaction0;
        uint32_t unk1C[4];
        uint32_t itemClass;
    };

    CLIENT_FUNCTION(CGTooltip_SetItemLoadingText, 0x623760, __thiscall, void, (void*))
    CLIENT_FUNCTION(CGItem_C__BuildItemName, 0x706D70, __cdecl, void, (char*, uint32_t, int, int))

    uint8_t FloatColorToByte(const float value) {
        const float clamped = std::clamp(value, 0.0f, 1.0f);
        return static_cast<uint8_t>(clamped * 255.0f + 0.5f);
    }

    uint32_t MakeTooltipColor(const ItemDifficultyTextRow* row) {
        const uint32_t red = FloatColorToByte(row->red);
        const uint32_t green = FloatColorToByte(row->green);
        const uint32_t blue = FloatColorToByte(row->blue);
        return 0xFF000000u | (red << 16) | (green << 8) | blue;
    }

    void* GetQualityColor(const ItemCacheTooltipView* itemCache) {
        if (!itemCache) {
            return reinterpret_cast<void*>(kColorWhite);
        }

        return reinterpret_cast<void*>(kItemQualityColors + itemCache->quality * 4);
    }
}

void ItemTooltipExtensions::Apply() {
    uint8_t patchBytes[] = {
        0xE9, 0, 0, 0, 0
    };

    // Util::OverwriteBytesAtAddress(0x6277F0, patchBytes, sizeof(patchBytes));
    // Util::OverwriteUInt32AtAddress(
    //     0x6277F1,
    //     Util::CalculateAddress(reinterpret_cast<uint32_t>(&SetItemTooltipHook), 0x6277F5));
}

void __declspec(naked) ItemTooltipExtensions::SetItemTooltipHook() {
    __asm {
        push ebx
        push esi
        push edi
        mov eax, esp

        push [eax + 0x4C]
        push [eax + 0x48]
        push [eax + 0x44]
        push [eax + 0x40]
        push [eax + 0x3C]
        push [eax + 0x38]
        push [eax + 0x34]
        push [eax + 0x30]
        push [eax + 0x2C]
        push [eax + 0x28]
        push [eax + 0x24]
        push [eax + 0x20]
        push [eax + 0x1C]
        push [eax + 0x18]
        push [eax + 0x14]
        push [eax + 0x10]
        push ecx

        call SetItemTooltipImpl
        add esp, 0x44
        pop edi
        pop esi
        pop ebx
        ret 0x40
    }
}

int __cdecl ItemTooltipExtensions::SetItemTooltipImpl(
    void* tooltip,
    int itemId,
    unsigned int arg4,
    void* objectGuidPtr,
    int arg5,
    int bagFamily,
    int randomPropertySeed,
    int updateExisting,
    uint32_t objectGuidLow,
    uint32_t objectGuidHigh,
    int arg10,
    void* pFile2,
    int recoveryTime,
    void* enchantment,
    int arg14,
    int subClass,
    int arg16)
{
    if (!tooltip) {
        return 0;
    }

    if (!updateExisting) {
        CGTooltipInternal::ClearTooltip(tooltip);

        if (objectGuidPtr) {
            const uint32_t* sourceGuid = reinterpret_cast<const uint32_t*>(objectGuidPtr);
            *reinterpret_cast<uint32_t*>(static_cast<char*>(tooltip) + 0x340) = sourceGuid[0];
            *reinterpret_cast<uint32_t*>(static_cast<char*>(tooltip) + 0x344) = sourceGuid[1];
        } else {
            *reinterpret_cast<uint32_t*>(static_cast<char*>(tooltip) + 0x340) = 0;
            *reinterpret_cast<uint32_t*>(static_cast<char*>(tooltip) + 0x344) = 0;
        }

        *reinterpret_cast<uint32_t*>(static_cast<char*>(tooltip) + 0x360) = static_cast<uint32_t>(itemId);
        *reinterpret_cast<uint32_t*>(static_cast<char*>(tooltip) + 0x3B4) = static_cast<uint32_t>(arg5);
        *reinterpret_cast<uint32_t*>(static_cast<char*>(tooltip) + 0x3B8) = static_cast<uint32_t>(arg10);
        *reinterpret_cast<uint32_t*>(static_cast<char*>(tooltip) + 0x3BC) = static_cast<uint32_t>(bagFamily);
        *reinterpret_cast<uint32_t*>(static_cast<char*>(tooltip) + 0x3C0) = objectGuidLow;
        *reinterpret_cast<uint32_t*>(static_cast<char*>(tooltip) + 0x3C4) = objectGuidHigh;
        *reinterpret_cast<uint32_t*>(static_cast<char*>(tooltip) + 0x4C0) = static_cast<uint32_t>(randomPropertySeed);
        *reinterpret_cast<uint32_t*>(static_cast<char*>(tooltip) + 0x4C4) = static_cast<uint32_t>(recoveryTime);
        *reinterpret_cast<uint32_t*>(static_cast<char*>(tooltip) + 0x4C8) = static_cast<uint32_t>(arg14);
        *reinterpret_cast<uint32_t*>(static_cast<char*>(tooltip) + 0x4CC) = static_cast<uint32_t>(arg16);
    }

    CGPlayer* activePlayer = reinterpret_cast<CGPlayer*>(
        ClntObjMgr::ObjectPtr(ClntObjMgr::GetActivePlayer(), TYPEMASK_PLAYER));
    if (!activePlayer) {
        CSimpleFrame::Hide(tooltip);
        return 0;
    }

    void* itemCacheBlock = DBItemCache_GetInfoBlockByID(
        kWdbCacheItem,
        static_cast<uint32_t>(itemId),
        reinterpret_cast<void*>(arg4),
        reinterpret_cast<void*>(0x626650),
        tooltip,
        1);
    if (!itemCacheBlock) {
        CGTooltip_SetItemLoadingText(tooltip);
        CSimpleFrame::Show(tooltip);
        CGTooltipInternal::CalculateSize(tooltip);
        return 0;
    }

    ItemCacheTooltipView* itemCache = reinterpret_cast<ItemCacheTooltipView*>(itemCacheBlock);

    char itemName[1024] = {};
    CGItem_C__BuildItemName(itemName, sizeof(itemName), itemId, randomPropertySeed);

    void* nameColor = arg5
        ? reinterpret_cast<void*>(kColorWhite)
        : GetQualityColor(itemCache);
    CGTooltip::AddLine(tooltip, itemName, nullptr, nameColor, nameColor, 0);

    const ItemDifficultyTextRow* row = GlobalCDBCMap.getRow<ItemDifficultyTextRow>("ItemDifficultyText", itemId);
    if (row && row->text && row->text[0]) {
        uint32_t customColor = MakeTooltipColor(row);
        CGTooltip::AddLine(tooltip, row->text, nullptr, &customColor, &customColor, 0);
    } else if ((itemCache->flagsAndFaction0 & 0x8) != 0) {
        char* heroicText = FrameScript::GetText(const_cast<char*>("ITEM_HEROIC"), -1, 0);
        if (heroicText && heroicText[0]) {
            void* heroicColor = reinterpret_cast<void*>(kColorGreen0);
            CGTooltip::AddLine(tooltip, heroicText, nullptr, heroicColor, heroicColor, 0);
        }
    }

    CSimpleFrame::Show(tooltip);
    CGTooltipInternal::CalculateSize(tooltip);
    return 0;
}
