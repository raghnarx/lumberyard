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

#include "EditorCommon.h"

#include <AzToolsFramework/API/ToolsApplicationAPI.h>

#include <IFont.h>

class RulerWidget;

class ViewportWidget
    : public QViewport
    , private AzToolsFramework::EditorPickModeNotificationBus::Handler
    , private FontNotificationBus::Handler
{
    Q_OBJECT

public: // types

    enum DrawElementBorders
    {
        DrawElementBorders_Unselected = 0x1,
        DrawElementBorders_Visual     = 0x2,
        DrawElementBorders_Parent     = 0x4,
        DrawElementBorders_Hidden     = 0x8,
    };

public: // member functions

    explicit ViewportWidget(EditorWindow* parent);
    virtual ~ViewportWidget();

    ViewportInteraction* GetViewportInteraction();

    bool IsDrawingElementBorders(uint32 flags) const;
    void ToggleDrawElementBorders(uint32 flags);

    void UpdateViewportBackground();

    void ActiveCanvasChanged();
    void EntityContextChanged();

    //! Flags the viewport display as needing a refresh
    void Refresh();

    //! Used to clear the viewport and prevent rendering until the viewport layout updates
    void ClearUntilSafeToRedraw();

    //! Set whether to render the canvas
    void SetRedrawEnabled(bool enabled);

    //! Get the canvas scale factor being used for the preview mode
    float GetPreviewCanvasScale() { return m_previewCanvasScale; }

    //! Used by ViewportInteraction for drawing
    ViewportHighlight* GetViewportHighlight() { return m_viewportHighlight.get(); }

    bool IsInObjectPickMode() { return m_inObjectPickMode; }
    void PickItem(AZ::EntityId entityId);

    QWidget* CreateViewportWithRulersWidget(QWidget* parent);
    void ShowRulers(bool show);
    bool AreRulersShown() { return m_rulersVisible; }
    void RefreshRulers();
    void SetRulerCursorPositions(const QPoint& globalPos);

    void ShowGuides(bool show);
    bool AreGuidesShown() { return m_guidesVisible; }

protected:

    void contextMenuEvent(QContextMenuEvent* e) override;

private slots:

    void HandleSignalRender(const SRenderContext& context);
    void UserSelectionChanged(HierarchyItemRawPtrList* items);

    void EnableCanvasRender();

    //! Called by a timer at the max frequency that we want to refresh the display
    void RefreshTick();

protected:

    //! Forwards mouse press events to ViewportInteraction.
    //!
    //! Event is NOT propagated to parent class.
    void mousePressEvent(QMouseEvent* ev) override;

    //! Forwards mouse move events to ViewportInteraction.
    //!
    //! Event is NOT propagated to parent class.
    void mouseMoveEvent(QMouseEvent* ev) override;

    //! Forwards mouse release events to ViewportInteraction.
    //!
    //! Event is NOT propagated to parent class.
    void mouseReleaseEvent(QMouseEvent* ev) override;

    //! Forwards mouse wheel events to ViewportInteraction.
    //!
    //! Event is propagated to parent class.
    void wheelEvent(QWheelEvent* ev) override;

    //! Prevents shortcuts from interfering with preview mode.
    bool event(QEvent* ev) override;

    //! Key press event from Qt
    void keyPressEvent(QKeyEvent* event) override;

    //! Key release event from Qt
    void keyReleaseEvent(QKeyEvent* event) override;

    void focusOutEvent(QFocusEvent* ev) override;

private: // member functions
    // EditorPickModeNotificationBus
    void OnEntityPickModeStarted() override;
    void OnEntityPickModeStopped() override;

    // FontNotifications
    void OnFontsReloaded() override;
    void OnFontTextureUpdated(IFFont* font) override;
    // ~FontNotifications

    //! Render the viewport when in edit mode
    void RenderEditMode();

    //! Render the viewport when in preview mode
    void RenderPreviewMode();

    //! Create shortcuts for manipulating the viewport
    void SetupShortcuts();

    //! Do the Qt stuff to hide/show the rulers
    void ApplyRulerVisibility();

private: // data

    void resizeEvent(QResizeEvent* ev) override;

    double WidgetToViewportFactor() const
    {
#if defined(AZ_PLATFORM_WINDOWS)
        // Needed for high DPI mode on windows
        return devicePixelRatioF();
#else
        return 1.0f;
#endif
    }

    QPointF WidgetToViewport(const QPointF &point) const;

    EditorWindow* m_editorWindow;

    std::unique_ptr< ViewportInteraction > m_viewportInteraction;
    std::unique_ptr< ViewportAnchor > m_viewportAnchor;
    std::unique_ptr< ViewportHighlight > m_viewportHighlight;
    std::unique_ptr< ViewportCanvasBackground > m_viewportBackground;
    std::unique_ptr< ViewportPivot > m_viewportPivot;

    uint32 m_drawElementBordersFlags;
    bool m_refreshRequested;
    bool m_canvasRenderIsEnabled;
    QTimer m_updateTimer;

    float m_previewCanvasScale;

    bool m_inObjectPickMode = false;

    RulerWidget* m_rulerHorizontal = nullptr;
    RulerWidget* m_rulerVertical = nullptr;
    QWidget* m_rulerCorner = nullptr;
    bool     m_rulersVisible;
    bool     m_guidesVisible;
    bool     m_fontTextureHasChanged = false;
};
