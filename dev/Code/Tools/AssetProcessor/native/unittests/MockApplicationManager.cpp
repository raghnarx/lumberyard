/*
* All or portions of this file Copyright(c) Amazon.com, Inc.or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution(the "License").All use of this software is governed by the License,
*or, if provided, by the license below or the license accompanying this file.Do not
* remove or modify any license notices.This file is distributed on an "AS IS" BASIS,
*WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/
#include <QElapsedTimer>
#include <QCoreApplication>

#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <AzCore/std/smart_ptr/make_shared.h>

#include <AzCore/Component/Entity.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzFramework/StringFunc/StringFunc.h>
#include <AzFramework/IO/LocalFileIO.h>
#include <AssetBuilderSDK/AssetBuilderSDK.h>
#include <AssetBuilderSDK/AssetBuilderBusses.h>

#include "native/resourcecompiler/rccontroller.h"
#include "native/resourcecompiler/rcjob.h"
#include "native/utilities/assetUtils.h"
#include "native/utilities/ApplicationManagerAPI.h"
#include "native/utilities/AssetBuilderInfo.h"
#include "MockApplicationManager.h"
#include "UnitTestRunner.h"

namespace AssetProcessor
{
    extern const BuilderIdAndName BUILDER_ID_COPY;
    extern const BuilderIdAndName BUILDER_ID_RC;
    extern const BuilderIdAndName BUILDER_ID_SKIP;

    struct MockRecognizerConfiguration
        : public RecognizerConfiguration
    {
        const RecognizerContainer& GetAssetRecognizerContainer() const override
        {
            return m_container;
        }
        const ExcludeRecognizerContainer& GetExcludeAssetRecognizerContainer() const override
        {
            return m_excludeContainer;
        }

        RecognizerContainer m_container;
        ExcludeRecognizerContainer m_excludeContainer;
    };
    class MockRCCompiler
        : public AssetProcessor::RCCompiler
    {
    public:
        MockRCCompiler()
            : m_executeResultResult(0, false, "c:\temp")
        {
        }

        bool Initialize(const QString& systemRoot, const QString& rcExecutableFullPath) override
        {
            m_initialize++;
            return m_initializeResult;
        }
        bool Execute(const QString& inputFile, const QString& watchFolder, const QString& platformIdentifier, const QString& params, const QString& dest, 
            const AssetBuilderSDK::JobCancelListener* jobCancelListener, Result& result) const override
        {
            m_execute++;
            result = m_executeResultResult;
            return m_executeResult;
        }
        void RequestQuit() override
        {
            m_request_quit++;
        }

        void ResetCounters()
        {
            this->m_initialize = 0;
            this->m_execute = 0;
            this->m_request_quit = 0;
        }

        void SetResultInitialize(bool result)
        {
            m_initializeResult = result;
        }

        void SetResultExecute(bool result)
        {
            m_executeResult = result;
        }

        void SetResultResultExecute(Result result)
        {
            m_executeResultResult = result;
        }

        bool m_initializeResult = true;
        bool m_executeResult = true;
        Result m_executeResultResult;
        mutable int m_initialize = 0;
        mutable int m_execute = 0;
        mutable int m_request_quit = 0;
    };



    InternalMockBuilder::InternalMockBuilder(const QHash<QString, BuilderIdAndName>& inputBuilderNameByIdMap)
        : InternalRecognizerBasedBuilder(inputBuilderNameByIdMap,AZ::Uuid::CreateRandom())
        , m_createJobCallsCount(0)
    {
        this->m_rcCompiler.reset(new MockRCCompiler());
    }

    InternalMockBuilder::~InternalMockBuilder()
    {
    }

    bool InternalMockBuilder::InitializeMockBuilder(const AssetRecognizer& assetRecognizer)
    {
        MockRecognizerConfiguration conf;
        conf.m_container[QString(assetRecognizer.m_name.data())] = assetRecognizer;
        return InternalRecognizerBasedBuilder::Initialize(conf);
    }


    AssetBuilderSDK::AssetBuilderDesc InternalMockBuilder::CreateBuilderDesc(const QString& builderName,const QString& builderId, const AZStd::vector<AssetBuilderSDK::AssetBuilderPattern>& builderPatterns)
    {
        AssetBuilderSDK::AssetBuilderDesc   builderDesc;

        builderDesc.m_name = builderName.toUtf8().data();
        builderDesc.m_patterns = builderPatterns;
        builderDesc.m_busId = AZ::Uuid::CreateString(builderId.toUtf8().data());
        builderDesc.m_builderType = AssetBuilderSDK::AssetBuilderDesc::AssetBuilderType::Internal;
        builderDesc.m_analysisFingerprint = "xyz"; // Normally this would include the data included in the CreateJobs fingerprint but it's not important for these unit tests currently, it just needs to exist
        builderDesc.m_createJobFunction = AZStd::bind(&InternalMockBuilder::CreateJobs, this, AZStd::placeholders::_1, AZStd::placeholders::_2);
        builderDesc.m_processJobFunction = AZStd::bind(&InternalMockBuilder::ProcessJob, this, AZStd::placeholders::_1, AZStd::placeholders::_2);
        return builderDesc;
    }

    void InternalMockBuilder::CreateJobs(const AssetBuilderSDK::CreateJobsRequest& request, AssetBuilderSDK::CreateJobsResponse& response)
    {
        m_lastCreateJobRequest = request;
        InternalRecognizerBasedBuilder::CreateJobs(request, response);
        m_createJobCallsCount++;

        m_lastCreateJobResponse = response;
    }

    void InternalMockBuilder::ProcessJob(const AssetBuilderSDK::ProcessJobRequest& request, AssetBuilderSDK::ProcessJobResponse& response)
    {
        InternalRecognizerBasedBuilder::ProcessJob(request, response);
    }

    void InternalMockBuilder::ResetCounters()
    {
        m_createJobCallsCount = 0;
        m_processJobCallsCount = 0;
    }
    int InternalMockBuilder::GetCreateJobCalls()
    {
        return m_createJobCallsCount;
    }

    const AssetBuilderSDK::CreateJobsResponse& InternalMockBuilder::GetLastCreateJobResponse()
    {
        return m_lastCreateJobResponse;
    }
    const AssetBuilderSDK::CreateJobsRequest& InternalMockBuilder::GetLastCreateJobRequest()
    {
        return m_lastCreateJobRequest;
    }

    bool MockApplicationManager::RegisterAssetRecognizerAsBuilder(const AssetProcessor::AssetRecognizer& rec)
    {
        QString newBuilderId = BUILDER_ID_RC.GetId();
        QString newBuilderName = rec.m_name;
        QHash<QString, BuilderIdAndName> inputBuilderNameByIdMap =
        {
            { newBuilderId, BUILDER_ID_RC }
        };

        AZStd::shared_ptr<InternalMockBuilder>   builder =
            AZStd::make_shared<AssetProcessor::InternalMockBuilder>(inputBuilderNameByIdMap);

        if (m_internalBuilderRegistrationCount++ > 0)
        {
            // After the first initialization, the builder with id BUILDER_ID_RC will have already been registered to the builder bus.  
            // After the initial registration, make sure to unregister based on the fixed internal rc uuid so we can register it again
            AZ::Uuid uuid = AZ::Uuid::CreateString(newBuilderId.toUtf8().data());
            EBUS_EVENT(AssetBuilderRegistrationBus, UnRegisterBuilderDescriptor, uuid);
        }

        if (!builder->InitializeMockBuilder(rec))
        {
            return false;
        }

        AZStd::vector<AssetBuilderSDK::AssetBuilderPattern> patterns;
        patterns.push_back(rec.m_patternMatcher.GetBuilderPattern());


        AZStd::string buildAZName(rec.m_name.toUtf8().data());
        if (m_internalBuilders.find(buildAZName) != m_internalBuilders.end())
        {
            m_internalBuilders.erase(buildAZName);
        }
        m_internalBuilders[buildAZName] = builder;

        AssetBuilderSDK::AssetBuilderDesc   builderDesc = builder->CreateBuilderDesc(rec.m_name, newBuilderId, patterns);


        AZ::Uuid        internalUuid = AZ::Uuid::CreateRandom();
        m_internalBuilderUUIDByName[buildAZName] = internalUuid;

        BuilderFilePatternMatcherAndBuilderDesc matcherAndbuilderDesc;
        matcherAndbuilderDesc.m_builderDesc = builderDesc;
        matcherAndbuilderDesc.m_matcherBuilderPattern = AssetUtilities::BuilderFilePatternMatcher(rec.m_patternMatcher.GetBuilderPattern(), builderDesc.m_busId);
        matcherAndbuilderDesc.m_internalUuid = internalUuid;
        m_matcherBuilderPatterns.push_back(matcherAndbuilderDesc);

        return true;
    }

    bool MockApplicationManager::UnRegisterAssetRecognizerAsBuilder(const char* name)
    {
        AZStd::string key(name);
        AZ::Uuid    uuid = m_internalBuilderUUIDByName[key];

        if (m_internalBuilders.find(key) != m_internalBuilders.end())
        {
            // Find the builder and remove the builder desc map and build patterns
            AZStd::shared_ptr<InternalRecognizerBasedBuilder>   builder = m_internalBuilders[key];
            // Remove from the matcher builder pattern
            for (auto remover = this->m_matcherBuilderPatterns.begin();
                 remover != this->m_matcherBuilderPatterns.end();
                 remover++)
            {
                AZStd::string pattern = remover->m_matcherBuilderPattern.GetBuilderPattern().m_pattern;
                if (remover->m_internalUuid == uuid)
                {
                    auto deleteIter = remover;
                    remover++;
                    this->m_matcherBuilderPatterns.erase(deleteIter);
                }
            }
            m_internalBuilders.erase(key);
            m_internalBuilderUUIDByName.erase(key);
            return true;
        }
        else
        {
            return false;
        }
    }

    void MockApplicationManager::UnRegisterAllBuilders()
    {
        AZStd::list<AZStd::string> registeredBuilderNames;
        for (auto builderIter = m_internalBuilders.begin();
             builderIter != m_internalBuilders.end();
             builderIter++)
        {
            registeredBuilderNames.push_back(builderIter->first);
        }
        for (auto builderName : registeredBuilderNames)
        {
            UnRegisterAssetRecognizerAsBuilder(builderName.c_str());
        }
    }

    void MockApplicationManager::GetMatchingBuildersInfo(const AZStd::string& assetPath, AssetProcessor::BuilderInfoList& builderInfoList)
    {
        AZStd::set<AZ::Uuid>  uniqueBuilderDescIDs;
        m_getMatchingBuildersInfoFunctionCalls++;
        for (auto matcherInfo : m_matcherBuilderPatterns)
        {
            if (matcherInfo.m_matcherBuilderPattern.MatchesPath(assetPath))
            {
                uniqueBuilderDescIDs.insert(matcherInfo.m_matcherBuilderPattern.GetBuilderDescID());
                builderInfoList.push_back(matcherInfo.m_builderDesc);
            }
        }
    };

    void MockApplicationManager::GetAllBuildersInfo(AssetProcessor::BuilderInfoList& builderInfoList)
    {
        for (auto matcherInfo : m_matcherBuilderPatterns)
        {
            builderInfoList.push_back(matcherInfo.m_builderDesc);
        }
    };

    bool MockApplicationManager::GetBuilderByID(const AZStd::string& builderName, AZStd::shared_ptr<InternalMockBuilder>& builder)
    {
        if (m_internalBuilders.find(builderName) == m_internalBuilders.end())
        {
            return false;
        }
        builder = m_internalBuilders[builderName];
        return true;
    }
    bool MockApplicationManager::GetBuildUUIDFromName(const AZStd::string& builderName, AZ::Uuid& buildUUID)
    {
        if (m_internalBuilderUUIDByName.find(builderName) != m_internalBuilderUUIDByName.end())
        {
            AZ::Uuid    internalUuid = m_internalBuilderUUIDByName[builderName];
            for (auto matcherBuildPattern : m_matcherBuilderPatterns)
            {
                if (matcherBuildPattern.m_internalUuid == internalUuid)
                {
                    buildUUID = matcherBuildPattern.m_matcherBuilderPattern.GetBuilderDescID();
                    return true;
                }
            }
        }
        return false;
    }



    void MockApplicationManager::ResetMatchingBuildersInfoFunctionCalls()
    {
        m_getMatchingBuildersInfoFunctionCalls = 0;
    }

    int MockApplicationManager::GetMatchingBuildersInfoFunctionCalls()
    {
        return m_getMatchingBuildersInfoFunctionCalls;
    }

    void MockApplicationManager::ResetMockBuilderCreateJobCalls()
    {
        for (auto builderIter = m_internalBuilders.begin();
             builderIter != m_internalBuilders.end();
             builderIter++)
        {
            builderIter->second->ResetCounters();
        }
    }

    int MockApplicationManager::GetMockBuilderCreateJobCalls()
    {
        int jobCalls = 0;
        for (auto builderIter = m_internalBuilders.begin();
             builderIter != m_internalBuilders.end();
             builderIter++)
        {
            jobCalls += builderIter->second->GetCreateJobCalls();
        }
        return jobCalls;
    }

    MockAssetBuilderInfoHandler::MockAssetBuilderInfoHandler()
    {
        m_assetBuilderDesc.m_name = "Mock_Foo_Builder";
        m_assetBuilderDesc.m_patterns.push_back(AssetBuilderSDK::AssetBuilderPattern("*.foo", AssetBuilderSDK::AssetBuilderPattern::Regex));
        m_assetBuilderDesc.m_busId = AZ::Uuid::CreateRandom();
        m_assetBuilderDesc.m_createJobFunction = [this](const AssetBuilderSDK::CreateJobsRequest& request, AssetBuilderSDK::CreateJobsResponse& response)
        {
            for (int idx = 0; idx < m_numberOfJobsToCreate; idx++)
            {
                if (request.HasPlatform("pc"))
                {
                    AssetBuilderSDK::JobDescriptor descriptor;
                    AZStd::string indexString;
                    AZStd::to_string(indexString, idx);
                    descriptor.m_jobKey = "RandomJobKey" + indexString;
                    descriptor.SetPlatformIdentifier("pc");
                    response.m_createJobOutputs.push_back(descriptor);
                }
            }

            response.m_result = AssetBuilderSDK::CreateJobsResultCode::Success;
        };
        AssetProcessor::AssetBuilderInfoBus::Handler::BusConnect();
    }

    MockAssetBuilderInfoHandler::~MockAssetBuilderInfoHandler()
    {
        AssetProcessor::AssetBuilderInfoBus::Handler::BusDisconnect();
    }

    void MockAssetBuilderInfoHandler::GetMatchingBuildersInfo(const AZStd::string& assetPath, AssetProcessor::BuilderInfoList& builderInfoList)
    {
        builderInfoList.push_back(m_assetBuilderDesc);
    }

    void MockAssetBuilderInfoHandler::GetAllBuildersInfo(AssetProcessor::BuilderInfoList& builderInfoList)
    {
        builderInfoList.push_back(m_assetBuilderDesc);
    };
}
