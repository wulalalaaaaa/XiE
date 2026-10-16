#pragma once

#include "Graph/EntityView.h"
#include "Ink/CrystalInkController.h"
#include "Model/CrystalModels.h"
#include "Net/CrystalApiClient.h"
#include "Net/CrystalRealtimeClient.h"
#include "UI2D/SelectionModel.h"
#include "UI2D/ToolController.h"
#include "UI2D/WorkspaceCommandHistory.h"
#include "Workspace/ProjectController.h"

#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Crystal {

enum class WorkspacePanel { None, QueryBuilder, SchemaEditor };

struct ContextMenuState {
    bool open = false;
    Engine::Vec2F screenPosition{};
    std::string targetId;
    std::string targetType;
};

struct PropertyPickerState {
    bool open = false;
    std::string property;
    std::string filter;
    std::vector<std::string> entityIds;
};

struct InlineEditState {
    bool active = false;
    std::string targetType;
    std::string targetId;
    std::string draft;
};

class CrystalWorkspace {
public:
    struct SelectionBox {
        bool active = false;
        Engine::Vec2F start{};
        Engine::Vec2F end{};
    };

    explicit CrystalWorkspace(std::string serverUrl = "http://127.0.0.1:8787");
    ~CrystalWorkspace();

    [[nodiscard]] ApiResult Connect(const std::string& projectId);
    [[nodiscard]] ApiResult SwitchSpace(const std::string& spaceId);
    [[nodiscard]] ApiResult CreateNote(
        const std::string& title, Engine::Vec2F worldPosition);
    [[nodiscard]] ApiResult SaveView(const EntityView& view);
    [[nodiscard]] ApiResult DeleteView(const std::string& entityId, const std::string& spaceId);
    [[nodiscard]] ApiResult RenameEntity(const std::string& entityId, std::string title);
    [[nodiscard]] ApiResult UpdateEntity(CrystalEntity entity);
    [[nodiscard]] ApiResult UpdateCustomProperty(
        const std::string& entityId, std::string property, Engine::Json::Value value);
    [[nodiscard]] ApiResult DeleteEntity(const std::string& entityId);
    [[nodiscard]] ApiResult CreateRelationship(
        const std::string& sourceEntityId, const std::string& targetEntityId,
        RelationshipType type);
    [[nodiscard]] ApiResult DeleteRelationship(const std::string& relationshipId);
    [[nodiscard]] ApiResult SetRelationshipVisible(const std::string& relationshipId, bool visible);
    [[nodiscard]] ApiResult UpdateTaskStatus(const std::string& entityId, TaskStatus status);
    [[nodiscard]] ApiResult Search(const std::string& query, std::vector<CrystalEntity>& output);
    [[nodiscard]] ApiResult SetSearchQuery(std::string query);
    [[nodiscard]] ApiResult CreateStroke(const Engine::UI2D::InkStroke& stroke);
    [[nodiscard]] ApiResult DeleteStroke(const std::string& strokeId);
    [[nodiscard]] ApiResult AttachInkToEntity(
        const std::string& strokeId, const std::string& entityId);
    [[nodiscard]] ApiResult AttachInkToBoard(
        const std::string& strokeId, const std::string& boardId,
        Engine::Vec2F localOffset = {});
    [[nodiscard]] ApiResult GroupInkStrokes(
        const std::vector<std::string>& strokeIds, std::string name = {});
    void SelectRelationship(std::string relationshipId);
    [[nodiscard]] const std::string& SelectedRelationshipId() const noexcept {
        return m_SelectedRelationshipId;
    }
    [[nodiscard]] ApiResult Acquire(
        const std::string& sceneId, const std::string& workerId,
        const std::string& workerName, const std::string& task,
        const std::string& branch, const std::string& taskId = {},
        const std::string& taskNote = {});
    [[nodiscard]] ApiResult Heartbeat(
        const std::string& sceneId, const std::string& workerId);
    [[nodiscard]] ApiResult Release(const std::string& sceneId, const std::string& workerId);
    [[nodiscard]] ApiResult RenameSpace(const std::string& spaceId, std::string name);
    [[nodiscard]] ApiResult DuplicateSpace(const std::string& spaceId, std::string name);
    [[nodiscard]] ApiResult DeleteSpace(const std::string& spaceId);
    [[nodiscard]] ApiResult CreateBoard(BoardModel board, BoardModel* created = nullptr);
    [[nodiscard]] ApiResult UpdateBoard(BoardModel board);
    [[nodiscard]] ApiResult DeleteBoard(const std::string& boardId);
    [[nodiscard]] ApiResult AttachViewToBoard(
        const std::string& boardId, const std::string& viewId, Engine::Vec2F localPosition);
    [[nodiscard]] ApiResult DetachViewFromBoard(
        const std::string& boardId, const std::string& viewId);
    [[nodiscard]] ApiResult CreateLayer(UserLayerModel layer, UserLayerModel* created = nullptr);
    [[nodiscard]] ApiResult UpdateLayer(UserLayerModel layer);
    [[nodiscard]] ApiResult DeleteLayer(const std::string& layerId, bool deleteVisuals = false);
    [[nodiscard]] ApiResult CreateSchema(CrystalEntitySchema schema,
        CrystalEntitySchema* created = nullptr);
    [[nodiscard]] ApiResult DeleteSchema(const std::string& schemaId);
    [[nodiscard]] ApiResult CreateSchemaProperty(CrystalPropertyDefinition property,
        CrystalPropertyDefinition* created = nullptr);
    [[nodiscard]] ApiResult UpdateSchemaProperty(CrystalPropertyDefinition property);
    [[nodiscard]] ApiResult DeleteSchemaProperty(const std::string& propertyId);
    [[nodiscard]] ApiResult CreateQueryView(QueryViewModel query, QueryViewModel* created = nullptr);
    [[nodiscard]] ApiResult UpdateQueryView(QueryViewModel query);
    [[nodiscard]] ApiResult DeleteQueryView(const std::string& queryId);
    [[nodiscard]] ApiResult CreateDashboardWidget(
        DashboardWidgetModel widget, DashboardWidgetModel* created = nullptr);
    [[nodiscard]] ApiResult UpdateDashboardWidget(DashboardWidgetModel widget);
    [[nodiscard]] ApiResult DeleteDashboardWidget(const std::string& widgetId);
    [[nodiscard]] bool MoveSelectionLocal(Engine::Vec2F delta);
    [[nodiscard]] bool ResizeSelectedBoard(Engine::Vec2F size);
    [[nodiscard]] bool ResizeSelectedWidget(Engine::Vec2F size);
    [[nodiscard]] ApiResult ToggleSelectedBoardCollapsed();
    [[nodiscard]] ApiResult PersistVisualSelection();
    [[nodiscard]] ApiResult DeleteVisualSelection();
    [[nodiscard]] ApiResult DuplicateVisualSelection();
    void CopyVisualSelection();
    [[nodiscard]] bool PasteVisualSelectionLocal(Engine::Vec2F offset = {40.0f, 40.0f});
    [[nodiscard]] bool UndoVisualChange();
    [[nodiscard]] bool RedoVisualChange();
    [[nodiscard]] bool FocusSelection();
    [[nodiscard]] bool FitContent();
    void ToggleQueryBuilder();
    void ToggleSchemaEditor();
    void AddQueryFilter();
    void CycleLastQueryFilterOperator();
    void RemoveLastQueryFilter();
    void SetLastQueryFilterValue(Engine::Json::Value value);
    void BeginNewSchemaDraft();
    void AddSchemaDraftProperty();
    void RemoveSchemaDraftProperty();
    [[nodiscard]] WorkspacePanel Panel() const noexcept { return m_Panel; }
    [[nodiscard]] const QueryViewModel& QueryDraft() const noexcept { return m_QueryDraft; }
    [[nodiscard]] const CrystalEntitySchema& SchemaDraft() const noexcept { return m_SchemaDraft; }
    [[nodiscard]] ApiResult SaveQueryDraft();
    [[nodiscard]] ApiResult SaveSchemaDraft();
    void OpenContextMenu(Engine::Vec2F screenPosition, std::string targetId, std::string targetType) {
        m_ContextMenu = {true, screenPosition, std::move(targetId), std::move(targetType)};
    }
    void CloseContextMenu() noexcept { m_ContextMenu.open = false; }
    [[nodiscard]] const ContextMenuState& ContextMenu() const noexcept { return m_ContextMenu; }
    void OpenPropertyPicker(std::string property, std::vector<std::string> entityIds) {
        m_PropertyPicker = {true, std::move(property), {}, std::move(entityIds)};
    }
    void ClosePropertyPicker() noexcept { m_PropertyPicker.open = false; }
    void SetPropertyPickerFilter(std::string filter) { m_PropertyPicker.filter = std::move(filter); }
    [[nodiscard]] const PropertyPickerState& PropertyPicker() const noexcept { return m_PropertyPicker; }
    [[nodiscard]] const InlineEditState& InlineEdit() const noexcept { return m_InlineEdit; }
    void BeginInlineEdit(std::string targetType, std::string targetId, std::string draft) {
        m_InlineEdit = {true, std::move(targetType), std::move(targetId), std::move(draft)};
    }
    void SetInlineEditDraft(std::string draft) { m_InlineEdit.draft = std::move(draft); }
    void CancelInlineEdit() noexcept { m_InlineEdit = {}; }
    [[nodiscard]] ApiResult CommitInlineEdit();
    [[nodiscard]] bool CanUndoVisualChange() const noexcept { return m_VisualHistory.CanUndo(); }
    [[nodiscard]] bool CanRedoVisualChange() const noexcept { return m_VisualHistory.CanRedo(); }
    [[nodiscard]] const std::string& ActiveLayerId() const noexcept { return m_ActiveLayerId; }
    void SetActiveLayerId(std::string layerId) { m_ActiveLayerId = std::move(layerId); }
    [[nodiscard]] bool IsLayerLocked(const std::string& layerId) const noexcept;
    [[nodiscard]] const std::string& BoardHoverId() const noexcept { return m_BoardHoverId; }
    [[nodiscard]] bool BoardHoverLocked() const noexcept { return m_BoardHoverLocked; }
    void SetBoardHover(std::string boardId, bool locked) {
        m_BoardHoverId = std::move(boardId);
        m_BoardHoverLocked = locked;
    }
    void ClearBoardHover() noexcept { m_BoardHoverId.clear(); m_BoardHoverLocked = false; }
    [[nodiscard]] const SelectionBox& SelectionRectangle() const noexcept { return m_SelectionBox; }
    void BeginSelectionRectangle(Engine::Vec2F screen) noexcept {
        m_SelectionBox = {true, screen, screen};
    }
    void UpdateSelectionRectangle(Engine::Vec2F screen) noexcept {
        if (m_SelectionBox.active) m_SelectionBox.end = screen;
    }
    void EndSelectionRectangle() noexcept { m_SelectionBox.active = false; }

    void SetViewport(Engine::Vec2F size) noexcept;
    [[nodiscard]] CrystalApiClient& Api() noexcept { return m_Api; }
    [[nodiscard]] const CrystalApiClient& Api() const noexcept { return m_Api; }
    [[nodiscard]] ProjectController& Projects() noexcept { return m_Projects; }
    [[nodiscard]] const ProjectController& Projects() const noexcept { return m_Projects; }
    [[nodiscard]] CrystalInkController& Ink() noexcept { return m_Ink; }
    [[nodiscard]] const CrystalInkController& Ink() const noexcept { return m_Ink; }
    [[nodiscard]] Engine::UI2D::SelectionModel& Selection() noexcept { return m_Selection; }
    [[nodiscard]] const Engine::UI2D::SelectionModel& Selection() const noexcept { return m_Selection; }
    [[nodiscard]] Engine::UI2D::ToolController& Tools() noexcept { return m_Tools; }
    [[nodiscard]] const Engine::UI2D::ToolController& Tools() const noexcept { return m_Tools; }
    [[nodiscard]] const std::vector<CrystalRelationship>& Relationships() const noexcept {
        return m_Projects.Workspace().relationships;
    }
    [[nodiscard]] const std::vector<CrystalEntitySchema>& Schemas() const noexcept {
        return m_Projects.Workspace().schemas;
    }
    [[nodiscard]] const std::vector<BoardModel>& Boards() const noexcept {
        return m_Projects.Workspace().boards;
    }
    [[nodiscard]] const std::vector<UserLayerModel>& Layers() const noexcept {
        return m_Projects.Workspace().userLayers;
    }
    [[nodiscard]] const std::vector<QueryViewModel>& QueryViews() const noexcept {
        return m_Projects.Workspace().queryViews;
    }
    [[nodiscard]] const std::vector<DashboardWidgetModel>& DashboardWidgets() const noexcept {
        return m_Projects.Workspace().dashboardWidgets;
    }
    [[nodiscard]] const std::vector<ActivityModel>& Activities() const noexcept {
        return m_Projects.Workspace().activities;
    }
    [[nodiscard]] const std::string& SearchQuery() const noexcept { return m_SearchQuery; }
    [[nodiscard]] const std::vector<CrystalEntity>& SearchResults() const noexcept { return m_SearchResults; }
    [[nodiscard]] const std::string& ConnectionStatus() const noexcept { return m_ConnectionStatus; }
    [[nodiscard]] const std::string& Notification() const noexcept { return m_Notification; }
    void SetLocalNotification(std::string notification) { m_Notification = std::move(notification); }
    void CycleDisplayMode() noexcept {
        m_DisplayMode = m_DisplayMode == DisplayMode::All ? DisplayMode::Graph
            : m_DisplayMode == DisplayMode::Graph ? DisplayMode::Draw
            : m_DisplayMode == DisplayMode::Draw ? DisplayMode::Presentation : DisplayMode::All;
    }
    [[nodiscard]] DisplayMode GetDisplayMode() const noexcept { return m_DisplayMode; }
    [[nodiscard]] const std::string& CurrentSceneStatus(const std::string& sceneId) const;

    // Must be called by the application/UI thread once per frame. Network
    // callbacks only enqueue CrystalRealtimeEvent values.
    void SetRealtimeWakeCallback(std::function<void()> callback) {
        m_Realtime.SetWakeCallback(std::move(callback));
    }
    [[nodiscard]] bool PumpRealtimeEvents();
    void ApplyRealtimeEvent(const CrystalRealtimeEvent& event);
    void LoadSyntheticPerformanceFixtureForDebug();
    void ForceRealtimeDisconnectForDebug() noexcept {
        m_Realtime.ForceDisconnectForDebug();
        m_Notification = "DEBUG: REALTIME DISCONNECT INJECTED";
    }

private:
    struct VisualState {
        std::vector<EntityView> views;
        std::vector<BoardModel> boards;
        std::vector<Engine::UI2D::InkStroke> strokes;
        std::vector<DashboardWidgetModel> widgets;
    };

    [[nodiscard]] VisualState CaptureVisualState() const;
    void ApplyVisualState(const VisualState& state);
    void RecordVisualChange(std::string label, VisualState before, VisualState after);
    [[nodiscard]] bool SelectionHas(Engine::UI2D::SelectionKind kind, const std::string& id) const;
    [[nodiscard]] bool SelectionHasBoard(const std::string& id) const;
    [[nodiscard]] bool SelectionHasInk(const Engine::UI2D::InkStroke& stroke) const;
    [[nodiscard]] std::string NewLocalId(std::string_view prefix) const;
    [[nodiscard]] ApiResult RequireConnected(std::string_view operation);
    [[nodiscard]] ApiResult FinishApiResult(std::string_view operation, ApiResult result);
    void ApplyAuthoritativeSnapshot(const Engine::Json::Value& workspace);
    void ApplyConnectionState(Engine::Network::WebSocketState state);
    void MaybeHeartbeat();
    void ReplaceEntity(const CrystalEntity& entity);
    void ReplaceView(const EntityView& view);
    void RefreshQueryResults();
    void RefreshWorkerLockSummaries();

    CrystalApiClient m_Api;
    ProjectController m_Projects;
    CrystalRealtimeClient m_Realtime;
    CrystalInkController m_Ink;
    Engine::UI2D::SelectionModel m_Selection;
    Engine::UI2D::ToolController m_Tools;
    std::string m_ConnectionStatus = "Disconnected";
    std::string m_Notification;
    std::string m_ProjectId;
    std::string m_SearchQuery;
    std::vector<CrystalEntity> m_SearchResults;
    std::string m_SelectedRelationshipId;
    DisplayMode m_DisplayMode = DisplayMode::All;
    std::string m_ActiveLayerId = "Main";
    SelectionBox m_SelectionBox;
    VisualState m_Clipboard;
    bool m_HasClipboard = false;
    Engine::UI2D::WorkspaceCommandHistory m_VisualHistory;
    WorkspacePanel m_Panel = WorkspacePanel::None;
    QueryViewModel m_QueryDraft;
    CrystalEntitySchema m_SchemaDraft;
    ContextMenuState m_ContextMenu;
    PropertyPickerState m_PropertyPicker;
    InlineEditState m_InlineEdit;
    std::string m_BoardHoverId;
    bool m_BoardHoverLocked = false;
    std::unordered_map<std::string, std::string> m_HeldLocks;
    std::chrono::steady_clock::time_point m_NextHeartbeat{};
    mutable std::mutex m_Mutex;
};

} // namespace Crystal
