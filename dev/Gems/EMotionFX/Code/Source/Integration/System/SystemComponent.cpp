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


#include "EMotionFX_precompiled.h"

#include <AzCore/Component/ComponentApplication.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/RTTI/BehaviorContext.h>

#include <AzFramework/Physics/CharacterBus.h>

#include <LmbrCentral/Physics/CryCharacterPhysicsBus.h>

#include <EMotionFX/Source/Allocators.h>
#include <EMotionFX/Source/SingleThreadScheduler.h>
#include <EMotionFX/Source/EMotionFXManager.h>
#include <EMotionFX/Source/AnimGraphManager.h>
#include <EMotionFX/Source/AnimGraphObjectFactory.h>
#include <EMotionFX/Source/MotionSet.h>
#include <EMotionFX/Source/ConstraintTransformRotationAngles.h>
#include <EMotionFX/Source/Parameter/ParameterFactory.h>
#include <EMotionFX/Source/TwoStringEventData.h>
#include <EMotionFX/Source/EventDataFootIK.h>

#include <EMotionFX/Source/PhysicsSetup.h>
#include <MCore/Source/Command.h>
#include <EMotionFX/CommandSystem/Source/MotionEventCommands.h>

#include <EMotionFX/Source/PoseData.h>
#include <EMotionFX/Source/PoseDataRagdoll.h>

#include <Integration/EMotionFXBus.h>
#include <Integration/Assets/ActorAsset.h>
#include <Integration/Assets/MotionAsset.h>
#include <Integration/Assets/MotionSetAsset.h>
#include <Integration/Assets/AnimGraphAsset.h>

#include <Integration/System/SystemComponent.h>

#include <AzFramework/Physics/World.h>


#if defined(EMOTIONFXANIMATION_EDITOR) // EMFX tools / editor includes
// Qt
#   include <QtGui/QSurfaceFormat>
// EMStudio tools and main window registration
#   include <LyViewPaneNames.h>
#   include <AzToolsFramework/API/ViewPaneOptions.h>
#   include <AzCore/std/string/wildcard.h>
#   include <QApplication>
#   include <EMotionStudio/EMStudioSDK/Source/EMStudioManager.h>
#   include <EMotionStudio/EMStudioSDK/Source/MainWindow.h>
#   include <EMotionStudio/EMStudioSDK/Source/PluginManager.h>
// EMStudio plugins
#   include <EMotionStudio/Plugins/StandardPlugins/Source/LogWindow/LogWindowPlugin.h>
#   include <EMotionStudio/Plugins/StandardPlugins/Source/CommandBar/CommandBarPlugin.h>
#   include <EMotionStudio/Plugins/StandardPlugins/Source/ActionHistory/ActionHistoryPlugin.h>
#   include <EMotionStudio/Plugins/StandardPlugins/Source/MotionWindow/MotionWindowPlugin.h>
#   include <EMotionStudio/Plugins/StandardPlugins/Source/MorphTargetsWindow/MorphTargetsWindowPlugin.h>
#   include <EMotionStudio/Plugins/StandardPlugins/Source/TimeView/TimeViewPlugin.h>
#   include <EMotionStudio/Plugins/StandardPlugins/Source/Attachments/AttachmentsPlugin.h>
#   include <EMotionStudio/Plugins/StandardPlugins/Source/SceneManager/SceneManagerPlugin.h>
#   include <EMotionStudio/Plugins/StandardPlugins/Source/NodeWindow/NodeWindowPlugin.h>
#   include <EMotionStudio/Plugins/StandardPlugins/Source/MotionEvents/MotionEventsPlugin.h>
#   include <EMotionStudio/Plugins/StandardPlugins/Source/MotionSetsWindow/MotionSetsWindowPlugin.h>
#   include <EMotionStudio/Plugins/StandardPlugins/Source/NodeGroups/NodeGroupsPlugin.h>
#   include <EMotionStudio/Plugins/StandardPlugins/Source/AnimGraph/AnimGraphPlugin.h>
#   include <EMotionStudio/Plugins/RenderPlugins/Source/OpenGLRender/OpenGLRenderPlugin.h>
#   include <Editor/Plugins/HitDetection/HitDetectionJointInspectorPlugin.h>
#   include <Editor/Plugins/SkeletonOutliner/SkeletonOutlinerPlugin.h>
#   include <Editor/Plugins/Ragdoll/RagdollNodeInspectorPlugin.h>
#   include <Editor/Plugins/Cloth/ClothJointInspectorPlugin.h>
#   include <Source/Editor/PropertyWidgets/PropertyTypes.h>
#endif // EMOTIONFXANIMATION_EDITOR

#include <ISystem.h>

// include required AzCore headers
#include <AzCore/IO/FileIO.h>
#include <AzFramework/API/ApplicationAPI.h>

namespace EMotionFX
{
    namespace Integration
    {
        //////////////////////////////////////////////////////////////////////////
        class EMotionFXEventHandler
            : public EMotionFX::EventHandler
        {
        public:
            AZ_CLASS_ALLOCATOR(EMotionFXEventHandler, EMotionFXAllocator, 0);

            const AZStd::vector<EventTypes> GetHandledEventTypes() const
            {
                return {
                           EVENT_TYPE_ON_EVENT,
                           EVENT_TYPE_ON_HAS_LOOPED,
                           EVENT_TYPE_ON_STATE_ENTERING,
                           EVENT_TYPE_ON_STATE_ENTER,
                           EVENT_TYPE_ON_STATE_END,
                           EVENT_TYPE_ON_STATE_EXIT,
                           EVENT_TYPE_ON_START_TRANSITION,
                           EVENT_TYPE_ON_END_TRANSITION
                };
            }

            /// Dispatch motion events to listeners via ActorNotificationBus::OnMotionEvent.
            void OnEvent(const EMotionFX::EventInfo& emfxInfo) override
            {
                const ActorInstance* actorInstance = emfxInfo.mActorInstance;
                if (actorInstance)
                {
                    const AZ::EntityId owningEntityId = actorInstance->GetEntityId();

                    // Fill engine-compatible structure to dispatch to game code.
                    MotionEvent motionEvent;
                    motionEvent.m_entityId = owningEntityId;
                    motionEvent.m_actorInstance = emfxInfo.mActorInstance;
                    motionEvent.m_motionInstance = emfxInfo.mMotionInstance;
                    motionEvent.m_time = emfxInfo.mTimeValue;
                    // TODO
                    for (const auto& eventData : emfxInfo.mEvent->GetEventDatas())
                    {
                        if (const EMotionFX::TwoStringEventData* twoStringEventData = azrtti_cast<const EMotionFX::TwoStringEventData*>(eventData.get()))
                        {
                            motionEvent.m_eventTypeName = twoStringEventData->GetSubject().c_str();
                            motionEvent.SetParameterString(twoStringEventData->GetParameters().c_str(), twoStringEventData->GetParameters().size());
                            break;
                        }
                    }
                    motionEvent.m_globalWeight = emfxInfo.mGlobalWeight;
                    motionEvent.m_localWeight = emfxInfo.mLocalWeight;
                    motionEvent.m_isEventStart = emfxInfo.IsEventStart();

                    // Queue the event to flush on the main thread.
                    ActorNotificationBus::QueueEvent(owningEntityId, &ActorNotificationBus::Events::OnMotionEvent, AZStd::move(motionEvent));
                }
            }

            void OnHasLooped(EMotionFX::MotionInstance* motionInstance) override
            {
                const ActorInstance* actorInstance = motionInstance->GetActorInstance();
                if (actorInstance)
                {
                    const AZ::EntityId owningEntityId = actorInstance->GetEntityId();
                    ActorNotificationBus::QueueEvent(owningEntityId, &ActorNotificationBus::Events::OnMotionLoop, motionInstance->GetMotion()->GetName());
                }
            }

            void OnStateEntering(EMotionFX::AnimGraphInstance* animGraphInstance, EMotionFX::AnimGraphNode* state) override
            {
                const ActorInstance* actorInstance = animGraphInstance->GetActorInstance();
                if (actorInstance && state)
                {
                    const AZ::EntityId owningEntityId = actorInstance->GetEntityId();
                    ActorNotificationBus::QueueEvent(owningEntityId, &ActorNotificationBus::Events::OnStateEntering, state->GetName());
                }
            }

            void OnStateEnter(EMotionFX::AnimGraphInstance* animGraphInstance, EMotionFX::AnimGraphNode* state) override
            {
                const ActorInstance* actorInstance = animGraphInstance->GetActorInstance();
                if (actorInstance && state)
                {
                    const AZ::EntityId owningEntityId = actorInstance->GetEntityId();
                    ActorNotificationBus::QueueEvent(owningEntityId, &ActorNotificationBus::Events::OnStateEntered, state->GetName());
                }
            }

            void OnStateEnd(EMotionFX::AnimGraphInstance* animGraphInstance, EMotionFX::AnimGraphNode* state) override
            {
                const ActorInstance* actorInstance = animGraphInstance->GetActorInstance();
                if (actorInstance && state)
                {
                    const AZ::EntityId owningEntityId = actorInstance->GetEntityId();
                    ActorNotificationBus::QueueEvent(owningEntityId, &ActorNotificationBus::Events::OnStateExiting, state->GetName());
                }
            }

            void OnStateExit(EMotionFX::AnimGraphInstance* animGraphInstance, EMotionFX::AnimGraphNode* state) override
            {
                const ActorInstance* actorInstance = animGraphInstance->GetActorInstance();
                if (actorInstance && state)
                {
                    const AZ::EntityId owningEntityId = actorInstance->GetEntityId();
                    ActorNotificationBus::QueueEvent(owningEntityId, &ActorNotificationBus::Events::OnStateExited, state->GetName());
                }
            }

            void OnStartTransition(EMotionFX::AnimGraphInstance* animGraphInstance, EMotionFX::AnimGraphStateTransition* transition) override
            {
                const ActorInstance* actorInstance = animGraphInstance->GetActorInstance();
                if (actorInstance)
                {
                    const AZ::EntityId owningEntityId = actorInstance->GetEntityId();
                    const char* sourceName = transition->GetSourceNode() ? transition->GetSourceNode()->GetName() : "";
                    const char* targetName = transition->GetTargetNode() ? transition->GetTargetNode()->GetName() : "";
                    ActorNotificationBus::QueueEvent(owningEntityId, &ActorNotificationBus::Events::OnStateTransitionStart, sourceName, targetName);
                }
            }

            void OnEndTransition(EMotionFX::AnimGraphInstance* animGraphInstance, EMotionFX::AnimGraphStateTransition* transition) override
            {
                const ActorInstance* actorInstance = animGraphInstance->GetActorInstance();
                if (actorInstance)
                {
                    const AZ::EntityId owningEntityId = actorInstance->GetEntityId();
                    const char* sourceName = transition->GetSourceNode() ? transition->GetSourceNode()->GetName() : "";
                    const char* targetName = transition->GetTargetNode() ? transition->GetTargetNode()->GetName() : "";
                    ActorNotificationBus::QueueEvent(owningEntityId, &ActorNotificationBus::Events::OnStateTransitionEnd, sourceName, targetName);
                }
            }
        };

        //////////////////////////////////////////////////////////////////////////
        class ActorNotificationBusHandler
            : public ActorNotificationBus::Handler
            , public AZ::BehaviorEBusHandler
        {
        public:
            AZ_EBUS_BEHAVIOR_BINDER(ActorNotificationBusHandler, "{D2CD62E7-5FCF-4DC2-85DF-C205D5AB1E8B}", AZ::SystemAllocator,
                OnMotionEvent,
                OnMotionLoop,
                OnStateEntering,
                OnStateEntered,
                OnStateExiting,
                OnStateExited,
                OnStateTransitionStart,
                OnStateTransitionEnd);

            void OnMotionEvent(MotionEvent motionEvent) override
            {
                Call(FN_OnMotionEvent, motionEvent);
            }

            void OnMotionLoop(const char* motionName) override
            {
                Call(FN_OnMotionLoop, motionName);
            }

            void OnStateEntering(const char* stateName) override
            {
                Call(FN_OnStateEntering, stateName);
            }

            void OnStateEntered(const char* stateName) override
            {
                Call(FN_OnStateEntered, stateName);
            }

            void OnStateExiting(const char* stateName) override
            {
                Call(FN_OnStateExiting, stateName);
            }

            void OnStateExited(const char* stateName) override
            {
                Call(FN_OnStateExited, stateName);
            }

            void OnStateTransitionStart(const char* fromState, const char* toState) override
            {
                Call(FN_OnStateTransitionStart, fromState, toState);
            }

            void OnStateTransitionEnd(const char* fromState, const char* toState) override
            {
                Call(FN_OnStateTransitionEnd, fromState, toState);
            }
        };

        SystemComponent::~SystemComponent() = default;
        
        int SystemComponent::emfx_updateEnabled = 1;
        int SystemComponent::emfx_actorRenderEnabled = 1;

        void SystemComponent::ReflectEMotionFX(AZ::ReflectContext* context)
        {
            MCore::ReflectionSerializer::Reflect(context);
            MCore::StringIdPoolIndex::Reflect(context);
            EMotionFX::ConstraintTransformRotationAngles::Reflect(context);

            // Actor
            EMotionFX::PhysicsSetup::Reflect(context);

            EMotionFX::PoseData::Reflect(context);
            EMotionFX::PoseDataRagdoll::Reflect(context);

            // Motion set
            EMotionFX::MotionSet::Reflect(context);
            EMotionFX::MotionSet::MotionEntry::Reflect(context);

            // Base AnimGraph objects
            EMotionFX::AnimGraphObject::Reflect(context);
            EMotionFX::AnimGraph::Reflect(context);
            EMotionFX::AnimGraphNodeGroup::Reflect(context);
            EMotionFX::AnimGraphGameControllerSettings::Reflect(context);

            // Anim graph objects
            EMotionFX::AnimGraphObjectFactory::ReflectTypes(context);

            // Anim graph's parameters
            EMotionFX::ParameterFactory::ReflectParameterTypes(context);

            EMotionFX::MotionEventTable::Reflect(context);
            EMotionFX::MotionEventTrack::Reflect(context);
            EMotionFX::AnimGraphSyncTrack::Reflect(context);
            EMotionFX::Event::Reflect(context);
            EMotionFX::MotionEvent::Reflect(context);
            EMotionFX::EventData::Reflect(context);
            EMotionFX::EventDataSyncable::Reflect(context);
            EMotionFX::TwoStringEventData::Reflect(context);
            EMotionFX::EventDataFootIK::Reflect(context);

            MCore::Command::Reflect(context);
            CommandSystem::MotionIdCommandMixin::Reflect(context);
            CommandSystem::CommandAdjustMotion::Reflect(context);
            CommandSystem::CommandClearMotionEvents::Reflect(context);
            CommandSystem::CommandCreateMotionEventTrack::Reflect(context);
            CommandSystem::CommandAdjustMotionEventTrack::Reflect(context);
            CommandSystem::CommandCreateMotionEvent::Reflect(context);
            CommandSystem::CommandAdjustMotionEvent::Reflect(context);
        }

        //////////////////////////////////////////////////////////////////////////
        void SystemComponent::Reflect(AZ::ReflectContext* context)
        {
            ReflectEMotionFX(context);

            // Reflect component for serialization.
            AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
            if (serializeContext)
            {
                serializeContext->Class<SystemComponent, AZ::Component>()
                    ->Version(1)
                    ->Field("NumThreads", &SystemComponent::m_numThreads)
                ;

                serializeContext->Class<MotionEvent>()
                    ->Version(1)
                ;

                if (AZ::EditContext* ec = serializeContext->GetEditContext())
                {
                    ec->Class<SystemComponent>("EMotion FX Animation", "Enables the EMotion FX animation solution")
                        ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("System", 0xc94d118b))
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                        ->DataElement(AZ::Edit::UIHandlers::Default, &SystemComponent::m_numThreads, "Number of threads", "Number of threads used internally by EMotion FX")
                    ;
                }
            }

            // Reflect system-level types and EBuses to behavior context.
            AZ::BehaviorContext* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context);
            if (behaviorContext)
            {
                behaviorContext->EBus<SystemRequestBus>("SystemRequestBus")
                ;

                behaviorContext->EBus<SystemNotificationBus>("SystemNotificationBus")
                ;

                // In order for a property to be displayed in ScriptCanvas. Both a setter and a getter are necessary(both must be non-null).
                // This is being worked on in dragon branch, once this is complete the dummy lambda functions can be removed.
                behaviorContext->Class<MotionEvent>("MotionEvent")
                    ->Attribute(AZ::Script::Attributes::ExcludeFrom, AZ::Script::Attributes::Preview)
                    ->Property("entityId", BehaviorValueGetter(&MotionEvent::m_entityId), [](MotionEvent*, const AZ::EntityId&) {})
                    ->Property("parameter", BehaviorValueGetter(&MotionEvent::m_parameter), [](MotionEvent*, const char*) {})
                    ->Property("eventType", BehaviorValueGetter(&MotionEvent::m_eventType), [](MotionEvent*, const AZ::u32&) {})
                    ->Property("eventTypeName", BehaviorValueGetter(&MotionEvent::m_eventTypeName), [](MotionEvent*, const char*) {})
                    ->Property("time", BehaviorValueGetter(&MotionEvent::m_time), [](MotionEvent*, const float&) {})
                    ->Property("globalWeight", BehaviorValueGetter(&MotionEvent::m_globalWeight), [](MotionEvent*, const float&) {})
                    ->Property("localWeight", BehaviorValueGetter(&MotionEvent::m_localWeight), [](MotionEvent*, const float&) {})
                    ->Property("isEventStart", BehaviorValueGetter(&MotionEvent::m_isEventStart), [](MotionEvent*, const bool&) {})
                ;

                behaviorContext->EBus<ActorNotificationBus>("ActorNotificationBus")
                    ->Attribute(AZ::Script::Attributes::ExcludeFrom, AZ::Script::Attributes::Preview)
                    ->Handler<ActorNotificationBusHandler>()
                    ->Event("OnMotionEvent", &ActorNotificationBus::Events::OnMotionEvent)
                    ->Event("OnMotionLoop", &ActorNotificationBus::Events::OnMotionLoop)
                    ->Event("OnStateEntering", &ActorNotificationBus::Events::OnStateEntering)
                    ->Event("OnStateEntered", &ActorNotificationBus::Events::OnStateEntered)
                    ->Event("OnStateExiting", &ActorNotificationBus::Events::OnStateExiting)
                    ->Event("OnStateExited", &ActorNotificationBus::Events::OnStateExited)
                    ->Event("OnStateTransitionStart", &ActorNotificationBus::Events::OnStateTransitionStart)
                    ->Event("OnStateTransitionEnd", &ActorNotificationBus::Events::OnStateTransitionEnd)
                ;
            }
        }

        //////////////////////////////////////////////////////////////////////////
        void SystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
        {
            provided.push_back(AZ_CRC("EMotionFXAnimationService", 0x3f8a6369));
        }

        //////////////////////////////////////////////////////////////////////////
        void SystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
        {
            incompatible.push_back(AZ_CRC("EMotionFXAnimationService", 0x3f8a6369));
        }

        //////////////////////////////////////////////////////////////////////////
        void SystemComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
        {
            required.push_back(AZ_CRC("AssetDatabaseService", 0x3abf5601));
        }

        //////////////////////////////////////////////////////////////////////////
        void SystemComponent::GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent)
        {
            dependent.push_back(AZ_CRC("AssetCatalogService", 0xc68ffc57));
            dependent.push_back(AZ_CRC("JobsService", 0xd5ab5a50));
        }

        //////////////////////////////////////////////////////////////////////////
        SystemComponent::SystemComponent()
            : m_numThreads(1)
        {
        }

        //////////////////////////////////////////////////////////////////////////
        void SystemComponent::Init()
        {
        }

        //////////////////////////////////////////////////////////////////////////
        void SystemComponent::Activate()
        {
            // Start EMotionFX allocator.
            EMotionFXAllocator::Descriptor allocatorDescriptor;
            allocatorDescriptor.m_custom = &AZ::AllocatorInstance<AZ::SystemAllocator>::Get();
            AZ::AllocatorInstance<EMotionFXAllocator>::Create(allocatorDescriptor);

            // Initialize MCore, which is EMotionFX's standard library of containers and systems.
            MCore::Initializer::InitSettings coreSettings;
            coreSettings.mMemAllocFunction = &EMotionFXAlloc;
            coreSettings.mMemReallocFunction = &EMotionFXRealloc;
            coreSettings.mMemFreeFunction = &EMotionFXFree;
            if (!MCore::Initializer::Init(&coreSettings))
            {
                AZ_Error("EMotion FX Animation", false, "Failed to initialize EMotion FX SDK Core");
                return;
            }

            // Initialize EMotionFX runtime.
            EMotionFX::Initializer::InitSettings emfxSettings;
            emfxSettings.mUnitType = MCore::Distance::UNITTYPE_METERS;

            if (!EMotionFX::Initializer::Init(&emfxSettings))
            {
                AZ_Error("EMotion FX Animation", false, "Failed to initialize EMotion FX SDK Runtime");
                return;
            }

            // Initialize media root to asset cache.
            // \todo We're in touch with the Systems team. Seems like the aliases are initialized pretty late and we can't access it when activating the system component.
            // Until this is resolved, we're going to init everything in the OnCrySystemInitialized().
            SetMediaRoot("@assets@");

            // Register EMotionFX event handler
            m_eventHandler.reset(aznew EMotionFXEventHandler());
            EMotionFX::GetEventManager().AddEventHandler(m_eventHandler.get());

            // Setup asset types.
            RegisterAssetTypesAndHandlers();

            SystemRequestBus::Handler::BusConnect();
            AZ::TickBus::Handler::BusConnect();
            CrySystemEventBus::Handler::BusConnect();
            EMotionFXRequestBus::Handler::BusConnect();
            EnableRayRequests();

#if defined (EMOTIONFXANIMATION_EDITOR)
            AzToolsFramework::EditorEvents::Bus::Handler::BusConnect();
            AzToolsFramework::EditorAnimationSystemRequestsBus::Handler::BusConnect();
            AzToolsFramework::AssetBrowser::AssetBrowserInteractionNotificationBus::Handler::BusConnect();
            m_updateTimer.Stamp();

            // Register custom property handlers for the reflected property editor.
            m_propertyHandlers = RegisterPropertyTypes();
#endif // EMOTIONFXANIMATION_EDITOR
        }

        //////////////////////////////////////////////////////////////////////////
        void SystemComponent::Deactivate()
        {
#if defined(EMOTIONFXANIMATION_EDITOR)
            // Unregister custom property handlers for the reflected property editor.
            UnregisterPropertyTypes(m_propertyHandlers);
            m_propertyHandlers.clear();

            if (EMStudio::GetManager())
            {
                EMStudio::Initializer::Shutdown();
                MysticQt::Initializer::Shutdown();
            }

            {
                using namespace AzToolsFramework;
                EditorRequests::Bus::Broadcast(&EditorRequests::UnregisterViewPane, EMStudio::MainWindow::GetEMotionFXPaneName());
            }

            AzToolsFramework::AssetBrowser::AssetBrowserInteractionNotificationBus::Handler::BusDisconnect();
            AzToolsFramework::EditorAnimationSystemRequestsBus::Handler::BusDisconnect();
            AzToolsFramework::EditorEvents::Bus::Handler::BusDisconnect();
#endif // EMOTIONFXANIMATION_EDITOR

            EMotionFX::GetEventManager().RemoveEventHandler(m_eventHandler.get());
            m_eventHandler.reset();

            AZ::TickBus::Handler::BusDisconnect();
            CrySystemEventBus::Handler::BusDisconnect();
            EMotionFXRequestBus::Handler::BusDisconnect();
            DisableRayRequests();

            if (SystemRequestBus::Handler::BusIsConnected())
            {
                SystemRequestBus::Handler::BusDisconnect();

                m_assetHandlers.resize(0);

                EMotionFX::Initializer::Shutdown();
                MCore::Initializer::Shutdown();
            }

            // Memory leaks will be reported.
            AZ::AllocatorInstance<EMotionFXAllocator>::Destroy();
        }

        //////////////////////////////////////////////////////////////////////////
        void SystemComponent::EnableRayRequests()
        {
            RaycastRequestBus::Handler::BusDisconnect();
            RaycastRequestBus::Handler::BusConnect();
        }

        void SystemComponent::DisableRayRequests()
        {
            RaycastRequestBus::Handler::BusDisconnect();
        }
        //////////////////////////////////////////////////////////////////////////
        void SystemComponent::OnCrySystemInitialized(ISystem& system, const SSystemInitParams&)
        {
#if !defined(AZ_MONOLITHIC_BUILD)
            // When module is linked dynamically, we must set our gEnv pointer.
            // When module is linked statically, we'll share the application's gEnv pointer.
            gEnv = system.GetGlobalEnvironment();
#endif

            REGISTER_CVAR2("emfx_updateEnabled", &emfx_updateEnabled, 1, VF_DEV_ONLY, "Enable main EMFX update");
            REGISTER_CVAR2("emfx_actorRenderEnabled", &emfx_actorRenderEnabled, 1, VF_DEV_ONLY, "Enable ActorRenderNode rendering");
        }

        //////////////////////////////////////////////////////////////////////////
        void SystemComponent::OnCryEditorInitialized()
        {
            // \todo Right now we're pointing at the @devassets@ location (source) and working from there, because .actor and .motion (motion) aren't yet processed through
            // the FBX pipeline. Once they are, we'll need to update various segments of the Tool to always read from the @assets@ cache, but write to the @devassets@ data/metadata.
            SetMediaRoot("@assets@");

            EMotionFX::GetEMotionFX().InitAssetFolderPaths();
        }

        //////////////////////////////////////////////////////////////////////////
        void SystemComponent::OnCrySystemShutdown(ISystem&)
        {
            gEnv->pConsole->UnregisterVariable("emfx_updateEnabled");
            gEnv->pConsole->UnregisterVariable("emfx_actorRenderEnabled");

#if !defined(AZ_MONOLITHIC_BUILD)
            gEnv = nullptr;
#endif
        }

        //////////////////////////////////////////////////////////////////////////
#if defined (EMOTIONFXANIMATION_EDITOR)
        void SystemComponent::UpdateAnimationEditorPlugins(float delta)
        {
            if (!EMStudio::GetManager())
            {
                return;
            }

            EMStudio::PluginManager* pluginManager = EMStudio::GetPluginManager();
            if (!pluginManager)
            {
                return;
            }

            // Process the plugins.
            const AZ::u32 numPlugins = pluginManager->GetNumActivePlugins();
            for (AZ::u32 i = 0; i < numPlugins; ++i)
            {
                EMStudio::EMStudioPlugin* plugin = pluginManager->GetActivePlugin(i);
                plugin->ProcessFrame(delta);
            }
        }
#endif

        //////////////////////////////////////////////////////////////////////////
        void SystemComponent::OnTick(float delta, AZ::ScriptTimePoint timePoint)
        {
            AZ_UNUSED(timePoint);

#if defined (EMOTIONFXANIMATION_EDITOR)
            AZ_UNUSED(delta);
            const float realDelta = m_updateTimer.StampAndGetDeltaTimeInSeconds();

            // Flush events prior to updating EMotion FX.
            ActorNotificationBus::ExecuteQueuedEvents();

            if (emfx_updateEnabled)
            {
                // Main EMotionFX runtime update.
                GetEMotionFX().Update(realDelta);
            }

            // Update all the animation editor plugins (redraw viewports, timeline, and graph windows etc).
            // But only update this when the main window is visible.
            if (EMotionFX::GetEMotionFX().GetIsInEditorMode() && EMStudio::GetManager() && EMStudio::HasMainWindow() && !EMStudio::GetMainWindow()->visibleRegion().isEmpty())
            {
                UpdateAnimationEditorPlugins(realDelta);
            }
#else
            // Flush events prior to updating EMotion FX.
            ActorNotificationBus::ExecuteQueuedEvents();

            if (emfx_updateEnabled)
            {
                // Main EMotionFX runtime update.
                GetEMotionFX().Update(delta);
            }
#endif

            const float timeDelta = delta;
            const ActorManager* actorManager = GetEMotionFX().GetActorManager();
            const AZ::u32 numActorInstances = actorManager->GetNumActorInstances();
            for (AZ::u32 i = 0; i < numActorInstances; ++i)
            {
                const ActorInstance* actorInstance = actorManager->GetActorInstance(i);

                if (actorInstance && actorInstance->GetIsEnabled() && actorInstance->GetIsOwnedByRuntime())
                {
                    AZ::Entity* entity = actorInstance->GetEntity();
                    const Actor* actor = actorInstance->GetActor();

                    if (entity && actor && actor->GetMotionExtractionNode())
                    {
                        const AZ::EntityId entityId = entity->GetId();

                        // Check if we have any physics character controllers.
                        bool hasPhysicsController = false;
                        bool hasCryPhysicsController = false;
                        Physics::CharacterRequestBus::EventResult(hasPhysicsController, entityId, &Physics::CharacterRequests::IsPresent);
                        LmbrCentral::CryCharacterPhysicsRequestBus::EventResult(hasCryPhysicsController, entityId, &LmbrCentral::CryCharacterPhysicsRequests::IsCryCharacterControllerPresent);

                        // If we have a physics controller.
                        AZ::TransformInterface* entityTransform = entity->GetTransform();
                        if (hasPhysicsController || hasCryPhysicsController)
                        {
                            const float deltaTimeInv = (timeDelta > 0.0f) ? (1.0f / timeDelta) : 0.0f;

                            AZ::Transform currentTransform = entityTransform->GetWorldTM();
                            const AZ::Vector3 actorInstancePosition = actorInstance->GetWorldSpaceTransform().mPosition;

                            const AZ::Vector3 currentPos = currentTransform.GetPosition();
                            const AZ::Vector3 positionDelta = actorInstancePosition - currentPos;

                            if (hasPhysicsController)
                            {
                                Physics::CharacterRequestBus::Event(entityId, &Physics::CharacterRequests::TryRelativeMove, positionDelta, timeDelta);
                            }
                            else if (hasCryPhysicsController)
                            {
                                const AZ::Vector3 scale = currentTransform.ExtractScaleExact();
                                const AZ::Vector3 velocity = positionDelta * scale * deltaTimeInv;
                                EBUS_EVENT_ID(entityId, LmbrCentral::CryCharacterPhysicsRequestBus, RequestVelocity, velocity, 0);
                            }

                            // Calculate the difference in rotation and apply that to the entity transform.
                            const AZ::Quaternion actorInstanceRotation = MCore::EmfxQuatToAzQuat(actorInstance->GetWorldSpaceTransform().mRotation);
                            const AZ::Quaternion rotationDelta = AZ::Quaternion::CreateFromTransform(currentTransform).GetInverseFull() * actorInstanceRotation;
                            if (!rotationDelta.IsIdentity(AZ::g_fltEps))
                            {
                                currentTransform = currentTransform * AZ::Transform::CreateFromQuaternion(rotationDelta);
                                entityTransform->SetWorldTM(currentTransform);
                            }
                        }
                        else // There is no physics controller, just use EMotion FX's actor instance transform directly.
                        {                            
                            const AZ::Transform newTransform = MCore::EmfxTransformToAzTransform(actorInstance->GetWorldSpaceTransform());
                            entityTransform->SetWorldTM(newTransform);
                        }
                    }
                }
            }
        }

        int SystemComponent::GetTickOrder()
        {
            return AZ::TICK_ANIMATION;
        }

        //////////////////////////////////////////////////////////////////////////
        void SystemComponent::RegisterAnimGraphObjectType(EMotionFX::AnimGraphObject* objectTemplate)
        {
            EMotionFX::AnimGraphObjectFactory::GetUITypes().emplace(azrtti_typeid(objectTemplate));
        }

        //////////////////////////////////////////////////////////////////////////
        void SystemComponent::RegisterAssetTypesAndHandlers()
        {
            // Initialize asset handlers.
            m_assetHandlers.emplace_back(aznew ActorAssetHandler);
            m_assetHandlers.emplace_back(aznew MotionAssetHandler);
            m_assetHandlers.emplace_back(aznew MotionSetAssetHandler);
            m_assetHandlers.emplace_back(aznew AnimGraphAssetHandler);

            // Add asset types and extensions to AssetCatalog.
            auto assetCatalog = AZ::Data::AssetCatalogRequestBus::FindFirstHandler();
            if (assetCatalog)
            {
                assetCatalog->EnableCatalogForAsset(azrtti_typeid<ActorAsset>());
                assetCatalog->EnableCatalogForAsset(azrtti_typeid<MotionAsset>());
                assetCatalog->EnableCatalogForAsset(azrtti_typeid<MotionSetAsset>());
                assetCatalog->EnableCatalogForAsset(azrtti_typeid<AnimGraphAsset>());

                assetCatalog->AddExtension("actor");        // Actor
                assetCatalog->AddExtension("motion");       // Motion
                assetCatalog->AddExtension("motionset");    // Motion set
                assetCatalog->AddExtension("animgraph");    // Anim graph
            }
        }

        //////////////////////////////////////////////////////////////////////////
        void SystemComponent::SetMediaRoot(const char* alias)
        {
            const char* rootPath = AZ::IO::FileIOBase::GetInstance()->GetAlias(alias);
            if (rootPath)
            {
                AZStd::string mediaRootPath = rootPath;
                EBUS_EVENT(AzFramework::ApplicationRequests::Bus, NormalizePathKeepCase, mediaRootPath);
                EMotionFX::GetEMotionFX().SetMediaRootFolder(mediaRootPath.c_str());
            }
            else
            {
                AZ_Warning("EMotionFX", false, "Failed to set media root because alias \"%s\" could not be resolved.", alias);
            }
        }


        //////////////////////////////////////////////////////////////////////////
        RaycastRequests::RaycastResult SystemComponent::Raycast(AZ::EntityId entityId, const RaycastRequests::RaycastRequest& rayRequest)
        {
            RaycastRequests::RaycastResult rayResult;

            // Build the ray request in the physics system.
            Physics::RayCastRequest physicsRayRequest;
            physicsRayRequest.m_start       = rayRequest.m_start;
            physicsRayRequest.m_direction   = rayRequest.m_direction;
            physicsRayRequest.m_distance    = rayRequest.m_distance;
            physicsRayRequest.m_queryType   = rayRequest.m_queryType;

            // Cast the ray in the physics system.
            Physics::RayCastHit physicsRayResult;
            Physics::WorldRequestBus::EventResult(physicsRayResult, AZ_CRC("AZPhysicalWorld", 0x18f33e24), &Physics::WorldRequests::RayCast, physicsRayRequest);
            if (physicsRayResult) // We intersected.
            {
                rayResult.m_position    = physicsRayResult.m_position;
                rayResult.m_normal      = physicsRayResult.m_normal;
                rayResult.m_intersected = true;
            }

            return rayResult;
        }


#if defined (EMOTIONFXANIMATION_EDITOR)

        //////////////////////////////////////////////////////////////////////////
        void InitializeEMStudioPlugins()
        {
            // Register EMFX plugins.
            EMStudio::PluginManager* pluginManager = EMStudio::GetPluginManager();
            pluginManager->RegisterPlugin(new EMStudio::LogWindowPlugin());
            pluginManager->RegisterPlugin(new EMStudio::CommandBarPlugin());
            pluginManager->RegisterPlugin(new EMStudio::ActionHistoryPlugin());
            pluginManager->RegisterPlugin(new EMStudio::MotionWindowPlugin());
            pluginManager->RegisterPlugin(new EMStudio::MorphTargetsWindowPlugin());
            pluginManager->RegisterPlugin(new EMStudio::TimeViewPlugin());
            pluginManager->RegisterPlugin(new EMStudio::AttachmentsPlugin());
            pluginManager->RegisterPlugin(new EMStudio::SceneManagerPlugin());
            pluginManager->RegisterPlugin(new EMStudio::NodeWindowPlugin());
            pluginManager->RegisterPlugin(new EMStudio::MotionEventsPlugin());
            pluginManager->RegisterPlugin(new EMStudio::MotionSetsWindowPlugin());
            pluginManager->RegisterPlugin(new EMStudio::NodeGroupsPlugin());
            pluginManager->RegisterPlugin(new EMStudio::AnimGraphPlugin());
            pluginManager->RegisterPlugin(new EMStudio::OpenGLRenderPlugin());
            pluginManager->RegisterPlugin(new EMotionFX::HitDetectionJointInspectorPlugin());
            pluginManager->RegisterPlugin(new EMotionFX::SkeletonOutlinerPlugin());
            pluginManager->RegisterPlugin(new EMotionFX::RagdollNodeInspectorPlugin());
            // Note: Cloth collider editor is disabled as it is in preview
            //pluginManager->RegisterPlugin(new EMotionFX::ClothJointInspectorPlugin());
        }

        //////////////////////////////////////////////////////////////////////////
        void SystemComponent::NotifyRegisterViews()
        {
            using namespace AzToolsFramework;

            // Construct data folder that is used by the tool for loading assets (images etc.).
            AZStd::string devRootPath;
            AzFramework::ApplicationRequests::Bus::BroadcastResult(devRootPath, &AzFramework::ApplicationRequests::GetEngineRoot);
            devRootPath += "Gems/EMotionFX/Assets/Editor/";
            AzFramework::ApplicationRequests::Bus::Broadcast(&AzFramework::ApplicationRequests::NormalizePathKeepCase, devRootPath);

            // Re-initialize EMStudio.
            int argc = 0;
            char** argv = nullptr;

            MysticQt::Initializer::Init("", devRootPath.c_str());
            EMStudio::Initializer::Init(qApp, argc, argv);

            InitializeEMStudioPlugins();

            // Get the MainWindow the first time so it is constructed
            EMStudio::GetManager()->GetMainWindow();

            EMStudio::GetManager()->ExecuteApp();

            AZStd::function<QWidget*()> windowCreationFunc = []()
                {
                    return EMStudio::GetMainWindow();
                };

            // Register EMotionFX window with the main editor.
            AzToolsFramework::ViewPaneOptions emotionFXWindowOptions;
            emotionFXWindowOptions.isPreview = true;
            emotionFXWindowOptions.isDeletable = true;
            emotionFXWindowOptions.isDockable = false;
#ifdef AZ_PLATFORM_APPLE
            emotionFXWindowOptions.detachedWindow = true;
#endif
            emotionFXWindowOptions.optionalMenuText = "Animation Editor (PREVIEW)";
            EditorRequests::Bus::Broadcast(&EditorRequests::RegisterViewPane, EMStudio::MainWindow::GetEMotionFXPaneName(), LyViewPane::CategoryTools, emotionFXWindowOptions, windowCreationFunc);
        }

        //////////////////////////////////////////////////////////////////////////
        bool SystemComponent::IsSystemActive(EditorAnimationSystemRequests::AnimationSystem systemType)
        {
            return (systemType == AnimationSystem::EMotionFX);
        }

        // AzToolsFramework::AssetBrowser::AssetBrowserInteractionNotificationBus::Handler
        AzToolsFramework::AssetBrowser::SourceFileDetails SystemComponent::GetSourceFileDetails(const char* fullSourceFileName)
        {
            using namespace AzToolsFramework::AssetBrowser;
            if (AZStd::wildcard_match("*.motionset", fullSourceFileName))
            {
                return SourceFileDetails("Editor/Images/AssetBrowser/MotionSet_16.png");
            }
            else if (AZStd::wildcard_match("*.animgraph", fullSourceFileName))
            {
                return SourceFileDetails("Editor/Images/AssetBrowser/AnimGraph_16.png");
            }
            return SourceFileDetails(); // no result
        }

#endif // EMOTIONFXANIMATION_EDITOR
    }
}
