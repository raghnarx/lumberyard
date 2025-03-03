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

#include <AzCore/EBus/EBus.h>
#include <AzCore/std/containers/unordered_map.h>

#include <GraphCanvas/Editor/EditorTypes.h>

namespace GraphCanvas
{
    class PersistentIdRequests : public AZ::EBusTraits
    {
    public:
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::ById;
        using BusIdType = PersistentGraphMemberId;

        virtual AZ::EntityId MapToEntityId() const = 0;
    };

    using PersistentIdRequestBus = AZ::EBus<PersistentIdRequests>;

    class PersistentIdNotifications : public AZ::EBusTraits
    {
    public:
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Multiple;
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::ById;
        using BusIdType = EditorId;

        virtual void OnPersistentIdsRemapped(const AZStd::unordered_map< PersistentGraphMemberId, PersistentGraphMemberId >& persistentIdRemapping) = 0;
    };

    using PersistentIdNotificationBus = AZ::EBus<PersistentIdNotifications>;
    
    class PersistentMemberRequests 
        : public AZ::EBusTraits
    {
    public:
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::ById;
        using BusIdType = AZ::EntityId;
        
        //! If the persistent graph member was remapped(such as during a copy). This will return the original
        //! value.
        virtual PersistentGraphMemberId GetPreviousGraphMemberId() const = 0;

        virtual PersistentGraphMemberId GetPersistentGraphMemberId() const = 0;
    };
    
    using PersistentMemberRequestBus = AZ::EBus<PersistentMemberRequests>;
}