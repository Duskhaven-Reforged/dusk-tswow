#include <Spells/Spells.h>
#include <ClientDetours.h>
#include <ClientLua.h>
#include <ClientNetwork.h>
#include <SpellAttrDefines.h>
#include <Spells/DangerZoneVisuals.h>
#include <Spells/ScriptedMissileVisuals.h>

namespace
{
    CLIENT_FUNCTION(GetMissileImpactPosition, 0x006FF320, __thiscall, C3Vector*, (void* self, C3Vector* outPosition))

    constexpr uintptr_t CMISSILE_SPELL_ID_OFFSET = 0x1C;
    constexpr uintptr_t CMISSILE_SPEED_OFFSET    = 0x24;
    constexpr uintptr_t CMISSILE_BASE_SPEED_OFFSET = 0x28;
}

void Spells::Apply() {
    g_spell_min_clip_distance_percentage_cvar = CVar_C::Register("spellMinClipDistancePercentage", "Sets the minimum distance the clipping needs to be to activate", 1, "0.0", SpellMinClipDistancePercentage_CVarCallback, 5, 0, 0, 0);
    DangerZoneVisuals::Apply();
    ScriptedMissileVisuals::Apply();
}

char Spells::SpellMinClipDistancePercentage_CVarCallback(CVar* cvar, const char*, const char* value, const char*)
{
    const float clip = std::atof(value);
    if (clip != std::clamp(clip, 0.0f, 1.0f))
        return 0;
    cvar->m_numberValue = clip;
    return 1;
}

CLIENT_DETOUR_THISCALL_NOARGS(MountedCombatAllowed, 0x00715F70, int)
{
    WoWClientDB* spellDB = reinterpret_cast<WoWClientDB*>(0x00AD49D0);
    CGUnit* unit         = (CGUnit*)self;
    SpellRow row{};

    for (size_t i = 0; i < CGUnit_C::GetAuraCount(unit); i++)
    {
        AuraData* data = CGUnit_C::GetAura(unit, i);

        if (ClientDB::GetLocalizedRow(spellDB, data->spellId, &row))
        {
            for (size_t i = 0; i < 3; i++)
            {
                if (row.m_effectAura[i] == 312) // 312 SPELL_AURA_COMBAT_MOUNT_ILLUSION
                {
                    return 1;
                }
            }
        }
    }
    return MountedCombatAllowed(self);
}

CLIENT_DETOUR(CastSpell, 0x00540310, __cdecl, int, (lua_State* L))
{
    ScriptedMissileVisuals::SendCasterFacing(true);

    if (!SStrCmpI(ClientLua::ToLString(L, 2, 0), "cursor", 6))
    {
        ClientLua::SetTop(L, -2);
        Spells::s_castAtCursor = true;
    }
    else if (!SStrCmpI(ClientLua::ToLString(L, 2, 0), "self", 4))
    {
        ClientLua::SetTop(L, -2);
        CastSpell(L);

        CGPlayer* activeObjectPtr = ClntObjMgr::GetActivePlayerObj();
        if (activeObjectPtr)
        {
            C3Vector position = activeObjectPtr->unitBase.movementInfo->position;

            TerrainClickEvent terrainClickEvent = {};
            terrainClickEvent.GUID              = 0;
            terrainClickEvent.x                 = position.x;
            terrainClickEvent.y                 = position.y;
            terrainClickEvent.z                 = position.z;
            terrainClickEvent.button            = 1;
            TerrainClick(&terrainClickEvent);
        }

        return 0;
    }
    return CastSpell(L);
}

CLIENT_DETOUR_THISCALL(OnLayerTrackTerrain, 0x004F66C0, int, (WorldHitTest* a2))
{
    if (Spells::s_castAtCursor)
    {
        Spells::s_castAtCursor              = false;
        TerrainClickEvent terrainClickEvent = {};
        terrainClickEvent.GUID              = 0;
        terrainClickEvent.x                 = a2->hitpoint.x;
        terrainClickEvent.y                 = a2->hitpoint.y;
        terrainClickEvent.z                 = a2->hitpoint.z;
        terrainClickEvent.button            = 1;
        TerrainClick(&terrainClickEvent);
        return 0;
    }
    return OnLayerTrackTerrain(self, a2);
}

C3Vector GetPointAtDistance(const C3Vector& start, const C3Vector& end, float distance)
{
    C3Vector dir;
    dir.x = end.x - start.x;
    dir.y = end.y - start.y;
    dir.z = end.z - start.z;

    float length = sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);

    if (length == 0.0f)
    {
        return start;
    }

    dir.x /= length;
    dir.y /= length;
    dir.z /= length;

    C3Vector point;
    point.x = start.x + dir.x * distance;
    point.y = start.y + dir.y * distance;
    point.z = start.z + dir.z * distance;

    return point;
}

float Dot(const C3Vector& a, const C3Vector& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

C3Vector Normalize(const C3Vector& v)
{
    float lenSq = v.x * v.x + v.y * v.y + v.z * v.z;
    if (lenSq > 0.0f)
    {
        float invLen = 1.0f / sqrtf(lenSq);
        return {v.x * invLen, v.y * invLen, v.z * invLen};
    }
    return {0.0f, 0.0f, 0.0f};
}

CLIENT_DETOUR_THISCALL(GetLineSegment, 0x004F6450, int, (float a2, float a3, C3Vector* a4, C3Vector* a5))
{
    int ret = GetLineSegment(self, a2, a3, a4, a5);

    if (Spell_C::IsTargeting() && Spell_C::CanTargetTerrain())
    {
        CGPlayer* activeObjectPtr = ClntObjMgr::GetActivePlayerObj();
        if (activeObjectPtr)
        {
            PendingSpellCast* targetingSpell = *reinterpret_cast<PendingSpellCast**>(0x00D3F4E4);
            if (targetingSpell)
            {
                auto data     = &targetingSpell->data;
                float minSpellDist = 0.0f, maxSpellDist = 0.0f;
                Spell_C::GetSpellRange(activeObjectPtr, data->spellId, &minSpellDist, &maxSpellDist, 0);

                float left = 0.0f, right = maxSpellDist * 3;
                float bestDist = -1.0f;
                C3Vector bestPoint, bestPos;

                C3Vector point, pos, hitpoint;
                float distance     = 1.0f;
                C3Vector playerPos = activeObjectPtr->unitBase.movementInfo->position;

                bool shouldCheck = true;

                if (TraceLine(a4, a5, &hitpoint, &distance, 0x10111, 0))
                {
                    WorldHitTest test = {0, hitpoint, distance, *a4, *a5};
                    OnLayerTrackTerrain(self, &test);
                    shouldCheck = *(int*)0x00AC79A4 != 0; // s_spellShadowStyle - 0 Is "Success"
                }

                if (!shouldCheck)
                    return ret;

                C3Vector start = *a4;
                C3Vector end   = *a5;
                C3Vector dir   = Normalize({end.x - start.x, end.y - start.y, end.z - start.z});
                C3Vector camToPlayer = {playerPos.x - start.x, playerPos.y - start.y, playerPos.z - start.z};
                float t = Dot(camToPlayer, dir);

                if (t > 0.0f)
                {
                    start = {start.x + dir.x * t, start.y + dir.y * t, start.z + dir.z * t};
                }

                
                float minDist = maxSpellDist * Spells::g_spell_min_clip_distance_percentage_cvar->m_numberValue;
                while (right - left > 0.5f)
                {
                    float mid = (left + right) * 0.5f;
                    point     = GetPointAtDistance(start, end, mid);
                    pos       = {point.x, point.y, point.z - 500.0f};
                    distance  = 1.0f;

                    if (TraceLine(&point, &pos, &hitpoint, &distance, 0x10111, 0))
                    {
                        float dist = CGUnit_C::GetDistanceToPos((CGUnit*)activeObjectPtr, &hitpoint);

                        if (dist < minDist * minDist)
                        {
                            left = mid;
                            continue;
                        }

                        WorldHitTest test{};
                        test.distance = distance;
                        test.start    = point;
                        test.end      = pos;
                        test.hitpoint = hitpoint;
                        OnLayerTrackTerrain(self, &test);

                        if (*(int*)0x00AC79A4 == 0) // s_spellShadowStyle - 0 Is "Success"
                        {
                            bestDist  = dist;
                            bestPoint = point;
                            bestPos   = pos;
                            left      = mid;
                        }
                        else
                        {
                            right = mid;
                        }
                    }
                    else
                    {
                        right = mid;
                    }
                }

                if (bestDist > minDist * minDist)
                {
                    *a4 = bestPoint;
                    *a5 = bestPos;
                }
            }
        }
    }

    return ret;
}

CLIENT_DETOUR_THISCALL(MissileLaunchSetup, 0x007022D0, void, (C3Vector* launchPosition))
{
    if (self && launchPosition)
    {
        SpellRow spell{};
        WoWClientDB* spellDB = reinterpret_cast<WoWClientDB*>(0x00AD49D0);
        uint32_t spellId = *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(self) + CMISSILE_SPELL_ID_OFFSET);

        if (spellId && ClientDB::GetLocalizedRow(spellDB, spellId, &spell) &&
            HasAttribute(&spell, SPELL_ATTR1_CU_MISSILE_SPEED_IS_DELAY_IN_SEC))
        {
            float* missileSpeed = reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(self) + CMISSILE_SPEED_OFFSET);
            float* missileBaseSpeed = reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(self) + CMISSILE_BASE_SPEED_OFFSET);
            float delaySeconds = *missileSpeed;

            if (delaySeconds > 0.0001f)
            {
                C3Vector impactPosition{};
                if (GetMissileImpactPosition(self, &impactPosition))
                {
                    const float dx = impactPosition.x - launchPosition->x;
                    const float dy = impactPosition.y - launchPosition->y;
                    const float dz = impactPosition.z - launchPosition->z;
                    const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

                    if (distance > 0.0001f)
                    {
                        const float convertedSpeed = distance / delaySeconds;
                        *missileSpeed = convertedSpeed;
                        *missileBaseSpeed = convertedSpeed;
                    }
                }
            }
        }
    }

    MissileLaunchSetup(self, launchPosition);
}
