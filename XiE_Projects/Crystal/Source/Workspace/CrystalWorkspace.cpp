#include "Workspace/CrystalWorkspace.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Crystal {
namespace {

CrystalRealtimeEvent MakeRealtimeEvent(std::string_view wireType, Engine::Json::Value payload) {
    CrystalRealtimeEvent event;
    event.type = RealtimeEventTypeFromWire(wireType);
    event.wireType = wireType;
    event.payload = std::move(payload);
    return event;
}

std::string FailureNotification(std::string_view operation, const ApiResult& result) {
    const std::string& error = result.error;
    if (error.find("revision_conflict") != std::string::npos)
        return "CONFLICT: latest server Entity applied";
    if (error.find("already_locked") != std::string::npos)
        return "SCENE ALREADY LOCKED";
    if (error.find("relationship_already_exists") != std::string::npos ||
        error.find("entity_detail_conflict") != std::string::npos)
        return "RELATIONSHIP DUPLICATE";
    if (error.find("invalid_entity") != std::string::npos ||
        error.find("entity_not_found") != std::string::npos)
        return "INVALID ENTITY";
    if (error.find("server_disconnected") != std::string::npos)
        return "SERVER DISCONNECTED";
    return std::string(operation) + ": REQUEST FAILED";
}

bool QueryPropertyValue(
    const CrystalEntity& entity, std::string_view property, Engine::Json::Value& output) {
    if (const Engine::Json::Value* value = entity.properties.Find(property)) {
        output = *value;
        return true;
    }
    const Engine::Json::Value serialized = ToJson(entity);
    const Engine::Json::Value* details = serialized.Find("details");
    const Engine::Json::Value* value = details == nullptr ? nullptr : details->Find(property);
    if (value == nullptr) return false;
    output = *value;
    return true;
}

bool QueryMatches(const CrystalEntity& entity, const QueryViewModel& query) {
    if (!query.entitySchema.empty() && entity.typeName != query.entitySchema && entity.schemaId != query.entitySchema)
        return false;
    for (const QueryFilterModel& filter : query.filters) {
        Engine::Json::Value actual;
        if (!QueryPropertyValue(entity, filter.property, actual)) {
            if (filter.property == "entity_type") actual = entity.typeName;
            else if (filter.property == "title") actual = entity.title;
            else if (filter.property == "description") actual = entity.description;
        }
        const bool equal = actual.Dump() == filter.value.Dump();
        if (filter.op == "Equals" && !equal) return false;
        if (filter.op == "NotEquals" && equal) return false;
        if (filter.op == "Contains") {
            if (actual.IsString() && filter.value.IsString()) {
                if (actual.AsString().find(filter.value.AsString()) == std::string::npos) return false;
            } else if (actual.IsArray()) {
                const bool contains = std::any_of(actual.AsArray().begin(), actual.AsArray().end(),
                    [&](const Engine::Json::Value& item) { return item.Dump() == filter.value.Dump(); });
                if (!contains) return false;
            } else if (!equal) return false;
        }
    }
    return true;
}

} // namespace

CrystalWorkspace::CrystalWorkspace(std::string serverUrl)
    : m_Api(std::move(serverUrl)), m_Projects(m_Api),
      m_NextHeartbeat(std::chrono::steady_clock::now() + std::chrono::seconds(20)) {}

CrystalWorkspace::~CrystalWorkspace() {
    if (!m_Projects.CurrentProject().id.empty()) {
        m_Projects.CurrentSpace().SyncModelFromCamera();
        (void)m_Api.SaveCamera(m_Projects.CurrentSpace().Current());
    }
    m_Realtime.Stop();
}

ApiResult CrystalWorkspace::Connect(const std::string& projectId) {
    if (!m_Projects.CurrentProject().id.empty()) {
        m_Projects.CurrentSpace().SyncModelFromCamera();
        (void)m_Api.SaveCamera(m_Projects.CurrentSpace().Current());
    }
    m_Realtime.Stop();
    m_Realtime.ClearEvents();
    m_HeldLocks.clear();
    ApiResult result = m_Projects.SwitchProject(projectId);
    if (!result.ok) {
        m_ConnectionStatus = "Disconnected";
        m_Notification = "CONNECT FAILED";
        return result;
    }
    m_ProjectId = projectId;
    m_Ink.Load(m_Projects.Workspace().strokes);
    m_Selection.Clear();
    m_Realtime.Configure(m_Api.BaseUrl(), projectId);
    m_ConnectionStatus = "Connecting";
    m_Notification.clear();
    if (!m_Realtime.Start()) {
        m_ConnectionStatus = "Disconnected";
        m_Notification = "CONNECT FAILED";
        return {false, 0, {}, "realtime_start_failed"};
    }
    m_NextHeartbeat = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    return result;
}

ApiResult CrystalWorkspace::RequireConnected(std::string_view operation) {
    if (m_ConnectionStatus == "Connected") return {true, 0, {}, {}};
    m_Notification = "SERVER DISCONNECTED: " + std::string(operation);
    return {false, 0, {}, "server_disconnected"};
}

ApiResult CrystalWorkspace::FinishApiResult(std::string_view operation, ApiResult result) {
    if (result.ok) {
        m_Notification.clear();
        return result;
    }
    if (result.statusCode == 409) {
        if (const Engine::Json::Value* detail = result.data.Find("detail"); detail != nullptr) {
            if (const Engine::Json::Value* current = detail->Find("current_entity");
                current != nullptr && current->IsObject()) {
                ReplaceEntity(EntityFromJson(*current));
            }
            if (const Engine::Json::Value* current = detail->Find("current_view");
                current != nullptr && current->IsObject()) {
                ReplaceView(EntityViewFromJson(*current));
            }
        }
    }
    m_Notification = FailureNotification(operation, result);
    return result;
}

ApiResult CrystalWorkspace::SwitchSpace(const std::string& spaceId) {
    ApiResult connected = RequireConnected("SWITCH SPACE");
    if (!connected.ok) return connected;
    m_Projects.CurrentSpace().SyncModelFromCamera();
    if (!m_Projects.CurrentSpace().Current().id.empty())
        (void)m_Api.SaveCamera(m_Projects.CurrentSpace().Current());
    ApiResult result = m_Projects.SwitchSpace(spaceId);
    if (result.ok) {
        m_Ink.Load(m_Projects.Workspace().strokes);
        m_Selection.Clear();
    }
    return FinishApiResult("SWITCH SPACE", std::move(result));
}

ApiResult CrystalWorkspace::CreateNote(
    const std::string& title, Engine::Vec2F worldPosition) {
    ApiResult connected = RequireConnected("CREATE NOTE");
    if (!connected.ok) return connected;
    const auto& workspace = m_Projects.Workspace();
    CrystalEntity entity;
    entity.projectId = workspace.project.id;
    entity.type = EntityType::Note;
    entity.title = title;
    entity.description = "Created from Crystal canvas";
    entity.data = Engine::Json::Value::Object{{"kind", "note"}};
    CrystalEntity created;
    ApiResult result = m_Api.CreateEntity(entity, created);
    if (!result.ok) return FinishApiResult("CREATE NOTE", std::move(result));
    const auto& space = m_Projects.CurrentSpace().Current();
    EntityView view;
    view.id = "view:" + created.id + ":" + space.id;
    view.entityId = created.id;
    view.spaceId = space.id;
    view.position = worldPosition;
    EntityView saved;
    ApiResult viewResult = m_Api.SaveView(view, -1, &saved);
    if (viewResult.ok) {
        ApplyRealtimeEvent(MakeRealtimeEvent("entity.created",
            Engine::Json::Value::Object{{"entity", ToJson(created)}}));
        ApplyRealtimeEvent(MakeRealtimeEvent("view.created",
            Engine::Json::Value::Object{{"view", ToJson(saved)}}));
    }
    return FinishApiResult("CREATE NOTE VIEW", std::move(viewResult));
}

ApiResult CrystalWorkspace::SaveView(const EntityView& view) {
    ApiResult connected = RequireConnected("SAVE VIEW");
    if (!connected.ok) return connected;
    EntityView saved;
    ApiResult result = m_Api.SaveView(view, view.revision, &saved);
    if (result.ok) {
        ApplyRealtimeEvent(MakeRealtimeEvent("view.updated",
            Engine::Json::Value::Object{{"view", ToJson(saved)}}));
    }
    return FinishApiResult("SAVE VIEW", std::move(result));
}

ApiResult CrystalWorkspace::DeleteView(const std::string& entityId, const std::string& spaceId) {
    ApiResult connected = RequireConnected("DELETE VIEW");
    if (!connected.ok) return connected;
    ApiResult result = m_Api.DeleteView(entityId, spaceId);
    if (result.ok) {
        ApplyRealtimeEvent(MakeRealtimeEvent("view.deleted",
            Engine::Json::Value::Object{{"view_id", "view:" + entityId + ":" + spaceId}}));
    }
    return FinishApiResult("DELETE VIEW", std::move(result));
}

ApiResult CrystalWorkspace::RenameEntity(const std::string& entityId, std::string title) {
    ApiResult connected = RequireConnected("RENAME ENTITY");
    if (!connected.ok) return connected;
    const auto& entities = m_Projects.Workspace().entities;
    const auto found = std::find_if(entities.begin(), entities.end(),
        [&](const CrystalEntity& entity) { return entity.id == entityId; });
    if (found == entities.end()) return FinishApiResult(
        "RENAME ENTITY", {false, 404, {}, "entity_not_found"});
    CrystalEntity updated;
    ApiResult result = m_Api.UpdateEntityTitle(entityId, title, found->revision, &updated);
    if (result.ok) {
        ApplyRealtimeEvent(MakeRealtimeEvent("entity.updated",
            Engine::Json::Value::Object{{"entity", ToJson(updated)}}));
    }
    return FinishApiResult("RENAME ENTITY", std::move(result));
}

ApiResult CrystalWorkspace::UpdateEntity(CrystalEntity entity) {
    ApiResult connected = RequireConnected("UPDATE ENTITY");
    if (!connected.ok) return connected;
    CrystalEntity updated;
    ApiResult result = m_Api.UpdateEntity(entity, entity.revision, &updated);
    if (result.ok) {
        ApplyRealtimeEvent(MakeRealtimeEvent("entity.updated",
            Engine::Json::Value::Object{{"entity", ToJson(updated)}}));
    }
    return FinishApiResult("UPDATE ENTITY", std::move(result));
}

ApiResult CrystalWorkspace::UpdateCustomProperty(
    const std::string& entityId, std::string property, Engine::Json::Value value) {
    const auto found = std::find_if(
        m_Projects.Workspace().entities.begin(), m_Projects.Workspace().entities.end(),
        [&](const CrystalEntity& entity) { return entity.id == entityId; });
    if (found == m_Projects.Workspace().entities.end() || found->schemaId.empty())
        return FinishApiResult("UPDATE PROPERTY", {false, 404, {}, "custom_entity_not_found"});
    CrystalEntity updated = *found;
    if (!updated.properties.IsObject()) updated.properties = Engine::Json::Value::Object{};
    updated.properties[std::move(property)] = std::move(value);
    return UpdateEntity(std::move(updated));
}

ApiResult CrystalWorkspace::DeleteEntity(const std::string& entityId) {
    ApiResult connected = RequireConnected("DELETE ENTITY");
    if (!connected.ok) return connected;
    ApiResult result = m_Api.DeleteEntity(entityId);
    if (result.ok) {
        ApplyRealtimeEvent(MakeRealtimeEvent("entity.deleted",
            Engine::Json::Value::Object{{"entity_id", entityId}}));
    }
    return FinishApiResult("DELETE ENTITY", std::move(result));
}

ApiResult CrystalWorkspace::CreateRelationship(
    const std::string& sourceEntityId, const std::string& targetEntityId,
    RelationshipType type) {
    ApiResult connected = RequireConnected("CREATE RELATIONSHIP");
    if (!connected.ok) return connected;
    CrystalRelationship relation;
    relation.projectId = m_Projects.CurrentProject().id;
    relation.sourceEntityId = sourceEntityId;
    relation.targetEntityId = targetEntityId;
    relation.type = type;
    CrystalRelationship created;
    ApiResult result = m_Api.CreateRelationship(relation, created);
    if (result.ok) {
        ApplyRealtimeEvent(MakeRealtimeEvent("relationship.created",
            Engine::Json::Value::Object{{"relationship", ToJson(created)}}));
    }
    return FinishApiResult("CREATE RELATIONSHIP", std::move(result));
}

ApiResult CrystalWorkspace::DeleteRelationship(const std::string& relationshipId) {
    ApiResult connected = RequireConnected("DELETE RELATIONSHIP");
    if (!connected.ok) return connected;
    ApiResult result = m_Api.DeleteRelationship(relationshipId);
    if (result.ok) {
        ApplyRealtimeEvent(MakeRealtimeEvent("relationship.deleted",
            Engine::Json::Value::Object{{"relationship_id", relationshipId}}));
    }
    return FinishApiResult("DELETE RELATIONSHIP", std::move(result));
}

ApiResult CrystalWorkspace::SetRelationshipVisible(const std::string& relationshipId, bool visible) {
    ApiResult connected = RequireConnected("UPDATE RELATIONSHIP");
    if (!connected.ok) return connected;
    CrystalRelationship updated;
    ApiResult result = m_Api.UpdateRelationshipVisibility(relationshipId, visible, &updated);
    if (result.ok) {
        ApplyRealtimeEvent(MakeRealtimeEvent(
            "relationship.updated", Engine::Json::Value::Object{{"relationship", ToJson(updated)}}));
    }
    return FinishApiResult("UPDATE RELATIONSHIP", std::move(result));
}

ApiResult CrystalWorkspace::UpdateTaskStatus(const std::string& entityId, TaskStatus status) {
    const auto& entities = m_Projects.Workspace().entities;
    const auto found = std::find_if(entities.begin(), entities.end(),
        [&](const CrystalEntity& entity) { return entity.id == entityId; });
    if (found == entities.end() || found->type != EntityType::Task)
        return FinishApiResult("UPDATE TASK", {false, 422, {}, "task_not_found"});
    CrystalEntity copy = *found;
    TaskDetails details = std::holds_alternative<TaskDetails>(copy.details)
        ? std::get<TaskDetails>(copy.details) : TaskDetails{};
    details.status = status;
    copy.details = details;
    return UpdateEntity(std::move(copy));
}

ApiResult CrystalWorkspace::Search(const std::string& query, std::vector<CrystalEntity>& output) {
    ApiResult connected = RequireConnected("SEARCH");
    if (!connected.ok) return connected;
    ApiResult result = m_Api.Search(m_Projects.CurrentProject().id, query, output);
    return FinishApiResult("SEARCH", std::move(result));
}

ApiResult CrystalWorkspace::SetSearchQuery(std::string query) {
    m_SearchQuery = std::move(query);
    if (m_SearchQuery.empty()) {
        m_SearchResults.clear();
        return {true, 200, {}, {}};
    }
    return Search(m_SearchQuery, m_SearchResults);
}

ApiResult CrystalWorkspace::CreateStroke(const Engine::UI2D::InkStroke& stroke) {
    ApiResult connected = RequireConnected("CREATE INK");
    if (!connected.ok) return connected;
    const auto& current = m_Projects.CurrentSpace().Current();
    Engine::UI2D::InkStroke saved;
    ApiResult result = m_Api.CreateStroke(
        m_Projects.CurrentProject().id, current.id, stroke, &saved);
    if (result.ok) {
        ApplyRealtimeEvent(MakeRealtimeEvent("ink.created",
            Engine::Json::Value::Object{{"stroke", ToJson(saved)}}));
    }
    return FinishApiResult("CREATE INK", std::move(result));
}

ApiResult CrystalWorkspace::DeleteStroke(const std::string& strokeId) {
    ApiResult connected = RequireConnected("DELETE INK");
    if (!connected.ok) return connected;
    ApiResult result = m_Api.DeleteStroke(strokeId);
    if (result.ok) {
        ApplyRealtimeEvent(MakeRealtimeEvent("ink.deleted",
            Engine::Json::Value::Object{{"stroke_id", strokeId}}));
    }
    return FinishApiResult("DELETE INK", std::move(result));
}

ApiResult CrystalWorkspace::AttachInkToEntity(
    const std::string& strokeId, const std::string& entityId) {
    ApiResult connected = RequireConnected("ATTACH INK");
    if (!connected.ok) return connected;
    const EntityView* view = nullptr;
    for (const auto& candidate : m_Projects.Workspace().views) {
        if (candidate.entityId == entityId &&
            candidate.spaceId == m_Projects.CurrentSpace().Current().id) {
            view = &candidate;
            break;
        }
    }
    if (view == nullptr) return FinishApiResult(
        "ATTACH INK", {false, 404, {}, "entity_view_not_found"});
    if (!m_Ink.AttachToEntity(strokeId, entityId, view->position))
        return FinishApiResult("ATTACH INK", {false, 404, {}, "stroke_not_found"});
    const auto found = std::find_if(m_Ink.Strokes().begin(), m_Ink.Strokes().end(),
        [&](const Engine::UI2D::InkStroke& stroke) { return stroke.id == strokeId; });
    if (found == m_Ink.Strokes().end()) return FinishApiResult(
        "ATTACH INK", {false, 404, {}, "stroke_not_found"});
    ApiResult result = m_Api.UpdateStrokeAnchor(*found);
    if (result.ok) {
        ApplyRealtimeEvent(MakeRealtimeEvent("ink.updated",
            Engine::Json::Value::Object{{"stroke", ToJson(*found)}}));
    }
    return FinishApiResult("ATTACH INK", std::move(result));
}

ApiResult CrystalWorkspace::AttachInkToBoard(
    const std::string& strokeId, const std::string& boardId, Engine::Vec2F localOffset) {
    ApiResult connected = RequireConnected("ATTACH INK BOARD");
    if (!connected.ok) return connected;
    const BoardModel* board = nullptr;
    for (const auto& candidate : m_Projects.Workspace().boards) {
        if (candidate.id == boardId && candidate.spaceId == m_Projects.CurrentSpace().Current().id) {
            board = &candidate;
            break;
        }
    }
    if (board == nullptr) return FinishApiResult(
        "ATTACH INK BOARD", {false, 404, {}, "board_not_found"});
    auto found = std::find_if(m_Projects.Workspace().strokes.begin(),
        m_Projects.Workspace().strokes.end(),
        [&](const Engine::UI2D::InkStroke& stroke) { return stroke.id == strokeId; });
    if (found == m_Projects.Workspace().strokes.end()) return FinishApiResult(
        "ATTACH INK BOARD", {false, 404, {}, "stroke_not_found"});
    found->anchorEntityId.clear();
    found->anchorBoardId = boardId;
    found->localOffset = localOffset;
    ApiResult result = m_Api.UpdateStrokeAnchor(*found);
    if (result.ok) {
        Engine::UI2D::InkStroke saved = result.data.IsObject() ? StrokeFromJson(result.data) : *found;
        ApplyRealtimeEvent(MakeRealtimeEvent(
            "ink.updated", Engine::Json::Value::Object{{"stroke", ToJson(saved)}}));
    }
    return FinishApiResult("ATTACH INK BOARD", std::move(result));
}

ApiResult CrystalWorkspace::GroupInkStrokes(
    const std::vector<std::string>& strokeIds, std::string name) {
    ApiResult connected = RequireConnected("GROUP INK");
    if (!connected.ok) return connected;
    ApiResult result = m_Api.GroupInkStrokes(
        m_Projects.CurrentProject().id, m_Projects.CurrentSpace().Current().id, strokeIds, name);
    if (result.ok) {
        if (const Engine::Json::Value* strokes = result.data.Find("strokes");
            strokes != nullptr && strokes->IsArray()) {
            for (const auto& item : strokes->AsArray()) {
                ApplyRealtimeEvent(MakeRealtimeEvent(
                    "ink.updated", Engine::Json::Value::Object{{"stroke", item}}));
            }
        }
    }
    return FinishApiResult("GROUP INK", std::move(result));
}

void CrystalWorkspace::SelectRelationship(std::string relationshipId) {
    m_SelectedRelationshipId = std::move(relationshipId);
    m_Selection.Clear();
}

ApiResult CrystalWorkspace::Acquire(
    const std::string& sceneId, const std::string& workerId,
    const std::string& workerName, const std::string& task,
    const std::string& branch, const std::string& taskId,
    const std::string& taskNote) {
    ApiResult connected = RequireConnected("ACQUIRE SCENE");
    if (!connected.ok) return connected;
    SceneLockModel lock;
    ApiResult result = m_Api.AcquireScene(
        sceneId, workerId, workerName, task, branch, lock, taskId, taskNote);
    if (result.ok) {
        m_HeldLocks[sceneId] = workerId;
        ApplyRealtimeEvent(MakeRealtimeEvent("scene_lock.acquired",
            Engine::Json::Value::Object{{"lock", Engine::Json::Value::Object{
                {"scene_id", lock.sceneId}, {"project_id", lock.projectId},
                {"status", lock.status}, {"owner_id", lock.ownerId},
                {"owner_name", lock.ownerName}, {"worker_id", lock.workerId},
                {"task_id", lock.taskId}, {"task", lock.task},
                {"task_note", lock.taskNote}, {"branch", lock.branch},
                {"acquired_at", lock.acquiredAt}, {"heartbeat_at", lock.heartbeatAt}}}}));
    }
    return FinishApiResult("ACQUIRE SCENE", std::move(result));
}

ApiResult CrystalWorkspace::Heartbeat(const std::string& sceneId, const std::string& workerId) {
    ApiResult connected = RequireConnected("HEARTBEAT SCENE");
    if (!connected.ok) return connected;
    SceneLockModel lock;
    ApiResult result = m_Api.HeartbeatScene(sceneId, workerId, lock);
    if (result.ok) {
        ApplyRealtimeEvent(MakeRealtimeEvent("scene_lock.heartbeat",
            Engine::Json::Value::Object{{"lock", Engine::Json::Value::Object{
                {"scene_id", lock.sceneId}, {"project_id", lock.projectId},
                {"status", lock.status}, {"owner_id", lock.ownerId},
                {"owner_name", lock.ownerName}, {"worker_id", lock.workerId},
                {"task_id", lock.taskId}, {"task", lock.task},
                {"task_note", lock.taskNote}, {"branch", lock.branch},
                {"acquired_at", lock.acquiredAt}, {"heartbeat_at", lock.heartbeatAt}}}}));
    }
    return FinishApiResult("HEARTBEAT SCENE", std::move(result));
}

ApiResult CrystalWorkspace::Release(const std::string& sceneId, const std::string& workerId) {
    ApiResult connected = RequireConnected("RELEASE SCENE");
    if (!connected.ok) return connected;
    SceneLockModel lock;
    ApiResult result = m_Api.ReleaseScene(sceneId, workerId, lock);
    if (result.ok) {
        m_HeldLocks.erase(sceneId);
        ApplyRealtimeEvent(MakeRealtimeEvent("scene_lock.released",
            Engine::Json::Value::Object{{"lock", Engine::Json::Value::Object{
                {"scene_id", lock.sceneId}, {"project_id", lock.projectId},
                {"status", lock.status}, {"owner_id", lock.ownerId},
                {"owner_name", lock.ownerName}, {"worker_id", lock.workerId},
                {"task_id", lock.taskId}, {"task", lock.task},
                {"task_note", lock.taskNote}, {"branch", lock.branch},
                {"acquired_at", lock.acquiredAt}, {"heartbeat_at", lock.heartbeatAt}}}}));
    }
    return FinishApiResult("RELEASE SCENE", std::move(result));
}

ApiResult CrystalWorkspace::RenameSpace(const std::string& spaceId, std::string name) {
    ApiResult connected = RequireConnected("RENAME SPACE");
    if (!connected.ok) return connected;
    SpaceModel space;
    const auto found = std::find_if(m_Projects.Workspace().spaces.begin(), m_Projects.Workspace().spaces.end(),
        [&](const SpaceModel& item) { return item.id == spaceId; });
    if (found == m_Projects.Workspace().spaces.end()) return FinishApiResult(
        "RENAME SPACE", {false, 404, {}, "space_not_found"});
    space = *found;
    SpaceModel updated;
    ApiResult result = m_Api.RenameSpace(space, std::move(name), &updated);
    if (result.ok) {
        Engine::Json::Value::Object item;
        item["id"] = updated.id;
        item["project_id"] = updated.projectId;
        item["name"] = updated.name;
        item["camera_x"] = updated.cameraX;
        item["camera_y"] = updated.cameraY;
        item["zoom"] = updated.zoom;
        item["view_type"] = SpaceViewTypeName(updated.viewType);
        item["sort_order"] = updated.sortOrder;
        ApplyRealtimeEvent(MakeRealtimeEvent("space.updated",
            Engine::Json::Value::Object{{"space", std::move(item)}}));
    }
    return FinishApiResult("RENAME SPACE", std::move(result));
}

ApiResult CrystalWorkspace::DuplicateSpace(const std::string& spaceId, std::string name) {
    ApiResult connected = RequireConnected("DUPLICATE SPACE");
    if (!connected.ok) return connected;
    SpaceModel duplicated;
    ApiResult result = m_Api.DuplicateSpace(m_Projects.CurrentProject().id, spaceId, name, &duplicated);
    if (result.ok) {
        Engine::Json::Value::Object item;
        item["id"] = duplicated.id;
        item["project_id"] = duplicated.projectId;
        item["name"] = duplicated.name;
        item["camera_x"] = duplicated.cameraX;
        item["camera_y"] = duplicated.cameraY;
        item["zoom"] = duplicated.zoom;
        item["view_type"] = SpaceViewTypeName(duplicated.viewType);
        item["sort_order"] = duplicated.sortOrder;
        ApplyRealtimeEvent(MakeRealtimeEvent("space.created",
            Engine::Json::Value::Object{{"space", std::move(item)}}));
    }
    return FinishApiResult("DUPLICATE SPACE", std::move(result));
}

ApiResult CrystalWorkspace::DeleteSpace(const std::string& spaceId) {
    ApiResult connected = RequireConnected("DELETE SPACE");
    if (!connected.ok) return connected;
    const auto found = std::find_if(m_Projects.Workspace().spaces.begin(), m_Projects.Workspace().spaces.end(),
        [&](const SpaceModel& item) { return item.id == spaceId; });
    if (found == m_Projects.Workspace().spaces.end()) return FinishApiResult(
        "DELETE SPACE", {false, 404, {}, "space_not_found"});
    ApiResult result = m_Api.DeleteSpace(*found);
    if (result.ok) ApplyRealtimeEvent(MakeRealtimeEvent(
        "space.deleted", Engine::Json::Value::Object{{"space_id", spaceId}}));
    return FinishApiResult("DELETE SPACE", std::move(result));
}

ApiResult CrystalWorkspace::CreateBoard(BoardModel board, BoardModel* created) {
    ApiResult connected = RequireConnected("CREATE BOARD");
    if (!connected.ok) return connected;
    if (board.projectId.empty()) board.projectId = m_Projects.CurrentProject().id;
    if (board.spaceId.empty()) board.spaceId = m_Projects.CurrentSpace().Current().id;
    BoardModel saved;
    ApiResult result = m_Api.CreateBoard(board, &saved);
    if (result.ok) {
        if (created != nullptr) *created = saved;
        ApplyRealtimeEvent(MakeRealtimeEvent("board.created", Engine::Json::Value::Object{{"board", ToJson(saved)}}));
    }
    return FinishApiResult("CREATE BOARD", std::move(result));
}

ApiResult CrystalWorkspace::UpdateBoard(BoardModel board) {
    ApiResult connected = RequireConnected("UPDATE BOARD");
    if (!connected.ok) return connected;
    BoardModel saved;
    ApiResult result = m_Api.UpdateBoard(board, &saved);
    if (result.ok) ApplyRealtimeEvent(MakeRealtimeEvent("board.updated", Engine::Json::Value::Object{{"board", ToJson(saved)}}));
    return FinishApiResult("UPDATE BOARD", std::move(result));
}

ApiResult CrystalWorkspace::DeleteBoard(const std::string& boardId) {
    ApiResult connected = RequireConnected("DELETE BOARD");
    if (!connected.ok) return connected;
    ApiResult result = m_Api.DeleteBoard(boardId);
    if (result.ok) ApplyRealtimeEvent(MakeRealtimeEvent("board.deleted", Engine::Json::Value::Object{{"board_id", boardId}}));
    return FinishApiResult("DELETE BOARD", std::move(result));
}

ApiResult CrystalWorkspace::AttachViewToBoard(
    const std::string& boardId, const std::string& viewId, Engine::Vec2F localPosition) {
    ApiResult connected = RequireConnected("ATTACH VIEW BOARD");
    if (!connected.ok) return connected;
    EntityView updated;
    ApiResult result = m_Api.AttachViewToBoard(boardId, viewId, localPosition, &updated);
    if (result.ok) ApplyRealtimeEvent(MakeRealtimeEvent("view.updated", Engine::Json::Value::Object{{"view", ToJson(updated)}}));
    return FinishApiResult("ATTACH VIEW BOARD", std::move(result));
}

ApiResult CrystalWorkspace::DetachViewFromBoard(
    const std::string& boardId, const std::string& viewId) {
    ApiResult connected = RequireConnected("DETACH VIEW BOARD");
    if (!connected.ok) return connected;
    ApiResult result = m_Api.DetachViewFromBoard(boardId, viewId);
    if (result.ok) {
        EntityView detached;
        bool found = false;
        for (const EntityView& candidate : m_Projects.Workspace().views) {
            if (candidate.id != viewId) continue;
            detached = candidate;
            detached.boardId.clear();
            detached.boardLocalPosition = {};
            found = true;
            break;
        }
        if (found) ApplyRealtimeEvent(MakeRealtimeEvent(
            "view.updated", Engine::Json::Value::Object{{"view", ToJson(detached)}}));
    }
    return FinishApiResult("DETACH VIEW BOARD", std::move(result));
}

ApiResult CrystalWorkspace::CreateLayer(UserLayerModel layer, UserLayerModel* created) {
    ApiResult connected = RequireConnected("CREATE LAYER");
    if (!connected.ok) return connected;
    if (layer.projectId.empty()) layer.projectId = m_Projects.CurrentProject().id;
    UserLayerModel saved;
    ApiResult result = m_Api.CreateLayer(layer, &saved);
    if (result.ok) {
        if (created != nullptr) *created = saved;
        ApplyRealtimeEvent(MakeRealtimeEvent("layer.created", Engine::Json::Value::Object{{"layer", ToJson(saved)}}));
    }
    return FinishApiResult("CREATE LAYER", std::move(result));
}

ApiResult CrystalWorkspace::UpdateLayer(UserLayerModel layer) {
    ApiResult connected = RequireConnected("UPDATE LAYER");
    if (!connected.ok) return connected;
    UserLayerModel saved;
    ApiResult result = m_Api.UpdateLayer(layer, &saved);
    if (result.ok) ApplyRealtimeEvent(MakeRealtimeEvent("layer.updated", Engine::Json::Value::Object{{"layer", ToJson(saved)}}));
    return FinishApiResult("UPDATE LAYER", std::move(result));
}

ApiResult CrystalWorkspace::DeleteLayer(const std::string& layerId, bool deleteVisuals) {
    ApiResult connected = RequireConnected("DELETE LAYER");
    if (!connected.ok) return connected;
    ApiResult result = m_Api.DeleteLayer(layerId, deleteVisuals);
    if (result.ok) ApplyRealtimeEvent(MakeRealtimeEvent("layer.deleted", Engine::Json::Value::Object{{"layer_id", layerId}}));
    return FinishApiResult("DELETE LAYER", std::move(result));
}

ApiResult CrystalWorkspace::CreateSchema(CrystalEntitySchema schema, CrystalEntitySchema* created) {
    ApiResult connected = RequireConnected("CREATE SCHEMA");
    if (!connected.ok) return connected;
    if (schema.projectId.empty()) schema.projectId = m_Projects.CurrentProject().id;
    if (schema.name.empty()) schema.name = "custom_schema";
    if (schema.displayName.empty()) schema.displayName = schema.name;
    CrystalEntitySchema saved;
    ApiResult result = m_Api.CreateSchema(schema.projectId, schema, &saved);
    if (result.ok) {
        if (created != nullptr) *created = saved;
        m_SchemaDraft = saved;
        ApplyRealtimeEvent(MakeRealtimeEvent(
            "schema.created", Engine::Json::Value::Object{{"schema", ToJson(saved)}}));
    }
    return FinishApiResult("CREATE SCHEMA", std::move(result));
}

ApiResult CrystalWorkspace::DeleteSchema(const std::string& schemaId) {
    ApiResult connected = RequireConnected("DELETE SCHEMA");
    if (!connected.ok) return connected;
    ApiResult result = m_Api.DeleteSchema(schemaId);
    if (result.ok) {
        ApplyRealtimeEvent(MakeRealtimeEvent(
            "schema.deleted", Engine::Json::Value::Object{{"schema_id", schemaId}}));
        if (m_SchemaDraft.id == schemaId) {
            m_SchemaDraft = m_Projects.Workspace().schemas.empty()
                ? CrystalEntitySchema{} : m_Projects.Workspace().schemas.front();
        }
    }
    return FinishApiResult("DELETE SCHEMA", std::move(result));
}

ApiResult CrystalWorkspace::CreateSchemaProperty(
    CrystalPropertyDefinition property, CrystalPropertyDefinition* created) {
    ApiResult connected = RequireConnected("CREATE PROPERTY");
    if (!connected.ok) return connected;
    if (property.schemaId.empty()) property.schemaId = m_SchemaDraft.id;
    if (property.name.empty()) property.name = "custom_field";
    if (property.displayName.empty()) property.displayName = property.name;
    CrystalPropertyDefinition saved;
    ApiResult result = m_Api.CreateSchemaProperty(property, &saved);
    if (result.ok) {
        if (created != nullptr) *created = saved;
        ApplyRealtimeEvent(MakeRealtimeEvent("schema.updated", Engine::Json::Value::Object{
            {"schema_id", saved.schemaId}, {"property", ToJson(saved)}}));
        if (m_SchemaDraft.id == saved.schemaId) {
            auto& properties = m_SchemaDraft.properties;
            const auto found = std::find_if(properties.begin(), properties.end(),
                [&](const CrystalPropertyDefinition& value) { return value.id == saved.id; });
            if (found == properties.end()) properties.push_back(saved);
            else *found = saved;
        }
    }
    return FinishApiResult("CREATE PROPERTY", std::move(result));
}

ApiResult CrystalWorkspace::UpdateSchemaProperty(CrystalPropertyDefinition property) {
    ApiResult connected = RequireConnected("UPDATE PROPERTY");
    if (!connected.ok) return connected;
    CrystalPropertyDefinition saved;
    ApiResult result = m_Api.UpdateSchemaProperty(property, &saved);
    if (result.ok) ApplyRealtimeEvent(MakeRealtimeEvent("schema.updated", Engine::Json::Value::Object{
        {"schema_id", saved.schemaId}, {"property", ToJson(saved)}}));
    return FinishApiResult("UPDATE PROPERTY", std::move(result));
}

ApiResult CrystalWorkspace::DeleteSchemaProperty(const std::string& propertyId) {
    ApiResult connected = RequireConnected("DELETE PROPERTY");
    if (!connected.ok) return connected;
    std::string schemaId;
    for (const CrystalEntitySchema& schema : m_Projects.Workspace().schemas) {
        if (std::any_of(schema.properties.begin(), schema.properties.end(),
                [&](const CrystalPropertyDefinition& property) { return property.id == propertyId; })) {
            schemaId = schema.id;
            break;
        }
    }
    ApiResult result = m_Api.DeleteSchemaProperty(propertyId);
    if (result.ok) ApplyRealtimeEvent(MakeRealtimeEvent("schema.updated", Engine::Json::Value::Object{
        {"schema_id", schemaId}, {"property_id", propertyId}}));
    return FinishApiResult("DELETE PROPERTY", std::move(result));
}

ApiResult CrystalWorkspace::CreateQueryView(QueryViewModel query, QueryViewModel* created) {
    ApiResult connected = RequireConnected("CREATE QUERY");
    if (!connected.ok) return connected;
    if (query.projectId.empty()) query.projectId = m_Projects.CurrentProject().id;
    if (query.spaceId.empty()) query.spaceId = m_Projects.CurrentSpace().Current().id;
    QueryViewModel saved;
    ApiResult result = m_Api.CreateQueryView(query, &saved);
    if (result.ok) {
        if (created != nullptr) *created = saved;
        ApplyRealtimeEvent(MakeRealtimeEvent("query_view.created", Engine::Json::Value::Object{{"query_view", ToJson(saved)}}));
    }
    return FinishApiResult("CREATE QUERY", std::move(result));
}

ApiResult CrystalWorkspace::UpdateQueryView(QueryViewModel query) {
    ApiResult connected = RequireConnected("UPDATE QUERY");
    if (!connected.ok) return connected;
    QueryViewModel saved;
    ApiResult result = m_Api.UpdateQueryView(query, &saved);
    if (result.ok) ApplyRealtimeEvent(MakeRealtimeEvent("query_view.updated", Engine::Json::Value::Object{{"query_view", ToJson(saved)}}));
    return FinishApiResult("UPDATE QUERY", std::move(result));
}

ApiResult CrystalWorkspace::DeleteQueryView(const std::string& queryId) {
    ApiResult connected = RequireConnected("DELETE QUERY");
    if (!connected.ok) return connected;
    ApiResult result = m_Api.DeleteQueryView(queryId);
    if (result.ok) ApplyRealtimeEvent(MakeRealtimeEvent("query_view.deleted", Engine::Json::Value::Object{{"query_view_id", queryId}}));
    return FinishApiResult("DELETE QUERY", std::move(result));
}

ApiResult CrystalWorkspace::CreateDashboardWidget(
    DashboardWidgetModel widget, DashboardWidgetModel* created) {
    ApiResult connected = RequireConnected("CREATE WIDGET");
    if (!connected.ok) return connected;
    if (widget.projectId.empty()) widget.projectId = m_Projects.CurrentProject().id;
    if (widget.spaceId.empty()) widget.spaceId = m_Projects.CurrentSpace().Current().id;
    DashboardWidgetModel saved;
    ApiResult result = m_Api.CreateDashboardWidget(widget, &saved);
    if (result.ok) {
        if (created != nullptr) *created = saved;
        ApplyRealtimeEvent(MakeRealtimeEvent("dashboard_widget.created", Engine::Json::Value::Object{{"widget", ToJson(saved)}}));
    }
    return FinishApiResult("CREATE WIDGET", std::move(result));
}

ApiResult CrystalWorkspace::UpdateDashboardWidget(DashboardWidgetModel widget) {
    ApiResult connected = RequireConnected("UPDATE WIDGET");
    if (!connected.ok) return connected;
    DashboardWidgetModel saved;
    ApiResult result = m_Api.UpdateDashboardWidget(widget, &saved);
    if (result.ok) ApplyRealtimeEvent(MakeRealtimeEvent("dashboard_widget.updated", Engine::Json::Value::Object{{"widget", ToJson(saved)}}));
    return FinishApiResult("UPDATE WIDGET", std::move(result));
}

ApiResult CrystalWorkspace::DeleteDashboardWidget(const std::string& widgetId) {
    ApiResult connected = RequireConnected("DELETE WIDGET");
    if (!connected.ok) return connected;
    ApiResult result = m_Api.DeleteDashboardWidget(widgetId);
    if (result.ok) ApplyRealtimeEvent(MakeRealtimeEvent("dashboard_widget.deleted", Engine::Json::Value::Object{{"widget_id", widgetId}}));
    return FinishApiResult("DELETE WIDGET", std::move(result));
}

CrystalWorkspace::VisualState CrystalWorkspace::CaptureVisualState() const {
    VisualState state;
    state.views = m_Projects.Workspace().views;
    state.boards = m_Projects.Workspace().boards;
    state.strokes = m_Projects.Workspace().strokes;
    state.widgets = m_Projects.Workspace().dashboardWidgets;
    return state;
}

void CrystalWorkspace::ApplyVisualState(const VisualState& state) {
    m_Projects.Workspace().views = state.views;
    m_Projects.Workspace().boards = state.boards;
    m_Projects.Workspace().strokes = state.strokes;
    m_Projects.Workspace().dashboardWidgets = state.widgets;
    m_Ink.Load(state.strokes);
}

void CrystalWorkspace::RecordVisualChange(
    std::string label, VisualState before, VisualState after) {
    m_VisualHistory.Execute({
        std::move(label),
        [this, before = std::move(before)]() { ApplyVisualState(before); },
        [this, after = std::move(after)]() { ApplyVisualState(after); },
    });
}

bool CrystalWorkspace::SelectionHas(
    Engine::UI2D::SelectionKind kind, const std::string& id) const {
    return !id.empty() && m_Selection.IsSelected(kind, id);
}

bool CrystalWorkspace::SelectionHasBoard(const std::string& id) const {
    return SelectionHas(Engine::UI2D::SelectionKind::Board, id);
}

bool CrystalWorkspace::SelectionHasInk(const Engine::UI2D::InkStroke& stroke) const {
    if (SelectionHas(Engine::UI2D::SelectionKind::InkStroke, stroke.id)) return true;
    return !stroke.groupId.empty() &&
        SelectionHas(Engine::UI2D::SelectionKind::InkGroup, stroke.groupId);
}

std::string CrystalWorkspace::NewLocalId(std::string_view prefix) const {
    static std::uint64_t serial = 0;
    return std::string(prefix) + "-local-" + std::to_string(++serial);
}

bool CrystalWorkspace::IsLayerLocked(const std::string& layerId) const noexcept {
    for (const UserLayerModel& layer : m_Projects.Workspace().userLayers)
        if (layer.id == layerId || layer.name == layerId) return layer.locked;
    return false;
}

bool CrystalWorkspace::MoveSelectionLocal(Engine::Vec2F delta) {
    if (delta.x == 0.0f && delta.y == 0.0f) return false;
    const std::string spaceId = m_Projects.CurrentSpace().Current().id;
    const VisualState before = CaptureVisualState();
    bool changed = false;
    std::unordered_set<std::string> movedBoards;

    for (BoardModel& board : m_Projects.Workspace().boards) {
        if (board.spaceId != spaceId || !SelectionHasBoard(board.id) || board.locked ||
            IsLayerLocked(board.layerId)) continue;
        board.position.x += delta.x;
        board.position.y += delta.y;
        movedBoards.insert(board.id);
        changed = true;
        // Child views use board-local coordinates on the server. Keep their
        // cached absolute positions in step with the board for immediate
        // rendering; the board save will derive the same values server-side.
        for (EntityView& view : m_Projects.Workspace().views) {
            if (view.spaceId == spaceId && view.boardId == board.id) {
                view.position.x += delta.x;
                view.position.y += delta.y;
            }
        }
    }

    for (EntityView& view : m_Projects.Workspace().views) {
        if (view.spaceId != spaceId ||
            (!SelectionHas(Engine::UI2D::SelectionKind::EntityView, view.entityId) &&
             !SelectionHas(Engine::UI2D::SelectionKind::EntityView, view.id)) ||
            movedBoards.find(view.boardId) != movedBoards.end() || IsLayerLocked(view.layerId))
            continue;
        view.position.x += delta.x;
        view.position.y += delta.y;
        if (!view.boardId.empty()) {
            view.boardLocalPosition.x += delta.x;
            view.boardLocalPosition.y += delta.y;
        }
        changed = true;
    }

    for (DashboardWidgetModel& widget : m_Projects.Workspace().dashboardWidgets) {
        if (widget.spaceId != spaceId ||
            !SelectionHas(Engine::UI2D::SelectionKind::DashboardWidget, widget.id) ||
            IsLayerLocked(widget.layerId)) continue;
        widget.position.x += delta.x;
        widget.position.y += delta.y;
        changed = true;
    }

    for (Engine::UI2D::InkStroke& stroke : m_Projects.Workspace().strokes) {
        if (!SelectionHasInk(stroke) || IsLayerLocked(stroke.layer)) continue;
        for (Engine::Vec2F& point : stroke.points) {
            point.x += delta.x;
            point.y += delta.y;
        }
        // Anchor offsets describe the anchor position, not the stroke's
        // local transform. Leave them unchanged so anchored ink remains
        // attached while the selected stroke moves relative to its parent.
        changed = true;
    }

    if (!changed) return false;
    const VisualState after = CaptureVisualState();
    m_Ink.Load(after.strokes);
    RecordVisualChange("Move visual selection", before, after);
    return true;
}

bool CrystalWorkspace::ResizeSelectedBoard(Engine::Vec2F size) {
    const std::string spaceId = m_Projects.CurrentSpace().Current().id;
    const VisualState before = CaptureVisualState();
    bool changed = false;
    for (BoardModel& board : m_Projects.Workspace().boards) {
        if (board.spaceId != spaceId || !SelectionHasBoard(board.id) || board.locked ||
            IsLayerLocked(board.layerId)) continue;
        const Engine::Vec2F clamped{
            std::max(120.0f, size.x), std::max(80.0f, size.y)};
        if (board.size == clamped) continue;
        board.size = clamped;
        changed = true;
    }
    if (!changed) return false;
    const VisualState after = CaptureVisualState();
    RecordVisualChange("Resize board", before, after);
    return true;
}

bool CrystalWorkspace::ResizeSelectedWidget(Engine::Vec2F size) {
    const std::string spaceId = m_Projects.CurrentSpace().Current().id;
    const VisualState before = CaptureVisualState();
    bool changed = false;
    for (DashboardWidgetModel& widget : m_Projects.Workspace().dashboardWidgets) {
        if (widget.spaceId != spaceId ||
            !SelectionHas(Engine::UI2D::SelectionKind::DashboardWidget, widget.id) ||
            IsLayerLocked(widget.layerId)) continue;
        const Engine::Vec2F clamped{
            std::max(160.0f, size.x), std::max(90.0f, size.y)};
        if (widget.size == clamped) continue;
        widget.size = clamped;
        changed = true;
    }
    if (!changed) return false;
    const VisualState after = CaptureVisualState();
    RecordVisualChange("Resize dashboard widget", before, after);
    return true;
}

ApiResult CrystalWorkspace::ToggleSelectedBoardCollapsed() {
    ApiResult connected = RequireConnected("TOGGLE BOARD");
    if (!connected.ok) return connected;
    for (BoardModel board : m_Projects.Workspace().boards) {
        if (!SelectionHasBoard(board.id) || board.spaceId != m_Projects.CurrentSpace().Current().id)
            continue;
        if (board.locked || IsLayerLocked(board.layerId))
            return FinishApiResult("TOGGLE BOARD", {false, 409, {}, "board_locked"});
        board.collapsed = !board.collapsed;
        return UpdateBoard(std::move(board));
    }
    return {false, 404, {}, "board_not_selected"};
}

ApiResult CrystalWorkspace::PersistVisualSelection() {
    ApiResult connected = RequireConnected("SAVE VISUAL SELECTION");
    if (!connected.ok) return connected;
    const std::string spaceId = m_Projects.CurrentSpace().Current().id;
    std::unordered_set<std::string> selectedStrokeIds;

    for (const BoardModel& board : m_Projects.Workspace().boards) {
        if (board.spaceId != spaceId || !SelectionHasBoard(board.id)) continue;
        ApiResult result = UpdateBoard(board);
        if (!result.ok) return result;
    }
    for (const EntityView& view : m_Projects.Workspace().views) {
        if (view.spaceId != spaceId ||
            (!SelectionHas(Engine::UI2D::SelectionKind::EntityView, view.entityId) &&
             !SelectionHas(Engine::UI2D::SelectionKind::EntityView, view.id)) ||
            (!view.boardId.empty() && SelectionHasBoard(view.boardId))) continue;
        ApiResult result = SaveView(view);
        if (!result.ok) return result;
    }
    for (const DashboardWidgetModel& widget : m_Projects.Workspace().dashboardWidgets) {
        if (widget.spaceId != spaceId ||
            !SelectionHas(Engine::UI2D::SelectionKind::DashboardWidget, widget.id)) continue;
        ApiResult result = UpdateDashboardWidget(widget);
        if (!result.ok) return result;
    }
    for (const Engine::UI2D::InkStroke& stroke : m_Projects.Workspace().strokes) {
        if (!SelectionHasInk(stroke) || !selectedStrokeIds.insert(stroke.id).second) continue;
        ApiResult result = m_Api.UpdateStroke(stroke, nullptr);
        if (result.ok) {
            ApplyRealtimeEvent(MakeRealtimeEvent(
                "ink.updated", Engine::Json::Value::Object{{"stroke", ToJson(stroke)}}));
        } else {
            return FinishApiResult("SAVE INK", std::move(result));
        }
    }
    m_Notification.clear();
    return {true, 200, {}, {}};
}

ApiResult CrystalWorkspace::DeleteVisualSelection() {
    ApiResult connected = RequireConnected("DELETE VISUAL SELECTION");
    if (!connected.ok) return connected;
    const std::string spaceId = m_Projects.CurrentSpace().Current().id;
    std::unordered_set<std::string> strokeIds;
    for (const Engine::UI2D::InkStroke& stroke : m_Projects.Workspace().strokes)
        if (SelectionHasInk(stroke)) strokeIds.insert(stroke.id);

    for (const EntityView& view : m_Projects.Workspace().views) {
        if (view.spaceId != spaceId ||
            (!SelectionHas(Engine::UI2D::SelectionKind::EntityView, view.entityId) &&
             !SelectionHas(Engine::UI2D::SelectionKind::EntityView, view.id))) continue;
        if (IsLayerLocked(view.layerId)) return FinishApiResult(
            "DELETE VIEW", {false, 409, {}, "layer_locked"});
        ApiResult result = DeleteView(view.entityId, view.spaceId);
        if (!result.ok) return result;
    }
    for (const BoardModel& board : m_Projects.Workspace().boards) {
        if (board.spaceId != spaceId || !SelectionHasBoard(board.id)) continue;
        if (board.locked || IsLayerLocked(board.layerId)) return FinishApiResult(
            "DELETE BOARD", {false, 409, {}, "board_locked"});
        ApiResult result = DeleteBoard(board.id);
        if (!result.ok) return result;
    }
    for (const DashboardWidgetModel& widget : m_Projects.Workspace().dashboardWidgets) {
        if (widget.spaceId != spaceId ||
            !SelectionHas(Engine::UI2D::SelectionKind::DashboardWidget, widget.id)) continue;
        if (IsLayerLocked(widget.layerId)) return FinishApiResult(
            "DELETE WIDGET", {false, 409, {}, "layer_locked"});
        ApiResult result = DeleteDashboardWidget(widget.id);
        if (!result.ok) return result;
    }
    for (const std::string& strokeId : strokeIds) {
        const auto found = std::find_if(m_Projects.Workspace().strokes.begin(),
            m_Projects.Workspace().strokes.end(),
            [&](const Engine::UI2D::InkStroke& stroke) { return stroke.id == strokeId; });
        if (found != m_Projects.Workspace().strokes.end() && IsLayerLocked(found->layer))
            return FinishApiResult("DELETE INK", {false, 409, {}, "layer_locked"});
        ApiResult result = DeleteStroke(strokeId);
        if (!result.ok) return result;
    }
    m_Selection.Clear();
    return {true, 204, {}, {}};
}

void CrystalWorkspace::CopyVisualSelection() {
    m_Clipboard = {};
    const std::string spaceId = m_Projects.CurrentSpace().Current().id;
    for (const EntityView& view : m_Projects.Workspace().views) {
        if (view.spaceId == spaceId &&
            (SelectionHas(Engine::UI2D::SelectionKind::EntityView, view.entityId) ||
             SelectionHas(Engine::UI2D::SelectionKind::EntityView, view.id)))
            m_Clipboard.views.push_back(view);
    }
    for (const BoardModel& board : m_Projects.Workspace().boards)
        if (board.spaceId == spaceId && SelectionHasBoard(board.id)) m_Clipboard.boards.push_back(board);
    for (const DashboardWidgetModel& widget : m_Projects.Workspace().dashboardWidgets)
        if (widget.spaceId == spaceId &&
            SelectionHas(Engine::UI2D::SelectionKind::DashboardWidget, widget.id))
            m_Clipboard.widgets.push_back(widget);
    for (const Engine::UI2D::InkStroke& stroke : m_Projects.Workspace().strokes)
        if (SelectionHasInk(stroke)) m_Clipboard.strokes.push_back(stroke);
    m_HasClipboard = !m_Clipboard.views.empty() || !m_Clipboard.boards.empty() ||
        !m_Clipboard.widgets.empty() || !m_Clipboard.strokes.empty();
}

bool CrystalWorkspace::PasteVisualSelectionLocal(Engine::Vec2F offset) {
    if (!m_HasClipboard) return false;
    ApiResult connected = RequireConnected("PASTE VISUAL SELECTION");
    if (!connected.ok) return false;
    const std::string spaceId = m_Projects.CurrentSpace().Current().id;
    bool pasted = false;
    m_Selection.Clear();
    for (const BoardModel& source : m_Clipboard.boards) {
        BoardModel copy = source;
        copy.id.clear();
        copy.position.x += offset.x;
        copy.position.y += offset.y;
        copy.title += " Copy";
        copy.locked = false;
        BoardModel saved;
        ApiResult result = CreateBoard(std::move(copy), &saved);
        if (!result.ok) return false;
        m_Selection.SelectBoard(saved.id, pasted);
        pasted = true;
    }
    for (const DashboardWidgetModel& source : m_Clipboard.widgets) {
        DashboardWidgetModel copy = source;
        copy.id.clear();
        copy.spaceId = spaceId;
        copy.position.x += offset.x;
        copy.position.y += offset.y;
        copy.revision = 1;
        DashboardWidgetModel saved;
        ApiResult result = CreateDashboardWidget(std::move(copy), &saved);
        if (!result.ok) return false;
        m_Selection.SelectWidget(saved.id, pasted);
        pasted = true;
    }
    std::string copiedGroupId;
    for (const Engine::UI2D::InkStroke& source : m_Clipboard.strokes) {
        Engine::UI2D::InkStroke copy = source;
        copy.id.clear();
        copy.anchorEntityId.clear();
        copy.anchorBoardId.clear();
        copy.localOffset = {};
        for (Engine::Vec2F& point : copy.points) {
            point.x += offset.x;
            point.y += offset.y;
        }
        if (!source.groupId.empty()) {
            if (copiedGroupId.empty()) copiedGroupId = NewLocalId("ink-group");
            copy.groupId = copiedGroupId;
        }
        ApiResult result = CreateStroke(copy);
        if (!result.ok) return false;
        const auto found = std::find_if(m_Projects.Workspace().strokes.begin(),
            m_Projects.Workspace().strokes.end(),
            [&](const Engine::UI2D::InkStroke& stroke) {
                return stroke.id == copy.id || (copy.id.empty() && stroke.points == copy.points);
            });
        if (found != m_Projects.Workspace().strokes.end()) {
            if (!found->groupId.empty()) m_Selection.SelectInkGroup(found->groupId, pasted);
            else m_Selection.SelectInk(found->id, pasted);
        }
        pasted = true;
    }
    for (const EntityView& sourceView : m_Clipboard.views) {
        const bool alreadyVisible = std::any_of(m_Projects.Workspace().views.begin(),
            m_Projects.Workspace().views.end(),
            [&](const EntityView& view) {
                return view.entityId == sourceView.entityId && view.spaceId == spaceId;
            });
        // A view clipboard never creates an Entity. Pasting in the same
        // space is a no-op because the server deliberately keeps one view per
        // entity/space pair; switching space before paste creates the new
        // visual representation.
        if (alreadyVisible) continue;
        EntityView view = sourceView;
        view.id.clear();
        view.spaceId = spaceId;
        view.position.x += offset.x;
        view.position.y += offset.y;
        view.boardId.clear();
        view.boardLocalPosition = {};
        view.revision = 1;
        EntityView saved;
        ApiResult viewResult = m_Api.SaveView(view, -1, &saved);
        if (!viewResult.ok) {
            (void)FinishApiResult("PASTE VIEW", std::move(viewResult));
            return false;
        }
        ApplyRealtimeEvent(MakeRealtimeEvent(
            "view.created", Engine::Json::Value::Object{{"view", ToJson(saved)}}));
        m_Selection.Select(saved.entityId, pasted);
        pasted = true;
    }
    return pasted;
}

ApiResult CrystalWorkspace::DuplicateVisualSelection() {
    CopyVisualSelection();
    if (!m_HasClipboard) return {false, 404, {}, "visual_selection_empty"};
    return PasteVisualSelectionLocal() ? ApiResult{true, 201, {}, {}}
                                       : ApiResult{false, 0, {}, "paste_failed"};
}

bool CrystalWorkspace::UndoVisualChange() { return m_VisualHistory.Undo(); }

bool CrystalWorkspace::RedoVisualChange() { return m_VisualHistory.Redo(); }

bool CrystalWorkspace::FocusSelection() {
    const std::string spaceId = m_Projects.CurrentSpace().Current().id;
    float left = 0.0f;
    float right = 0.0f;
    float top = 0.0f;
    float bottom = 0.0f;
    bool found = false;
    const auto include = [&](Engine::Vec2F position, Engine::Vec2F size) {
        if (!found) {
            left = position.x; right = position.x + size.x;
            top = position.y; bottom = position.y + size.y; found = true;
        } else {
            left = std::min(left, position.x); right = std::max(right, position.x + size.x);
            top = std::min(top, position.y); bottom = std::max(bottom, position.y + size.y);
        }
    };
    for (const EntityView& view : m_Projects.Workspace().views)
        if (view.spaceId == spaceId &&
            (SelectionHas(Engine::UI2D::SelectionKind::EntityView, view.entityId) ||
             SelectionHas(Engine::UI2D::SelectionKind::EntityView, view.id)))
            include(view.position, view.size);
    for (const BoardModel& board : m_Projects.Workspace().boards)
        if (board.spaceId == spaceId && SelectionHasBoard(board.id)) include(board.position, board.size);
    for (const DashboardWidgetModel& widget : m_Projects.Workspace().dashboardWidgets)
        if (widget.spaceId == spaceId &&
            SelectionHas(Engine::UI2D::SelectionKind::DashboardWidget, widget.id))
            include(widget.position, widget.size);
    for (const Engine::UI2D::InkStroke& stroke : m_Projects.Workspace().strokes) {
        if (!SelectionHasInk(stroke) || stroke.points.empty()) continue;
        for (const Engine::Vec2F point : stroke.points) include(point, {});
    }
    if (!found) return false;
    const float width = std::max(1.0f, right - left);
    const float height = std::max(1.0f, bottom - top);
    auto& camera = m_Projects.CurrentSpace().Canvas().Camera();
    camera.centerX = (left + right) * 0.5;
    camera.centerY = (top + bottom) * 0.5;
    const float viewportWidth = std::max(320.0f, camera.viewportSize.x - 320.0f);
    const float viewportHeight = std::max(240.0f, camera.viewportSize.y - 100.0f);
    camera.SetZoom(std::clamp(static_cast<double>(std::min(
        viewportWidth / (width + 80.0f), viewportHeight / (height + 80.0f))), 0.08, 4.0));
    return true;
}

bool CrystalWorkspace::FitContent() {
    m_Selection.Clear();
    for (const EntityView& view : m_Projects.Workspace().views)
        if (view.spaceId == m_Projects.CurrentSpace().Current().id && view.visible) {
            m_Selection.Select(view.entityId, true);
        }
    if (!m_Selection.HasSelection()) return false;
    const bool focused = FocusSelection();
    m_Selection.Clear();
    return focused;
}

void CrystalWorkspace::ToggleQueryBuilder() {
    if (m_Panel == WorkspacePanel::QueryBuilder) {
        m_Panel = WorkspacePanel::None;
        return;
    }
    m_Panel = WorkspacePanel::QueryBuilder;
    const auto& queries = m_Projects.Workspace().queryViews;
    if (!queries.empty()) {
        m_QueryDraft = queries.front();
        return;
    }
    m_QueryDraft = {};
    m_QueryDraft.projectId = m_Projects.CurrentProject().id;
    m_QueryDraft.spaceId = m_Projects.CurrentSpace().Current().id;
    m_QueryDraft.name = "New Query";
    if (!m_Projects.Workspace().schemas.empty())
        m_QueryDraft.entitySchema = m_Projects.Workspace().schemas.front().name;
    else
        m_QueryDraft.entitySchema = "task";
    m_QueryDraft.resultLimit = 50;
    RefreshQueryResults();
}

void CrystalWorkspace::ToggleSchemaEditor() {
    if (m_Panel == WorkspacePanel::SchemaEditor) {
        m_Panel = WorkspacePanel::None;
        return;
    }
    m_Panel = WorkspacePanel::SchemaEditor;
    if (!m_Projects.Workspace().schemas.empty()) m_SchemaDraft = m_Projects.Workspace().schemas.front();
    else m_SchemaDraft = {};
}

void CrystalWorkspace::AddQueryFilter() {
    if (m_Panel != WorkspacePanel::QueryBuilder) return;
    QueryFilterModel filter;
    for (const auto& schema : m_Projects.Workspace().schemas) {
        if (schema.name != m_QueryDraft.entitySchema && schema.displayName != m_QueryDraft.entitySchema)
            continue;
        if (!schema.properties.empty()) filter.property = schema.properties.front().name;
        break;
    }
    if (filter.property.empty()) filter.property = "title";
    filter.value = Engine::Json::Value("");
    m_QueryDraft.filters.push_back(std::move(filter));
    RefreshQueryResults();
}

void CrystalWorkspace::CycleLastQueryFilterOperator() {
    if (m_Panel != WorkspacePanel::QueryBuilder || m_QueryDraft.filters.empty()) return;
    QueryFilterModel& filter = m_QueryDraft.filters.back();
    if (filter.op == "Equals") filter.op = "NotEquals";
    else if (filter.op == "NotEquals") filter.op = "Contains";
    else filter.op = "Equals";
    RefreshQueryResults();
}

void CrystalWorkspace::RemoveLastQueryFilter() {
    if (m_Panel != WorkspacePanel::QueryBuilder || m_QueryDraft.filters.empty()) return;
    m_QueryDraft.filters.pop_back();
    RefreshQueryResults();
}

void CrystalWorkspace::SetLastQueryFilterValue(Engine::Json::Value value) {
    if (m_Panel != WorkspacePanel::QueryBuilder || m_QueryDraft.filters.empty()) return;
    m_QueryDraft.filters.back().value = std::move(value);
    RefreshQueryResults();
}

void CrystalWorkspace::BeginNewSchemaDraft() {
    m_Panel = WorkspacePanel::SchemaEditor;
    m_SchemaDraft = {};
    m_SchemaDraft.projectId = m_Projects.CurrentProject().id;
    m_SchemaDraft.name = "TestAsset";
    m_SchemaDraft.displayName = "Test Asset";
    m_SchemaDraft.description = "Phase5 temporary schema";
    const std::array<std::pair<const char*, PropertyType>, 5> properties = {{
        {"Status", PropertyType::Enum}, {"Owner", PropertyType::Text},
        {"Tags", PropertyType::Tags}, {"Progress", PropertyType::Progress},
        {"AssetPath", PropertyType::URL}}};
    for (std::size_t index = 0; index < properties.size(); ++index) {
        CrystalPropertyDefinition property;
        property.name = properties[index].first;
        property.displayName = properties[index].first;
        property.type = properties[index].second;
        property.sortOrder = static_cast<int>(index);
        m_SchemaDraft.properties.push_back(std::move(property));
    }
}

void CrystalWorkspace::AddSchemaDraftProperty() {
    CrystalPropertyDefinition property;
    property.schemaId = m_SchemaDraft.id;
    property.name = "CustomField" + std::to_string(m_SchemaDraft.properties.size() + 1);
    property.displayName = property.name;
    property.type = PropertyType::Text;
    property.sortOrder = static_cast<int>(m_SchemaDraft.properties.size());
    if (m_SchemaDraft.id.empty()) m_SchemaDraft.properties.push_back(std::move(property));
    else (void)CreateSchemaProperty(std::move(property));
}

void CrystalWorkspace::RemoveSchemaDraftProperty() {
    if (m_SchemaDraft.properties.empty()) return;
    const std::string propertyId = m_SchemaDraft.properties.back().id;
    if (m_SchemaDraft.id.empty() || propertyId.empty()) m_SchemaDraft.properties.pop_back();
    else (void)DeleteSchemaProperty(propertyId);
}

ApiResult CrystalWorkspace::SaveQueryDraft() {
    if (m_QueryDraft.name.empty()) m_QueryDraft.name = "New Query";
    m_QueryDraft.projectId = m_Projects.CurrentProject().id;
    m_QueryDraft.spaceId = m_Projects.CurrentSpace().Current().id;
    if (m_QueryDraft.id.empty()) return CreateQueryView(m_QueryDraft, &m_QueryDraft);
    return UpdateQueryView(m_QueryDraft);
}

ApiResult CrystalWorkspace::SaveSchemaDraft() {
    if (m_SchemaDraft.id.empty()) return CreateSchema(m_SchemaDraft, &m_SchemaDraft);
    ApiResult result = m_Api.UpdateSchema(m_SchemaDraft, &m_SchemaDraft);
    if (result.ok) ApplyRealtimeEvent(MakeRealtimeEvent(
        "schema.updated", Engine::Json::Value::Object{{"schema", ToJson(m_SchemaDraft)}}));
    return FinishApiResult("SAVE SCHEMA", std::move(result));
}

ApiResult CrystalWorkspace::CommitInlineEdit() {
    if (!m_InlineEdit.active) return {false, 400, {}, "inline_edit_not_active"};
    if (m_InlineEdit.draft.empty()) {
        CancelInlineEdit();
        return {false, 422, {}, "inline_edit_empty"};
    }
    ApiResult result{true, 200, {}, {}};
    if (m_InlineEdit.targetType == "Board") {
        for (const BoardModel& candidate : m_Projects.Workspace().boards) {
            if (candidate.id != m_InlineEdit.targetId) continue;
            BoardModel board = candidate;
            board.title = m_InlineEdit.draft;
            result = UpdateBoard(std::move(board));
            break;
        }
    } else if (m_InlineEdit.targetType == "Layer") {
        for (const UserLayerModel& candidate : m_Projects.Workspace().userLayers) {
            if (candidate.id != m_InlineEdit.targetId) continue;
            UserLayerModel layer = candidate;
            layer.name = m_InlineEdit.draft;
            result = UpdateLayer(std::move(layer));
            break;
        }
    } else if (m_InlineEdit.targetType == "Widget") {
        for (const DashboardWidgetModel& candidate : m_Projects.Workspace().dashboardWidgets) {
            if (candidate.id != m_InlineEdit.targetId) continue;
            DashboardWidgetModel widget = candidate;
            widget.title = m_InlineEdit.draft;
            result = UpdateDashboardWidget(std::move(widget));
            break;
        }
    } else if (m_InlineEdit.targetType == "Schema") {
        if (m_SchemaDraft.id != m_InlineEdit.targetId) {
            result = {false, 404, {}, "schema_not_selected"};
        } else {
            m_SchemaDraft.displayName = m_InlineEdit.draft;
            result = SaveSchemaDraft();
        }
    } else {
        result = {false, 400, {}, "inline_edit_target_unknown"};
    }
    if (result.ok) CancelInlineEdit();
    return result;
}

void CrystalWorkspace::SetViewport(Engine::Vec2F size) noexcept {
    m_Projects.CurrentSpace().SetViewport(size);
}

const std::string& CrystalWorkspace::CurrentSceneStatus(const std::string& sceneId) const {
    static const std::string unknown = "UNKNOWN";
    for (const SceneLockModel& lock : m_Projects.Workspace().sceneLocks)
        if (lock.sceneId == sceneId) return lock.status;
    return unknown;
}

bool CrystalWorkspace::PumpRealtimeEvents() {
    std::deque<CrystalRealtimeEvent> pending;
    for (CrystalRealtimeEvent& event : m_Realtime.DrainEvents()) pending.push_back(std::move(event));
    const bool hadEvents = !pending.empty();
    while (!pending.empty()) {
        CrystalRealtimeEvent event = std::move(pending.front());
        pending.pop_front();
        if (event.type == CrystalRealtimeEventType::ConnectionState &&
            event.connectionState == Engine::Network::WebSocketState::Connected) {
            ApplyConnectionState(event.connectionState);
            for (CrystalRealtimeEvent& queued : m_Realtime.DrainEvents())
                pending.push_back(std::move(queued));
            continue;
        }
        ApplyRealtimeEvent(event);
    }
    MaybeHeartbeat();
    return hadEvents;
}

void CrystalWorkspace::LoadSyntheticPerformanceFixtureForDebug() {
#if defined(_DEBUG)
    auto& model = m_Projects.Workspace();
    const std::string spaceId = m_Projects.CurrentSpace().Current().id.empty()
        ? (model.currentSpace.id.empty() ? "perf-space" : model.currentSpace.id)
        : m_Projects.CurrentSpace().Current().id;
    if (model.currentSpace.id.empty()) {
        model.currentSpace.id = spaceId;
        model.currentSpace.projectId = model.project.id;
        model.currentSpace.name = "Performance Fixture";
        model.spaces.push_back(model.currentSpace);
    }
    model.entities.clear();
    model.views.clear();
    model.relationships.clear();
    model.strokes.clear();
    model.boards.clear();
    model.dashboardWidgets.clear();
    if (model.userLayers.empty()) {
        UserLayerModel main;
        main.id = model.project.id + ":Main";
        main.projectId = model.project.id;
        main.name = "Main";
        main.spaceId = spaceId;
        model.userLayers.push_back(std::move(main));
    }
    for (int index = 0; index < 300; ++index) {
        CrystalEntity entity;
        entity.id = "perf-entity-" + std::to_string(index);
        entity.projectId = model.project.id;
        entity.type = EntityType::Note;
        entity.typeName = "note";
        entity.title = "Fixture View " + std::to_string(index);
        model.entities.push_back(entity);
        EntityView view;
        view.id = "perf-view-" + std::to_string(index);
        view.entityId = entity.id;
        view.spaceId = spaceId;
        view.position = {static_cast<float>((index % 20) * 270),
            static_cast<float>((index / 20) * 150)};
        view.layerId = "Main";
        model.views.push_back(std::move(view));
    }
    for (int index = 0; index < 100; ++index) {
        CrystalRelationship relationship;
        relationship.id = "perf-relationship-" + std::to_string(index);
        relationship.projectId = model.project.id;
        relationship.sourceEntityId = "perf-entity-" + std::to_string(index % 300);
        relationship.targetEntityId = "perf-entity-" + std::to_string((index * 7 + 3) % 300);
        relationship.type = RelationshipType::RelatedTo;
        model.relationships.push_back(std::move(relationship));
    }
    for (int index = 0; index < 20; ++index) {
        BoardModel board;
        board.id = "perf-board-" + std::to_string(index);
        board.projectId = model.project.id;
        board.spaceId = spaceId;
        board.title = "Fixture Board " + std::to_string(index);
        board.position = {static_cast<float>((index % 5) * 1050),
            static_cast<float>((index / 5) * 850)};
        board.size = {920.0f, 700.0f};
        board.layerId = "Main";
        board.zOrder = index;
        model.boards.push_back(std::move(board));
    }
    for (int index = 0; index < 500; ++index) {
        Engine::UI2D::InkStroke stroke;
        stroke.id = "perf-stroke-" + std::to_string(index);
        stroke.layer = "Main";
        stroke.points = {{static_cast<float>((index % 50) * 110),
            static_cast<float>((index / 50) * 55)},
            {static_cast<float>((index % 50) * 110 + 70),
            static_cast<float>((index / 50) * 55 + 24)}};
        stroke.width = 2.0f;
        model.strokes.push_back(std::move(stroke));
    }
    for (int index = 0; index < 20; ++index) {
        DashboardWidgetModel widget;
        widget.id = "perf-widget-" + std::to_string(index);
        widget.projectId = model.project.id;
        widget.spaceId = spaceId;
        widget.widgetType = index % 2 == 0 ? "EntityList" : "RecentActivity";
        widget.title = "Fixture Widget " + std::to_string(index);
        widget.position = {static_cast<float>((index % 5) * 520),
            static_cast<float>((index / 5) * 360)};
        widget.layerId = "Main";
        widget.config = Engine::Json::Value::Object{{"text", "PERF"}};
        model.dashboardWidgets.push_back(std::move(widget));
    }
    m_Ink.Load(model.strokes);
    m_Selection.Clear();
    m_Notification = "DEBUG: FIXTURE 300V/100R/500INK/20B/20W LOADED";
#else
    m_Notification = "DEBUG FIXTURE AVAILABLE IN DEBUG BUILD";
#endif
}

void CrystalWorkspace::ApplyRealtimeEvent(const CrystalRealtimeEvent& event) {
    switch (event.type) {
    case CrystalRealtimeEventType::ConnectionState:
        if (event.connectionState != Engine::Network::WebSocketState::Connected)
            ApplyConnectionState(event.connectionState);
        return;
    case CrystalRealtimeEventType::WorkspaceSnapshot: {
        const Engine::Json::Value* workspace = event.payload.Find("workspace");
        ApplyAuthoritativeSnapshot(workspace == nullptr ? event.payload : *workspace);
        return;
    }
    case CrystalRealtimeEventType::EntityCreated:
    case CrystalRealtimeEventType::EntityUpdated:
    case CrystalRealtimeEventType::WorkerUpdated:
        if (const Engine::Json::Value* item = event.payload.Find("entity"))
            ReplaceEntity(EntityFromJson(*item));
        return;
    case CrystalRealtimeEventType::EntityDeleted: {
        const std::string id = event.payload.GetString("entity_id");
        auto& entities = m_Projects.Workspace().entities;
        entities.erase(std::remove_if(entities.begin(), entities.end(),
            [&](const CrystalEntity& item) { return item.id == id; }), entities.end());
        auto& views = m_Projects.Workspace().views;
        views.erase(std::remove_if(views.begin(), views.end(),
            [&](const EntityView& item) { return item.entityId == id; }), views.end());
        auto& relationships = m_Projects.Workspace().relationships;
        relationships.erase(std::remove_if(relationships.begin(), relationships.end(),
            [&](const CrystalRelationship& item) {
                return item.sourceEntityId == id || item.targetEntityId == id;
            }), relationships.end());
        if (m_Selection.SelectedId() == id) m_Selection.Clear();
        RefreshQueryResults();
        return;
    }
    case CrystalRealtimeEventType::ViewCreated:
    case CrystalRealtimeEventType::ViewUpdated:
        if (const Engine::Json::Value* item = event.payload.Find("view"))
            ReplaceView(EntityViewFromJson(*item));
        return;
    case CrystalRealtimeEventType::ViewDeleted: {
        const std::string id = event.payload.GetString("view_id");
        auto& views = m_Projects.Workspace().views;
        views.erase(std::remove_if(views.begin(), views.end(),
            [&](const EntityView& item) { return item.id == id; }), views.end());
        return;
    }
    case CrystalRealtimeEventType::RelationshipCreated:
    case CrystalRealtimeEventType::RelationshipUpdated:
        if (const Engine::Json::Value* item = event.payload.Find("relationship")) {
            CrystalRelationship relationship = RelationshipFromJson(*item);
            auto& relationships = m_Projects.Workspace().relationships;
            const auto found = std::find_if(relationships.begin(), relationships.end(),
                [&](const CrystalRelationship& item) { return item.id == relationship.id; });
            if (found == relationships.end()) relationships.push_back(std::move(relationship));
            else *found = std::move(relationship);
        }
        return;
    case CrystalRealtimeEventType::RelationshipDeleted: {
        const Engine::Json::Value* item = event.payload.Find("relationship");
        const std::string id = item == nullptr
            ? event.payload.GetString("relationship_id") : item->GetString("id");
        auto& relationships = m_Projects.Workspace().relationships;
        relationships.erase(std::remove_if(relationships.begin(), relationships.end(),
            [&](const CrystalRelationship& relationship) { return relationship.id == id; }),
            relationships.end());
        return;
    }
    case CrystalRealtimeEventType::SpaceCreated: {
        const Engine::Json::Value* item = event.payload.Find("space");
        if (item == nullptr) return;
        SpaceModel space;
        space.id = item->GetString("id");
        space.projectId = item->GetString("project_id");
        space.name = item->GetString("name");
        space.cameraX = item->GetNumber("camera_x");
        space.cameraY = item->GetNumber("camera_y");
        space.zoom = item->GetNumber("zoom", 1.0);
        space.viewType = SpaceViewTypeFromString(item->GetString("view_type", "Graph"));
        space.sortOrder = static_cast<int>(item->GetNumber("sort_order", 0));
        auto& spaces = m_Projects.Workspace().spaces;
        const auto found = std::find_if(spaces.begin(), spaces.end(),
            [&](const SpaceModel& value) { return value.id == space.id; });
        if (found == spaces.end()) spaces.push_back(std::move(space));
        else *found = std::move(space);
        return;
    }
    case CrystalRealtimeEventType::SpaceUpdated: {
        const Engine::Json::Value* item = event.payload.Find("space");
        if (item == nullptr) return;
        const std::string id = item->GetString("id");
        auto& spaces = m_Projects.Workspace().spaces;
        const auto found = std::find_if(spaces.begin(), spaces.end(),
            [&](const SpaceModel& value) { return value.id == id; });
        SpaceModel space;
        space.id = id;
        space.projectId = item->GetString("project_id");
        space.name = item->GetString("name");
        space.cameraX = item->GetNumber("camera_x");
        space.cameraY = item->GetNumber("camera_y");
        space.zoom = item->GetNumber("zoom", 1.0);
        space.viewType = SpaceViewTypeFromString(item->GetString("view_type", "Graph"));
        space.sortOrder = static_cast<int>(item->GetNumber("sort_order", 0));
        if (found == spaces.end()) spaces.push_back(std::move(space));
        else *found = std::move(space);
        return;
    }
    case CrystalRealtimeEventType::SpaceDeleted: {
        const std::string id = event.payload.GetString("space_id");
        auto& spaces = m_Projects.Workspace().spaces;
        spaces.erase(std::remove_if(spaces.begin(), spaces.end(),
            [&](const SpaceModel& value) { return value.id == id; }), spaces.end());
        return;
    }
    case CrystalRealtimeEventType::SchemaCreated:
    case CrystalRealtimeEventType::SchemaUpdated: {
        const Engine::Json::Value* item = event.payload.Find("schema");
        if (item == nullptr) {
            const std::string schemaId = event.payload.GetString("schema_id");
            auto schemaFound = std::find_if(m_Projects.Workspace().schemas.begin(),
                m_Projects.Workspace().schemas.end(),
                [&](const CrystalEntitySchema& value) { return value.id == schemaId; });
            if (schemaFound == m_Projects.Workspace().schemas.end()) return;
            if (const Engine::Json::Value* property = event.payload.Find("property")) {
                const CrystalPropertyDefinition definition = PropertyDefinitionFromJson(*property);
                const auto propertyFound = std::find_if(schemaFound->properties.begin(),
                    schemaFound->properties.end(),
                    [&](const CrystalPropertyDefinition& value) { return value.id == definition.id; });
                if (propertyFound == schemaFound->properties.end()) schemaFound->properties.push_back(definition);
                else *propertyFound = definition;
            } else {
                const std::string propertyId = event.payload.GetString("property_id");
                schemaFound->properties.erase(std::remove_if(schemaFound->properties.begin(),
                    schemaFound->properties.end(),
                    [&](const CrystalPropertyDefinition& value) { return value.id == propertyId; }),
                    schemaFound->properties.end());
            }
            return;
        }
        CrystalEntitySchema schema = EntitySchemaFromJson(*item);
        auto& schemas = m_Projects.Workspace().schemas;
        const auto found = std::find_if(schemas.begin(), schemas.end(),
            [&](const CrystalEntitySchema& value) { return value.id == schema.id; });
        if (found == schemas.end()) schemas.push_back(std::move(schema));
        else *found = std::move(schema);
        return;
    }
    case CrystalRealtimeEventType::SchemaDeleted: {
        const std::string id = event.payload.GetString("schema_id");
        auto& schemas = m_Projects.Workspace().schemas;
        schemas.erase(std::remove_if(schemas.begin(), schemas.end(),
            [&](const CrystalEntitySchema& value) { return value.id == id; }), schemas.end());
        return;
    }
    case CrystalRealtimeEventType::BoardCreated:
    case CrystalRealtimeEventType::BoardUpdated: {
        const Engine::Json::Value* item = event.payload.Find("board");
        if (item == nullptr) return;
        const BoardModel board = BoardFromJson(*item);
        auto& boards = m_Projects.Workspace().boards;
        const auto found = std::find_if(boards.begin(), boards.end(),
            [&](const BoardModel& value) { return value.id == board.id; });
        Engine::Vec2F delta{};
        if (found != boards.end()) delta = {
            board.position.x - found->position.x, board.position.y - found->position.y};
        if (found == boards.end()) boards.push_back(board);
        else *found = board;
        if (delta.x != 0 || delta.y != 0) {
            for (auto& view : m_Projects.Workspace().views) {
                if (view.boardId == board.id) {
                    view.position.x += delta.x;
                    view.position.y += delta.y;
                }
            }
        }
        return;
    }
    case CrystalRealtimeEventType::BoardDeleted: {
        const std::string id = event.payload.GetString("board_id");
        auto& boards = m_Projects.Workspace().boards;
        boards.erase(std::remove_if(boards.begin(), boards.end(),
            [&](const BoardModel& value) { return value.id == id; }), boards.end());
        for (auto& view : m_Projects.Workspace().views)
            if (view.boardId == id) view.boardId.clear();
        for (auto& stroke : m_Projects.Workspace().strokes)
            if (stroke.anchorBoardId == id) stroke.anchorBoardId.clear();
        m_Ink.Load(m_Projects.Workspace().strokes);
        return;
    }
    case CrystalRealtimeEventType::LayerCreated:
    case CrystalRealtimeEventType::LayerUpdated: {
        const Engine::Json::Value* item = event.payload.Find("layer");
        if (item == nullptr) return;
        const UserLayerModel layer = UserLayerFromJson(*item);
        auto& layers = m_Projects.Workspace().userLayers;
        const auto found = std::find_if(layers.begin(), layers.end(),
            [&](const UserLayerModel& value) { return value.id == layer.id; });
        const std::string oldName = found == layers.end() ? std::string{} : found->name;
        if (found == layers.end()) layers.push_back(layer);
        else *found = layer;
        // The server migrates visual layer references when a layer is renamed.
        // Apply the same migration locally so a realtime rename cannot leave
        // the current client with invisible/stale visuals until the next REST
        // snapshot.
        if (!oldName.empty() && oldName != layer.name) {
            for (EntityView& view : m_Projects.Workspace().views)
                if (view.layerId == oldName) view.layerId = layer.name;
            for (Engine::UI2D::InkStroke& stroke : m_Projects.Workspace().strokes)
                if (stroke.layer == oldName) stroke.layer = layer.name;
            for (BoardModel& board : m_Projects.Workspace().boards)
                if (board.layerId == oldName) board.layerId = layer.name;
            for (DashboardWidgetModel& widget : m_Projects.Workspace().dashboardWidgets)
                if (widget.layerId == oldName) widget.layerId = layer.name;
            m_Ink.Load(m_Projects.Workspace().strokes);
        }
        return;
    }
    case CrystalRealtimeEventType::LayerDeleted: {
        const std::string id = event.payload.GetString("layer_id");
        auto& layers = m_Projects.Workspace().userLayers;
        layers.erase(std::remove_if(layers.begin(), layers.end(),
            [&](const UserLayerModel& value) { return value.id == id; }), layers.end());
        return;
    }
    case CrystalRealtimeEventType::QueryViewCreated:
    case CrystalRealtimeEventType::QueryViewUpdated: {
        const Engine::Json::Value* item = event.payload.Find("query_view");
        if (item == nullptr) return;
        const QueryViewModel query = QueryViewFromJson(*item);
        auto& queries = m_Projects.Workspace().queryViews;
        const auto found = std::find_if(queries.begin(), queries.end(),
            [&](const QueryViewModel& value) { return value.id == query.id; });
        if (found == queries.end()) queries.push_back(query);
        else *found = query;
        RefreshQueryResults();
        return;
    }
    case CrystalRealtimeEventType::QueryViewDeleted: {
        const std::string id = event.payload.GetString("query_view_id");
        auto& queries = m_Projects.Workspace().queryViews;
        queries.erase(std::remove_if(queries.begin(), queries.end(),
            [&](const QueryViewModel& value) { return value.id == id; }), queries.end());
        return;
    }
    case CrystalRealtimeEventType::DashboardWidgetCreated:
    case CrystalRealtimeEventType::DashboardWidgetUpdated: {
        const Engine::Json::Value* item = event.payload.Find("widget");
        if (item == nullptr) return;
        const DashboardWidgetModel widget = DashboardWidgetFromJson(*item);
        auto& widgets = m_Projects.Workspace().dashboardWidgets;
        const auto found = std::find_if(widgets.begin(), widgets.end(),
            [&](const DashboardWidgetModel& value) { return value.id == widget.id; });
        if (found == widgets.end()) widgets.push_back(widget);
        else *found = widget;
        return;
    }
    case CrystalRealtimeEventType::DashboardWidgetDeleted: {
        const std::string id = event.payload.GetString("widget_id");
        auto& widgets = m_Projects.Workspace().dashboardWidgets;
        widgets.erase(std::remove_if(widgets.begin(), widgets.end(),
            [&](const DashboardWidgetModel& value) { return value.id == id; }), widgets.end());
        return;
    }
    case CrystalRealtimeEventType::ActivityCreated: {
        const Engine::Json::Value* item = event.payload.Find("activity");
        if (item == nullptr) return;
        const ActivityModel activity = ActivityFromJson(*item);
        auto& activities = m_Projects.Workspace().activities;
        if (std::none_of(activities.begin(), activities.end(),
            [&](const ActivityModel& value) { return value.id == activity.id; }))
            activities.insert(activities.begin(), activity);
        if (activities.size() > 100) activities.resize(100);
        return;
    }
    case CrystalRealtimeEventType::InkCreated:
    case CrystalRealtimeEventType::InkUpdated:
        if (const Engine::Json::Value* item = event.payload.Find("stroke")) {
            const Engine::UI2D::InkStroke stroke = StrokeFromJson(*item);
            auto& strokes = m_Projects.Workspace().strokes;
            const auto found = std::find_if(strokes.begin(), strokes.end(),
                [&](const Engine::UI2D::InkStroke& value) { return value.id == stroke.id; });
            if (found == strokes.end()) strokes.push_back(stroke);
            else *found = stroke;
            m_Ink.Load(strokes);
        }
        return;
    case CrystalRealtimeEventType::InkDeleted: {
        const std::string id = event.payload.GetString("stroke_id");
        auto& strokes = m_Projects.Workspace().strokes;
        strokes.erase(std::remove_if(strokes.begin(), strokes.end(),
            [&](const Engine::UI2D::InkStroke& stroke) { return stroke.id == id; }), strokes.end());
        m_Ink.Load(strokes);
        return;
    }
    case CrystalRealtimeEventType::SceneLockAcquired:
    case CrystalRealtimeEventType::SceneLockHeartbeat:
    case CrystalRealtimeEventType::SceneLockReleased:
    case CrystalRealtimeEventType::SceneLockStale: {
        const Engine::Json::Value* item = event.payload.Find("lock");
        if (item == nullptr) return;
        const SceneLockModel lock = SceneLockFromJson(*item);
        auto& locks = m_Projects.Workspace().sceneLocks;
        const auto found = std::find_if(locks.begin(), locks.end(),
            [&](const SceneLockModel& value) { return value.sceneId == lock.sceneId; });
        if (found == locks.end()) locks.push_back(lock);
        else *found = lock;
        RefreshWorkerLockSummaries();
        return;
    }
    case CrystalRealtimeEventType::Unknown:
        return;
    }
}

void CrystalWorkspace::ApplyAuthoritativeSnapshot(const Engine::Json::Value& workspace) {
    ProjectWorkspaceModel snapshot = WorkspaceFromJson(workspace);
    if (snapshot.project.id.empty()) return;
    const ProjectWorkspaceModel previous = m_Projects.Workspace();
    const bool sameProject = previous.project.id == snapshot.project.id;
    m_Projects.AdoptWorkspace(std::move(snapshot));
    if (sameProject) {
        // A snapshot and a queued incremental event can cross at the network
        // boundary. Preserve a locally observed higher revision rather than
        // allowing an older snapshot to roll the model backwards.
        for (const CrystalEntity& entity : previous.entities) ReplaceEntity(entity);
        for (const EntityView& view : previous.views) ReplaceView(view);
    }
    m_Ink.Load(m_Projects.Workspace().strokes);
    RefreshWorkerLockSummaries();
    m_Selection.Clear();
}

void CrystalWorkspace::ApplyConnectionState(Engine::Network::WebSocketState state) {
    switch (state) {
    case Engine::Network::WebSocketState::Connected: {
        const bool reconnecting = m_ConnectionStatus == "Reconnecting";
        m_ConnectionStatus = reconnecting ? "Reconnecting" : "Connecting";
        if (m_ProjectId.empty()) {
            m_ConnectionStatus = "Connected";
            return;
        }
        ApiResult result = m_Projects.ReloadCurrentWorkspace();
        if (result.ok) {
            m_Ink.Load(m_Projects.Workspace().strokes);
            RefreshWorkerLockSummaries();
            m_Selection.Clear();
            m_ConnectionStatus = "Connected";
            m_Notification.clear();
        } else {
            m_ConnectionStatus = "Disconnected";
            m_Notification = "RELOAD FAILED";
        }
        return;
    }
    case Engine::Network::WebSocketState::Connecting:
        m_ConnectionStatus = "Connecting";
        return;
    case Engine::Network::WebSocketState::Reconnecting:
        m_ConnectionStatus = "Reconnecting";
        m_Notification = "RECONNECTING";
        return;
    case Engine::Network::WebSocketState::Stopping:
    case Engine::Network::WebSocketState::Disconnected:
        m_ConnectionStatus = "Disconnected";
        m_Notification = "SERVER DISCONNECTED";
        return;
    }
}

void CrystalWorkspace::MaybeHeartbeat() {
    const auto now = std::chrono::steady_clock::now();
    if (now < m_NextHeartbeat) return;
    m_NextHeartbeat = now + std::chrono::seconds(20);
    if (m_ConnectionStatus != "Connected" || m_HeldLocks.empty()) return;
    const std::vector<std::pair<std::string, std::string>> held(
        m_HeldLocks.begin(), m_HeldLocks.end());
    for (const auto& [sceneId, workerId] : held) (void)Heartbeat(sceneId, workerId);
}

void CrystalWorkspace::RefreshWorkerLockSummaries() {
    auto& entities = m_Projects.Workspace().entities;
    for (auto& entity : entities) {
        if (auto* details = std::get_if<WorkerDetails>(&entity.details))
            details->currentLockIds.clear();
    }
    for (const auto& lock : m_Projects.Workspace().sceneLocks) {
        if (lock.status == "FREE") continue;
        for (auto& entity : entities) {
            if (entity.type != EntityType::Worker) continue;
            auto* details = std::get_if<WorkerDetails>(&entity.details);
            if (details != nullptr && (entity.id == lock.workerId || details->workerId == lock.workerId)) {
                details->currentLockIds.push_back(lock.sceneId);
                break;
            }
        }
    }
}

void CrystalWorkspace::ReplaceEntity(const CrystalEntity& entity) {
    auto& entities = m_Projects.Workspace().entities;
    const auto found = std::find_if(entities.begin(), entities.end(),
        [&](const CrystalEntity& item) { return item.id == entity.id; });
    if (found == entities.end()) entities.push_back(entity);
    else if (entity.revision >= found->revision) *found = entity;
    RefreshQueryResults();
}

void CrystalWorkspace::RefreshQueryResults() {
    const auto& entities = m_Projects.Workspace().entities;
    for (auto& query : m_Projects.Workspace().queryViews) {
        query.results.clear();
        for (const auto& entity : entities) {
            if (!QueryMatches(entity, query)) continue;
            query.results.push_back(entity);
            if (static_cast<int>(query.results.size()) >= query.resultLimit) break;
        }
    }
}

void CrystalWorkspace::ReplaceView(const EntityView& view) {
    auto& views = m_Projects.Workspace().views;
    const auto found = std::find_if(views.begin(), views.end(),
        [&](const EntityView& item) { return item.id == view.id; });
    if (found == views.end()) views.push_back(view);
    else if (view.revision >= found->revision) *found = view;
}

} // namespace Crystal
