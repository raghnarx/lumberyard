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
#include "precompiled.h"

#include <QGraphicsItem>
#include <QGraphicsLayoutItem>
#include <QGraphicsLinearLayout>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPointF>
#include <QGraphicsScene>

#include <Components/Slots/SlotConnectionPin.h>
#include <GraphCanvas/Components/SceneBus.h>
#include <GraphCanvas/Components/Slots/SlotBus.h>
#include <GraphCanvas/Editor/GraphCanvasProfiler.h>
#include <GraphCanvas/Styling/definitions.h>

namespace GraphCanvas
{
    //////////////////////
    // SlotConnectionPin
    //////////////////////

    SlotConnectionPin::SlotConnectionPin(const AZ::EntityId& slotId)
        : m_slotId(slotId)
        , m_connectionType(ConnectionType::CT_Invalid)
    {
        setFlags(ItemSendsScenePositionChanges);
        setZValue(1);
        setOwnedByLayout(true);
    }

    SlotConnectionPin::~SlotConnectionPin()
    {
    }

    void SlotConnectionPin::Activate()
    {
        SlotUIRequestBus::Handler::BusConnect(m_slotId);
        SlotNotificationBus::Handler::BusConnect(m_slotId);
    }

    void SlotConnectionPin::Deactivate()
    {
        SlotNotificationBus::Handler::BusDisconnect();
        SlotUIRequestBus::Handler::BusDisconnect();
    }

    void SlotConnectionPin::OnRegisteredToNode(const AZ::EntityId& nodeId)
    {
        StateController<RootGraphicsItemDisplayState>* stateController = nullptr;
        RootGraphicsItemRequestBus::EventResult(stateController, nodeId, &RootGraphicsItemRequests::GetDisplayStateStateController);

        m_nodeDisplayStateStateSetter.AddStateController(stateController);
        
        SlotRequestBus::EventResult(m_connectionType, GetEntityId(), &SlotRequests::GetConnectionType);
    }
	
	void SlotConnectionPin::RefreshStyle()
    {
        OnRefreshStyle();
        setCacheMode(QGraphicsItem::CacheMode::ItemCoordinateCache);
    }
	
	QPointF SlotConnectionPin::GetConnectionPoint() const
    {
        qreal padding = m_style.GetAttribute(Styling::Attribute::Padding, 2.0);
        QPointF localPoint = boundingRect().center();

        switch (m_connectionType)
        {
        case ConnectionType::CT_Input:
            localPoint.setX(boundingRect().left() + padding);
            break;
        case ConnectionType::CT_Output:
            localPoint.setX(boundingRect().right() - padding);
            break;
        default:
            break;
        }

        return mapToScene(localPoint);
    }

    QPointF SlotConnectionPin::GetJutDirection() const
    {
        switch (m_connectionType)
        {
        case ConnectionType::CT_Input:
            return QPointF(-1, 0);
        case ConnectionType::CT_Output:
            return QPointF(1, 0);
        default:
            return QPointF(0, 0);
        }
    }

    QRectF SlotConnectionPin::boundingRect() const
    {
        return{ {0, 0} , geometry().size() };
    }

    void SlotConnectionPin::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget /*= nullptr*/)
    {
        GRAPH_CANVAS_DETAILED_PROFILE_FUNCTION();
        qreal decorationPadding = m_style.GetAttribute(Styling::Attribute::Padding, 2.);
        qreal borderWidth = m_style.GetBorder().widthF();

        painter->setBrush(m_style.GetBrush(Styling::Attribute::BackgroundColor));

        // determine rect for drawing shape
        qreal margin = decorationPadding + (borderWidth * 0.5);
        QRectF drawRect = boundingRect().marginsRemoved({ margin, margin, margin, margin });

        bool hasConnections = false;
        SlotRequestBus::EventResult(hasConnections, GetEntityId(), &SlotRequests::HasConnections);

        DrawConnectionPin(painter, drawRect, hasConnections);
    }

    void SlotConnectionPin::keyPressEvent(QKeyEvent* keyEvent)
    {
        SlotLayoutItem::keyPressEvent(keyEvent);

        if (m_hovered)
        {
            if (keyEvent->key() == Qt::Key::Key_Alt)
            {
                SlotRequestBus::Event(m_slotId, &SlotRequests::SetConnectionDisplayState, RootGraphicsItemDisplayState::Deletion);
            }
        }
    }

    void SlotConnectionPin::keyReleaseEvent(QKeyEvent* keyEvent)
    {
        SlotLayoutItem::keyReleaseEvent(keyEvent);

        if (m_hovered)
        {
            if (keyEvent->key() == Qt::Key::Key_Alt)
            {
                SlotRequestBus::Event(m_slotId, &SlotRequests::SetConnectionDisplayState, RootGraphicsItemDisplayState::Inspection);
            }
        }
    }

    void SlotConnectionPin::hoverEnterEvent(QGraphicsSceneHoverEvent* hoverEvent)
    {
        m_hovered = true;
        grabKeyboard();

        if (hoverEvent->modifiers() & Qt::KeyboardModifier::AltModifier)
        {
            SlotRequestBus::Event(m_slotId, &SlotRequests::SetConnectionDisplayState, RootGraphicsItemDisplayState::Deletion);
        }
        else
        {
            SlotRequestBus::Event(m_slotId, &SlotRequests::SetConnectionDisplayState, RootGraphicsItemDisplayState::Inspection);
        }

        m_nodeDisplayStateStateSetter.SetState(RootGraphicsItemDisplayState::Inspection);
    }

    void SlotConnectionPin::hoverLeaveEvent(QGraphicsSceneHoverEvent* hoverEvent)
    {
        m_nodeDisplayStateStateSetter.ReleaseState();

        m_hovered = false;
        ungrabKeyboard();

        AZ::EntityId nodeId;
        SlotRequestBus::EventResult(nodeId, m_slotId, &SlotRequests::GetNode);

        SlotRequestBus::Event(m_slotId, &SlotRequests::ReleaseConnectionDisplayState);
    }

    void SlotConnectionPin::mousePressEvent(QGraphicsSceneMouseEvent* event)
    {
        if (event->button() == Qt::LeftButton && boundingRect().contains(event->pos()))
        {
            if (event->modifiers() & Qt::KeyboardModifier::AltModifier)
            {
                SlotRequestBus::Event(m_slotId, &SlotRequests::ClearConnections);
            }
            else
            {
                m_nodeDisplayStateStateSetter.ReleaseState();

                AZ::EntityId nodeId;
                SlotRequestBus::EventResult(nodeId, m_slotId, &SlotRequests::GetNode);

                AZ::EntityId sceneId;
                SceneMemberRequestBus::EventResult(sceneId, nodeId, &SceneMemberRequests::GetScene);

                SceneRequestBus::Event(sceneId, &SceneRequestBus::Events::ClearSelection);
                SlotRequestBus::Event(m_slotId, &SlotRequests::DisplayConnection);
            }

            return;
        }

        SlotLayoutItem::mousePressEvent(event);
    }

    void SlotConnectionPin::setGeometry(const QRectF& rect)
    {
        prepareGeometryChange();
        QGraphicsLayoutItem::setGeometry(rect);
        setPos(rect.topLeft());
        updateGeometry();
    }

    QSizeF SlotConnectionPin::sizeHint(Qt::SizeHint which, const QSizeF& constraint) const
    {
        const qreal decorationPadding = m_style.GetAttribute(Styling::Attribute::Padding, 2.);

        QSizeF rectSize = m_style.GetSize({ 8., 8. }) + QSizeF{ decorationPadding, decorationPadding } * 2.;

        switch (which)
        {
        case Qt::MinimumSize:
            return m_style.GetMinimumSize(rectSize);
        case Qt::PreferredSize:
            return rectSize;
        case Qt::MaximumSize:
            return m_style.GetMaximumSize();
        case Qt::MinimumDescent:
        default:
            return QSizeF();
        }
    }

    void SlotConnectionPin::DrawConnectionPin(QPainter* painter, QRectF drawRect, bool isConnected)
    {
        painter->setPen(m_style.GetBorder());

        if (isConnected)
        {
            painter->fillRect(drawRect, Qt::BrushStyle::SolidPattern);
        }
        else
        {
            painter->drawRect(drawRect);
        }
    }

    void SlotConnectionPin::OnRefreshStyle()
    {
        m_style.SetStyle(m_slotId, Styling::Elements::ConnectionPin);
    }
}