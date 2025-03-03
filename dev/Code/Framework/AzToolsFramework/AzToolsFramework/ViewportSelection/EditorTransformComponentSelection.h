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

#include <AzCore/std/containers/vector.h>
#include <AzCore/std/optional.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzFramework/Components/CameraBus.h>
#include <AzToolsFramework/API/EditorCameraBus.h>
#include <AzToolsFramework/Commands/EntityManipulatorCommand.h>
#include <AzToolsFramework/ComponentMode/EditorComponentModeBus.h>
#include <AzToolsFramework/Manipulators/BaseManipulator.h>
#include <AzToolsFramework/ToolsComponents/EditorLockComponentBus.h>
#include <AzToolsFramework/ToolsComponents/EditorVisibilityBus.h>
#include <AzToolsFramework/UI/PropertyEditor/PropertyEditorAPI.h>
#include <AzToolsFramework/Viewport/EditorContextMenu.h>
#include <AzToolsFramework/Viewport/ViewportMessages.h>
#include <AzToolsFramework/ViewportSelection/EditorBoxSelect.h>
#include <AzToolsFramework/ViewportSelection/EditorHelpers.h>
#include <AzToolsFramework/ViewportSelection/EditorTransformComponentSelectionRequestBus.h>

namespace AzToolsFramework
{
    class EditorVisibleEntityDataCache;

    /// Generic wrapper to handle specific manipulators controlling 1-* entities.
    struct EntityIdManipulators
    {
        /// Entity related data required by manipulators during action.
        struct Lookup
        {
            AZ::Transform m_initial; /// Transform of Entity at mouse down on manipulator.
        };

        AZStd::unordered_map<AZ::EntityId, Lookup> m_lookups; ///< Mapping between EntityId and the transform of the
                                                              ///< entity at the point the manipulator started adjusting it.
        AZStd::unique_ptr<Manipulators> m_manipulators; ///< The manipulator aggregate currently in use.
    };

    using EntityIdSet = AZStd::unordered_set<AZ::EntityId>; ///< Alias for unordered_set of EntityIds.

    /// Store translation and orientation only (no scale).
    struct Frame
    {
        AZ::Vector3 m_translation = AZ::Vector3::CreateZero(); ///< Position of frame.
        AZ::Quaternion m_orientation = AZ::Quaternion::CreateIdentity(); ///< Orientation of frame.
    };

    /// Temporary manipulator frame used during selection.
    struct OptionalFrame
    {
        /// What part of the transform did we pick (when using ditto on
        /// the manipulator). This will depend on the transform mode we're in.
        struct PickType
        {
            enum : AZ::u8
            {
                None = 0,
                Orientation = 1 << 0,
                Translation = 1 << 1
            };
        };

        bool HasTranslationOverride() const;
        bool HasOrientationOverride() const;
        bool HasTransformOverride() const;
        bool HasEntityOverride() const;
        bool PickedTranslation() const;
        bool PickedOrientation() const;

        /// Clear all state associated with the frame.
        void Reset();
        /// Clear only picked translation state.
        void ResetPickedTranslation();
        /// Clear only picked orientation state.
        void ResetPickedOrientation();

        AZ::EntityId m_pickedEntityIdOverride; ///< 'Picked' Entity - frame and parent space relative to this if active.
        AZStd::optional<AZ::Vector3> m_translationOverride; ///< Translation override, if set, reset when selection is empty.
        AZStd::optional<AZ::Quaternion> m_orientationOverride; ///< Orientation override, if set, reset when selection is empty.
        AZ::u8 m_pickTypes = PickType::None; ///< What mode(s) were we in when picking an EntityId override.
    };

    /// What frame/space is the manipulator currently operating in.
    enum class ReferenceFrame
    {
        Local,
        Parent,
    };

    /// Entity selection/interaction handling.
    /// Provide a suite of functionality for manipulating entities, primarily through their TransformComponent.
    class EditorTransformComponentSelection
        : public ViewportInteraction::ViewportSelectionRequests
        , private EditorEventsBus::Handler
        , private EditorTransformComponentSelectionRequestBus::Handler
        , private ToolsApplicationNotificationBus::Handler
        , private Camera::EditorCameraNotificationBus::Handler
        , private PropertyEditorChangeNotificationBus::Handler
        , private ComponentModeFramework::EditorComponentModeNotificationBus::Handler
        , private EditorContextVisibilityNotificationBus::Handler
        , private EditorContextLockComponentNotificationBus::Handler
        , private EditorManipulatorCommandUndoRedoRequestBus::Handler
    {
    public:
        AZ_CLASS_ALLOCATOR_DECL

        EditorTransformComponentSelection() = default;
        explicit EditorTransformComponentSelection(const EditorVisibleEntityDataCache* entityDataCache);
        EditorTransformComponentSelection(const EditorTransformComponentSelection&) = delete;
        EditorTransformComponentSelection& operator=(const EditorTransformComponentSelection&) = delete;
        virtual ~EditorTransformComponentSelection();

        /// Register entity manipulators with the ManipulatorManager.
        /// After being registered, the entity manipulators will draw and check for input.
        void RegisterManipulator();
        /// Unregister entity manipulators with the ManipulatorManager.
        /// No longer draw or respond to input.
        void UnregisterManipulator();

        /// ViewportInteraction::ViewportSelectionRequests
        /// Intercept all viewport mouse events and respond to inputs.
        bool HandleMouseInteraction(
            const ViewportInteraction::MouseInteractionEvent& mouseInteraction) override;
        void DisplayViewportSelection(
            const AzFramework::ViewportInfo& viewportInfo,
            AzFramework::DebugDisplayRequests& debugDisplay) override;
        void DisplayViewportSelection2d(
            const AzFramework::ViewportInfo& viewportInfo,
            AzFramework::DebugDisplayRequests& debugDisplay) override;

        /// Add an entity to the current selection
        void AddEntityToSelection(AZ::EntityId entityId);
        /// Remove an entity from the current selection
        void RemoveEntityFromSelection(AZ::EntityId entityId);

    private:
        void CreateEntityIdManipulators();
        void CreateTranslationManipulators();
        void CreateRotationManipulators();
        void CreateScaleManipulators();
        void RegenerateManipulators();

        void ClearManipulatorTranslationOverride();
        void ClearManipulatorOrientationOverride();
        /// Handle an event triggered by the user to clear any manipulator overrides.
        /// Delegate to either translation or orientation reset/clear depending on the state we're in.
        void DelegateClearManipulatorOverride();

        void CopyTranslationToSelectedEntitiesIndividual(const AZ::Vector3& translation);
        void CopyTranslationToSelectedEntitiesGroup(const AZ::Vector3& translation);
        void CopyOrientationToSelectedEntitiesIndividual(const AZ::Quaternion& orientation);
        void CopyOrientationToSelectedEntitiesGroup(const AZ::Quaternion& orientation);
        void ResetOrientationForSelectedEntitiesLocal();
        void ResetTranslationForSelectedEntitiesLocal();
        void CopyScaleToSelectedEntitiesIndividualWorld(const AZ::Vector3& scale);
        void CopyScaleToSelectedEntitiesIndividualLocal(const AZ::Vector3& scale);
        void ToggleCenterPivotSelection();

        bool IsEntitySelected(AZ::EntityId entityId) const;
        void SetSelectedEntities(const EntityIdList& entityIds);
        void DeselectEntities();
        bool SelectDeselect(AZ::EntityId entityIdUnderCursor);

        void RefreshSelectedEntityIds();
        void RefreshSelectedEntityIds(const EntityIdList& selectedEntityIds);
        void InitializeManipulators(Manipulators& manipulators);

        void SetupBoxSelect();

        void RegisterActions();
        void UnregisterActions();

        void BeginRecordManipulatorCommand();
        void EndRecordManipulatorCommand();

        // If entity state has changed, make sure to refresh the selection and update
        // the active manipulators accordingly
        void CheckDirtyEntityIds();
        void RefreshSelectedEntityIdsAndRegenerateManipulators();

        // Builds an EntityManipulatorCommand::State based on the current state of the
        // EditorTransformComponentSelection (manipulators, frame overrides and picked entities).
        // Note: Precondition of calling CreateManipulatorCommandStateFromSelf is
        // m_entityIdManipulators.m_manipulators must be valid.
        EntityManipulatorCommand::State CreateManipulatorCommandStateFromSelf() const;

        // Helper to record the manipulator before and after a deselect so it
        // can be returned to its previous state after an undo/redo operation
        void CreateEntityManipulatorDeselectCommand(ScopedUndoBatch& undoBatch);

        // EditorTransformComponentSelectionRequestBus
        Mode GetTransformMode() override;
        void SetTransformMode(Mode mode) override;
        void RefreshManipulators(RefreshType refreshType) override;
        AZStd::optional<AZ::Transform> GetManipulatorTransform() override;
        void OverrideManipulatorOrientation(const AZ::Quaternion& orientation) override;
        void OverrideManipulatorTranslation(const AZ::Vector3& translation) override;

        // EditorManipulatorCommandUndoRedoRequestBus
        void UndoRedoEntityManipulatorCommand(
            AZ::u8 pivotOverride, const AZ::Transform& transform, AZ::EntityId entityId) override;

        // AzToolsFramework::EditorEvents
        void PopulateEditorGlobalContextMenu(QMenu *menu, const AZ::Vector2& point, int flags) override;
        void OnEscape() override;

        // ToolsApplicationNotificationBus
        void BeforeEntitySelectionChanged() override;
        void AfterEntitySelectionChanged(
            const EntityIdList& newlySelectedEntities, const EntityIdList& newlyDeselectedEntities) override;

        // AzToolsFramework::PropertyEditorChangeNotificationBus
        void OnComponentPropertyChanged(AZ::Uuid componentType) override;

        // Camera::EditorCameraNotificationBus
        void OnViewportViewEntityChanged(const AZ::EntityId& newViewId) override;

        // EditorContextVisibilityNotificationBus
        void OnEntityVisibilityChanged(AZ::EntityId entityId, bool visibility) override;

        // EditorContextLockComponentNotificationBus
        void OnEntityLockChanged(AZ::EntityId entityId, bool locked) override;

        // EditorComponentModeNotificationBus
        void EnteredComponentMode(const AZStd::vector<AZ::Uuid>& componentModeTypes) override;
        void LeftComponentMode(const AZStd::vector<AZ::Uuid>& componentModeTypes) override;

        AZ::EntityId m_hoveredEntityId; ///< What EntityId is the mouse currently hovering over (if any).
        AZ::EntityId m_cachedEntityIdUnderCursor; ///< Store the EntityId on each mouse move for use in Display.
        AZ::EntityId m_editorCameraComponentEntityId; ///< The EditorCameraComponent EntityId if it is set.
        EntityIdSet m_selectedEntityIds; ///< Represents the current entities in the selection.

        const EditorVisibleEntityDataCache* m_entityDataCache = nullptr; ///< A cache of packed EntityData that can be
                                                                         ///< iterated over efficiently without the need
                                                                         ///< to make individual EBus calls.
        AZStd::unique_ptr<EditorHelpers> m_editorHelpers; ///< Editor visualization of entities (icons, shapes, debug visuals etc).
        EntityIdManipulators m_entityIdManipulators; ///< Mapping from a Manipulator to potentially many EntityIds.

        EditorBoxSelect m_boxSelect; ///< Type responsible for handling box select.
        AZStd::unique_ptr<EntityManipulatorCommand> m_manipulatorMoveCommand; ///< Track adjustments to manipulator translation and orientation (during mouse press/move).
        AZStd::vector<AZStd::unique_ptr<QAction>> m_actions; ///< What actions are tied to this handler.
        ViewportInteraction::KeyboardModifiers m_previousModifiers; ///< What modifiers were held last frame.
        EditorContextMenu m_contextMenu; ///< Viewport right click context menu.
        OptionalFrame m_pivotOverrideFrame; ///< Has a pivot override been set.
        Mode m_mode = Mode::Translation; ///< Manipulator mode - default to translation.
        Pivot m_pivotMode = Pivot::Object; ///< Entity pivot mode - default to object (authored root).
        ReferenceFrame m_referenceFrame = ReferenceFrame::Local; ///< What reference frame is the Manipulator currently operating in.
        Frame m_axisPreview; ///< Axes of entity at the time of mouse down to indicate delta of translation.
        bool m_triedToRefresh = false; ///< Did a refresh event occur to recalculate the current Manipulator transform.
        bool m_didSetSelectedEntities = false; ///< Was EditorTransformComponentSelection responsible for the most recent entity selection change.
        bool m_selectedEntityIdsAndManipulatorsDirty = false; ///< Do the active manipulators need to recalculated after a modification (lock/visibility etc).
    };
} // namespace AzToolsFramework