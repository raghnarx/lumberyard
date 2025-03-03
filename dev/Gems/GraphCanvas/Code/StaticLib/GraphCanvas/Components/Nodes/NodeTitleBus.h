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

#include <AzCore/Component/EntityId.h>
#include <AzCore/EBus/EBus.h>
#include <AzCore/std/string/string.h>

#include <GraphCanvas/Types/TranslationTypes.h>
#include <GraphCanvas/Components/StyleBus.h>

class QGraphicsWidget;

namespace GraphCanvas
{
    static const AZ::Crc32 NodeTitleServiceCrc = AZ_CRC("GraphCanvas_TitleService", 0xfe6d63bc);

    //! NodeTitleRequests
    //! Requests that get/set an entity's Node Title
    //
    // Most of these pushes should become pulls to avoid needing to over expose information in this bus.
    // May also come up with a way changing up the tag type here so we can pull specific widgets from a generic bus
    // to improve the customization.
    class NodeTitleRequests
        : public AZ::EBusTraits
    {
    public:

        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::ById;
        using BusIdType = AZ::EntityId;

        virtual QGraphicsWidget* GetGraphicsWidget() = 0;

        //! Set the Node's title.
        virtual void SetTitle(const AZStd::string& value) = 0;

        virtual void SetTranslationKeyedTitle(const TranslationKeyedString& value) = 0;

        //! Get the Node's title.
        virtual AZStd::string GetTitle() const = 0;

        //! Set the Node's sub-title.
        virtual void SetSubTitle(const AZStd::string& value) = 0;

        virtual void SetTranslationKeyedSubTitle(const TranslationKeyedString& value) = 0;

        //! Get the Node's sub-title.
        virtual AZStd::string GetSubTitle() const = 0;

        //! Sets the base palette for the title. This won't be saved out.
        virtual void SetDefaultPalette(const AZStd::string& basePalette) = 0;

        //! Sets an override for the palette. This will be saved out.
        virtual void SetPaletteOverride(const AZStd::string& paletteOverride) = 0;
        virtual void SetDataPaletteOverride(const AZ::Uuid& uuid) = 0;
        virtual void SetColorPaletteOverride(const QColor& color) = 0;

        virtual void ConfigureIconConfiguration(PaletteIconConfiguration& paletteConfiguration) = 0;

        virtual void ClearPaletteOverride() = 0;
    };

    using NodeTitleRequestBus = AZ::EBus<NodeTitleRequests>;

    //! NodeTitleNotifications
    //! Notifications about changes to the state of a Node Title
    class NodeTitleNotifications
        : public AZ::EBusTraits
    {
    public:
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::ById;
        using BusIdType = AZ::EntityId;

        virtual void OnTitleChanged() = 0;
    };

    using NodeTitleNotificationsBus = AZ::EBus<NodeTitleNotifications>;
}