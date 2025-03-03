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

#pragma once

#include <AzTest/AzTest.h>

namespace EMotionFX
{
    class SimulatedJoint;

    class SimulatedObject
    {
    public:
        AZ_TYPE_INFO(SimulatedObject, "{8CF0F474-69DC-4DE3-AF19-002F19DA27DB}");
        MOCK_CONST_METHOD1(FindSimulatedJointBySkeletonJointIndex, SimulatedJoint*(AZ::u32));

        MOCK_METHOD1(AddSimulatedJointAndChildren, void(AZ::u32));
        MOCK_METHOD1(AddSimulatedJoint, SimulatedJoint*(AZ::u32));

        MOCK_METHOD2(RemoveSimulatedJoint, void(AZ::u32, bool));
        MOCK_METHOD1(RemoveSimulatedJoint, void(AZ::u32));

        MOCK_CONST_METHOD1(SetSimulatedJoints, void(const AZStd::vector<SimulatedJoint*>& joints));
        MOCK_CONST_METHOD0(GetSimulatedJoints, const AZStd::vector<SimulatedJoint*>&());
        MOCK_CONST_METHOD1(GetSimulatedJoint, SimulatedJoint*(size_t index));

        MOCK_CONST_METHOD0(GetName, const AZStd::string&());
        MOCK_METHOD1(SetName, void(const AZStd::string&));
        MOCK_CONST_METHOD0(GetGravityFactor, float());
        MOCK_METHOD1(SetGravityFactor, void(float));
        MOCK_CONST_METHOD0(GetStiffnessFactor, float());
        MOCK_METHOD1(SetStiffnessFactor, void(float));
        MOCK_CONST_METHOD0(GetDampingFactor, float());
        MOCK_METHOD1(SetDampingFactor, void(float));
        MOCK_CONST_METHOD0(GetCollideWithJoints, AZStd::vector<AZStd::string>&());
        MOCK_METHOD1(SetCollideWithJoints, void(const AZStd::vector<AZStd::string>&));

        MOCK_METHOD0(Clear, void());
        MOCK_METHOD1(InitAfterLoading, void(SimulatedObjectSetup*));
    };
}
