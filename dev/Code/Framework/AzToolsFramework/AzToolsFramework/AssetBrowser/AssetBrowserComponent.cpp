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

#include <AzCore/base.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/EBus/Results.h>
#include <AzCore/Asset/AssetManager.h>

#include <AzFramework/StringFunc/StringFunc.h>

#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <AzToolsFramework/AssetDatabase/AssetDatabaseConnection.h>
#include <AzToolsFramework/AssetBrowser/AssetBrowserComponent.h>
#include <AzToolsFramework/AssetBrowser/AssetBrowserModel.h>
#include <AzToolsFramework/AssetBrowser/Entries/RootAssetBrowserEntry.h>
#include <AzToolsFramework/AssetBrowser/AssetEntryChangeset.h>
#include <AzToolsFramework/AssetBrowser/Entries/AssetBrowserEntryCache.h>
#include <AzToolsFramework/Thumbnails/ThumbnailerBus.h>
#include <AzToolsFramework/AssetBrowser/Thumbnails/FolderThumbnail.h>
#include <AzToolsFramework/AssetBrowser/Thumbnails/SourceThumbnail.h>
#include <AzToolsFramework/AssetBrowser/Thumbnails/ProductThumbnail.h>
#include <AzToolsFramework/Slice/SliceUtilities.h>

#include <chrono>

#include <QApplication>
#include <QStyle>
#include <QSharedPointer>
#include <QDesktopServices>
#include <QUrl>
#include <QMenu>
#include <Asset/AssetProcessorMessages.h>

namespace AzToolsFramework
{
    namespace AssetBrowser
    {
        AssetBrowserComponent::AssetBrowserComponent()
            : m_databaseConnection(aznew AssetDatabase::AssetDatabaseConnection)
            , m_rootEntry(aznew RootAssetBrowserEntry)
            , m_dbReady(false)
            , m_waitingForMore(false)
            , m_disposed(false)
            , m_assetBrowserModel(aznew AssetBrowserModel)
            , m_changeset(new AssetEntryChangeset(m_databaseConnection, m_rootEntry))
        {
            m_assetBrowserModel->SetRootEntry(m_rootEntry);
        }

        AssetBrowserComponent::~AssetBrowserComponent() {}

        void AssetBrowserComponent::Activate()
        {
            m_disposed = false;
            m_waitingForMore = false;
            m_thread = AZStd::thread(AZStd::bind(&AssetBrowserComponent::UpdateAssets, this));

            AssetDatabaseLocationNotificationBus::Handler::BusConnect();
            AssetBrowserComponentRequestBus::Handler::BusConnect();
            AzFramework::AssetCatalogEventBus::Handler::BusConnect();
            AZ::TickBus::Handler::BusConnect();
            AssetSystemBus::Handler::BusConnect();
            AssetBrowserInteractionNotificationBus::Handler::BusConnect();

            using namespace Thumbnailer;
            const char* contextName = "AssetBrowser";
            int thumbnailSize = qApp->style()->pixelMetric(QStyle::PM_SmallIconSize);
            ThumbnailerRequestBus::Broadcast(&ThumbnailerRequests::RegisterContext, contextName, thumbnailSize);
            ThumbnailerRequestBus::Broadcast(&ThumbnailerRequests::RegisterThumbnailProvider, MAKE_TCACHE(FolderThumbnailCache), contextName);
            ThumbnailerRequestBus::Broadcast(&ThumbnailerRequests::RegisterThumbnailProvider, MAKE_TCACHE(SourceThumbnailCache), contextName);
            ThumbnailerRequestBus::Broadcast(&ThumbnailerRequests::RegisterThumbnailProvider, MAKE_TCACHE(ProductThumbnailCache), contextName);

            AzFramework::SocketConnection* socketConn = AzFramework::SocketConnection::GetInstance();
            AZ_Assert(socketConn, "AzToolsFramework::AssetBrowser::AssetBrowserComponent requires a valid socket conection!");
            if (socketConn)
            {
                m_cbHandle = socketConn->AddMessageHandler(AZ_CRC("FileProcessor::FileInfosNotification", 0x001c43f5),
                    [this](unsigned int /*typeId*/, unsigned int /*serial*/, const void* buffer, unsigned int bufferSize)
                {
                    HandleFileInfoNotification(buffer, bufferSize);
                });
            }
        }

        void AssetBrowserComponent::Deactivate()
        {
            m_disposed = true;
            NotifyUpdateThread();
            if (m_thread.joinable())
            {
                m_thread.join(); // wait for the thread to finish
                m_thread = AZStd::thread(); // destroy
            }
            AssetBrowserInteractionNotificationBus::Handler::BusDisconnect();
            AssetDatabaseLocationNotificationBus::Handler::BusDisconnect();
            AssetBrowserComponentRequestBus::Handler::BusDisconnect();
            AzFramework::AssetCatalogEventBus::Handler::BusDisconnect();
            AZ::TickBus::Handler::BusDisconnect();
            AssetSystemBus::Handler::BusDisconnect();
            EntryCache::DestroyInstance();
        }

        void AssetBrowserComponent::Reflect(AZ::ReflectContext* context)
        {
            AssetSystem::FileInfosNotificationMessage::Reflect(context);

            AZ::SerializeContext* serialize = azrtti_cast<AZ::SerializeContext*>(context);
            if (serialize)
            {
                serialize->Class<AssetBrowserComponent, AZ::Component>();
            }
        }

        void AssetBrowserComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType & services)
        {
            services.push_back(AZ_CRC("AssetBrowserService", 0x1e54fffb));
        }

        void AssetBrowserComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
        {
            required.push_back(AZ_CRC("ThumbnailerService", 0x65422b97));
        }

        void AssetBrowserComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
        {
            incompatible.push_back(AZ_CRC("AssetBrowserService", 0x1e54fffb));
        }

        void AssetBrowserComponent::OnDatabaseInitialized()
        {
            m_databaseConnection->OpenDatabase();
            PopulateAssets();
            m_dbReady = true;
        }

        AssetBrowserModel* AssetBrowserComponent::GetAssetBrowserModel()
        {
            return m_assetBrowserModel.get();
        }

        void AssetBrowserComponent::OnTick(float /*deltaTime*/, AZ::ScriptTimePoint /*time*/)
        {
            m_changeset->Synchronize();
        }

        // We listen to this bus so that a file that wasn't previously a 'source file' (just a file) can become a source file
        // for example, when a file first appears on disk, it will come in as just a file (with a file id).  Later, if its something
        // that the Asset Processor actually cares about and feeds to a builder, it gets assigned a UUID and this function is called.
        // we can then associate an existing file with a UUID.
        void AssetBrowserComponent::SourceFileChanged(AZStd::string /*relativePath*/, AZStd::string /*scanFolder*/, AZ::Uuid sourceUuid) 
        {
            m_changeset->AddSource(sourceUuid);
            if (m_dbReady)
            {
                NotifyUpdateThread();
            }
        }

        // this function handles the common built-in source file details for types built into the LMBRCENTRAL and AzToolsFramework libs
        // if you are writing a gem that produces new asset types and you'd like to set an icon, just listen to this bus and return
        // your own SourceFileDetails for your types of files in your gem!  Don't add your gem-embedded types here.
        // this is only here so that other applications (besides Editor.exe) may use AssetBrowser and see icons for types
        // that are either built into Cry DLLs, or into Editor Plugins. 
        SourceFileDetails AssetBrowserComponent::GetSourceFileDetails(const char* fullSourceFileName)
        {
            using namespace AzToolsFramework::AssetBrowser;
            AZStd::string extension;
            if (AzFramework::StringFunc::Path::GetExtension(fullSourceFileName, extension, true))
            {
                if (AzFramework::StringFunc::Equal(extension.c_str(), ".abc"))
                {
                    return SourceFileDetails("Editor/Icons/AssetBrowser/ABC_16.png");
                }

                if (AzFramework::StringFunc::Equal(extension.c_str(), ".bnk"))
                {
                    return SourceFileDetails("Editor/Icons/AssetBrowser/Audio_16.png");
                }

                if (AzFramework::StringFunc::Equal(extension.c_str(), ".cgf"))
                {
                    return SourceFileDetails("Editor/Icons/AssetBrowser/LegacyMesh_16.png");
                }

                if (AzFramework::StringFunc::Equal(extension.c_str(), ".font"))
                {
                    return SourceFileDetails("Editor/Icons/AssetBrowser/Font_16.png");
                }

                if (AzFramework::StringFunc::Equal(extension.c_str(), ".fontfamily"))
                {
                    return SourceFileDetails("Editor/Icons/AssetBrowser/Font_16.png");
                }

                if (AzFramework::StringFunc::Equal(extension.c_str(), ".i_caf"))
                {
                    return SourceFileDetails("Editor/Icons/AssetBrowser/LegacyAnimation_16.png");
                }

                if (AzFramework::StringFunc::Equal(extension.c_str(), ".inputbindings"))
                {
                    return SourceFileDetails("Editor/Icons/AssetBrowser/InputBindings_16.png");
                }

                if (AzFramework::StringFunc::Equal(extension.c_str(), ".lua"))
                {
                    return SourceFileDetails("Editor/Icons/AssetBrowser/Lua_16.png");
                }

                if (AzFramework::StringFunc::Equal(extension.c_str(), ".mtl"))
                {
                    return SourceFileDetails("Editor/Icons/AssetBrowser/Material_16.png");
                }

                if (AzFramework::StringFunc::Equal(extension.c_str(), AzToolsFramework::SliceUtilities::GetSliceFileExtension().c_str()))
                {
                    return SourceFileDetails("Editor/Icons/AssetBrowser/Slice_16.png");
                }

                if (AzFramework::StringFunc::Equal(extension.c_str(), ".skin"))
                {
                    return SourceFileDetails("Editor/Icons/AssetBrowser/LegacySkin_16.png");
                }

                if (AzFramework::StringFunc::Equal(extension.c_str(), ".ttf"))
                {
                    return SourceFileDetails("Editor/Icons/AssetBrowser/Font_16.png");
                }

                if (AzFramework::StringFunc::Equal(extension.c_str(), ".xml"))
                {
                    return SourceFileDetails("Editor/Icons/AssetBrowser/XML_16.png");
                }


                // this is here to prevent having to include IResourceCompilerHelper, which is in CryCommon.
                static const char* sourceFormats[] = { ".tif", ".bmp", ".gif", ".jpg", ".jpeg", ".jpe", ".tga", ".png" };

                for (unsigned int sourceImageFormatIndex = 0, numSources = AZ_ARRAY_SIZE(sourceFormats); sourceImageFormatIndex < numSources; ++sourceImageFormatIndex)
                {
                    const char* sourceFormatExtension = sourceFormats[sourceImageFormatIndex];
                    if (AzFramework::StringFunc::Equal(extension.c_str(), sourceFormatExtension))
                    {
                        return SourceFileDetails("Editor/Icons/AssetBrowser/Image_16.png");
                    }
                }
            }
            return SourceFileDetails();
        }


        void AssetBrowserComponent::AddFile(const AZ::s64& fileId) 
        {
            m_changeset->AddFile(fileId);
            if (m_dbReady)
            {
                NotifyUpdateThread();
            }
        }

        void AssetBrowserComponent::RemoveFile(const AZ::s64& fileId) 
        {
            m_changeset->RemoveFile(fileId);
            if (m_dbReady)
            {
                NotifyUpdateThread();
            }
        }

        void AssetBrowserComponent::OnCatalogAssetAdded(const AZ::Data::AssetId& assetId)
        {
            m_changeset->AddProduct(assetId);
            if (m_dbReady)
            {
                NotifyUpdateThread();
            }
        }

        // sometimes this happens when there is new info about a product, so treat it as an Add!
        // it can also happen when a source file disappears and then reappears very rapidly to the point where
        // certain jobs are able to run so quickly that the asset was never removed.
        void AssetBrowserComponent::OnCatalogAssetChanged(const AZ::Data::AssetId& assetId)
        {
            m_changeset->AddProduct(assetId);
            if (m_dbReady)
            {
                NotifyUpdateThread();
            }
        }

        void AssetBrowserComponent::OnCatalogAssetRemoved(const AZ::Data::AssetId& assetId)
        {
            m_changeset->RemoveProduct(assetId);
            if (m_dbReady)
            {
                NotifyUpdateThread();
            }
        }

        void AssetBrowserComponent::PopulateAssets()
        {
            m_changeset->PopulateEntries();
            NotifyUpdateThread();
        }

        void AssetBrowserComponent::UpdateAssets()
        {
            while (true)
            {
                m_updateWait.acquire();

                // kill thread if component is destroyed
                if (m_disposed)
                {
                    return;
                }

                // wait for db or more updates
                m_waitingForMore = true;
                AZStd::this_thread::sleep_for(AZStd::chrono::milliseconds(100));
                m_waitingForMore = false;

                if (m_dbReady)
                {
                    m_changeset->Update();
                }
            }
        }

        void AssetBrowserComponent::NotifyUpdateThread()
        {
            // do not release the sempahore again if query thread is waiting for more update requests
            // otherwise it would needlessly spin another turn
            if (!m_waitingForMore)
            {
                m_updateWait.release();
            }
        }

        void AssetBrowserComponent::HandleFileInfoNotification(const void* buffer, unsigned bufferSize)
        {
            AssetSystem::FileInfosNotificationMessage message;
            if (!AZ::Utils::LoadObjectFromBufferInPlace(buffer, bufferSize, message))
            {
                AZ_WarningOnce("AssetSystem", false, "Problem deserializing FileInfosNotificationMessage.  Discarded.\n");
                return;
            }

            switch (message.m_type)
            {
            case AssetSystem::FileInfosNotificationMessage::Synced:
                PopulateAssets();
                break;
            case AssetSystem::FileInfosNotificationMessage::FileAdded:
                AddFile(message.m_fileID);
                break;
            case AssetSystem::FileInfosNotificationMessage::FileRemoved:
                RemoveFile(message.m_fileID);
                break;
            default:
                AZ_WarningOnce("AssetSystem", false, "Unknown FileInfosNotificationMessage type");
                break;
            }
        }
    } // namespace AssetBrowser
} // namespace AzToolsFramework
#include <AssetBrowser/AssetBrowserComponent.moc>
