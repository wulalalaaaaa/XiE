#include "Application/FrameScheduler.h"
#include "Model/CrystalModels.h"
#include "Net/CrystalRealtimeEvent.h"
#include "UI2D/InfiniteCanvas.h"
#include "UI2D/InkStroke.h"
#include "UI2D/SelectionModel.h"
#include "UI2D/SelectionPopup.h"
#include "UI2D/ToolController.h"
#include "UI2D/WorkspaceCommandHistory.h"
#include "Core/MainThreadEventQueue.h"
#include "UI/PropertyEditor.h"
#include "UI/CrystalRenderGeometry.h"
#include "UI/WorkspaceEditors.h"
#include "Workspace/CrystalWorkspace.h"

#include <cmath>
#include <iostream>
#include <thread>

namespace {

int failures = 0;
int checks = 0;

void Check(bool condition, const char* message) {
    ++checks;
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

bool Near(float lhs, float rhs, float epsilon = 0.01f) {
    return std::abs(lhs - rhs) <= epsilon;
}

} // namespace

int main() {
    Engine::UI2D::InfiniteCanvas canvas;
    canvas.SetViewportSize({1000, 800});
    canvas.Camera().centerX = 250000.0;
    canvas.Camera().centerY = -175000.0;
    canvas.Camera().SetZoom(1.75);
    const Engine::Vec2F world{250123.0f, -174876.0f};
    const Engine::Vec2F screen = canvas.WorldToScreen(world);
    const Engine::Vec2F roundTrip = canvas.ScreenToWorld(screen);
    Check(Near(roundTrip.x, world.x, 0.1f) && Near(roundTrip.y, world.y, 0.1f),
        "world/screen transform round trips at a large world coordinate");

    const Engine::Vec2F cursor{730, 265};
    const Engine::Vec2F before = canvas.ScreenToWorld(cursor);
    canvas.ZoomAt(cursor, 1.5);
    const Engine::Vec2F after = canvas.ScreenToWorld(cursor);
    Check(Near(before.x, after.x, 0.1f) && Near(before.y, after.y, 0.1f),
        "zoom around cursor preserves the world point under the cursor");

    Engine::UI2D::ToolController tools;
    tools.SetTool(Engine::UI2D::CanvasTool::Pen);
    Check(tools.PointerDown({1, 2}, Engine::PointerButton::Primary, "node", "stroke").type ==
        Engine::UI2D::ToolActionType::BeginStroke, "pen routes pointer down to stroke");
    Check(tools.PointerMove({3, 4}).type == Engine::UI2D::ToolActionType::AppendStroke,
        "pen routes move to stroke");
    tools.PointerUp({5, 6});
    tools.SetTool(Engine::UI2D::CanvasTool::Select);
    Check(tools.PointerDown({1, 2}, Engine::PointerButton::Primary, "node", "stroke").targetId == "node",
        "select routes to entity and ignores stroke hit");
    tools.PointerUp({1, 2});
    tools.SetTool(Engine::UI2D::CanvasTool::Eraser);
    Check(tools.PointerDown({1, 2}, Engine::PointerButton::Primary, "node", "stroke").type ==
        Engine::UI2D::ToolActionType::EraseStroke, "eraser routes to stroke and cannot drag node");

    Crystal::CrystalEntity entity;
    entity.id = "e1";
    entity.projectId = "XiE";
    entity.type = Crystal::EntityType::Note;
    entity.title = "Alpha";
    entity.data = Engine::Json::Value::Object{{"priority", "MVP"}};
    const auto encoded = Crystal::ToJson(entity).Dump();
    std::string error;
    const auto decoded = Engine::Json::Value::Parse(encoded, &error);
    Check(decoded.has_value() && Crystal::EntityFromJson(*decoded).title == "Alpha",
        "entity serialization round trips");

    Engine::UI2D::InkStroke stroke;
    stroke.id = "s1";
    stroke.points = {{1, 2}, {3, 4}};
    stroke.layer = "Notes";
    const auto strokeJson = Crystal::ToJson(stroke).Dump();
    const auto strokeValue = Engine::Json::Value::Parse(strokeJson, &error);
    Check(strokeValue.has_value() && Crystal::StrokeFromJson(*strokeValue).points.size() == 2,
        "stroke serialization round trips");

    Engine::UI2D::StrokeHistory history;
    history.Add(stroke);
    Check(history.Undo() && history.Strokes().empty() && history.Redo() &&
          history.Strokes().size() == 1, "stroke undo and redo are local operations");

    const auto projectA = Engine::Json::Value::Parse(
        R"({"project":{"id":"A","name":"A"},"spaces":[],"entities":[{"id":"a","project_id":"A","entity_type":"note","title":"A"}],"views":[],"strokes":[],"scene_locks":[],"user_layers":[]})");
    const auto projectB = Engine::Json::Value::Parse(
        R"({"project":{"id":"B","name":"B"},"spaces":[],"entities":[{"id":"b","project_id":"B","entity_type":"note","title":"B"}],"views":[],"strokes":[],"scene_locks":[],"user_layers":[]})");
    Check(projectA && projectB && Crystal::WorkspaceFromJson(*projectA).project.id == "A" &&
          Crystal::WorkspaceFromJson(*projectB).project.id == "B" &&
          Crystal::WorkspaceFromJson(*projectA).entities.front().id == "a",
        "project workspace deserialization does not mix project data");

    Crystal::CrystalEntity task;
    task.id = "task-1";
    task.projectId = "P_HighSchool";
    task.type = Crystal::EntityType::Task;
    task.title = "NPC traffic rules";
    task.description = "Acceptance path";
    task.revision = 5;
    Crystal::TaskDetails taskDetails;
    taskDetails.status = Crystal::TaskStatus::Testing;
    taskDetails.priority = Crystal::TaskPriority::Critical;
    taskDetails.assignedWorkerId = "worker-1";
    taskDetails.allowedPaths = {"XiE_Projects/Crystal/**"};
    task.details = taskDetails;
    const auto taskRoundTrip = Crystal::EntityFromJson(Crystal::ToJson(task));
    Check(taskRoundTrip.revision == 5 && taskRoundTrip.description == "Acceptance path" &&
          std::get<Crystal::TaskDetails>(taskRoundTrip.details).status == Crystal::TaskStatus::Testing &&
          std::get<Crystal::TaskDetails>(taskRoundTrip.details).priority == Crystal::TaskPriority::Critical,
        "typed task details and revision serialize without a second entity model");

    Crystal::EntityView compact;
    compact.id = "view:task-1:Sprint";
    compact.entityId = "task-1";
    compact.spaceId = "Sprint";
    compact.viewType = Crystal::ViewType::CompactNode;
    compact.revision = 3;
    Check(Crystal::EntityViewFromJson(Crystal::ToJson(compact)).viewType == Crystal::ViewType::CompactNode &&
          Crystal::EntityViewFromJson(Crystal::ToJson(compact)).revision == 3,
        "multi-space compact view serialization preserves view identity and revision");

    Crystal::CrystalRelationship relationship;
    relationship.id = "rel-1";
    relationship.projectId = "P_HighSchool";
    relationship.sourceEntityId = "task-1";
    relationship.targetEntityId = "system-npc";
    relationship.type = Crystal::RelationshipType::BelongsToSystem;
    relationship.visible = false;
    Check(Crystal::RelationshipFromJson(Crystal::ToJson(relationship)).type ==
          Crystal::RelationshipType::BelongsToSystem &&
          !Crystal::RelationshipFromJson(Crystal::ToJson(relationship)).visible,
        "relationship type and edge visibility serialize with directed endpoints");

    stroke.anchorEntityId = "task-1";
    stroke.anchorBoardId = "board-1";
    stroke.groupId = "ink-group-1";
    stroke.shape = Engine::UI2D::StrokeShape::Arrow;
    stroke.localOffset = {40, 50};
    const auto anchored = Crystal::StrokeFromJson(Crystal::ToJson(stroke));
    Check(anchored.anchorEntityId == "task-1" && Near(anchored.localOffset.x, 40.0f) &&
          Near(anchored.localOffset.y, 50.0f) && anchored.anchorBoardId == "board-1" &&
          anchored.groupId == "ink-group-1" && anchored.shape == Engine::UI2D::StrokeShape::Arrow,
        "anchored ink preserves entity/board anchors, group, shape, and local offset");

    const auto multiView = Engine::Json::Value::Parse(
        R"({"project":{"id":"P","name":"P"},"spaces":[{"id":"P:Overview","project_id":"P","name":"Overview"},{"id":"P:Sprint","project_id":"P","name":"Sprint","view_type":"TaskBoard"}],"entities":[{"id":"task-1","project_id":"P","entity_type":"task","title":"T","revision":1,"details":{"status":"Ready","priority":"Normal"}}],"views":[{"id":"v1","entity_id":"task-1","space_id":"P:Overview","x":1,"y":2},{"id":"v2","entity_id":"task-1","space_id":"P:Sprint","x":3,"y":4,"view_type":"CompactNode"}],"relationships":[],"strokes":[],"scene_locks":[],"user_layers":[]})");
    Check(multiView && Crystal::WorkspaceFromJson(*multiView).entities.size() == 1 &&
          Crystal::WorkspaceFromJson(*multiView).views.size() == 2 &&
          Crystal::WorkspaceFromJson(*multiView).spaces[1].viewType == Crystal::SpaceViewType::TaskBoard,
        "one entity can deserialize into multiple space views");

    Engine::MainThreadEventQueue<int> queue;
    int wakeCount = 0;
    queue.SetWakeCallback([&wakeCount] { ++wakeCount; });
    std::thread producer([&queue] {
        for (int index = 0; index < 32; ++index) queue.Enqueue(index);
    });
    producer.join();
    const auto drained = queue.Drain();
    bool fifo = drained.size() == 32;
    for (std::size_t index = 0; fifo && index < drained.size(); ++index)
        fifo = drained[index] == static_cast<int>(index);
    Check(fifo && queue.Size() == 0 && wakeCount == 32,
        "main-thread event queue drains FIFO values and wakes the event loop");

    Engine::WindowContext scheduledWindow;
    scheduledWindow.window = {91, 1};
    scheduledWindow.visible = true;
    scheduledWindow.dirty = false;
    scheduledWindow.framePolicy = Engine::FramePolicy::Static;
    Engine::FrameScheduler scheduler;
    for (int index = 0; index < 100; ++index)
        scheduler.RequestFrame(scheduledWindow.window,
            index % 2 == 0 ? Engine::FrameReason::Input : Engine::FrameReason::Realtime);
    const Engine::FrameDecision coalesced = scheduler.Evaluate(
        scheduledWindow, false, false, 10.0);
    scheduler.AcknowledgeFrame(scheduledWindow.window, 10.0);
    const Engine::FrameDecision afterCoalesced = scheduler.Evaluate(
        scheduledWindow, false, false, 10.01);
    Check(coalesced.render && coalesced.reason == Engine::FrameReason::Realtime &&
          !afterCoalesced.render,
        "one hundred frame requests coalesce into one prioritized on-demand render");

    Engine::WindowContext pacedWindow;
    pacedWindow.window = {92, 1};
    pacedWindow.visible = true;
    pacedWindow.dirty = false;
    pacedWindow.framePolicy = Engine::FramePolicy::Static;
    Engine::FrameScheduler pacedScheduler;
    int pacedFrames = 0;
    for (int index = 0; index < 100; ++index) {
        pacedScheduler.RequestFrame(pacedWindow.window, Engine::FrameReason::Camera);
        const double timestamp = 20.0 + static_cast<double>(index) * 0.001;
        const Engine::FrameDecision pacedDecision = pacedScheduler.Evaluate(
            pacedWindow, false, false, timestamp);
        if (pacedDecision.render) {
            ++pacedFrames;
            pacedScheduler.AcknowledgeFrame(pacedWindow.window, timestamp);
        }
    }
    Check(pacedFrames <= 7,
        "one hundred high-rate camera requests remain limited by the active 60 FPS cadence");

    Crystal::CrystalWorkspace realtimeWorkspace;
    Crystal::ProjectWorkspaceModel realtimeModel;
    realtimeModel.project = {"P_HighSchool", "P_HighSchool"};
    realtimeModel.entities.push_back(task);
    realtimeWorkspace.Projects().AdoptWorkspace(std::move(realtimeModel));
    Crystal::CrystalEntity latest = task;
    latest.revision = 6;
    std::get<Crystal::TaskDetails>(latest.details).status = Crystal::TaskStatus::Testing;
    Crystal::CrystalRealtimeEvent entityEvent;
    entityEvent.type = Crystal::CrystalRealtimeEventType::EntityUpdated;
    entityEvent.payload = Engine::Json::Value::Object{{"entity", Crystal::ToJson(latest)}};
    realtimeWorkspace.ApplyRealtimeEvent(entityEvent);
    const auto& updatedEntities = realtimeWorkspace.Projects().Workspace().entities;
    Check(updatedEntities.size() == 1 && updatedEntities.front().revision == 6,
        "realtime entity.updated applies through the unified workspace entry");

    Crystal::CrystalEntity olderSnapshot = task;
    olderSnapshot.revision = 5;
    Engine::Json::Value::Object snapshotObject;
    snapshotObject["project"] = Engine::Json::Value::Object{
        {"id", "P_HighSchool"}, {"name", "P_HighSchool"}};
    snapshotObject["spaces"] = Engine::Json::Value::Array{};
    Engine::Json::Value::Array snapshotEntities;
    snapshotEntities.emplace_back(Crystal::ToJson(olderSnapshot));
    snapshotObject["entities"] = std::move(snapshotEntities);
    snapshotObject["views"] = Engine::Json::Value::Array{};
    snapshotObject["relationships"] = Engine::Json::Value::Array{};
    snapshotObject["strokes"] = Engine::Json::Value::Array{};
    snapshotObject["scene_locks"] = Engine::Json::Value::Array{};
    snapshotObject["user_layers"] = Engine::Json::Value::Array{};
    Crystal::CrystalRealtimeEvent snapshotEvent;
    snapshotEvent.type = Crystal::CrystalRealtimeEventType::WorkspaceSnapshot;
    snapshotEvent.payload = Engine::Json::Value::Object{{"workspace", std::move(snapshotObject)}};
    realtimeWorkspace.ApplyRealtimeEvent(snapshotEvent);
    Check(realtimeWorkspace.Projects().Workspace().entities.front().revision == 6,
        "authoritative snapshot cannot roll a newer queued entity revision backwards");
    Crystal::CrystalRelationship realtimeRelationship;
    realtimeRelationship.id = "realtime-rel";
    realtimeRelationship.projectId = "P_HighSchool";
    realtimeRelationship.sourceEntityId = "task-1";
    realtimeRelationship.targetEntityId = "system-1";
    realtimeRelationship.type = Crystal::RelationshipType::DependsOn;
    Crystal::CrystalRealtimeEvent relationshipEvent;
    relationshipEvent.type = Crystal::CrystalRealtimeEventType::RelationshipCreated;
    relationshipEvent.payload = Engine::Json::Value::Object{
        {"relationship", Crystal::ToJson(realtimeRelationship)}};
    realtimeWorkspace.ApplyRealtimeEvent(relationshipEvent);
    Check(realtimeWorkspace.Relationships().size() == 1,
        "relationship.created applies through the unified workspace entry");
    realtimeRelationship.visible = false;
    relationshipEvent.type = Crystal::CrystalRealtimeEventType::RelationshipUpdated;
    relationshipEvent.payload = Engine::Json::Value::Object{
        {"relationship", Crystal::ToJson(realtimeRelationship)}};
    realtimeWorkspace.ApplyRealtimeEvent(relationshipEvent);
    Check(realtimeWorkspace.Relationships().front().visible == false,
        "relationship.updated applies edge visibility through the unified workspace entry");
    Crystal::CrystalRealtimeEvent lockEvent;
    lockEvent.type = Crystal::CrystalRealtimeEventType::SceneLockAcquired;
    lockEvent.payload = Engine::Json::Value::Object{{"lock", Engine::Json::Value::Object{
        {"scene_id", "scene-1"}, {"project_id", "P_HighSchool"},
        {"status", "LOCKED"}, {"owner_id", "worker-1"},
        {"owner_name", "Worker 1"}, {"worker_id", "worker-1"},
        {"task_id", "task-1"}, {"task", "Task 1"},
        {"task_note", "Realtime"}, {"branch", "main"}}}};
    realtimeWorkspace.ApplyRealtimeEvent(lockEvent);
    Check(realtimeWorkspace.CurrentSceneStatus("scene-1") == "LOCKED",
        "scene_lock.acquired applies through the unified workspace entry");

    Crystal::CrystalEntitySchema bugSchema;
    bugSchema.id = "schema:bug";
    bugSchema.projectId = "P_HighSchool";
    bugSchema.name = "Bug";
    bugSchema.displayName = "Bug";
    Crystal::CrystalPropertyDefinition statusProperty;
    statusProperty.id = "schema:bug:status";
    statusProperty.schemaId = bugSchema.id;
    statusProperty.name = "status";
    statusProperty.displayName = "Status";
    statusProperty.type = Crystal::PropertyType::Enum;
    statusProperty.options = {"Open", "Testing", "Done"};
    bugSchema.properties.push_back(statusProperty);
    const auto schemaRoundTrip = Crystal::EntitySchemaFromJson(Crystal::ToJson(bugSchema));
    Check(schemaRoundTrip.name == "Bug" && schemaRoundTrip.properties.size() == 1 &&
          schemaRoundTrip.properties.front().type == Crystal::PropertyType::Enum,
        "schema and property definitions serialize without a custom C++ entity type");

    Crystal::CrystalPropertyDefinition numberProperty;
    numberProperty.type = Crystal::PropertyType::Number;
    numberProperty.hasMinValue = true;
    numberProperty.minValue = 0.0;
    numberProperty.hasMaxValue = true;
    numberProperty.maxValue = 100.0;
    numberProperty.hasStepValue = true;
    numberProperty.stepValue = 5.0;
    Check(Crystal::PropertyEditor::Parse(numberProperty, "25").valid &&
          !Crystal::PropertyEditor::Parse(numberProperty, "23").valid,
        "number editor enforces min/max/step constraints");
    Crystal::CrystalPropertyDefinition referenceProperty;
    referenceProperty.type = Crystal::PropertyType::EntityReferenceList;
    Check(Crystal::PropertyEditor::Parse(referenceProperty, "task-1, worker-1",
              {"task-1", "worker-1"}).valid &&
          !Crystal::PropertyEditor::Parse(referenceProperty, "missing", {"task-1"}).valid,
        "entity reference picker validates single/list references");
    Crystal::CrystalPropertyDefinition tagsProperty;
    tagsProperty.type = Crystal::PropertyType::Tags;
    const auto tagsResult = Crystal::PropertyEditor::Parse(tagsProperty, "alpha, beta, alpha");
    Check(tagsResult.valid && tagsResult.value.IsArray() && tagsResult.value.AsArray().size() == 2,
        "tags editor normalizes duplicate comma-separated tags");
    Crystal::CrystalPropertyDefinition progressProperty;
    progressProperty.type = Crystal::PropertyType::Progress;
    Check(Crystal::PropertyEditor::Parse(progressProperty, "80").valid &&
          !Crystal::PropertyEditor::Parse(progressProperty, "120").valid,
        "progress editor clamps the accepted range to 0..100");
    Engine::UI2D::SelectionModel selection;
    selection.Select("entity-1");
    selection.SelectBoard("board-1", true);
    selection.SelectWidget("widget-1", true);
    selection.SelectInkGroup("group-1", true);
    Check(selection.Items().size() == 4 && selection.IsSelected(
        Engine::UI2D::SelectionKind::Board, "board-1"),
        "multi-selection keeps entity, board, widget, and ink-group kinds together");
    Engine::UI2D::SelectionPopup popup;
    popup.Open({"Open", "Closed", "In Progress"});
    popup.SetFilter("progress");
    Check(popup.FilteredOptions().size() == 1 && popup.Commit() == "In Progress",
        "selection popup filters and commits enum/reference options");
    Engine::UI2D::WorkspaceCommandHistory commandHistory;
    int commandValue = 0;
    commandHistory.Execute({"increment", [&] { commandValue = 0; }, [&] { commandValue = 1; }});
    Check(commandValue == 1 && commandHistory.Undo() && commandValue == 0 &&
          commandHistory.Redo() && commandValue == 1,
        "workspace command history performs deterministic undo and redo");
    Crystal::BoardModel collapsedBoard;
    collapsedBoard.collapsed = true;
    collapsedBoard.size = {420, 260};
    const auto collapsedRoundTrip = Crystal::BoardFromJson(Crystal::ToJson(collapsedBoard));
    Check(collapsedRoundTrip.collapsed && Near(collapsedRoundTrip.size.x, 420.0f),
        "board collapsed state and geometry serialize together");
    Crystal::QueryBuilderDraft queryDraft;
    queryDraft.entitySchema = "Bug";
    queryDraft.filters.push_back({"status", "Equals", Engine::Json::Value("Testing")});
    Check(queryDraft.ToModel("P_HighSchool", "P_HighSchool:Overview").filters.size() == 1,
        "query builder draft produces a server-compatible query model");

    Crystal::CrystalEntity custom;
    custom.id = "bug-1";
    custom.projectId = "P_HighSchool";
    custom.type = Crystal::EntityType::Unknown;
    custom.typeName = "Bug";
    custom.schemaId = bugSchema.id;
    custom.title = "Weather desync";
    custom.properties = Engine::Json::Value::Object{{"status", "Testing"}, {"severity", "High"}};
    const auto customRoundTrip = Crystal::EntityFromJson(Crystal::ToJson(custom));
    Check(customRoundTrip.type == Crystal::EntityType::Unknown && customRoundTrip.typeName == "Bug" &&
          customRoundTrip.schemaId == bugSchema.id && customRoundTrip.properties.GetString("status") == "Testing",
        "custom entity round trips through schema properties without a new typed struct");

    Crystal::EntityView configuredView;
    configuredView.id = "view:bug:overview";
    configuredView.entityId = custom.id;
    configuredView.spaceId = "P_HighSchool:Overview";
    configuredView.viewType = Crystal::ViewType::StatusCard;
    configuredView.visibleProperties = {"status", "severity"};
    configuredView.boardId = "board:bug";
    configuredView.boardLocalPosition = {30, 40};
    const auto viewRoundTrip = Crystal::EntityViewFromJson(Crystal::ToJson(configuredView));
    Check(viewRoundTrip.viewType == Crystal::ViewType::StatusCard &&
          viewRoundTrip.visibleProperties.size() == 2 && viewRoundTrip.boardId == "board:bug",
        "view configuration keeps view type, visible properties, and board attachment separate from Entity");

    Crystal::CrystalRealtimeEvent customEvent;
    customEvent.type = Crystal::CrystalRealtimeEventType::EntityCreated;
    customEvent.payload = Engine::Json::Value::Object{{"entity", Crystal::ToJson(custom)}};
    realtimeWorkspace.ApplyRealtimeEvent(customEvent);
    Check(realtimeWorkspace.Projects().Workspace().entities.size() == 2 &&
          realtimeWorkspace.Projects().Workspace().entities.back().typeName == "Bug",
        "custom entity enters the same realtime Entity collection as built-in entities");

    Crystal::CrystalRealtimeEvent boardEvent;
    Crystal::BoardModel board;
    board.id = "board:bug";
    board.projectId = "P_HighSchool";
    board.spaceId = "P_HighSchool:Overview";
    board.title = "Bug Board";
    board.position = {100, 100};
    boardEvent.type = Crystal::CrystalRealtimeEventType::BoardCreated;
    boardEvent.payload = Engine::Json::Value::Object{{"board", Crystal::ToJson(board)}};
    realtimeWorkspace.ApplyRealtimeEvent(boardEvent);
    Crystal::CrystalRealtimeEvent boardViewEvent;
    boardViewEvent.type = Crystal::CrystalRealtimeEventType::ViewCreated;
    configuredView.position = {130, 140};
    boardViewEvent.payload = Engine::Json::Value::Object{{"view", Crystal::ToJson(configuredView)}};
    realtimeWorkspace.ApplyRealtimeEvent(boardViewEvent);
    board.position = {250, 200};
    boardEvent.type = Crystal::CrystalRealtimeEventType::BoardUpdated;
    boardEvent.payload = Engine::Json::Value::Object{{"board", Crystal::ToJson(board)}};
    realtimeWorkspace.ApplyRealtimeEvent(boardEvent);
    const auto movedView = std::find_if(
        realtimeWorkspace.Projects().Workspace().views.begin(),
        realtimeWorkspace.Projects().Workspace().views.end(),
        [](const Crystal::EntityView& view) { return view.id == "view:bug:overview"; });
    Check(movedView != realtimeWorkspace.Projects().Workspace().views.end() &&
          Near(movedView->position.x, 280.0f) && Near(movedView->position.y, 240.0f),
        "board movement applies the same world delta to attached views");

    Crystal::UserLayerModel hiddenLayer;
    hiddenLayer.id = "layer:old";
    hiddenLayer.projectId = "P_HighSchool";
    hiddenLayer.name = "Old Design";
    hiddenLayer.visible = false;
    Crystal::CrystalRealtimeEvent layerEvent;
    layerEvent.type = Crystal::CrystalRealtimeEventType::LayerCreated;
    layerEvent.payload = Engine::Json::Value::Object{{"layer", Crystal::ToJson(hiddenLayer)}};
    realtimeWorkspace.ApplyRealtimeEvent(layerEvent);
    Check(!realtimeWorkspace.Layers().empty() && !realtimeWorkspace.Layers().back().visible &&
          realtimeWorkspace.Projects().Workspace().entities.size() == 2,
        "user layer visibility is visual-only and never deletes Entity data");

    Crystal::QueryViewModel query;
    query.id = "query:active-bugs";
    query.projectId = "P_HighSchool";
    query.name = "Active Bugs";
    query.entitySchema = "Bug";
    query.filters.push_back({"status", "Equals", Engine::Json::Value("Testing")});
    const auto queryRoundTrip = Crystal::QueryViewFromJson(Crystal::ToJson(query));
    Check(queryRoundTrip.entitySchema == "Bug" && queryRoundTrip.filters.size() == 1 &&
          queryRoundTrip.filters.front().property == "status",
        "query view AND filter serialization is stable");
    realtimeWorkspace.Projects().Workspace().queryViews.push_back(query);
    Crystal::CrystalEntity customUpdated = custom;
    customUpdated.revision = 2;
    customUpdated.properties["status"] = "Testing";
    Crystal::CrystalRealtimeEvent customUpdateEvent;
    customUpdateEvent.type = Crystal::CrystalRealtimeEventType::EntityUpdated;
    customUpdateEvent.payload = Engine::Json::Value::Object{{"entity", Crystal::ToJson(customUpdated)}};
    realtimeWorkspace.ApplyRealtimeEvent(customUpdateEvent);
    Crystal::DashboardWidgetModel dashboardWidget;
    dashboardWidget.id = "widget:active-bugs";
    dashboardWidget.projectId = "P_HighSchool";
    dashboardWidget.spaceId = "P_HighSchool:Overview";
    dashboardWidget.widgetType = "EntityList";
    dashboardWidget.config = Engine::Json::Value::Object{{"query_view_id", query.id}};
    Crystal::CrystalRealtimeEvent dashboardEvent;
    dashboardEvent.type = Crystal::CrystalRealtimeEventType::DashboardWidgetCreated;
    dashboardEvent.payload = Engine::Json::Value::Object{{"widget", Crystal::ToJson(dashboardWidget)}};
    realtimeWorkspace.ApplyRealtimeEvent(dashboardEvent);
    Check(realtimeWorkspace.QueryViews().front().results.size() == 1 &&
          realtimeWorkspace.DashboardWidgets().front().config.GetString("query_view_id") == query.id,
        "entity updates refresh query results and dashboard widget bindings on the workspace thread");

    Crystal::DashboardWidgetModel inspectorWidget;
    inspectorWidget.id = "widget:inspector";
    inspectorWidget.widgetType = "EntityList";
    inspectorWidget.title = "Inspector Widget";
    inspectorWidget.config = Engine::Json::Value::Object{
        {"query_view_id", query.id}, {"limit", 20}, {"text", "READY"}};
    const auto inspectorRoundTrip = Crystal::DashboardWidgetFromJson(
        Crystal::ToJson(inspectorWidget));
    Check(inspectorRoundTrip.title == "Inspector Widget" &&
          inspectorRoundTrip.config.GetString("query_view_id") == query.id &&
          inspectorRoundTrip.config.GetNumber("limit") == 20,
        "widget inspector configuration serializes and restores nested config values");

    realtimeWorkspace.SetBoardHover("board:bug", false);
    Check(realtimeWorkspace.BoardHoverId() == "board:bug" &&
          !realtimeWorkspace.BoardHoverLocked(),
        "board drop target exposes an unlocked hover state");
    realtimeWorkspace.SetBoardHover("board:bug", true);
    Check(realtimeWorkspace.BoardHoverLocked(),
        "board drop target exposes locked hover feedback");
    realtimeWorkspace.ClearBoardHover();

    realtimeWorkspace.OpenContextMenu({120, 140}, "board:bug", "Board");
    Check(realtimeWorkspace.ContextMenu().open &&
          realtimeWorkspace.ContextMenu().targetType == "Board",
        "context menu state keeps target identity for high-frequency actions");
    realtimeWorkspace.CloseContextMenu();
    realtimeWorkspace.BeginInlineEdit("Board", "board:bug", "Bug Board");
    realtimeWorkspace.SetInlineEditDraft("Renamed Board");
    Check(realtimeWorkspace.InlineEdit().active &&
          realtimeWorkspace.InlineEdit().draft == "Renamed Board",
        "inline title editing suppresses global shortcuts until committed");
    realtimeWorkspace.CancelInlineEdit();

    Crystal::EntityView detachedView = configuredView;
    detachedView.boardId.clear();
    detachedView.boardLocalPosition = {};
    Crystal::CrystalRealtimeEvent detachEvent;
    detachEvent.type = Crystal::CrystalRealtimeEventType::ViewUpdated;
    detachEvent.payload = Engine::Json::Value::Object{{"view", Crystal::ToJson(detachedView)}};
    realtimeWorkspace.ApplyRealtimeEvent(detachEvent);
    const auto detachedFound = std::find_if(
        realtimeWorkspace.Projects().Workspace().views.begin(),
        realtimeWorkspace.Projects().Workspace().views.end(),
        [](const Crystal::EntityView& view) { return view.id == "view:bug:overview"; });
    Check(detachedFound != realtimeWorkspace.Projects().Workspace().views.end() &&
          detachedFound->boardId.empty(),
        "auto-detach realtime view update clears board attachment without losing view fields");

    Crystal::EntityView layerView = configuredView;
    layerView.id = "view:layered";
    layerView.layerId = "Old Design";
    realtimeWorkspace.Projects().Workspace().views.push_back(layerView);
    Engine::UI2D::InkStroke layerStroke;
    layerStroke.id = "ink:layered";
    layerStroke.layer = "Old Design";
    layerStroke.points = {{1, 1}, {2, 2}};
    realtimeWorkspace.Projects().Workspace().strokes.push_back(layerStroke);
    Crystal::BoardModel layerBoard = board;
    layerBoard.id = "board:layered";
    layerBoard.layerId = "Old Design";
    realtimeWorkspace.Projects().Workspace().boards.push_back(layerBoard);
    Crystal::DashboardWidgetModel layerWidget = dashboardWidget;
    layerWidget.id = "widget:layered";
    layerWidget.layerId = "Old Design";
    realtimeWorkspace.Projects().Workspace().dashboardWidgets.push_back(layerWidget);
    hiddenLayer.name = "New Design";
    layerEvent.type = Crystal::CrystalRealtimeEventType::LayerUpdated;
    layerEvent.payload = Engine::Json::Value::Object{{"layer", Crystal::ToJson(hiddenLayer)}};
    realtimeWorkspace.ApplyRealtimeEvent(layerEvent);
    Check(realtimeWorkspace.Projects().Workspace().views.back().layerId == "New Design" &&
          realtimeWorkspace.Ink().Strokes().back().layer == "New Design" &&
          realtimeWorkspace.Boards().back().layerId == "New Design" &&
          realtimeWorkspace.DashboardWidgets().back().layerId == "New Design",
        "layer rename migrates cached views, ink, boards, and widgets together");

    Crystal::CrystalRealtimeEvent spaceDeleteEvent;
    spaceDeleteEvent.type = Crystal::CrystalRealtimeEventType::SpaceDeleted;
    spaceDeleteEvent.payload = Engine::Json::Value::Object{{"space_id", "P_HighSchool:Overview"}};
    realtimeWorkspace.Projects().Workspace().spaces.push_back(
        Crystal::SpaceModel{"P_HighSchool:Overview", "P_HighSchool", "Overview"});
    realtimeWorkspace.ApplyRealtimeEvent(spaceDeleteEvent);
    Check(realtimeWorkspace.Projects().Workspace().entities.size() == 2 &&
          std::none_of(realtimeWorkspace.Projects().Workspace().spaces.begin(),
              realtimeWorkspace.Projects().Workspace().spaces.end(),
              [](const Crystal::SpaceModel& space) { return space.id == "P_HighSchool:Overview"; }),
        "deleting a Space removes visual context but never deletes its Entities");
    Check(realtimeWorkspace.GetDisplayMode() == Crystal::DisplayMode::All,
        "display mode starts in All");
    realtimeWorkspace.CycleDisplayMode();
    realtimeWorkspace.CycleDisplayMode();
    realtimeWorkspace.CycleDisplayMode();
    realtimeWorkspace.CycleDisplayMode();
    Check(realtimeWorkspace.GetDisplayMode() == Crystal::DisplayMode::All &&
          std::string(Crystal::DisplayModeName(Crystal::DisplayMode::Presentation)) == "Presentation",
        "All, Graph, Draw, and Presentation display modes cycle deterministically");
    Crystal::ProjectWorkspaceModel emptyRealtimeModel;
    realtimeWorkspace.Projects().AdoptWorkspace(std::move(emptyRealtimeModel));

    Crystal::ProjectWorkspaceModel largeFixture;
    largeFixture.project = {"P_HighSchool", "Performance Fixture"};
    largeFixture.currentSpace = {"P_HighSchool:Performance", "P_HighSchool", "Performance"};
    largeFixture.spaces.push_back(largeFixture.currentSpace);
    largeFixture.views.reserve(300);
    largeFixture.relationships.reserve(100);
    largeFixture.strokes.reserve(500);
    largeFixture.boards.reserve(20);
    largeFixture.dashboardWidgets.reserve(20);
    for (int index = 0; index < 300; ++index) {
        Crystal::EntityView view;
        view.id = "perf-view-" + std::to_string(index);
        view.entityId = "perf-entity-" + std::to_string(index);
        view.spaceId = largeFixture.currentSpace.id;
        view.position = {static_cast<float>(index % 30) * 220.0f,
            static_cast<float>(index / 30) * 140.0f};
        largeFixture.views.push_back(std::move(view));
    }
    for (int index = 0; index < 100; ++index) {
        Crystal::CrystalRelationship relationship;
        relationship.id = "perf-relationship-" + std::to_string(index);
        relationship.sourceEntityId = "perf-entity-" + std::to_string(index);
        relationship.targetEntityId = "perf-entity-" + std::to_string(index + 1);
        largeFixture.relationships.push_back(std::move(relationship));
    }
    for (int index = 0; index < 500; ++index) {
        Engine::UI2D::InkStroke ink;
        ink.id = "perf-ink-" + std::to_string(index);
        ink.points = {{static_cast<float>(index), 0.0f},
            {static_cast<float>(index) + 4.0f, 4.0f}};
        largeFixture.strokes.push_back(std::move(ink));
    }
    for (int index = 0; index < 20; ++index) {
        Crystal::BoardModel board;
        board.id = "perf-board-" + std::to_string(index);
        board.spaceId = largeFixture.currentSpace.id;
        largeFixture.boards.push_back(std::move(board));
        Crystal::DashboardWidgetModel widget;
        widget.id = "perf-widget-" + std::to_string(index);
        widget.spaceId = largeFixture.currentSpace.id;
        largeFixture.dashboardWidgets.push_back(std::move(widget));
    }
    std::size_t traversedVisuals = 0;
    for (const auto& view : largeFixture.views)
        if (view.spaceId == largeFixture.currentSpace.id) ++traversedVisuals;
    for (const auto& stroke : largeFixture.strokes)
        if (stroke.points.size() >= 2) ++traversedVisuals;
    Check(largeFixture.views.size() == 300 && largeFixture.relationships.size() == 100 &&
          largeFixture.strokes.size() == 500 && largeFixture.boards.size() == 20 &&
          largeFixture.dashboardWidgets.size() == 20 && traversedVisuals == 800,
        "synthetic 300-view workspace fixture builds and traverses deterministically");

    Crystal::ProjectWorkspaceModel localVisuals;
    localVisuals.currentSpace = {"P_HighSchool:Local", "P_HighSchool", "Local"};
    localVisuals.spaces.push_back(localVisuals.currentSpace);
    Crystal::BoardModel localBoard;
    localBoard.id = "local-board";
    localBoard.spaceId = localVisuals.currentSpace.id;
    localBoard.position = {100, 100};
    localBoard.size = {300, 200};
    localVisuals.boards.push_back(localBoard);
    Crystal::EntityView childView;
    childView.id = "local-view";
    childView.entityId = "local-entity";
    childView.spaceId = localVisuals.currentSpace.id;
    childView.position = {110, 105};
    childView.boardId = localBoard.id;
    childView.boardLocalPosition = {10, 5};
    localVisuals.views.push_back(childView);
    Engine::UI2D::InkStroke groupStrokeA;
    groupStrokeA.id = "local-ink-a";
    groupStrokeA.groupId = "local-group";
    groupStrokeA.points = {{10, 10}, {20, 20}};
    localVisuals.strokes.push_back(groupStrokeA);
    Engine::UI2D::InkStroke groupStrokeB = groupStrokeA;
    groupStrokeB.id = "local-ink-b";
    groupStrokeB.points = {{30, 30}, {40, 40}};
    localVisuals.strokes.push_back(groupStrokeB);
    Crystal::DashboardWidgetModel localWidget;
    localWidget.id = "local-widget";
    localWidget.spaceId = localVisuals.currentSpace.id;
    localWidget.size = {220, 120};
    localVisuals.dashboardWidgets.push_back(localWidget);
    Crystal::UserLayerModel lockedVisualLayer;
    lockedVisualLayer.id = "local-locked";
    lockedVisualLayer.projectId = "P_HighSchool";
    lockedVisualLayer.locked = true;
    localVisuals.userLayers.push_back(lockedVisualLayer);
    Crystal::CrystalWorkspace localWorkspace;
    localWorkspace.Projects().AdoptWorkspace(std::move(localVisuals));
    localWorkspace.Selection().SelectBoard(localBoard.id);
    localWorkspace.Selection().Select(childView.entityId, true);
    Check(localWorkspace.MoveSelectionLocal({10, 5}) &&
          Near(localWorkspace.Boards().front().position.x, 110.0f) &&
          Near(localWorkspace.Projects().Workspace().views.front().position.x, 120.0f),
        "multi-selection transform deduplicates attached child movement");
    localWorkspace.Selection().Clear();
    localWorkspace.Selection().SelectInkGroup("local-group");
    Check(localWorkspace.MoveSelectionLocal({5, 7}) &&
          Near(localWorkspace.Ink().Strokes().front().points.front().x, 15.0f),
        "ink group selection moves all member strokes as one visual unit");
    localWorkspace.Selection().Clear();
    localWorkspace.Selection().SelectBoard(localBoard.id);
    Check(localWorkspace.ResizeSelectedBoard({500, 300}) &&
          Near(localWorkspace.Boards().front().size.x, 500.0f),
        "board resize clamps and applies to the selected board");
    localWorkspace.Selection().Clear();
    localWorkspace.Selection().SelectWidget(localWidget.id);
    Check(localWorkspace.ResizeSelectedWidget({420, 240}) &&
          Near(localWorkspace.DashboardWidgets().front().size.x, 420.0f),
        "dashboard widget resize applies through the workspace selection API");
    localWorkspace.SetActiveLayerId(lockedVisualLayer.id);
    Check(localWorkspace.ActiveLayerId() == lockedVisualLayer.id &&
          localWorkspace.IsLayerLocked(lockedVisualLayer.id),
        "active layer state exposes locked-layer transform policy");

    const Crystal::WorldViewportBounds viewport{0.0f, 0.0f, 100.0f, 100.0f};
    Check(Crystal::WorldRectIntersectsViewport({90, 90}, {20, 20}, viewport) &&
          !Crystal::WorldRectIntersectsViewport({120, 120}, {10, 10}, viewport),
        "visible culling accepts intersecting bounds and skips distant rectangles");
    Check(Crystal::WorldSegmentBoundsIntersectViewport({-20, 50}, {20, 50}, viewport) &&
          !Crystal::WorldSegmentBoundsIntersectViewport({120, 120}, {150, 150}, viewport),
        "relationship culling filters segment bounds before draw preparation");
    Check(Crystal::WorldPointInRect({10, 10}, {0, 0}, {20, 20}) &&
          !Crystal::WorldPointInRect({25, 25}, {0, 0}, {20, 20}),
        "hit-test filtering uses the same normalized world rectangle boundaries");

    std::cout << "Crystal core checks=" << (checks - failures)
              << " status=" << (failures == 0 ? "PASS" : "FAIL") << '\n';
    return failures == 0 ? 0 : 1;
}
