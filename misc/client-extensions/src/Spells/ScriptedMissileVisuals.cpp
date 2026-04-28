#include <Spells/ScriptedMissileVisuals.h>

#include <CDBCMgr/CDBCDefs/ScriptedMissileMotion.h>
#include <ClientDetours.h>
#include <ClientNetwork.h>
#include <CustomPacketRead.h>
#include <Logger.h>
#include <SharedDefines.h>
#include <Spells/DangerZoneVisuals.h>
#include <Spells/ScriptedMissileVisuals.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

namespace ScriptedMissileVisualsInternal
{
    constexpr opcode_t SCRIPTED_MISSILE_VISUAL_SPAWN_OPCODE = 0x7A20;
    constexpr opcode_t SCRIPTED_MISSILE_VISUAL_DESPAWN_OPCODE = 0x7A21;
    constexpr opcode_t SCRIPTED_MISSILE_VISUAL_STOP_OPCODE = 0x7A22;
    constexpr opcode_t SCRIPTED_MISSILE_CLIENT_FACING_OPCODE = 0x7A23;
    constexpr float TWO_PI = 6.2831853071795864769f;
    constexpr uint32_t FLAG_ANCHOR_TO_CASTER = 0x01;
    constexpr uint32_t FLAG_REVERSE_ODD = 0x02;
    constexpr uint32_t FLAG_USE_CASTER_ORIENTATION = 0x80000000u;
    constexpr uint32_t MISSILE_INDEX_ALL = 0xFFFFFFFFu;
    constexpr uint32_t MAX_VISUAL_MISSILES = 256;
    constexpr uintptr_t WORLD_SCENE_M2_SCENE = 0x00CD754C;

    enum class MotionType : uint32_t
    {
        Line = 0,
        Orbit = 1,
        Spiral = 2,
        SineWave = 3,
        Homing = 4,
    };

    enum class AttachMode : int32_t
    {
        None = 0,
        CasterPosition = 1,
        CasterAttachment = 2,
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

    CLIENT_FUNCTION(CM2Scene_CreateModel, 0x81F8F0, __thiscall, void*, (void* scene, void* filename, int flags))
    CLIENT_FUNCTION(CM2Model_Release, 0x824ED0, __thiscall, int, (void* model))
    CLIENT_FUNCTION(CM2Model_IsLoaded, 0x824F00, __thiscall, bool, (void* model, int, int))
    CLIENT_FUNCTION(CM2Model_GetAttachmentWorldTransform, 0x831410, __thiscall, C44Matrix*, (void* model, C44Matrix* matrix, uint32_t attachmentId))
    CLIENT_FUNCTION(RenderModelCreateWorldObject, 0x781A10, __cdecl, void*, (void* model, void* callback, int a3, int a4, int a5, void* owner, uint32_t flags))
    CLIENT_FUNCTION(World_ObjectUpdateRaw, 0x780240, __cdecl, void, (void* worldObject, C44Matrix* matrix, BoundingBox* bounds, C3Vector* sphere, C3Vector* localOffset, int a6, int a7))
    CLIENT_FUNCTION(World_ObjectDestroy, 0x7826E0, __cdecl, int, (void* worldObject))
    CLIENT_FUNCTION(WorldObjectKeepAliveCallback, 0x4F5910, __cdecl, bool, (int a1, int a2, int a3, int a4, void* owner))
    struct RenderModel
    {
        void* model = nullptr;
        void* worldObject = nullptr;

        void Destroy()
        {
            if (worldObject)
            {
                World_ObjectDestroy(worldObject);
                worldObject = nullptr;
            }

            if (model)
            {
                CM2Model_Release(model);
                model = nullptr;
            }
        }
    };

    struct VisualInstance
    {
        uint32_t instanceId = 0;
        MotionType motion = MotionType::Line;
        uint32_t count = 1;
        uint32_t durationMs = 15000;
        uint32_t flags = 0;
        uint32_t visualFlags = 0;
        AttachMode attachMode = AttachMode::None;
        int32_t attachPoint = 0;
        uint64_t casterGuid = 0;
        uint64_t targetGuid = 0;
        C3Vector origin{};
        float originO = 0.0f;
        C3Vector dest{};
        float direction = 0.0f;
        float radius = 0.0f;
        float radiusVelocity = 0.0f;
        float startDistance = 0.0f;
        float height = 1.25f;
        float verticalVelocity = 0.0f;
        float forwardSpeed = 0.0f;
        float angularSpeed = 0.0f;
        float sineAmplitude = 0.0f;
        float sineFrequency = 1.0f;
        float visualScale = 1.0f;
        uint64_t startMs = 0;
        std::string modelPath;
        std::vector<RenderModel> models;
        std::vector<bool> active;
        std::vector<bool> stopped;
        std::vector<C3Vector> stoppedPosition;
    };

    std::unordered_map<uint32_t, VisualInstance> s_instances;

    bool s_insideVisualPump = false;
    uint64_t s_lastVisualUpdateMs = 0;
    uint64_t s_lastFacingUpdateMs = 0;
    float s_lastSentFacing = 0.0f;
    bool s_hasLastSentFacing = false;

    C3Vector Add(C3Vector const& a, C3Vector const& b)
    {
        return { a.x + b.x, a.y + b.y, a.z + b.z };
    }

    C3Vector Sub(C3Vector const& a, C3Vector const& b)
    {
        return { a.x - b.x, a.y - b.y, a.z - b.z };
    }

    C3Vector Scale(C3Vector const& v, float scale)
    {
        return { v.x * scale, v.y * scale, v.z * scale };
    }

    float Length2D(C3Vector const& v)
    {
        return std::sqrt(v.x * v.x + v.y * v.y);
    }

    float AngleTo(C3Vector const& from, C3Vector const& to)
    {
        return std::atan2(to.y - from.y, to.x - from.x);
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

    bool UnitOrientation(uint64_t guid, float& out)
    {
        if (!guid)
            return false;

        CGUnit* unit = reinterpret_cast<CGUnit*>(ClntObjMgr::ObjectPtr(guid, TYPEMASK_UNIT | TYPEMASK_PLAYER));
        if (!unit || !unit->movementInfo)
            return false;

        if (guid == ClntObjMgr::GetActivePlayer())
        {
            out = unit->movementInfo->orientation;
            return true;
        }

        out = static_cast<float>(CGUnit_C::GetFacing(unit));
        return true;
    }

    bool UnitAttachmentPosition(uint64_t guid, uint32_t attachmentId, C3Vector& out)
    {
        CGUnit* unit = reinterpret_cast<CGUnit*>(ClntObjMgr::ObjectPtr(guid, TYPEMASK_UNIT | TYPEMASK_PLAYER));
        if (!unit)
            return false;

        void* model = reinterpret_cast<void**>(unit)[45];
        if (!model || !CM2Model_IsLoaded(model, 0, 0))
            return false;

        C44Matrix matrix{};
        CM2Model_GetAttachmentWorldTransform(model, &matrix, attachmentId);
        out = { matrix.d0, matrix.d1, matrix.d2 };
        return true;
    }

    C3Vector BasePosition(VisualInstance const& instance)
    {
        if ((instance.flags & FLAG_ANCHOR_TO_CASTER) == 0)
            return instance.origin;

        if (instance.attachMode == AttachMode::CasterAttachment && instance.attachPoint >= 0)
        {
            C3Vector attached{};
            if (UnitAttachmentPosition(instance.casterGuid, uint32_t(instance.attachPoint), attached))
                return attached;
        }

        return UnitPosition(instance.casterGuid, instance.origin);
    }

    C3Vector Evaluate(VisualInstance const& instance, uint32_t missileIndex, float elapsedSeconds)
    {
        C3Vector center = BasePosition(instance);
        float const indexPhase = instance.count > 0 ? TWO_PI * float(missileIndex) / float(instance.count) : 0.0f;
        float const directionSign = ((instance.flags & FLAG_REVERSE_ODD) && (missileIndex & 1)) ? -1.0f : 1.0f;
        float const baseAngle = instance.direction + indexPhase;
        float const angle = baseAngle + instance.angularSpeed * elapsedSeconds * directionSign;
        float const radius = (std::max)(0.0f, instance.radius + instance.radiusVelocity * elapsedSeconds);
        float const forward = instance.startDistance + instance.forwardSpeed * elapsedSeconds;

        C3Vector pos = center;
        pos.z += instance.height + instance.verticalVelocity * elapsedSeconds;

        switch (instance.motion)
        {
            case MotionType::Line:
                pos.x += std::cos(baseAngle) * forward;
                pos.y += std::sin(baseAngle) * forward;
                break;
            case MotionType::Orbit:
                pos.x += std::cos(angle) * radius;
                pos.y += std::sin(angle) * radius;
                break;
            case MotionType::Spiral:
                pos.x += std::cos(baseAngle) * forward + std::cos(angle) * radius;
                pos.y += std::sin(baseAngle) * forward + std::sin(angle) * radius;
                break;
            case MotionType::SineWave:
            {
                float const right = baseAngle + 1.57079632679f;
                float const wave = std::sin(elapsedSeconds * instance.sineFrequency * TWO_PI + indexPhase) * instance.sineAmplitude;
                pos.x += std::cos(baseAngle) * forward + std::cos(right) * wave;
                pos.y += std::sin(baseAngle) * forward + std::sin(right) * wave;
                break;
            }
            case MotionType::Homing:
            {
                float const targetAngle = AngleTo(center, instance.dest);
                pos.x += std::cos(targetAngle) * forward;
                pos.y += std::sin(targetAngle) * forward;
                break;
            }
        }

        return pos;
    }

    C44Matrix MakeMatrix(C3Vector const& position, C3Vector const& previous, float visualScale)
    {
        C3Vector forward = Sub(position, previous);
        if (Length2D(forward) < 0.001f)
            forward = { 1.0f, 0.0f, 0.0f };
        else
        {
            float const inv = 1.0f / (std::max)(0.001f, Length2D(forward));
            forward.x *= inv;
            forward.y *= inv;
            forward.z = 0.0f;
        }

        C3Vector right = { -forward.y, forward.x, 0.0f };
        C3Vector up = { 0.0f, 0.0f, 1.0f };
        float const scale = (std::max)(0.01f, visualScale);

        return {
            forward.x * scale, forward.y * scale, forward.z * scale, 0.0f,
            right.x * scale, right.y * scale, right.z * scale, 0.0f,
            up.x * scale, up.y * scale, up.z * scale, 0.0f,
            position.x, position.y, position.z, 1.0f
        };
    }

    void UpdateRenderModel(RenderModel& renderModel, VisualInstance const& instance, C3Vector const& position, C3Vector const& previous)
    {
        if (!renderModel.model && !instance.modelPath.empty())
        {
            void* scene = *reinterpret_cast<void**>(WORLD_SCENE_M2_SCENE);
            if (!scene)
                return;

            renderModel.model = CM2Scene_CreateModel(scene, const_cast<char*>(instance.modelPath.c_str()), 0);
            if (!renderModel.model)
                return;
        }

        if (!renderModel.model)
            return;

        C44Matrix matrix = MakeMatrix(position, previous, instance.visualScale);
        *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(renderModel.model) + 16) |= 0x8000u;
        *reinterpret_cast<C44Matrix*>(reinterpret_cast<uintptr_t>(renderModel.model) + 180) = matrix;

        if (!renderModel.worldObject && CM2Model_IsLoaded(renderModel.model, 0, 0))
            renderModel.worldObject = RenderModelCreateWorldObject(renderModel.model, reinterpret_cast<void*>(WorldObjectKeepAliveCallback), 0, 0, 0, nullptr, 6);

        if (renderModel.worldObject)
        {
            BoundingBox bounds{ -0.6f, -0.6f, -0.6f, 0.6f, 0.6f, 0.6f };
            C3Vector sphere{ 0.0f, 0.0f, 0.0f };
            C3Vector localOffset{ 0.0f, 0.0f, 0.0f };
            World_ObjectUpdateRaw(renderModel.worldObject, &matrix, &bounds, &sphere, &localOffset, 0, -1);
        }
    }

    void RemoveInstance(uint32_t instanceId)
    {
        auto itr = s_instances.find(instanceId);
        if (itr == s_instances.end())
            return;

        for (RenderModel& model : itr->second.models)
            model.Destroy();
        s_instances.erase(itr);
    }

    void HandleSpawn(CustomPacketRead* packet)
    {
        if (!packet)
            return;

        VisualInstance instance{};
        instance.instanceId = packet->Read<uint32_t>(0);
        instance.motion = MotionType(std::min<uint32_t>(packet->Read<uint32_t>(0), uint32_t(MotionType::Homing)));
        instance.count = std::max<uint32_t>(packet->Read<uint32_t>(1), 1);
        instance.durationMs = std::max<uint32_t>(packet->Read<uint32_t>(1), 1);
        instance.flags = packet->Read<uint32_t>(0);
        instance.visualFlags = packet->Read<uint32_t>(0);
        instance.attachMode = AttachMode(packet->Read<int32_t>(0));
        instance.attachPoint = packet->Read<int32_t>(0);
        instance.casterGuid = packet->Read<uint64_t>(0);
        instance.targetGuid = packet->Read<uint64_t>(0);
        instance.origin.x = packet->Read<float>(0.0f);
        instance.origin.y = packet->Read<float>(0.0f);
        instance.origin.z = packet->Read<float>(0.0f);
        instance.originO = packet->Read<float>(0.0f);
        instance.dest.x = packet->Read<float>(0.0f);
        instance.dest.y = packet->Read<float>(0.0f);
        instance.dest.z = packet->Read<float>(0.0f);
        instance.direction = packet->Read<float>(0.0f);
        if ((instance.flags & FLAG_USE_CASTER_ORIENTATION) != 0)
        {
            float casterOrientation = 0.0f;
            if (UnitOrientation(instance.casterGuid, casterOrientation))
                instance.direction = casterOrientation;
        }
        instance.radius = packet->Read<float>(0.0f);
        instance.radiusVelocity = packet->Read<float>(0.0f);
        instance.startDistance = packet->Read<float>(0.0f);
        instance.height = packet->Read<float>(1.25f);
        instance.verticalVelocity = packet->Read<float>(0.0f);
        instance.forwardSpeed = packet->Read<float>(0.0f);
        instance.angularSpeed = packet->Read<float>(0.0f);
        instance.sineAmplitude = packet->Read<float>(0.0f);
        instance.sineFrequency = packet->Read<float>(1.0f);
        instance.visualScale = packet->Read<float>(1.0f);
        instance.modelPath = packet->ReadString("");

        if (!instance.instanceId || instance.modelPath.empty() || instance.count > MAX_VISUAL_MISSILES)
            return;

        RemoveInstance(instance.instanceId);
        instance.startMs = OsGetAsyncTimeMs();
        instance.models.resize(instance.count);
        instance.active.assign(instance.count, true);
        instance.stopped.assign(instance.count, false);
        instance.stoppedPosition.resize(instance.count);
        s_instances.emplace(instance.instanceId, std::move(instance));
    }

    void HandleDespawn(CustomPacketRead* packet)
    {
        if (!packet)
            return;

        uint32_t instanceId = packet->Read<uint32_t>(0);
        uint32_t missileIndex = packet->Read<uint32_t>(MISSILE_INDEX_ALL);

        if (missileIndex == MISSILE_INDEX_ALL)
        {
            RemoveInstance(instanceId);
            return;
        }

        auto itr = s_instances.find(instanceId);
        if (itr == s_instances.end() || missileIndex >= itr->second.models.size())
            return;

        itr->second.active[missileIndex] = false;
        itr->second.models[missileIndex].Destroy();
    }

    void HandleStop(CustomPacketRead* packet)
    {
        if (!packet)
            return;

        uint32_t instanceId = packet->Read<uint32_t>(0);
        uint32_t missileIndex = packet->Read<uint32_t>(MISSILE_INDEX_ALL);
        C3Vector position{};
        position.x = packet->Read<float>(0.0f);
        position.y = packet->Read<float>(0.0f);
        position.z = packet->Read<float>(0.0f);

        auto itr = s_instances.find(instanceId);
        if (itr == s_instances.end() || missileIndex >= itr->second.models.size())
            return;

        itr->second.active[missileIndex] = true;
        itr->second.stopped[missileIndex] = true;
        itr->second.stoppedPosition[missileIndex] = position;
    }

    void UpdateInstances()
    {
        uint64_t const now = OsGetAsyncTimeMs();
        std::vector<uint32_t> expired;

        for (auto& pair : s_instances)
        {
            VisualInstance& instance = pair.second;
            uint32_t const elapsedMs = uint32_t(now - instance.startMs);
            if (elapsedMs >= instance.durationMs)
            {
                expired.push_back(pair.first);
                continue;
            }

            float const elapsedSeconds = float(elapsedMs) / 1000.0f;
            for (uint32_t i = 0; i < instance.models.size(); ++i)
            {
                if (!instance.active[i])
                    continue;

                C3Vector position = instance.stopped[i] ? instance.stoppedPosition[i] : Evaluate(instance, i, elapsedSeconds);
                C3Vector previous = instance.stopped[i] ? instance.stoppedPosition[i] : Evaluate(instance, i, (std::max)(0.0f, elapsedSeconds - 0.016f));
                UpdateRenderModel(instance.models[i], instance, position, previous);
            }
        }

        for (uint32_t instanceId : expired)
            RemoveInstance(instanceId);
    }

    void PumpVisuals()
    {
        if (s_insideVisualPump)
            return;

        uint64_t const now = OsGetAsyncTimeMs();
        if (now == s_lastVisualUpdateMs)
            return;

        s_lastVisualUpdateMs = now;
        s_insideVisualPump = true;
        ScriptedMissileVisuals::SendCasterFacing(false);
        UpdateInstances();
        DangerZoneVisuals::Pump();
        s_insideVisualPump = false;
    }
}

CLIENT_DETOUR_THISCALL_NOARGS(WorldFrameOnWorldUpdateScriptedMissile, 0x4FA5F0, int)
{
    int result = WorldFrameOnWorldUpdateScriptedMissile(self);
    ScriptedMissileVisualsInternal::PumpVisuals();
    return result;
}

void ScriptedMissileVisuals::Apply()
{
    ClientNetwork::OnCustomPacket(ScriptedMissileVisualsInternal::SCRIPTED_MISSILE_VISUAL_SPAWN_OPCODE, [](CustomPacketRead* packet)
    {
        ScriptedMissileVisualsInternal::HandleSpawn(packet);
    });

    ClientNetwork::OnCustomPacket(ScriptedMissileVisualsInternal::SCRIPTED_MISSILE_VISUAL_DESPAWN_OPCODE, [](CustomPacketRead* packet)
    {
        ScriptedMissileVisualsInternal::HandleDespawn(packet);
    });

    ClientNetwork::OnCustomPacket(ScriptedMissileVisualsInternal::SCRIPTED_MISSILE_VISUAL_STOP_OPCODE, [](CustomPacketRead* packet)
    {
        ScriptedMissileVisualsInternal::HandleStop(packet);
    });
}

void ScriptedMissileVisuals::SendCasterFacing(bool force)
{
    CGPlayer* activePlayer = ClntObjMgr::GetActivePlayerObj();
    if (!activePlayer)
        return;

    uint64_t const now = OsGetAsyncTimeMs();
    float facing = 0.0f;
    if (!ScriptedMissileVisualsInternal::UnitOrientation(ClntObjMgr::GetActivePlayer(), facing) || !std::isfinite(facing))
        return;

    if (!force)
    {
        if (now - ScriptedMissileVisualsInternal::s_lastFacingUpdateMs < 100)
            return;

        float delta = std::fabs(facing - ScriptedMissileVisualsInternal::s_lastSentFacing);
        delta = (std::min)(delta, ScriptedMissileVisualsInternal::TWO_PI - delta);
        if (ScriptedMissileVisualsInternal::s_hasLastSentFacing && delta < 0.001f && now - ScriptedMissileVisualsInternal::s_lastFacingUpdateMs < 500)
            return;
    }

    ScriptedMissileVisualsInternal::s_lastFacingUpdateMs = now;
    ScriptedMissileVisualsInternal::s_lastSentFacing = facing;
    ScriptedMissileVisualsInternal::s_hasLastSentFacing = true;

    ClientNetwork::SendCustomPacket(ScriptedMissileVisualsInternal::SCRIPTED_MISSILE_CLIENT_FACING_OPCODE, 4, [facing](CustomPacketWrite& packet)
    {
        packet.Write<float>(facing);
    });
}
