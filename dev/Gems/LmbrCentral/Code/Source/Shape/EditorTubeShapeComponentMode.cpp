/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/

#include "LmbrCentral_precompiled.h"
#include "EditorTubeShapeComponentMode.h"

#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <AzToolsFramework/Manipulators/LinearManipulator.h>
#include <AzToolsFramework/Manipulators/ManipulatorManager.h>
#include <AzToolsFramework/Viewport/ActionBus.h>
#include <LmbrCentral/Shape/SplineComponentBus.h>
#include <LmbrCentral/Shape/EditorTubeShapeComponentBus.h>
#include <LmbrCentral/Shape/TubeShapeComponentBus.h>

namespace LmbrCentral
{
    AZ_CLASS_ALLOCATOR_IMPL(EditorTubeShapeComponentMode, AZ::SystemAllocator, 0)

    static const float s_tubeManipulatorHandleSize = 0.05f;
    static const AZ::Color s_tubeManipulatorHandleColor = AZ::Color(0.06275f, 0.1647f, 0.1647f, 1.0f);

    static const AZ::Crc32 s_resetVariableRadii = AZ_CRC("com.amazon.action.tubeshape.reset_radii", 0x0f2ef8e2);
    static const char* const s_resetRadiiTitle = "Reset Radii";
    static const char* const s_resetRadiiDesc = "Reset all variable radius values to the default";

    EditorTubeShapeComponentMode::EditorTubeShapeComponentMode(
        const AZ::EntityComponentIdPair& entityComponentIdPair, const AZ::Uuid componentType)
        : EditorBaseComponentMode(entityComponentIdPair, componentType)
    {
        m_currentTransform = AZ::Transform::CreateIdentity();
        AZ::TransformBus::EventResult(
            m_currentTransform, entityComponentIdPair.GetEntityId(), &AZ::TransformBus::Events::GetWorldTM);

        AZ::TransformNotificationBus::Handler::BusConnect(entityComponentIdPair.GetEntityId());
        ShapeComponentNotificationsBus::Handler::BusConnect(entityComponentIdPair.GetEntityId());
        SplineComponentNotificationBus::Handler::BusConnect(entityComponentIdPair.GetEntityId());
        EditorSplineComponentNotificationBus::Handler::BusConnect(entityComponentIdPair.GetEntityId());

        CreateManipulators();
    }

    EditorTubeShapeComponentMode::~EditorTubeShapeComponentMode()
    {
        DestroyManipulators();

        EditorSplineComponentNotificationBus::Handler::BusDisconnect();
        SplineComponentNotificationBus::Handler::BusDisconnect();
        ShapeComponentNotificationsBus::Handler::BusDisconnect();
        AZ::TransformNotificationBus::Handler::BusDisconnect();
    }

    void EditorTubeShapeComponentMode::Refresh()
    {
        ContainerChanged();
    }

    AZStd::vector<AzToolsFramework::ActionOverride> EditorTubeShapeComponentMode::PopulateActionsImpl()
    {
        return AZStd::vector<AzToolsFramework::ActionOverride>
        {
            AzToolsFramework::ActionOverride()
                .SetUri(s_resetVariableRadii)
                .SetKeySequence(QKeySequence(Qt::Key_R))
                .SetTitle(s_resetRadiiTitle)
                .SetTip(s_resetRadiiDesc)
                .SetEntityComponentIdPair(AZ::EntityComponentIdPair(GetEntityId(), GetComponentId()))
                .SetCallback([this]()
            {
                const AZ::EntityId entityId = GetEntityId();

                // ensure we record undo command for reset
                AzToolsFramework::ScopedUndoBatch undoBatch("Reset variable radii");
                AzToolsFramework::ScopedUndoBatch::MarkEntityDirty(entityId);

                TubeShapeComponentRequestsBus::Event(
                    entityId, &TubeShapeComponentRequests::SetAllVariableRadii, 0.0f);

                RefreshManipulatorsLocal(entityId);

                EditorTubeShapeComponentRequestBus::Event(
                    entityId, &EditorTubeShapeComponentRequests::GenerateVertices);

                // ensure property grid values are refreshed
                AzToolsFramework::ToolsApplicationNotificationBus::Broadcast(
                    &AzToolsFramework::ToolsApplicationNotificationBus::Events::InvalidatePropertyDisplay,
                    AzToolsFramework::Refresh_Values);
            })
        };
    }

    void EditorTubeShapeComponentMode::CreateManipulators()
    {
        const AZ::EntityId entityId = GetEntityId();

        bool empty = false;
        SplineComponentRequestBus::EventResult(
            empty, entityId, &SplineComponentRequests::Empty);

        // if we have no vertices, do not attempt to create any manipulators
        if (empty)
        {
            return;
        }

        AZ::SplinePtr spline;
        SplineComponentRequestBus::EventResult(
            spline, entityId, &SplineComponentRequests::GetSpline);

        const auto tubeManipulatorStates = GenerateTubeManipulatorStates(*spline);

        for (size_t manipulatorIndex = 0; manipulatorIndex < tubeManipulatorStates.size(); ++manipulatorIndex)
        {
            const TubeManipulatorState tubeManipulatorState = tubeManipulatorStates[manipulatorIndex];

            const AZ::Vector3 normal = spline->GetNormal(tubeManipulatorState.m_splineAddress);
            const AZ::Vector3 position = spline->GetPosition(tubeManipulatorState.m_splineAddress);

            float radius = 0.0f;
            TubeShapeComponentRequestsBus::EventResult(
                radius, entityId, &TubeShapeComponentRequests::GetTotalRadius, tubeManipulatorState.m_splineAddress);

            auto linearManipulator = AzToolsFramework::LinearManipulator::MakeShared(m_currentTransform);
            linearManipulator->AddEntityId(entityId);
            linearManipulator->SetLocalTransform(AZ::Transform::CreateTranslation(position + normal * radius));
            linearManipulator->SetAxis(normal);

            AzToolsFramework::ManipulatorViews views;
            views.emplace_back(AzToolsFramework::CreateManipulatorViewQuadBillboard(
                s_tubeManipulatorHandleColor, s_tubeManipulatorHandleSize));
            linearManipulator->SetViews(AZStd::move(views));
            linearManipulator->Register(AzToolsFramework::g_mainManipulatorManagerId);

            struct SharedState
            {
                float m_startingVariableRadius = 0.0f;
                float m_startingFixedRadius = 0.0f;
            };

            auto sharedState = AZStd::make_shared<SharedState>();
            linearManipulator->InstallLeftMouseDownCallback(
                [this, tubeManipulatorState, sharedState]
                (const AzToolsFramework::LinearManipulator::Action& /*action*/) mutable
            {
                const AZ::EntityId entityId = GetEntityId();

                float variableRadius = 0.0f;
                TubeShapeComponentRequestsBus::EventResult(
                    variableRadius, entityId,
                    &TubeShapeComponentRequests::GetVariableRadius, tubeManipulatorState.m_vertIndex);

                // the base radius of the tube (when no variable radii are applied)
                float fixedRadius = 0.0f;
                TubeShapeComponentRequestsBus::EventResult(
                    fixedRadius, entityId, &TubeShapeComponentRequests::GetRadius);

                sharedState->m_startingVariableRadius = variableRadius;
                sharedState->m_startingFixedRadius = fixedRadius;
            });

            linearManipulator->InstallMouseMoveCallback(
                [this, tubeManipulatorState, manipulatorIndex, spline, sharedState]
                (const AzToolsFramework::LinearManipulator::Action& action)
            {
                const AZ::EntityId entityId = GetEntityId();

                const AZ::VectorFloat axisDisplacement = action.LocalPositionOffset().Dot(action.m_fixed.m_axis);

                // set clamped variable radius, it can be no more than the inverse
                // of the base/fixed radius of the tube
                TubeShapeComponentRequestsBus::Event(
                    entityId, &TubeShapeComponentRequests::SetVariableRadius, tubeManipulatorState.m_vertIndex,
                    (sharedState->m_startingVariableRadius + axisDisplacement).GetMax(-sharedState->m_startingFixedRadius));

                bool found = false;
                AZ::Vector3 localVertexPosition = AZ::Vector3::CreateZero();
                SplineComponentRequestBus::EventResult(
                    found, entityId, &SplineComponentRequests::GetVertex, tubeManipulatorState.m_vertIndex, localVertexPosition);

                const AZ::Vector3 localNormal = spline->GetNormal(tubeManipulatorState.m_splineAddress);
                const AZ::Vector3 manipulatorVector = action.LocalPosition() - localVertexPosition;
                const AZ::VectorFloat manipulatorDot = manipulatorVector.Dot(localNormal);

                // ensure the manipulator position cannot move passed
                // the center point of a tube vertex
                const AZ::Vector3 localPosition = manipulatorDot.IsGreaterEqualThan(AZ::VectorFloat::CreateZero())
                    ? action.LocalPosition()
                    : localVertexPosition;

                m_radiusManipulators[manipulatorIndex]->SetLocalTransform(
                    AZ::Transform::CreateTranslation(localPosition));
                m_radiusManipulators[manipulatorIndex]->SetBoundsDirty();

                EditorTubeShapeComponentRequestBus::Event(
                    entityId, &EditorTubeShapeComponentRequests::GenerateVertices);

                // ensure property grid values are refreshed
                AzToolsFramework::ToolsApplicationNotificationBus::Broadcast(
                    &AzToolsFramework::ToolsApplicationNotificationBus::Events::InvalidatePropertyDisplay,
                    AzToolsFramework::Refresh_Values);
            });

            m_radiusManipulators.emplace_back(std::move(linearManipulator));
        }
    }

    void EditorTubeShapeComponentMode::DestroyManipulators()
    {
        for (auto& linearManipulator : m_radiusManipulators)
        {
            if (linearManipulator)
            {
                linearManipulator->Unregister();
                linearManipulator.reset();
            }
        }

        m_radiusManipulators.clear();
    }

    void EditorTubeShapeComponentMode::ContainerChanged()
    {
        DestroyManipulators();
        CreateManipulators();
    }

    void EditorTubeShapeComponentMode::OnVertexAdded(size_t /*index*/)
    {
        ContainerChanged();
    }

    void EditorTubeShapeComponentMode::OnVertexRemoved(size_t /*index*/)
    {
        ContainerChanged();
    }

    void EditorTubeShapeComponentMode::OnVerticesSet(const AZStd::vector<AZ::Vector3>& /*vertices*/)
    {
        ContainerChanged();
    }

    void EditorTubeShapeComponentMode::OnVerticesCleared()
    {
        ContainerChanged();
    }

    void EditorTubeShapeComponentMode::OnShapeChanged(const ShapeChangeReasons /*changeReason*/)
    {
        RefreshManipulatorsLocal(GetEntityId());
    }

    void EditorTubeShapeComponentMode::OnSplineChanged()
    {
        RefreshManipulatorsLocal(GetEntityId());
    }

    void EditorTubeShapeComponentMode::OnTransformChanged(
        const AZ::Transform& /*local*/, const AZ::Transform& world)
    {
        m_currentTransform = world;
    }

    void EditorTubeShapeComponentMode::OnSplineTypeChanged()
    {
        ContainerChanged();
    }

    void EditorTubeShapeComponentMode::OnOpenCloseChanged(const bool /*closed*/)
    {
        ContainerChanged();
    }

    AZStd::vector<EditorTubeShapeComponentMode::TubeManipulatorState> EditorTubeShapeComponentMode::GenerateTubeManipulatorStates(
        const AZ::Spline& spline)
    {
        const AZ::u64 startVertex = spline.GetAddressByFraction(0.0f).m_segmentIndex;
        const AZ::u64 endVertex = startVertex + spline.GetSegmentCount() + (spline.IsClosed() ? 0 : 1);

        AZStd::vector<TubeManipulatorState> splineAddresses;
        for (AZ::u64 vertIndex = startVertex; vertIndex < endVertex; ++vertIndex)
        {
            if (vertIndex + 1 == endVertex)
            {
                splineAddresses.push_back({ AZ::SplineAddress(vertIndex - 1, 1.0f), vertIndex });
            }
            else
            {
                splineAddresses.push_back({ AZ::SplineAddress(vertIndex), vertIndex });
            }
        }

        return splineAddresses;
    }

    void EditorTubeShapeComponentMode::RefreshManipulatorsLocal(const AZ::EntityId entityId)
    {
        AZ::SplinePtr spline;
        SplineComponentRequestBus::EventResult(
            spline, entityId, &SplineComponentRequests::GetSpline);

        const auto tubeManipulatorStates = GenerateTubeManipulatorStates(*spline);
        AZ_Assert(tubeManipulatorStates.size() == m_radiusManipulators.size(),
            "tubeManipulatorStates.size() (%zu) does not equal m_radiusManipulators.size() (%zu), it's likely CreateManipulators has not been called",
            tubeManipulatorStates.size(), m_radiusManipulators.size());

        for (size_t manipulatorIndex = 0; manipulatorIndex < tubeManipulatorStates.size(); ++manipulatorIndex)
        {
            const TubeManipulatorState& tubeManipulatorState = tubeManipulatorStates[manipulatorIndex];

            const AZ::Vector3 normal = spline->GetNormal(tubeManipulatorState.m_splineAddress);
            const AZ::Vector3 position = spline->GetPosition(tubeManipulatorState.m_splineAddress);

            float radius = 0.0f;
            TubeShapeComponentRequestsBus::EventResult(
                radius, entityId, &TubeShapeComponentRequests::GetTotalRadius, tubeManipulatorState.m_splineAddress);

            m_radiusManipulators[manipulatorIndex]->SetLocalTransform(
                AZ::Transform::CreateTranslation(position + normal * radius));
            m_radiusManipulators[manipulatorIndex]->SetAxis(normal);
            m_radiusManipulators[manipulatorIndex]->SetBoundsDirty();
        }
    }
} // namespace LmbrCentral