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

#include <QColor>

#include <GraphCanvas/Components/Slots/Data/DataSlotBus.h>
#include <Components/Connections/ConnectionVisualComponent.h>

namespace GraphCanvas
{
    class DataConnectionGraphicsItem;

    class DataPinStyleMonitor
        : public StyleNotificationBus::MultiHandler
    {
    public:
        DataPinStyleMonitor(DataConnectionGraphicsItem& graphicsItem);

        // StyleNotificationBus
        void OnStyleChanged() override;
        ////

        void SetSourceId(const AZ::EntityId& sourceId);
        void SetTargetId(const AZ::EntityId& targetId);

    private:
        DataConnectionGraphicsItem& m_graphicsItem;

        AZ::EntityId m_sourceId;
        AZ::EntityId m_targetId;
    };

    class DataConnectionGraphicsItem
        : public ConnectionGraphicsItem
        , public RootGraphicsItemNotificationBus::Handler
        , public DataSlotNotificationBus::MultiHandler
    {
    public:	
        AZ_CLASS_ALLOCATOR(DataConnectionGraphicsItem, AZ::SystemAllocator, 0);
        
        DataConnectionGraphicsItem(const AZ::EntityId& connectionEntityId);
        ~DataConnectionGraphicsItem() override = default;

        // ConnectionGraphicsItem
        void OnStyleChanged() override;
        ////
        
        void OnSourceSlotIdChanged(const AZ::EntityId& oldSlotId, const AZ::EntityId& newSlotId) override;
        void OnTargetSlotIdChanged(const AZ::EntityId& oldSlotId, const AZ::EntityId& newSlotId) override;

        // DataSlotNotificationBus
        void OnDisplayTypeChanged(const AZ::Uuid& dataTypeId, const AZStd::vector<AZ::Uuid>& typeIds) override;
        ////

        void UpdateDataColors();

        // RootGraphicsItemNotifications
        void OnDisplayStateChanged(RootGraphicsItemDisplayState oldState, RootGraphicsItemDisplayState newState) override;
        ////

    protected:
        void UpdatePen() override;
        void OnPathChanged() override;
        
    private:
    
        void PopulateDataColor(QColor& targetColor, const AZ::EntityId& slotId);

        DataPinStyleMonitor m_dataPinStyleMonitor;
    
        QPen    m_pen;
        
        QColor m_sourceDataColor;
        QColor m_targetDataColor;
    };
}