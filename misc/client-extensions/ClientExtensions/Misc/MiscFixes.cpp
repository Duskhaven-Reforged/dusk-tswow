#include "MiscFixes.h"
#include "ClientDetours.h"
#include "Logger.h"

void MiscFixes::Apply() {
    UpdateObjectVtable();
    UpdateWoWTimeFunctions();
    CameraCollisionFade();
}

// Heavily inspired, and partially copied, from
// https://github.com/someweirdhuman/awesome_wotlk/pull/25
void MiscFixes::CameraCollisionFade() {
    uintptr_t fnAddress = reinterpret_cast<uintptr_t>(&CGWorldFrame_Intersect);

    Util::OverwriteUInt32AtAddress(0x006060FF, fnAddress - 0x00606103);

    int VectorIntersectDoodadDefs__Result = ClientDetours::Add("VectorIntersectDoodadDefs", &VectorIntersectDoodadDefs, VectorIntersectDoodadDefsDetour, __FILE__, __LINE__);
    int World__GetFacets__Result = ClientDetours::Add("World__GetFacets", &World__GetFacets, World__GetFacetsDetour, __FILE__, __LINE__);
}

char __cdecl MiscFixes::CGWorldFrame_Intersect(C3Vector* start, C3Vector* end, C3Vector* hitPoint, float* distance, uint32_t flag, uint32_t buffer)
{
    void* buf = std::malloc(2048);
    if (buf)
    {
        std::memset(buf, 0, 2048);
        if (CGWorldFrame::Intersect(start, end, hitPoint, distance, flag + 1, reinterpret_cast<uintptr_t>(buf)))
        {
            const uint32_t type  = *reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(buf) + 0);
            const uint32_t count = *reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(buf) + 4);

            if (type == 1 && count > 0)
            {
                void* modelPtr = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(buf) + 12 + 80);
                if (modelPtr)
                {
                    g_models_current.insert(modelPtr);
                    g_models_being_faded.insert(modelPtr);

                    if (g_models_original_alphas.find(modelPtr) == g_models_original_alphas.end())
                    {
                        g_models_original_alphas[modelPtr] = *reinterpret_cast<float*>(static_cast<char*>(modelPtr) + 0x17C);
                    }

                    float* alphaPtr = reinterpret_cast<float*>(static_cast<char*>(modelPtr) + 0x17C);
                    *alphaPtr += (0.6f - *alphaPtr) * 0.25f;
                }
                return 0;
            }
        }
        if (g_models_cleanup_timer > 2)
        {
            for (auto it = g_models_being_faded.begin(); it != g_models_being_faded.end();)
            {
                void* modelPtr = *it;

                if (!(g_models_current.find(modelPtr) != g_models_current.end()))
                {
                    float* alphaPtr     = reinterpret_cast<float*>(static_cast<char*>(modelPtr) + 0x17C);
                    float originalAlpha = g_models_original_alphas[modelPtr];

                    *alphaPtr += (originalAlpha - *alphaPtr) * 0.25f;

                    if (std::fabs(*alphaPtr - originalAlpha) < 0.01f)
                    {
                        *alphaPtr = originalAlpha;
                        g_models_original_alphas.erase(modelPtr);
                        it = g_models_being_faded.erase(it);
                        continue;
                    }
                }
                ++it;
            }
            g_models_current.clear();
            g_models_cleanup_timer = 0;
        }
        g_models_cleanup_timer++;
        std::free(buf);
    }

    // Remove M2 camera collisions for object fading
    return CGWorldFrame::Intersect(start, end, hitPoint, distance, flag & ~1u, buffer);
}

void __cdecl MiscFixes::VectorIntersectDoodadDefsDetour(TSList* a1, unsigned int a2)
{
    auto next = a1->m_terminator.m_next;
    while ((reinterpret_cast<uintptr_t>(next) & 1) == 0 && next)
    {
        auto v5       = *reinterpret_cast<uint32_t*>((uintptr_t)next + 4);
        auto modelPtr = *reinterpret_cast<void**>(v5 + 0x34);

        if (modelPtr)
        {
            if (g_models_current.find(modelPtr) != g_models_current.end())
            {
                *reinterpret_cast<uint32_t*>(v5 + 0x2C) = *(uint32_t*)0x00CE04C4;
            }
        }

        next = *reinterpret_cast<DWORD**>((uintptr_t)next + a1->m_linkoffset + 4);
    }

    VectorIntersectDoodadDefs(a1, a2);
}

int __cdecl MiscFixes::World__GetFacetsDetour(int a1, int a2, int a3, int a4) {
    // Remove M2 camera collisions for object fading
    return World__GetFacets(a1, a2, a3 & ~1u, a4);
}

void MiscFixes::UpdateObjectVtable() {
    Util::OverwriteUInt32AtAddress(0x9F3B54, reinterpret_cast<uint32_t>(&ShouldObjectFadeIn));
}

void MiscFixes::UpdateWoWTimeFunctions() {
    // Makes WoWTime::PackToDword call MiscFixes::PackWoWTimeToDword from within instead of running original code
    uint8_t byteArray[] = {0x8B, 0x55, 0x08, 0x50, 0x52, 0xE8, 0x00, 0x00, 0x00, 0x00, 0x83, 0xC4, 0x08, 0x5D, 0xC3};
    Util::OverwriteBytesAtAddress(0x76CA56, byteArray, sizeof(byteArray));
    Util::OverwriteUInt32AtAddress(0x76CA5C, (uint32_t)&PackWoWTimeToDword - 0x76CA60);
    //
    Util::OverwriteUInt32AtAddress(0x4C989C, (uint32_t)&PackTimeDataToDword - 0x4C98A0);
    Util::OverwriteUInt32AtAddress(0x4C98C2, (uint32_t)&PackTimeDataToDword - 0x4C98C6);
    // Changes all sub_76C970 calls to MiscFixes::PackWoWTimeToDword
    Util::OverwriteUInt32AtAddress(0x76CAD4, (uint32_t)&UnpackWoWTime - 0x76CAD8);
    Util::OverwriteUInt32AtAddress(0x76CB19, (uint32_t)&UnpackWoWTime - 0x76CB1D);
    Util::OverwriteUInt32AtAddress(0x76CB93, (uint32_t)&UnpackWoWTime - 0x76CB97);
    //
    Util::OverwriteUInt32AtAddress(0x76DA89, (uint32_t)&GetTimeString - 0x76DA8D);
    Util::OverwriteUInt32AtAddress(0x76DA9F, (uint32_t)&GetTimeString - 0x76DAA3);
    Util::OverwriteUInt32AtAddress(0x7E27B7, (uint32_t)&GetTimeString - 0x7E27BB);
    Util::OverwriteUInt32AtAddress(0x7E2AC7, (uint32_t)&GetTimeString - 0x7E2ACB);
    // Patching a bunch of year >= 31 checks
    Util::SetByteAtAddress((void*)0x5B7ACC, 0xEB);
    Util::SetByteAtAddress((void*)0x5B7ACD, 0x08);
    Util::OverwriteBytesAtAddress((void*)0x5B82C0, 0xFF, 0x03);
    Util::SetByteAtAddress((void*)0x5B82C3, 0x7F);
    Util::SetByteAtAddress((void*)0x5B8F35, 0xEB);
    Util::SetByteAtAddress((void*)0x5B8F99, 0xEB);
    Util::SetByteAtAddress((void*)0x5B8F9A, 0x08);
    Util::OverwriteBytesAtAddress((void*)0x5BFF44, 0x90, 0x03);
    Util::SetByteAtAddress((void*)0x5BFF58, 0xEB);
    Util::OverwriteBytesAtAddress((void*)0x5C01F9, 0x90, 0x03);
    Util::OverwriteBytesAtAddress((void*)0x5C01FF, 0x90, 0x02);
    Util::OverwriteBytesAtAddress((void*)0x5C04BF, 0x90, 0x03);
    Util::OverwriteBytesAtAddress((void*)0x5C04C5, 0x90, 0x02);
    Util::SetByteAtAddress((void*)0x5C2890, 0xEB);
    Util::SetByteAtAddress((void*)0x5C2891, 0x08);
    Util::OverwriteBytesAtAddress((void*)0x76C4A8, 0x90, 0x05);
}

bool MiscFixes::ShouldObjectFadeIn(CGObject* _this, uint32_t unused) {
    uint32_t type = _this->ObjectData->OBJECT_FIELD_TYPE;

    if (type == TYPEMASK_UNIT)
        return CGUnit_C::ShouldFadeIn(reinterpret_cast<CGUnit*>(_this));
    else
        return 1;
}

void MiscFixes::PackTimeDataToDword(uint32_t* packedTime, int32_t minute, int32_t hour, int32_t weekDay, int32_t monthDay, int32_t month, int32_t year, int32_t flags) {
    uint32_t temp = 0;
    temp += minute & 63;
    temp += (hour & 31) << 6;
    temp += (weekDay & 7) << 11;
    temp += (monthDay & 63) << 14;
    temp += (month & 15) << 20;
    temp += year >= 31 ? 31 << 24 : (year & 31) << 24;
    temp += (flags & 3) << 29;

    *packedTime = temp;
}

void MiscFixes::PackWoWTimeToDword(uint32_t* dword, WoWTime* time) {
    uint32_t temp = 0;
    temp += time->minute & 63;
    temp += (time->hour & 31) << 6;
    temp += (time->weekDay & 7) << 11;
    temp += (time->monthDay & 63) << 14;
    temp += (time->month & 15) << 20;
    temp += time->year >= 31 ? 31 << 24 : (time->year & 31) << 24;
    temp += (time->flags & 3) << 29;

    *dword = temp;
}

void MiscFixes::UnpackWoWTime(uint32_t packedTime, int32_t* minute, int32_t* hour, int32_t* weekDay, int32_t* monthDay, int32_t* month, int32_t* year, int32_t* flags) {
    if (minute) {
        if ((packedTime & 63) == 63)
            *minute = -1;
        else
            *minute = packedTime & 63;
    }

    if (hour) {
        if (((packedTime >> 6) & 31) == 31)
            *hour = -1;
        else
            *hour = (packedTime >> 6) & 31;
    }

    if (weekDay) {
        if (((packedTime >> 11) & 7) == 7)
            *weekDay = -1;
        else
            *weekDay = (packedTime >> 11) & 7;
    }

    if (monthDay) {
        if (((packedTime >> 14) & 63) == 63)
            *monthDay = -1;
        else
            *monthDay = (packedTime >> 14) & 63;
    }

    if (month) {
        if (((packedTime >> 20) & 15) == 15)
            *month = -1;
        else
            *month = (packedTime >> 20) & 15;
    }

    if (year)
        *year = (packedTime >> 24) & 31 + yearOffsetMult * 32;

    if (flags) {
        if (((packedTime >> 29) & 3) == 3)
            *flags = -1;
        else
            *flags = (packedTime >> 29) & 3;
    }
}

char* MiscFixes::GetTimeString(WoWTime* a1, char* a2, uint32_t a3)
{
    // Aleist3r: giving up a couple things from original function, originally it was doing a bunch of data & num operations in the first if
    // after that it's checking if values are >= 0...
    if (a1->minute > -1 && a1->hour > -1 && a1->weekDay > -1 && a1->monthDay > -1 && a1->month > -1 && a1->year > -1) {
        const char* weekDays[7] = {
            "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
        };

        char month[8];
        char monthDay[8];
        char year[8];
        char weekDay[8];
        char hour[8];
        char minute[8];

        SStr::Printf(month, 8, "%i", a1->month + 1);
        SStr::Printf(monthDay, 8, "%i", a1->monthDay + 1);
        SStr::Printf(year, 8, "%i", a1->year + 2000);
        SStr::Printf(weekDay, 8, "%s", weekDays[a1->weekDay]);
        SStr::Printf(hour, 8, "%i", a1->hour);
        SStr::Printf(minute, 8, "%i", a1->minute);

        SStr::Printf(a2, a3, "%s/%s/%s (%s) %s:%s", month, monthDay, year, weekDay, hour, minute);
    }
    else
        SStr::Printf(a2, a3, "Not Set");

    return a2;
}

void MiscFixes::SetYearOffsetMultiplier() {
    time_t now = time(0);
    tm* ltm = localtime(&now);

    yearOffsetMult = (ltm->tm_year - 100) / 32;
}
