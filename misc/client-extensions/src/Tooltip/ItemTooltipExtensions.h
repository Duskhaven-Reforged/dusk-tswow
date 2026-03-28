#pragma once

#include <Tooltip/TooltipDefines.h>

class ItemTooltipExtensions {
public:
    static void Apply();

    static void SetItemTooltipHook();

    static int __cdecl SetItemTooltipImpl(
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
        int arg16);

private:
    friend class ClientExtensions;
};
