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

#include "TranslationManipulators.h"

#include <AzCore/Math/VectorConversions.h>
#include <AzToolsFramework/Manipulators/ManipulatorView.h>

namespace AzToolsFramework
{
    static const float s_surfaceManipulatorTransparency = 0.75f;
    static const float s_axisLength = 2.0f;
    static const float s_surfaceManipulatorRadius = 0.1f;

    static const AZ::Color s_xAxisColor = AZ::Color(1.0f, 0.0f, 0.0f, 1.0f);
    static const AZ::Color s_yAxisColor = AZ::Color(0.0f, 1.0f, 0.0f, 1.0f);
    static const AZ::Color s_zAxisColor = AZ::Color(0.0f, 0.0f, 1.0f, 1.0f);
    static const AZ::Color s_surfaceManipulatorColor = AZ::Color(1.0f, 1.0f, 0.0f, 0.5f);

    TranslationManipulators::TranslationManipulators(
        const Dimensions dimensions, const AZ::Transform& worldFromLocal)
        : m_dimensions(dimensions)
    {
        switch (dimensions)
        {
        case Dimensions::Two:
            m_linearManipulators.reserve(2);
            for (size_t manipulatorIndex = 0; manipulatorIndex < 2; ++manipulatorIndex)
            {
                m_linearManipulators.emplace_back(LinearManipulator::MakeShared(worldFromLocal));
            }
            m_planarManipulators.emplace_back(PlanarManipulator::MakeShared(worldFromLocal));
            break;
        case Dimensions::Three:
            m_linearManipulators.reserve(3);
            m_planarManipulators.reserve(3);
            for (size_t manipulatorIndex = 0; manipulatorIndex < 3; ++manipulatorIndex)
            {
                m_linearManipulators.emplace_back(LinearManipulator::MakeShared(worldFromLocal));
                m_planarManipulators.emplace_back(PlanarManipulator::MakeShared(worldFromLocal));
            }
            m_surfaceManipulator = SurfaceManipulator::MakeShared(worldFromLocal);
            break;
        default:
            AZ_Assert(false, "Invalid dimensions provided");
            break;
        }
    }

    void TranslationManipulators::InstallLinearManipulatorMouseDownCallback(
        const LinearManipulator::MouseActionCallback& onMouseDownCallback)
    {
        for (AZStd::shared_ptr<LinearManipulator>& manipulator : m_linearManipulators)
        {
            manipulator->InstallLeftMouseDownCallback(onMouseDownCallback);
        }
    }

    void TranslationManipulators::InstallLinearManipulatorMouseMoveCallback(
        const LinearManipulator::MouseActionCallback& onMouseMoveCallback)
    {
        for (AZStd::shared_ptr<LinearManipulator>& manipulator : m_linearManipulators)
        {
            manipulator->InstallMouseMoveCallback(onMouseMoveCallback);
        }
    }

    void TranslationManipulators::InstallLinearManipulatorMouseUpCallback(
        const LinearManipulator::MouseActionCallback& onMouseUpCallback)
    {
        for (AZStd::shared_ptr<LinearManipulator>& manipulator : m_linearManipulators)
        {
            manipulator->InstallLeftMouseUpCallback(onMouseUpCallback);
        }
    }

    void TranslationManipulators::InstallPlanarManipulatorMouseDownCallback(
        const PlanarManipulator::MouseActionCallback& onMouseDownCallback)
    {
        for (AZStd::shared_ptr<PlanarManipulator>& manipulator : m_planarManipulators)
        {
            manipulator->InstallLeftMouseDownCallback(onMouseDownCallback);
        }
    }

    void TranslationManipulators::InstallPlanarManipulatorMouseMoveCallback(
        const PlanarManipulator::MouseActionCallback& onMouseMoveCallback)
    {
        for (AZStd::shared_ptr<PlanarManipulator>& manipulator : m_planarManipulators)
        {
            manipulator->InstallMouseMoveCallback(onMouseMoveCallback);
        }
    }

    void TranslationManipulators::InstallPlanarManipulatorMouseUpCallback(
        const PlanarManipulator::MouseActionCallback& onMouseUpCallback)
    {
        for (AZStd::shared_ptr<PlanarManipulator>& manipulator : m_planarManipulators)
        {
            manipulator->InstallLeftMouseUpCallback(onMouseUpCallback);
        }
    }

    void TranslationManipulators::InstallSurfaceManipulatorMouseDownCallback(
        const SurfaceManipulator::MouseActionCallback& onMouseDownCallback)
    {
        if (m_surfaceManipulator)
        {
            m_surfaceManipulator->InstallLeftMouseDownCallback(onMouseDownCallback);
        }
    }

    void TranslationManipulators::InstallSurfaceManipulatorMouseUpCallback(
        const SurfaceManipulator::MouseActionCallback& onMouseUpCallback)
    {
        if (m_surfaceManipulator)
        {
            m_surfaceManipulator->InstallLeftMouseUpCallback(onMouseUpCallback);
        }
    }

    void TranslationManipulators::InstallSurfaceManipulatorMouseMoveCallback(
        const SurfaceManipulator::MouseActionCallback& onMouseMoveCallback)
    {
        if (m_surfaceManipulator)
        {
            m_surfaceManipulator->InstallMouseMoveCallback(onMouseMoveCallback);
        }
    }

    void TranslationManipulators::SetLocalTransform(const AZ::Transform& localTransform)
    {
        for (AZStd::shared_ptr<LinearManipulator>& manipulator : m_linearManipulators)
        {
            manipulator->SetLocalTransform(localTransform);
        }

        for (AZStd::shared_ptr<PlanarManipulator>& manipulator : m_planarManipulators)
        {
            manipulator->SetLocalTransform(localTransform);
        }

        if (m_surfaceManipulator)
        {
            m_surfaceManipulator->SetPosition(localTransform.GetTranslation());
        }

        m_localTransform = localTransform;
    }

    void TranslationManipulators::SetLocalPosition(const AZ::Vector3& localPosition)
    {
        for (AZStd::shared_ptr<LinearManipulator>& manipulator : m_linearManipulators)
        {
            manipulator->SetLocalPosition(localPosition);
        }

        for (AZStd::shared_ptr<PlanarManipulator>& manipulator : m_planarManipulators)
        {
            manipulator->SetLocalPosition(localPosition);
        }

        if (m_surfaceManipulator)
        {
            m_surfaceManipulator->SetPosition(localPosition);
        }

        m_localTransform.SetPosition(localPosition);
    }

    void TranslationManipulators::SetLocalOrientation(const AZ::Quaternion& localOrientation)
    {
        for (AZStd::shared_ptr<LinearManipulator>& manipulator : m_linearManipulators)
        {
            manipulator->SetLocalOrientation(localOrientation);
        }

        for (AZStd::shared_ptr<PlanarManipulator>& manipulator : m_planarManipulators)
        {
            manipulator->SetLocalOrientation(localOrientation);
        }

        m_localTransform = AZ::Transform::CreateFromQuaternionAndTranslation(
            localOrientation, m_localTransform.GetTranslation());
    }

    void TranslationManipulators::SetSpace(const AZ::Transform& worldFromLocal)
    {
        for (AZStd::shared_ptr<LinearManipulator>& manipulator : m_linearManipulators)
        {
            manipulator->SetSpace(worldFromLocal);
        }

        for (AZStd::shared_ptr<PlanarManipulator>& manipulator : m_planarManipulators)
        {
            manipulator->SetSpace(worldFromLocal);
        }

        if (m_surfaceManipulator)
        {
            m_surfaceManipulator->SetSpace(worldFromLocal);
        }
    }

    void TranslationManipulators::SetAxes(
        const AZ::Vector3& axis1, const AZ::Vector3& axis2, const AZ::Vector3& axis3 /*= AZ::Vector3::CreateAxisZ()*/)
    {
        AZ::Vector3 axes[] = { axis1, axis2, axis3 };

        for (size_t manipulatorIndex = 0; manipulatorIndex < m_linearManipulators.size(); ++manipulatorIndex)
        {
            m_linearManipulators[manipulatorIndex]->SetAxis(axes[manipulatorIndex]);
        }

        for (size_t manipulatorIndex = 0; manipulatorIndex < m_planarManipulators.size(); ++manipulatorIndex)
        {
            m_planarManipulators[manipulatorIndex]->SetAxes(axes[manipulatorIndex], axes[(manipulatorIndex + 1) % 3]);
        }
    }

    void TranslationManipulators::ConfigureLinearView(
        float axisLength, const AZ::Color& axis1Color, const AZ::Color& axis2Color,
        const AZ::Color& axis3Color /*= AZ::Color(0.0f, 0.0f, 1.0f, 0.5f)*/)
    {
        const float coneLength = 0.28f;
        const float coneRadius = 0.07f;
        const float lineWidth = 0.05f;

        const AZ::Color axesColor[] = { axis1Color, axis2Color, axis3Color };

        const auto configureLinearView = [lineWidth, coneLength, axisLength, coneRadius](
            LinearManipulator* linearManipulator, const AZ::Color& color)
        {
            ManipulatorViews views;
            views.emplace_back(CreateManipulatorViewLine(
                *linearManipulator, color, axisLength, lineWidth));
            views.emplace_back(CreateManipulatorViewCone(
                *linearManipulator, color, linearManipulator->GetAxis() * (axisLength - coneLength),
                coneLength, coneRadius));
            linearManipulator->SetViews(AZStd::move(views));
        };

        for (size_t manipulatorIndex = 0; manipulatorIndex < m_linearManipulators.size(); ++manipulatorIndex)
        {
            configureLinearView(m_linearManipulators[manipulatorIndex].get(), axesColor[manipulatorIndex]);
        }
    }

    void TranslationManipulators::ConfigurePlanarView(
        const AZ::Color& plane1Color, const AZ::Color& plane2Color /*= AZ::Color(0.0f, 1.0f, 0.0f, 0.5f)*/,
        const AZ::Color& plane3Color /*= AZ::Color(0.0f, 0.0f, 1.0f, 0.5f)*/)
    {
        const float planeSize = 0.6f;
        const AZ::Color planesColor[] = { plane1Color, plane2Color, plane3Color };

        for (size_t manipulatorIndex = 0; manipulatorIndex < m_planarManipulators.size(); ++manipulatorIndex)
        {
            m_planarManipulators[manipulatorIndex]->SetView(
                CreateManipulatorViewQuad(
                    *m_planarManipulators[manipulatorIndex],planesColor[manipulatorIndex],
                    planesColor[(manipulatorIndex + 1) % 3],
                    planeSize)
            );
        }
    }

    void TranslationManipulators::ConfigureSurfaceView(
        const float radius, const AZ::Color& color)
    {
        if (m_surfaceManipulator)
        {
            m_surfaceManipulator->SetView(CreateManipulatorViewSphere(color, radius,
                [](const ViewportInteraction::MouseInteraction& /*mouseInteraction*/,
                    bool mouseOver, const AZ::Color& defaultColor) -> AZ::Color
            {
                const AZ::Color color[2] =
                {
                    defaultColor,
                    Vector3ToVector4(
                        BaseManipulator::s_defaultMouseOverColor.GetAsVector3(), s_surfaceManipulatorTransparency)
                };

                return color[mouseOver];
            }));
        }
    }

    void TranslationManipulators::ProcessManipulators(const AZStd::function<void(BaseManipulator*)>& manipulatorFn)
    {
        for (AZStd::shared_ptr<LinearManipulator>& manipulator : m_linearManipulators)
        {
            manipulatorFn(manipulator.get());
        }

        for (AZStd::shared_ptr<PlanarManipulator>& manipulator : m_planarManipulators)
        {
            manipulatorFn(manipulator.get());
        }

        if (m_surfaceManipulator)
        {
            manipulatorFn(m_surfaceManipulator.get());
        }
    }

    void ConfigureTranslationManipulatorAppearance3d(
        TranslationManipulators* translationManipulators)
    {
        translationManipulators->SetAxes(
            AZ::Vector3::CreateAxisX(),
            AZ::Vector3::CreateAxisY(),
            AZ::Vector3::CreateAxisZ());
        translationManipulators->ConfigurePlanarView(
            s_xAxisColor, s_yAxisColor, s_zAxisColor);
        translationManipulators->ConfigureLinearView(
            s_axisLength, s_xAxisColor, s_yAxisColor, s_zAxisColor);
        translationManipulators->ConfigureSurfaceView(
            s_surfaceManipulatorRadius, s_surfaceManipulatorColor);
    }

    void ConfigureTranslationManipulatorAppearance2d(
        TranslationManipulators* translationManipulators)
    {
        translationManipulators->SetAxes(
            AZ::Vector3::CreateAxisX(),
            AZ::Vector3::CreateAxisY());
        translationManipulators->ConfigurePlanarView(s_xAxisColor);
        translationManipulators->ConfigureLinearView(s_axisLength, s_xAxisColor, s_yAxisColor);
    }
} // namespace AzToolsFramework