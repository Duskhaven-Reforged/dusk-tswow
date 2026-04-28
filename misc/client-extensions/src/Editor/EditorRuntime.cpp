#include <Editor/EditorRuntime.h>

#include <ClientData/Draw.h>
#include <Editor/EditorState.h>
#include <ClientData/Event.h>
#include <ClientData/GameClient.h>
#include <ClientData/GameObject.h>
#include <Editor/GizmoDraw.h>
#include <Editor/GizmoPick.h>
#include <ClientData/GxDevice.h>
#include <ClientData/ObjectManager.h>
#include <ClientData/VectorMath.h>
#include <ClientData/WorldFrame.h>
#include <ClientDetours.h>
#include <CustomLua/Housing/QuatFunctions.h>

#include <cstdint>

namespace EditorRuntime
{
    using namespace ClientData;
    using namespace EState;
    using namespace GPick;

    namespace
    {
        constexpr float kTranslationGizmoScale = 1.0f;
        constexpr float kRotationGizmoScale = 1.3f;

        EditorState& State()
        {
            static EditorState state;
            return state;
        }

        bool& Enabled()
        {
            static bool enabled = false;
            return enabled;
        }

        bool& EventsRegistered()
        {
            static bool registered = false;
            return registered;
        }

        CGGameObject_C* GameObjectByGuid(uint64_t guid)
        {
            return AsClientGameObject(ObjectManager::GetObject(guid, TYPEMASK_OBJECT));
        }

        CGGameObject_C* SelectedGameObject()
        {
            EditorState& state = State();
            if (state.currentObjectGuid == 0)
                return nullptr;

            return GameObjectByGuid(state.currentObjectGuid);
        }

        bool UpdateMouseRay(EventDataMouse const* data)
        {
            if (!data)
                return false;

            CGWorldFrameFull* worldFrame = CGWorldFrameFull::Current();
            if (!worldFrame || !worldFrame->m_camera)
                return false;

            float ddcX = 0.0f;
            float ddcY = 0.0f;
            GameClient::NDCToDDC(data->x, data->y, &ddcX, &ddcY);
            worldFrame->MouseToWorld(ddcX, ddcY, &State().start, &State().end);
            return true;
        }

        void ClearSelection()
        {
            EditorState& state = State();
            state.currentObjectGuid = 0;
            state.gizmoTranslationAxis = Axis::None;
            state.gizmoRotationAxis = Axis::None;
            state.gizmoDragState = {};
        }

        int32_t OnMouseDown(const void* rawData, void*)
        {
            if (!Enabled())
                return 1;

            auto const* data = static_cast<EventDataMouse const*>(rawData);
            if (!data || data->button != MOUSE_BUTTON_LEFT)
                return 1;
            if (!UpdateMouseRay(data))
                return 1;

            EditorState& state = State();
            CGGameObject_C* gameObject = SelectedGameObject();

            if (gameObject && state.gizmoTranslationAxis != Axis::None)
            {
                C3Vector axisDir = AxisDirection(state.gizmoTranslationAxis);
                float t = ClosestRayAxisParameter(state.start, state.end, state.gizmoPosition, axisDir);

                state.gizmoDragState.active = true;
                state.gizmoDragState.isRotation = false;
                state.gizmoDragState.axis = state.gizmoTranslationAxis;
                state.gizmoDragState.startObjPos = state.gizmoPosition;
                state.gizmoDragState.anchorOnAxis = VectorMath::Add(state.gizmoPosition, VectorMath::Scale(axisDir, t));
                return 0;
            }

            if (gameObject && state.gizmoRotationAxis != Axis::None)
            {
                C3Vector axis{};
                C3Vector refDir{};
                C3Vector refPerp{};
                RingBasis(state.gizmoRotationAxis, axis, refDir, refPerp);

                float angle = 0.0f;
                if (!RayAngleOnRing(state.start, state.end, state.gizmoPosition, axis, refDir, refPerp, angle))
                    return 1;

                state.gizmoDragState.active = true;
                state.gizmoDragState.isRotation = true;
                state.gizmoDragState.axis = state.gizmoRotationAxis;
                state.gizmoDragState.startObjPos = state.gizmoPosition;
                state.gizmoDragState.anchorAngle = angle;
                state.gizmoDragState.ringAxis = axis;
                state.gizmoDragState.ringRefDir = refDir;
                state.gizmoDragState.ringRefPerp = refPerp;
                state.gizmoDragState.startRot = unpack_quat(gameObject->m_passenger.compressedRotation);
                return 0;
            }

            CGWorldFrameFull* worldFrame = CGWorldFrameFull::Current();
            if (!worldFrame || worldFrame->currentGuid == 0)
            {
                ClearSelection();
                return 1;
            }

            gameObject = GameObjectByGuid(worldFrame->currentGuid);
            if (!gameObject)
            {
                ClearSelection();
                return 1;
            }

            state.currentObjectGuid = worldFrame->currentGuid;
            state.gizmoPosition = gameObject->m_passenger.position;
            return 1;
        }

        int32_t OnMouseUp(const void* rawData, void*)
        {
            auto const* data = static_cast<EventDataMouse const*>(rawData);
            if (!data || data->button != MOUSE_BUTTON_LEFT)
                return 1;

            EditorState& state = State();
            state.gizmoDragState.active = false;
            state.gizmoDragState.axis = Axis::None;
            return 1;
        }

        int32_t OnMouseMove(const void* rawData, void*)
        {
            if (!Enabled())
                return 1;

            auto const* data = static_cast<EventDataMouse const*>(rawData);
            if (!UpdateMouseRay(data))
                return 1;

            EditorState& state = State();
            CGGameObject_C* gameObject = SelectedGameObject();
            if (!gameObject)
            {
                state.gizmoTranslationAxis = Axis::None;
                state.gizmoRotationAxis = Axis::None;
                state.gizmoDragState.active = false;
                return 1;
            }

            if (state.gizmoDragState.active)
            {
                if (!state.gizmoDragState.isRotation)
                {
                    C3Vector axisDir = AxisDirection(state.gizmoDragState.axis);
                    float t =
                        ClosestRayAxisParameter(state.start, state.end, state.gizmoDragState.startObjPos, axisDir);
                    C3Vector currentOnAxis =
                        VectorMath::Add(state.gizmoDragState.startObjPos, VectorMath::Scale(axisDir, t));
                    C3Vector delta = VectorMath::Subtract(currentOnAxis, state.gizmoDragState.anchorOnAxis);
                    state.gizmoPosition = VectorMath::Add(state.gizmoDragState.startObjPos, delta);

                    gameObject->m_passenger.position = state.gizmoPosition;
                    gameObject->UpdateWorldObject(0);
                    return 1;
                }

                float currentAngle = 0.0f;
                if (!RayAngleOnRing(state.start, state.end, state.gizmoDragState.startObjPos,
                                    state.gizmoDragState.ringAxis, state.gizmoDragState.ringRefDir,
                                    state.gizmoDragState.ringRefPerp, currentAngle))
                    return 1;

                float delta = NormalizeSignedAngleDelta(currentAngle - state.gizmoDragState.anchorAngle);
                Quat deltaRot = axis_angle_to_quat(state.gizmoDragState.ringAxis.x, state.gizmoDragState.ringAxis.y,
                                                   state.gizmoDragState.ringAxis.z, delta);
                Quat finalRot = multiply_quat(deltaRot, state.gizmoDragState.startRot);
                gameObject->m_passenger.compressedRotation = pack_quat(finalRot);
                gameObject->UpdateWorldObject(0);
                return 1;
            }

            state.gizmoPosition = gameObject->m_passenger.position;
            state.gizmoTranslationAxis =
                PickTranslationGizmo(state.start, state.end, state.gizmoPosition, kTranslationGizmoScale);
            state.gizmoRotationAxis = PickRotationGizmo(state.start, state.end, state.gizmoPosition, kRotationGizmoScale);
            return 1;
        }
    }

    void Apply()
    {
        //Temporarily disabled editor. This makes the gizmo show up
        return;
        Enabled() = true;
        if (GameClient::IsInitialized())
            OnGameClientInitialize();
    }

    void OnGameClientInitialize()
    {
        if (!Enabled() || EventsRegistered())
            return;

        EventRegisterEx(EVENT_ID_MOUSEDOWN, OnMouseDown, nullptr, 10.0f);
        EventRegisterEx(EVENT_ID_MOUSEUP, OnMouseUp, nullptr, 10.0f);
        EventRegisterEx(EVENT_ID_MOUSEMOVE, OnMouseMove, nullptr, 10.0f);
        EventsRegistered() = true;
    }

    void OnGameClientDestroy()
    {
        if (EventsRegistered())
        {
            EventUnregister(EVENT_ID_MOUSEDOWN, OnMouseDown);
            EventUnregister(EVENT_ID_MOUSEUP, OnMouseUp);
            EventUnregister(EVENT_ID_MOUSEMOVE, OnMouseMove);
            EventsRegistered() = false;
        }

        State() = {};
    }

    void OnWorldRender(CGWorldFrameFull* worldFrame)
    {
        EditorState& state = State();
        if (!Enabled() || !worldFrame || !worldFrame->m_camera || state.currentObjectGuid == 0)
            return;

        CGGameObject_C* gameObject = SelectedGameObject();
        if (!gameObject)
            return;
        if (!state.gizmoDragState.active)
            state.gizmoPosition = gameObject->m_passenger.position;

        CGxDevice* device = CGxDevice::Get();
        if (!device)
            return;

        device->Push();
        GxRsSet(GxRs_VertexShader, nullptr);
        GxRsSet(GxRs_PixelShader, nullptr);
        GxRsSet_int32_t(GxRs_Culling, 0);
        GxRsSet_int32_t(GxRs_Fog, 0);
        GxRsSet_int32_t(GxRs_Lighting, 0);

        CImVector transparent{};
        GxSceneClear(2, transparent);
        GxRsSet_int32_t(GxRs_DepthTest, 1);

        C3Vector drawPosition = VectorMath::Subtract(state.gizmoPosition, worldFrame->m_camera->m_position);
        GizmoDraw::DrawTranslationGizmo(drawPosition, kTranslationGizmoScale, state.gizmoTranslationAxis);
        GizmoDraw::DrawRotationGizmo(drawPosition, kRotationGizmoScale, state.gizmoRotationAxis);

        device->Pop();
    }
} // namespace EditorRuntime

namespace
{
    using namespace ClientData;

    void __cdecl ClientInitializeGame_EditorRuntimeDetour(int32_t a1, float a2, float a3, float a4)
    {
        GameClient::InitializeGame(a1, a2, a3, a4);
        EditorRuntime::OnGameClientInitialize();
    }

    void __cdecl ClientDestroyGame_EditorRuntimeDetour(bool a1, bool a2, bool a3)
    {
        if (GameClient::IsInitialized())
            EditorRuntime::OnGameClientDestroy();

        GameClient::DestroyGame(a1, a2, a3);
    }

    bool __fastcall CGWorldFrame_OnWorldRender_EditorRuntimeDetour(CGWorldFrameFull* worldFrame, void*)
    {
        bool result = CGWorldFrame_OnWorldRender(worldFrame);
        EditorRuntime::OnWorldRender(worldFrame);
        return result;
    }

    int ClientInitializeGame_EditorRuntimeResult =
        ClientDetours::Add("ClientData::GameClient::InitializeGame", &GameClient::InitializeGame,
                           ClientInitializeGame_EditorRuntimeDetour, __FILE__, __LINE__);

    int ClientDestroyGame_EditorRuntimeResult =
        ClientDetours::Add("ClientData::GameClient::DestroyGame", &GameClient::DestroyGame,
                           ClientDestroyGame_EditorRuntimeDetour, __FILE__, __LINE__);

    int CGWorldFrame_OnWorldRender_EditorRuntimeResult =
        ClientDetours::Add("ClientData::CGWorldFrame_OnWorldRender", &CGWorldFrame_OnWorldRender,
                           CGWorldFrame_OnWorldRender_EditorRuntimeDetour, __FILE__, __LINE__);
}
