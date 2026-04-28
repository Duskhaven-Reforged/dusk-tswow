#include <Spells/DangerZoneVisuals.h>

#include <CDBCMgr/CDBCMgr.h>
#include <CDBCMgr/CDBCDefs/DangerZoneVisualProfile.h>
#include <ClientDetours.h>
#include <Logger.h>
#include <SharedDefines.h>

#include <Windows.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

namespace DangerZoneVisualsInternal
{
    constexpr uint32_t SMSG_SPELL_START = 0x131;
    constexpr uint32_t SMSG_SPELL_GO = 0x132;
    constexpr uint32_t MSG_CHANNEL_START = 0x139;
    constexpr uint32_t MSG_CHANNEL_UPDATE = 0x13A;

    constexpr uint32_t CAST_FLAG_AMMO = 0x00000020;
    constexpr uint32_t CAST_FLAG_POWER_LEFT_SELF = 0x00000800;
    constexpr uint32_t CAST_FLAG_ADJUST_MISSILE = 0x00020000;
    constexpr uint32_t CAST_FLAG_VISUAL_CHAIN = 0x00080000;
    constexpr uint32_t CAST_FLAG_RUNE_LIST = 0x00200000;
    constexpr uint32_t CAST_FLAG_IMMUNITY = 0x04000000;

    constexpr uint32_t TARGET_FLAG_UNIT = 0x00000002;
    constexpr uint32_t TARGET_FLAG_UNIT_RAID = 0x00000004;
    constexpr uint32_t TARGET_FLAG_UNIT_PARTY = 0x00000008;
    constexpr uint32_t TARGET_FLAG_ITEM = 0x00000010;
    constexpr uint32_t TARGET_FLAG_SOURCE_LOCATION = 0x00000020;
    constexpr uint32_t TARGET_FLAG_DEST_LOCATION = 0x00000040;
    constexpr uint32_t TARGET_FLAG_UNIT_ENEMY = 0x00000080;
    constexpr uint32_t TARGET_FLAG_UNIT_ALLY = 0x00000100;
    constexpr uint32_t TARGET_FLAG_CORPSE_ENEMY = 0x00000200;
    constexpr uint32_t TARGET_FLAG_UNIT_DEAD = 0x00000400;
    constexpr uint32_t TARGET_FLAG_GAMEOBJECT = 0x00000800;
    constexpr uint32_t TARGET_FLAG_TRADE_ITEM = 0x00001000;
    constexpr uint32_t TARGET_FLAG_STRING = 0x00002000;
    constexpr uint32_t TARGET_FLAG_GAMEOBJECT_ITEM = 0x00004000;
    constexpr uint32_t TARGET_FLAG_CORPSE_ALLY = 0x00008000;
    constexpr uint32_t TARGET_FLAG_UNIT_MINIPET = 0x00010000;
    constexpr uint32_t TARGET_FLAG_UNIT_PASSENGER = 0x00100000;
    constexpr uint32_t TARGET_FLAG_UNIT_MASK = TARGET_FLAG_UNIT | TARGET_FLAG_UNIT_RAID | TARGET_FLAG_UNIT_PARTY |
        TARGET_FLAG_UNIT_ENEMY | TARGET_FLAG_UNIT_ALLY | TARGET_FLAG_UNIT_DEAD | TARGET_FLAG_UNIT_MINIPET | TARGET_FLAG_UNIT_PASSENGER;
    constexpr uint32_t TARGET_FLAG_GAMEOBJECT_MASK = TARGET_FLAG_GAMEOBJECT | TARGET_FLAG_GAMEOBJECT_ITEM;
    constexpr uint32_t TARGET_FLAG_CORPSE_MASK = TARGET_FLAG_CORPSE_ALLY | TARGET_FLAG_CORPSE_ENEMY;
    constexpr uint32_t TARGET_FLAG_ITEM_MASK = TARGET_FLAG_TRADE_ITEM | TARGET_FLAG_ITEM | TARGET_FLAG_GAMEOBJECT_ITEM;

    constexpr uint32_t PROFILE_FLAG_CREATURE_CASTERS = 0x01;
    constexpr uint32_t PROFILE_FLAG_DEBUG_PLAYER_CASTERS = 0x02;
    constexpr uint32_t PROFILE_FLAG_REQUIRE_HARMFUL = 0x04;
    constexpr uint32_t PROFILE_FLAG_SHOW_CAST = 0x08;
    constexpr uint32_t PROFILE_FLAG_SHOW_TRAVEL = 0x10;
    constexpr uint32_t PROFILE_FLAG_SHOW_CHANNEL = 0x20;
    constexpr uint32_t SHAPE_CIRCLE = 0x01;

    constexpr uintptr_t WORLD_SCENE_M2_SCENE = 0x00CD754C;
    constexpr uintptr_t SPELL_DB = 0x00AD49D0;

    struct ClientPacket {
        uint32_t m_padding;
        uint8_t* m_buffer;
        uint32_t m_base;
        uint32_t m_alloc;
        uint32_t m_size;
        uint32_t m_read;
    };

    struct C44Matrix
    {
        float a0, a1, a2, a3;
        float b0, b1, b2, b3;
        float c0, c1, c2, c3;
        float d0, d1, d2, d3;
    };

    struct BoundingBox
    {
        float minX, minY, minZ;
        float maxX, maxY, maxZ;
    };

    struct SpellRadiusRow
    {
        uint32_t id;
        float radius;
        float radiusPerLevel;
        float radiusMax;
    };

    struct TargetData
    {
        uint32_t flags = 0;
        uint64_t unitGuid = 0;
        uint64_t itemGuid = 0;
        C3Vector src{};
        C3Vector dst{};
        bool hasUnit = false;
        bool hasSrc = false;
        bool hasDst = false;
    };

    struct CastPacket
    {
        uint32_t opcode = 0;
        uint64_t casterGuid = 0;
        uint64_t casterUnitGuid = 0;
        uint8_t castId = 0;
        uint32_t spellId = 0;
        uint32_t castFlags = 0;
        uint32_t castTime = 0;
        TargetData target;
        bool hasMissileTrajectory = false;
        float missilePitch = 0.0f;
        uint32_t missileTravelMs = 0;
    };

    struct RenderModel
    {
        void* model = nullptr;
        void* worldObject = nullptr;
    };

    struct DangerZoneInstance
    {
        uint32_t instanceId = 0;
        uint64_t casterGuid = 0;
        uint32_t spellId = 0;
        C3Vector position{};
        float radius = 5.0f;
        uint64_t startMs = 0;
        uint64_t endMs = 0;
        std::string modelPath;
        float modelRadius = 5.0f;
        float radiusScale = 1.0f;
        float zOffset = 0.05f;
        float red = 1.0f;
        float green = 0.0f;
        float blue = 0.0f;
        float alpha = 0.75f;
        RenderModel renderModel;
    };

    CLIENT_FUNCTION(CDataStore_GetWowGUID, 0x76DC20, __cdecl, void, (ClientPacket* packet, uint64_t* guid))
    CLIENT_FUNCTION(CDataStore_GetUInt8, 0x47B340, __thiscall, void, (ClientPacket* packet, uint8_t* value))
    CLIENT_FUNCTION(CDataStore_GetUInt32, 0x47B3C0, __thiscall, void, (ClientPacket* packet, uint32_t* value))
    CLIENT_FUNCTION(CDataStore_GetUInt64, 0x47B400, __thiscall, void, (ClientPacket* packet, uint64_t* value))
    CLIENT_FUNCTION(CDataStore_GetFloat, 0x47B440, __thiscall, void, (ClientPacket* packet, float* value))
    CLIENT_FUNCTION(CM2Scene_CreateModel, 0x81F8F0, __thiscall, void*, (void* scene, void* filename, int flags))
    CLIENT_FUNCTION(CM2Model_Release, 0x824ED0, __thiscall, int, (void* model))
    CLIENT_FUNCTION(CM2Model_IsLoaded, 0x824F00, __thiscall, bool, (void* model, int, int))
    CLIENT_FUNCTION(RenderModelCreateWorldObject, 0x781A10, __cdecl, void*, (void* model, void* callback, int a3, int a4, int a5, void* owner, uint32_t flags))
    CLIENT_FUNCTION(World_ObjectUpdateRaw, 0x780240, __cdecl, void, (void* worldObject, C44Matrix* matrix, BoundingBox* bounds, C3Vector* sphere, C3Vector* localOffset, int a6, int a7))
    CLIENT_FUNCTION(World_ObjectDestroy, 0x7826E0, __cdecl, int, (void* worldObject))
    CLIENT_FUNCTION(WorldObjectKeepAliveCallback, 0x4F5910, __cdecl, bool, (int a1, int a2, int a3, int a4, void* owner))

    void DestroyRenderModel(RenderModel& renderModel)
    {
        if (renderModel.worldObject)
        {
            World_ObjectDestroy(renderModel.worldObject);
            renderModel.worldObject = nullptr;
        }

        if (renderModel.model)
        {
            CM2Model_Release(renderModel.model);
            renderModel.model = nullptr;
        }
    }

    std::unordered_map<uint32_t, SpellRadiusRow> s_spellRadii;
    bool s_spellRadiiLoaded = false;
    std::unordered_map<uint32_t, DangerZoneInstance> s_instances;
    std::unordered_map<uint64_t, uint32_t> s_instanceByCasterSpell;
    uint32_t s_nextInstanceId = 1;

    uint64_t MakeCasterSpellKey(uint64_t casterGuid, uint32_t spellId)
    {
        return (casterGuid ^ (uint64_t(spellId) << 32) ^ spellId);
    }

    void ReadGuid(ClientPacket* packet, uint64_t& value)
    {
        value = 0;
        CDataStore_GetWowGUID(packet, &value);
    }

    void ReadU8(ClientPacket* packet, uint8_t& value)
    {
        value = 0;
        CDataStore_GetUInt8(packet, &value);
    }

    void ReadU32(ClientPacket* packet, uint32_t& value)
    {
        value = 0;
        CDataStore_GetUInt32(packet, &value);
    }

    void ReadU64(ClientPacket* packet, uint64_t& value)
    {
        value = 0;
        CDataStore_GetUInt64(packet, &value);
    }

    void ReadFloat(ClientPacket* packet, float& value)
    {
        value = 0.0f;
        CDataStore_GetFloat(packet, &value);
    }

    void SkipCString(ClientPacket* packet)
    {
        if (!packet || !packet->m_buffer)
            return;

        while (packet->m_read < packet->m_base + packet->m_size)
        {
            uint8_t const value = packet->m_buffer[packet->m_read - packet->m_base];
            ++packet->m_read;
            if (!value)
                break;
        }
    }

    C3Vector UnitPosition(uint64_t guid, C3Vector fallback)
    {
        if (!guid)
            return fallback;

        CGUnit* unit = reinterpret_cast<CGUnit*>(ClntObjMgr::ObjectPtr(guid, TYPEMASK_UNIT | TYPEMASK_PLAYER));
        if (!unit || !unit->movementInfo)
            return fallback;

        return unit->movementInfo->position;
    }

    bool IsCreatureGuid(uint64_t guid)
    {
        CGObject* object = reinterpret_cast<CGObject*>(ClntObjMgr::ObjectPtr(guid, TYPEMASK_UNIT | TYPEMASK_PLAYER));
        if (!object || !object->ObjectData)
            return false;

        uint32_t const type = object->ObjectData->OBJECT_FIELD_TYPE;
        return (type & TYPEMASK_UNIT) != 0 && (type & TYPEMASK_PLAYER) == 0;
    }

    bool IsPlayerGuid(uint64_t guid)
    {
        CGObject* object = reinterpret_cast<CGObject*>(ClntObjMgr::ObjectPtr(guid, TYPEMASK_PLAYER));
        return object && object->ObjectData && (object->ObjectData->OBJECT_FIELD_TYPE & TYPEMASK_PLAYER) != 0;
    }

    float Distance2D(C3Vector const& a, C3Vector const& b)
    {
        float const dx = a.x - b.x;
        float const dy = a.y - b.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    bool GroundPosition(C3Vector const& position, float zOffset, C3Vector& out)
    {
        C3Vector start{ position.x, position.y, position.z + 50.0f };
        C3Vector end{ position.x, position.y, position.z - 200.0f };
        C3Vector hit{};
        float distance = 1.0f;
        if (TraceLine(&start, &end, &hit, &distance, 0x10111, 0))
        {
            out = { hit.x, hit.y, hit.z + zOffset };
            return true;
        }

        out = { position.x, position.y, position.z + zOffset };
        return false;
    }

    void LoadSpellRadiusDbc()
    {
        if (s_spellRadiiLoaded)
            return;

        s_spellRadiiLoaded = true;
        HANDLE file = nullptr;
        if (!SFile::OpenFile("DBFilesClient\\SpellRadius.dbc", &file) || !file)
            return;

        uint32_t header[5]{};
        uint32_t bytesRead = 0;
        if (!SFile::ReadFile(file, header, sizeof(header), &bytesRead, nullptr, 0) || bytesRead != sizeof(header))
        {
            SFile::CloseFile(file);
            return;
        }

        uint32_t const records = header[1];
        uint32_t const fields = header[2];
        uint32_t const recordSize = header[3];
        if (fields != 4 || recordSize != sizeof(SpellRadiusRow) || records > 4096)
        {
            SFile::CloseFile(file);
            return;
        }

        std::vector<SpellRadiusRow> rows(records);
        if (!SFile::ReadFile(file, rows.data(), records * sizeof(SpellRadiusRow), &bytesRead, nullptr, 0))
        {
            SFile::CloseFile(file);
            return;
        }

        for (SpellRadiusRow const& row : rows)
            s_spellRadii[row.id] = row;

        SFile::CloseFile(file);
    }

    float RadiusForSpell(SpellRow const& spell)
    {
        LoadSpellRadiusDbc();
        float radius = 0.0f;
        for (uint32_t radiusId : spell.m_effectRadiusIndex)
        {
            if (!radiusId)
                continue;

            auto itr = s_spellRadii.find(radiusId);
            if (itr != s_spellRadii.end())
                radius = (std::max)(radius, itr->second.radius);
        }

        return radius;
    }

    bool IsHarmfulEffect(uint32_t effect)
    {
        switch (effect)
        {
            case 2:   // SCHOOL_DAMAGE
            case 7:   // ENVIRONMENTAL_DAMAGE
            case 9:   // HEALTH_LEECH
            case 27:  // PERSISTENT_AREA_AURA
            case 32:  // TRIGGER_MISSILE
            case 129: // APPLY_AREA_AURA_ENEMY
            case 148: // TRIGGER_MISSILE_SPELL_WITH_VALUE
                return true;
            default:
                return false;
        }
    }

    bool HasHarmfulEffect(SpellRow const& spell)
    {
        for (uint32_t effect : spell.m_effect)
            if (IsHarmfulEffect(effect))
                return true;
        return false;
    }

    bool IsCasterAreaTarget(uint32_t target)
    {
        switch (target)
        {
            case 1:   // UNIT_CASTER
            case 15:  // UNIT_SRC_AREA_ENEMY
            case 18:  // DEST_CASTER
            case 20:  // UNIT_CASTER_AREA_PARTY
            case 30:  // UNIT_SRC_AREA_ALLY
            case 33:  // UNIT_SRC_AREA_PARTY
            case 36:  // DEST_CASTER_UNK36
            case 39:  // DEST_CASTER_FISHING
            case 41: case 42: case 43: case 44:
            case 47: case 48: case 49: case 50:
            case 56:  // UNIT_CASTER_AREA_RAID
            case 62:  // DEST_DYNOBJ_CASTER
            case 72:  // DEST_CASTER_RANDOM
            case 73:  // DEST_CASTER_RADIUS
            case 106: // DEST_CHANNEL_CASTER
            case 111: // UNIT_CASTER_AREA_SUMMONS
                return true;
            default:
                return false;
        }
    }

    bool IsTargetAreaTarget(uint32_t target)
    {
        switch (target)
        {
            case 8:   // UNIT_DEST_AREA_ENTRY
            case 16:  // UNIT_DEST_AREA_ENEMY
            case 28:  // DEST_DYNOBJ_ENEMY
            case 29:  // DEST_DYNOBJ_ALLY
            case 31:  // UNIT_DEST_AREA_ALLY
            case 34:  // UNIT_DEST_AREA_PARTY
            case 52:  // GAMEOBJECT_DEST_AREA
            case 53:  // DEST_TARGET_ENEMY
            case 63: case 64: case 65: case 66: case 67:
            case 68: case 69: case 70: case 71:
            case 74:  // DEST_TARGET_RANDOM
            case 75:  // DEST_TARGET_RADIUS
            case 76:  // DEST_CHANNEL_TARGET
            case 77:  // UNIT_CHANNEL_TARGET
            case 87:  // DEST_DEST
            case 88:  // DEST_DYNOBJ_NONE
            case 89:  // DEST_TRAJ
            case 91:  // DEST_DEST_RADIUS
                return true;
            default:
                return false;
        }
    }

    bool ResolveDangerPosition(CastPacket const& cast, SpellRow const& spell, C3Vector& out)
    {
        C3Vector casterPos = UnitPosition(cast.casterUnitGuid ? cast.casterUnitGuid : cast.casterGuid, {});

        if (cast.target.hasDst)
        {
            out = cast.target.dst;
            return true;
        }

        for (int i = 0; i < 3; ++i)
        {
            if (IsCasterAreaTarget(spell.m_implicitTargetA[i]) || IsCasterAreaTarget(spell.m_implicitTargetB[i]))
            {
                out = casterPos;
                return true;
            }

            if (IsTargetAreaTarget(spell.m_implicitTargetA[i]) || IsTargetAreaTarget(spell.m_implicitTargetB[i]))
            {
                if (cast.target.hasUnit)
                {
                    out = UnitPosition(cast.target.unitGuid, casterPos);
                    return true;
                }
            }
        }

        if (cast.target.hasUnit)
        {
            out = UnitPosition(cast.target.unitGuid, casterPos);
            return true;
        }

        out = casterPos;
        return casterPos.x != 0.0f || casterPos.y != 0.0f || casterPos.z != 0.0f;
    }

    DangerZoneVisualProfileRow const* FindProfile(SpellRow const& spell, uint32_t spellId, bool playerCaster, bool harmful)
    {
        auto itr = GlobalCDBCMap.allCDBCs.find("DangerZoneVisualProfile");
        if (itr == GlobalCDBCMap.allCDBCs.end())
            return nullptr;

        DangerZoneVisualProfileRow const* fallback = nullptr;
        for (auto const& pair : itr->second)
        {
            DangerZoneVisualProfileRow const* row = std::any_cast<DangerZoneVisualProfileRow>(&pair.second);
            if (!row || !row->modelPath || !row->modelPath[0])
                continue;

            if ((row->shapeMask & SHAPE_CIRCLE) == 0)
                continue;

            if ((row->schoolMask & int(spell.m_schoolMask)) == 0)
                continue;

            if ((row->flags & PROFILE_FLAG_REQUIRE_HARMFUL) != 0 && !harmful)
                continue;

            if (playerCaster)
            {
                if (row->debugSpellId != int(spellId) || (row->flags & PROFILE_FLAG_DEBUG_PLAYER_CASTERS) == 0)
                    continue;
                return row;
            }

            if ((row->flags & PROFILE_FLAG_CREATURE_CASTERS) == 0)
                continue;

            if (!fallback || row->debugSpellId == 0)
                fallback = row;
        }

        return fallback;
    }

    bool ReadLocation(ClientPacket* packet, C3Vector& out)
    {
        uint64_t transportGuid = 0;
        ReadGuid(packet, transportGuid);
        ReadFloat(packet, out.x);
        ReadFloat(packet, out.y);
        ReadFloat(packet, out.z);
        return true;
    }

    bool ReadTargetData(ClientPacket* packet, TargetData& target)
    {
        ReadU32(packet, target.flags);

        if (target.flags & (TARGET_FLAG_UNIT_MASK | TARGET_FLAG_GAMEOBJECT_MASK | TARGET_FLAG_CORPSE_MASK))
        {
            ReadGuid(packet, target.unitGuid);
            target.hasUnit = target.unitGuid != 0;
        }

        if (target.flags & TARGET_FLAG_ITEM_MASK)
            ReadGuid(packet, target.itemGuid);

        if (target.flags & TARGET_FLAG_SOURCE_LOCATION)
            target.hasSrc = ReadLocation(packet, target.src);

        if (target.flags & TARGET_FLAG_DEST_LOCATION)
            target.hasDst = ReadLocation(packet, target.dst);

        if (target.flags & TARGET_FLAG_STRING)
            SkipCString(packet);

        return true;
    }

    bool ParseCastPacket(ClientPacket* packet, uint32_t opcode, CastPacket& out)
    {
        if (!packet)
            return false;

        uint32_t const savedRead = packet->m_read;
        out.opcode = opcode;

        ReadGuid(packet, out.casterGuid);
        ReadGuid(packet, out.casterUnitGuid);
        ReadU8(packet, out.castId);
        ReadU32(packet, out.spellId);
        ReadU32(packet, out.castFlags);
        ReadU32(packet, out.castTime);

        if (opcode == SMSG_SPELL_GO)
        {
            uint8_t hitCount = 0;
            ReadU8(packet, hitCount);
            for (uint8_t i = 0; i < hitCount; ++i)
            {
                uint64_t ignored = 0;
                ReadU64(packet, ignored);
            }

            uint8_t missCount = 0;
            ReadU8(packet, missCount);
            for (uint8_t i = 0; i < missCount; ++i)
            {
                uint64_t ignored = 0;
                uint8_t reason = 0;
                ReadU64(packet, ignored);
                ReadU8(packet, reason);
                if (reason == 11)
                    ReadU8(packet, reason);
            }
        }

        ReadTargetData(packet, out.target);

        if (out.castFlags & CAST_FLAG_POWER_LEFT_SELF)
        {
            uint32_t ignored = 0;
            ReadU32(packet, ignored);
        }

        if (out.castFlags & CAST_FLAG_RUNE_LIST)
        {
            uint8_t start = 0;
            uint8_t count = 0;
            ReadU8(packet, start);
            ReadU8(packet, count);
            for (uint8_t i = 0; i < count; ++i)
                ReadU8(packet, start);
        }

        if (out.castFlags & CAST_FLAG_ADJUST_MISSILE)
        {
            ReadFloat(packet, out.missilePitch);
            ReadU32(packet, out.missileTravelMs);
            out.hasMissileTrajectory = out.missileTravelMs > 0;
        }

        packet->m_read = savedRead;
        return out.spellId != 0 && out.casterGuid != 0;
    }

    C44Matrix MakeMatrix(C3Vector const& position, float radius, DangerZoneInstance const& instance)
    {
        float scale = radius * instance.radiusScale / (std::max)(0.01f, instance.modelRadius);
        scale = (std::max)(0.01f, scale);
        return {
            scale, 0.0f, 0.0f, 0.0f,
            0.0f, scale, 0.0f, 0.0f,
            0.0f, 0.0f, scale, 0.0f,
            position.x, position.y, position.z, 1.0f
        };
    }

    void UpdateRenderModel(DangerZoneInstance& instance)
    {
        if (!instance.renderModel.model && !instance.modelPath.empty())
        {
            void* scene = *reinterpret_cast<void**>(WORLD_SCENE_M2_SCENE);
            if (!scene)
                return;

            instance.renderModel.model = CM2Scene_CreateModel(scene, const_cast<char*>(instance.modelPath.c_str()), 0);
            if (!instance.renderModel.model)
                return;
        }

        if (!instance.renderModel.model)
            return;

        C44Matrix matrix = MakeMatrix(instance.position, instance.radius, instance);
        *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(instance.renderModel.model) + 16) |= 0x8000u;
        *reinterpret_cast<C44Matrix*>(reinterpret_cast<uintptr_t>(instance.renderModel.model) + 180) = matrix;

        if (!instance.renderModel.worldObject && CM2Model_IsLoaded(instance.renderModel.model, 0, 0))
            instance.renderModel.worldObject = RenderModelCreateWorldObject(instance.renderModel.model, reinterpret_cast<void*>(WorldObjectKeepAliveCallback), 0, 0, 0, nullptr, 6);

        if (instance.renderModel.worldObject)
        {
            float const boundsRadius = (std::max)(1.0f, instance.radius + 2.0f);
            BoundingBox bounds{ -boundsRadius, -boundsRadius, -1.0f, boundsRadius, boundsRadius, 1.0f };
            C3Vector sphere{ 0.0f, 0.0f, 0.0f };
            C3Vector localOffset{ 0.0f, 0.0f, 0.0f };
            World_ObjectUpdateRaw(instance.renderModel.worldObject, &matrix, &bounds, &sphere, &localOffset, 0, -1);
        }
    }

    void RemoveInstance(uint32_t instanceId)
    {
        auto itr = s_instances.find(instanceId);
        if (itr == s_instances.end())
            return;

        s_instanceByCasterSpell.erase(MakeCasterSpellKey(itr->second.casterGuid, itr->second.spellId));
        DestroyRenderModel(itr->second.renderModel);
        s_instances.erase(itr);
    }

    void SpawnOrExtend(CastPacket const& cast, SpellRow const& spell, DangerZoneVisualProfileRow const& profile, C3Vector const& position, float radius, uint32_t durationMs)
    {
        uint64_t const now = OsGetAsyncTimeMs();
        uint32_t const clampedDuration = std::clamp<uint32_t>(
            durationMs,
            uint32_t((std::max)(1, profile.minDurationMs)),
            uint32_t((std::max)(profile.minDurationMs, profile.maxDurationMs)));

        uint64_t const key = MakeCasterSpellKey(cast.casterUnitGuid ? cast.casterUnitGuid : cast.casterGuid, cast.spellId);
        auto existing = s_instanceByCasterSpell.find(key);
        if (existing != s_instanceByCasterSpell.end())
        {
            auto instanceItr = s_instances.find(existing->second);
            if (instanceItr != s_instances.end())
            {
                DangerZoneInstance& instance = instanceItr->second;
                GroundPosition(position, profile.zOffset, instance.position);
                instance.radius = radius;
                instance.endMs = (std::max)(instance.endMs, now + clampedDuration);
                return;
            }
        }

        DangerZoneInstance instance{};
        instance.instanceId = s_nextInstanceId++;
        if (!s_nextInstanceId)
            s_nextInstanceId = 1;
        instance.casterGuid = cast.casterUnitGuid ? cast.casterUnitGuid : cast.casterGuid;
        instance.spellId = cast.spellId;
        GroundPosition(position, profile.zOffset, instance.position);
        instance.radius = radius;
        instance.startMs = now;
        instance.endMs = now + clampedDuration;
        instance.modelPath = profile.modelPath;
        instance.modelRadius = profile.modelRadius > 0.0f ? profile.modelRadius : 5.0f;
        instance.radiusScale = profile.radiusScale > 0.0f ? profile.radiusScale : 1.0f;
        instance.zOffset = profile.zOffset;
        instance.red = profile.red;
        instance.green = profile.green;
        instance.blue = profile.blue;
        instance.alpha = profile.alpha;

        s_instanceByCasterSpell[key] = instance.instanceId;
        s_instances.emplace(instance.instanceId, std::move(instance));
    }

    uint32_t EstimateTravelMs(CastPacket const& cast, SpellRow const& spell, C3Vector const& position)
    {
        if (cast.hasMissileTrajectory)
            return cast.missileTravelMs;

        if (spell.m_speed <= 0.001f)
            return 0;

        C3Vector casterPos = UnitPosition(cast.casterUnitGuid ? cast.casterUnitGuid : cast.casterGuid, position);
        float const distance = Distance2D(casterPos, position);
        if (distance <= 0.01f)
            return 0;

        return uint32_t(distance / spell.m_speed * 1000.0f);
    }

    void ConsiderCast(CastPacket const& cast, bool isStart)
    {
        SpellRow spell{};
        if (!ClientDB::GetLocalizedRow(reinterpret_cast<WoWClientDB*>(SPELL_DB), cast.spellId, &spell))
            return;

        bool const creatureCaster = IsCreatureGuid(cast.casterUnitGuid ? cast.casterUnitGuid : cast.casterGuid);
        bool const playerCaster = IsPlayerGuid(cast.casterUnitGuid ? cast.casterUnitGuid : cast.casterGuid);
        bool const harmful = HasHarmfulEffect(spell);
        DangerZoneVisualProfileRow const* profile = FindProfile(spell, cast.spellId, playerCaster, harmful);
        if (!profile)
            return;

        if (!creatureCaster && !(playerCaster && profile->debugSpellId == int(cast.spellId)))
            return;

        if (isStart && (profile->flags & PROFILE_FLAG_SHOW_CAST) == 0)
            return;

        if (!isStart && (profile->flags & PROFILE_FLAG_SHOW_TRAVEL) == 0)
            return;

        float radius = RadiusForSpell(spell);
        if (radius <= 0.01f)
            return;

        C3Vector position{};
        if (!ResolveDangerPosition(cast, spell, position))
            return;

        uint32_t durationMs = isStart ? cast.castTime : EstimateTravelMs(cast, spell, position);
        if (durationMs == 0)
            return;

        SpawnOrExtend(cast, spell, *profile, position, radius, durationMs);
    }

    void ConsiderChannel(uint64_t casterGuid, uint32_t spellId, uint32_t durationMs)
    {
        if (!casterGuid || !spellId)
            return;

        if (durationMs == 0)
        {
            auto existing = s_instanceByCasterSpell.find(MakeCasterSpellKey(casterGuid, spellId));
            if (existing != s_instanceByCasterSpell.end())
                RemoveInstance(existing->second);
            return;
        }

        SpellRow spell{};
        if (!ClientDB::GetLocalizedRow(reinterpret_cast<WoWClientDB*>(SPELL_DB), spellId, &spell))
            return;

        bool const creatureCaster = IsCreatureGuid(casterGuid);
        bool const playerCaster = IsPlayerGuid(casterGuid);
        bool const harmful = HasHarmfulEffect(spell);
        DangerZoneVisualProfileRow const* profile = FindProfile(spell, spellId, playerCaster, harmful);
        if (!profile || (profile->flags & PROFILE_FLAG_SHOW_CHANNEL) == 0)
            return;

        if (!creatureCaster && !(playerCaster && profile->debugSpellId == int(spellId)))
            return;

        float radius = RadiusForSpell(spell);
        if (radius <= 0.01f)
            return;

        CastPacket cast{};
        cast.casterGuid = casterGuid;
        cast.casterUnitGuid = casterGuid;
        cast.spellId = spellId;

        C3Vector position = UnitPosition(casterGuid, {});
        SpawnOrExtend(cast, spell, *profile, position, radius, durationMs);
    }

    void HandleSpellPacket(uint32_t opcode, ClientPacket* packet)
    {
        CastPacket cast{};
        if (!ParseCastPacket(packet, opcode, cast))
            return;

        ConsiderCast(cast, opcode == SMSG_SPELL_START);
    }

    void HandleChannelStart(ClientPacket* packet)
    {
        if (!packet)
            return;

        uint32_t const savedRead = packet->m_read;
        uint64_t casterGuid = 0;
        uint32_t spellId = 0;
        uint32_t durationMs = 0;
        ReadGuid(packet, casterGuid);
        ReadU32(packet, spellId);
        ReadU32(packet, durationMs);
        packet->m_read = savedRead;
        ConsiderChannel(casterGuid, spellId, durationMs);
    }

    void HandleChannelUpdate(ClientPacket* packet)
    {
        if (!packet)
            return;

        uint32_t const savedRead = packet->m_read;
        uint64_t casterGuid = 0;
        uint32_t durationMs = 0;
        ReadGuid(packet, casterGuid);
        ReadU32(packet, durationMs);
        packet->m_read = savedRead;

        CGUnit* unit = reinterpret_cast<CGUnit*>(ClntObjMgr::ObjectPtr(casterGuid, TYPEMASK_UNIT | TYPEMASK_PLAYER));
        if (!unit)
            return;

        uint32_t spellId = unit->currentChannelId;
        if (!spellId)
            spellId = unit->unitData ? unit->unitData->channelSpell : 0;

        ConsiderChannel(casterGuid, spellId, durationMs);
    }

    void UpdateInstances()
    {
        uint64_t const now = OsGetAsyncTimeMs();
        std::vector<uint32_t> expired;

        for (auto& pair : s_instances)
        {
            DangerZoneInstance& instance = pair.second;
            if (now >= instance.endMs)
            {
                expired.push_back(pair.first);
                continue;
            }

            UpdateRenderModel(instance);
        }

        for (uint32_t instanceId : expired)
            RemoveInstance(instanceId);
    }
}

CLIENT_DETOUR(DangerZoneSpellCastPacket, 0x80FEE0, __cdecl, int, (void* param, uint32_t opcode, int timestamp, DangerZoneVisualsInternal::ClientPacket* packet))
{
    DangerZoneVisualsInternal::HandleSpellPacket(opcode, packet);
    return DangerZoneSpellCastPacket(param, opcode, timestamp, packet);
}

CLIENT_DETOUR(DangerZoneChannelStartPacket, 0x801C90, __cdecl, int, (void* param, uint32_t opcode, int timestamp, DangerZoneVisualsInternal::ClientPacket* packet))
{
    DangerZoneVisualsInternal::HandleChannelStart(packet);
    return DangerZoneChannelStartPacket(param, opcode, timestamp, packet);
}

CLIENT_DETOUR(DangerZoneChannelUpdatePacket, 0x801DB0, __cdecl, int, (void* param, uint32_t opcode, int timestamp, DangerZoneVisualsInternal::ClientPacket* packet))
{
    DangerZoneVisualsInternal::HandleChannelUpdate(packet);
    return DangerZoneChannelUpdatePacket(param, opcode, timestamp, packet);
}

void DangerZoneVisuals::Apply()
{
}

void DangerZoneVisuals::Pump()
{
    DangerZoneVisualsInternal::UpdateInstances();
}
