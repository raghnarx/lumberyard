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

#include "LineSegmentSelectionManipulator.h"

#include <AzCore/Math/IntersectSegment.h>
#include <AzFramework/Entity/EntityDebugDisplayBus.h>
#include <AzToolsFramework/Manipulators/ManipulatorView.h>
#include <AzToolsFramework/Viewport/ViewportMessages.h>

namespace AzToolsFramework
{
    LineSegmentSelectionManipulator::Action CalculateManipulationDataAction(
        const AZ::Transform& worldFromLocal, const AZ::Vector3& rayOrigin, const AZ::Vector3& rayDirection,
        const float rayLength, const AZ::Vector3& localStart, const AZ::Vector3& localEnd)
    {
        AZ::Vector3 worldClosestPositionRay, worldClosestPositionLineSegment;
        AZ::VectorFloat rayProportion, lineSegmentProportion;
        AZ::Intersect::ClosestSegmentSegment(
            rayOrigin, rayOrigin + rayDirection * rayLength,
            worldFromLocal * localStart, worldFromLocal * localEnd,
            rayProportion, lineSegmentProportion, worldClosestPositionRay, worldClosestPositionLineSegment);

        AZ::Transform worldFromLocalNormalized = worldFromLocal;
        const AZ::Vector3 scale = worldFromLocalNormalized.ExtractScale();
        const AZ::Transform localFromWorldNormalized = worldFromLocalNormalized.GetInverseFast();

        return { localFromWorldNormalized * worldClosestPositionLineSegment * scale.GetReciprocal() };
    }

    LineSegmentSelectionManipulator::LineSegmentSelectionManipulator()
    {
        AttachLeftMouseDownImpl();
    }

    LineSegmentSelectionManipulator::~LineSegmentSelectionManipulator() {}

    void LineSegmentSelectionManipulator::InstallLeftMouseDownCallback(const MouseActionCallback& onMouseDownCallback)
    {
        m_onLeftMouseDownCallback = onMouseDownCallback;
    }

    void LineSegmentSelectionManipulator::InstallLeftMouseUpCallback(const MouseActionCallback& onMouseUpCallback)
    {
        m_onLeftMouseUpCallback = onMouseUpCallback;
    }

    void LineSegmentSelectionManipulator::OnLeftMouseDownImpl(
        const ViewportInteraction::MouseInteraction& interaction, float /*rayIntersectionDistance*/)
    {
        if (!interaction.m_keyboardModifiers.Ctrl())
        {
            return;
        }

        if (m_onLeftMouseDownCallback)
        {
            AzFramework::CameraState cameraState;
            ViewportInteraction::ViewportInteractionRequestBus::EventResult(
                cameraState, interaction.m_interactionId.m_viewportId,
                &ViewportInteraction::ViewportInteractionRequestBus::Events::GetCameraState);

            m_onLeftMouseDownCallback(CalculateManipulationDataAction(
                TransformUniformScale(m_worldFromLocal), interaction.m_mousePick.m_rayOrigin,
                interaction.m_mousePick.m_rayDirection, cameraState.m_farClip, m_localStart, m_localEnd));
        }
    }

    void LineSegmentSelectionManipulator::OnLeftMouseUpImpl(const ViewportInteraction::MouseInteraction& interaction)
    {
        if (MouseOver() && m_onLeftMouseUpCallback)
        {
            AzFramework::CameraState cameraState;
            ViewportInteraction::ViewportInteractionRequestBus::EventResult(
                cameraState, interaction.m_interactionId.m_viewportId,
                &ViewportInteraction::ViewportInteractionRequestBus::Events::GetCameraState);

            m_onLeftMouseUpCallback(CalculateManipulationDataAction(
                TransformUniformScale(m_worldFromLocal), interaction.m_mousePick.m_rayOrigin, interaction.m_mousePick.m_rayDirection,
                cameraState.m_farClip, m_localStart, m_localEnd));
        }
    }

    void LineSegmentSelectionManipulator::Draw(
        const ManipulatorManagerState& managerState,
        AzFramework::DebugDisplayRequests& debugDisplay,
        const AzFramework::CameraState& cameraState,
        const ViewportInteraction::MouseInteraction& mouseInteraction)
    {
        // if the ctrl modifier key state has changed - set out bounds to dirty and
        // update the active state.
        if (m_keyboardModifiers != mouseInteraction.m_keyboardModifiers)
        {
            SetBoundsDirty();
            m_keyboardModifiers = mouseInteraction.m_keyboardModifiers;
        }

        if (mouseInteraction.m_keyboardModifiers.Ctrl() && !mouseInteraction.m_keyboardModifiers.Shift())
        {
            m_manipulatorView->Draw(
                GetManipulatorManagerId(), managerState,
                GetManipulatorId(), {
                    TransformUniformScale(m_worldFromLocal),
                    m_localStart, MouseOver()
                },
                debugDisplay, cameraState, mouseInteraction);
        }
    }

    void LineSegmentSelectionManipulator::SetView(AZStd::unique_ptr<ManipulatorView>&& view)
    {
        m_manipulatorView = AZStd::move(view);
    }

    void LineSegmentSelectionManipulator::SetBoundsDirtyImpl()
    {
        m_manipulatorView->SetBoundDirty(GetManipulatorManagerId());
    }

    void LineSegmentSelectionManipulator::InvalidateImpl()
    {
        m_manipulatorView->Invalidate(GetManipulatorManagerId());
    }
}