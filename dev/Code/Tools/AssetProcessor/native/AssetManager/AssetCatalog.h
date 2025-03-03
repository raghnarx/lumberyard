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

#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QStringList>
#include <QHash>
#include <QDir>
#include "native/AssetDatabase/AssetDatabase.h"
#include "native/assetprocessor.h"
#include "native/utilities/AssetUtilEBusHelper.h"
#include "native/utilities/PlatformConfiguration.h"
#include <AzFramework/Asset/AssetRegistry.h>
#include <QMutex>
#include <QMultiMap>
#include <AzCore/IO/SystemFile.h>
#include <AzToolsFramework/ToolsComponents/ToolsAssetCatalogBus.h>

namespace AzFramework
{
    class AssetRegistry;
    namespace AssetSystem
    {
        class AssetNotificationMessage;
    }
}

namespace AssetProcessor
{
    class AssetProcessorManager;
    class AssetDatabaseConnection;

    class AssetCatalog 
        : public QObject
        , private AssetRegistryRequestBus::Handler
        , private AzToolsFramework::AssetSystemRequestBus::Handler
        , private AzToolsFramework::ToolsAssetSystemBus::Handler
        , private AZ::Data::AssetCatalogRequestBus::Handler
    {
        using NetworkRequestID = AssetProcessor::NetworkRequestID;
        using BaseAssetProcessorMessage = AzFramework::AssetSystem::BaseAssetProcessorMessage;
        Q_OBJECT;
    
    public:
        AssetCatalog(QObject* parent, AssetProcessor::PlatformConfiguration* platformConfiguration);
        virtual ~AssetCatalog();

    Q_SIGNALS:
        // outgoing message to the network
        void SendAssetMessage(QString platform, AzFramework::AssetSystem::AssetNotificationMessage message);
        void AsyncAssetCatalogStatusResponse(AssetCatalogStatus status);

    public Q_SLOTS:
        // incoming message from the AP
        void OnAssetMessage(QString platform, AzFramework::AssetSystem::AssetNotificationMessage message);
        void RequestReady(NetworkRequestID requestId, BaseAssetProcessorMessage* message, QString platform, bool fencingFailed = false);

        void SaveRegistry_Impl();
        void BuildRegistry();
        void OnSourceQueued(AZ::Uuid sourceUuid, AZ::Uuid legacyUuid, QString rootPath, QString relativeFilePath);
        void OnSourceFinished(AZ::Uuid sourceUuid, AZ::Uuid legacyUuid);
        void AsyncAssetCatalogStatusRequest();
        
    protected:

        //////////////////////////////////////////////////////////////////////////
        // AssetRegistryRequestBus::Handler overrides
        int SaveRegistry() override;
        //////////////////////////////////////////////////////////////////////////

        void RegistrySaveComplete(int assetCatalogVersion, bool allCatalogsSaved);

        //////////////////////////////////////////////////////////////////////////
        // AzToolsFramework::AssetSystem::AssetSystemRequestBus::Handler overrides
        const char* GetAbsoluteDevGameFolderPath() override;
        const char* GetAbsoluteDevRootFolderPath() override;
        bool GetRelativeProductPathFromFullSourceOrProductPath(const AZStd::string& fullPath, AZStd::string& relativeProductPath) override;
        bool GetFullSourcePathFromRelativeProductPath(const AZStd::string& relPath, AZStd::string& fullSourcePath) override;
        bool GetAssetInfoById(const AZ::Data::AssetId& assetId, const AZ::Data::AssetType& assetType, AZ::Data::AssetInfo& assetInfo, AZStd::string& rootFilePath) override;
        bool GetSourceInfoBySourcePath(const char* sourcePath, AZ::Data::AssetInfo& assetInfo, AZStd::string& watchFolder) override;
        bool GetSourceInfoBySourceUUID(const AZ::Uuid& sourceUuid, AZ::Data::AssetInfo& assetInfo, AZStd::string& watchFolder) override;
        bool GetScanFolders(AZStd::vector<AZStd::string>& scanFolders) override;
        bool GetAssetSafeFolders(AZStd::vector<AZStd::string>& assetSafeFolders) override;
        bool IsAssetPlatformEnabled(const char* platform) override;
        int GetPendingAssetsForPlatform(const char* platform) override;
        ////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////
        // AssetCatalogRequestBus overrides
        AZStd::string GetAssetPathById(const AZ::Data::AssetId& id) override;
        AZ::Data::AssetId GetAssetIdByPath(const char* path, const AZ::Data::AssetType& typeToRegister, bool autoRegisterIfNotFound) override;
        AZ::Data::AssetInfo GetAssetInfoById(const AZ::Data::AssetId& id) override;
        ////////////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // AzToolsFramework::ToolsAssetSystemBus::Handler
        void RegisterSourceAssetType(const AZ::Data::AssetType& assetType, const char* assetFileFilter);
        void UnregisterSourceAssetType(const AZ::Data::AssetType& assetType);
        //////////////////////////////////////////////////////////////////////////

        //! given some absolute path, please respond with its relative product path.  For now, this will be a
        //! string like 'textures/blah.tif' (we don't care about extensions), but eventually, this will
        //! be an actual asset UUID.
        void ProcessGetRelativeProductPathFromFullSourceOrProductPathRequest(const AZStd::string& fullPath, AZStd::string& relativeProductPath);

        //! This function helps in determining the full product path of an relative product path.
        //! In the future we will be sending an asset UUID to this function to request for full path.
        void ProcessGetFullSourcePathFromRelativeProductPathRequest(const AZStd::string& relPath, AZStd::string& fullSourcePath);

        //! Gets the source file info for an Asset by checking the DB first and the APM queue second
        bool GetSourceFileInfoFromAssetId(const AZ::Data::AssetId &assetId, AZStd::string& watchFolder, AZStd::string& relativePath);
        
        //! Gets the product AssetInfo based on a platform and assetId.  If you specify a null or empty platform the current or first available will be used.
        AZ::Data::AssetInfo GetProductAssetInfo(const char* platformName, const AZ::Data::AssetId& id);
        
        //! GetAssetInfo that tries to figure out if the asset is a product or source so it can return info about the product or source respectively
        bool GetAssetInfoByIdOnly(const AZ::Data::AssetId& id, AZ::Data::AssetInfo& assetInfo, AZStd::string& rootFilePath);
        
        //! Checks in the currently-in-queue assets list for info on an asset (by source Id)
        bool GetQueuedAssetInfoById(const AZ::Uuid& guid, AZStd::string& watchFolder, AZStd::string& relativePath);

        //! Checks in the currently-in-queue assets list for info on an asset (by source name)
        bool GetQueuedAssetInfoByRelativeSourceName(const char* sourceName, AZ::Data::AssetInfo& assetInfo, AZStd::string& watchFolder);

        //! Gets the source info for a source that is not in the DB or APM queue
        bool GetUncachedSourceInfoFromDatabaseNameAndWatchFolder(const char* sourceDatabasePath, const char* watchFolder, AZ::Data::AssetInfo& assetInfo);

        bool ConnectToDatabase();

        //! List of AssetTypes that should return info for the source instead of the product
        AZStd::unordered_set<AZ::Data::AssetType> m_sourceAssetTypes;
        AZStd::unordered_map<AZStd::string, AZ::Data::AssetType> m_sourceAssetTypeFilters;
        AZStd::mutex m_sourceAssetTypesMutex;

        //! Used to protect access to the database connection, only one thread can use it at a time
        AZStd::mutex m_databaseMutex;

        struct SourceInfo
        {
            QString m_watchFolder;
            QString m_sourceName;
        };

        AZStd::mutex m_sourceUUIDToSourceNameMapMutex;
        using SourceUUIDToSourceNameMap = AZStd::unordered_map<AZ::Uuid, SourceInfo>;
        using SourceNameToSourceUuidMap = AZStd::unordered_map<AZStd::string, AZ::Uuid>;

        SourceUUIDToSourceNameMap m_sourceUUIDToSourceNameMap; // map of uuids to source file names for assets that are currently in the processing queue
        SourceNameToSourceUuidMap m_sourceNameToSourceUUIDMap;

        QMutex m_registriesMutex;
        QHash<QString, AzFramework::AssetRegistry> m_registries; // per platform.
        AssetProcessor::PlatformConfiguration* m_platformConfig;
        QStringList m_platforms;
        AZStd::unique_ptr<AssetDatabaseConnection> m_db;
        QDir m_cacheRoot;

        bool m_registryBuiltOnce;
        bool m_catalogIsDirty = true;
        bool m_currentlySavingCatalog = false;
        int m_currentRegistrySaveVersion = 0;
        QMutex m_savingRegistryMutex;
        QMultiMap<int, AssetProcessor::NetworkRequestID> m_queuedSaveCatalogRequest;

        AZStd::vector<char> m_saveBuffer; // so that we don't realloc all the time

        char m_absoluteDevFolderPath[AZ_MAX_PATH_LEN];
        char m_absoluteDevGameFolderPath[AZ_MAX_PATH_LEN];
        QDir m_cacheRootDir;
    };
}
