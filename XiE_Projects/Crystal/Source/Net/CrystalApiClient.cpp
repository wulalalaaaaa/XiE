#include "Net/CrystalApiClient.h"

#include <cctype>
#include <iomanip>
#include <sstream>
#include <string_view>
#include <utility>

namespace Crystal {
namespace {

SceneLockModel LockFromResponse(const Engine::Json::Value& value) {
    const auto workspace = WorkspaceFromJson(Engine::Json::Value::Object{
        {"scene_locks", Engine::Json::Value::Array{value}}});
    return workspace.sceneLocks.empty() ? SceneLockModel{} : workspace.sceneLocks.front();
}

SpaceModel SpaceFromResponse(const Engine::Json::Value& value) {
    SpaceModel result;
    result.id = value.GetString("id");
    result.projectId = value.GetString("project_id");
    result.name = value.GetString("name");
    result.cameraX = value.GetNumber("camera_x");
    result.cameraY = value.GetNumber("camera_y");
    result.zoom = value.GetNumber("zoom", 1.0);
    result.viewType = SpaceViewTypeFromString(value.GetString("view_type", "Graph"));
    result.sortOrder = static_cast<int>(value.GetNumber("sort_order", 0));
    return result;
}

std::string UrlEncode(std::string_view value) {
    std::ostringstream encoded;
    encoded << std::uppercase << std::hex;
    for (const unsigned char character : value) {
        if (std::isalnum(character) || character == '-' || character == '_' ||
            character == '.' || character == '~') {
            encoded << static_cast<char>(character);
        } else {
            encoded << '%' << std::setw(2) << std::setfill('0')
                    << static_cast<int>(character);
        }
    }
    return encoded.str();
}

} // namespace

ApiResult CrystalApiClient::Decode(const Engine::Network::HttpResponse& response) const {
    ApiResult result;
    result.statusCode = response.statusCode;
    if (!response.error.empty()) {
        result.error = response.error;
        return result;
    }
    std::string parseError;
    const auto parsed = Engine::Json::Value::Parse(response.body, &parseError);
    if (parsed) result.data = *parsed;
    if (!response.IsSuccess()) {
        result.error = parsed && parsed->Find("detail")
            ? parsed->Find("detail")->Dump()
            : "HTTP status " + std::to_string(response.statusCode);
        return result;
    }
    result.ok = true;
    if (!parsed && !response.body.empty()) result.error = parseError;
    return result;
}

ApiResult CrystalApiClient::Health() const { return Decode(m_Http.Get("/api/v1/health")); }

ApiResult CrystalApiClient::ListProjects(std::vector<ProjectModel>& output) const {
    ApiResult result = Decode(m_Http.Get("/api/v1/projects"));
    if (!result.ok) return result;
    output.clear();
    if (const Engine::Json::Value* projects = result.data.Find("projects")) {
        for (const Engine::Json::Value& value : projects->AsArray()) {
            ProjectModel project;
            project.id = value.GetString("id");
            project.name = value.GetString("name");
            output.push_back(std::move(project));
        }
    }
    return result;
}

ApiResult CrystalApiClient::LoadWorkspace(
    const std::string& projectId, const std::string& spaceId, ProjectWorkspaceModel& output) const {
    const std::string path = "/api/v1/projects/" + projectId + "/workspace?space_id=" + spaceId;
    ApiResult result = Decode(m_Http.Get(path));
    if (result.ok) output = WorkspaceFromJson(result.data);
    return result;
}

ApiResult CrystalApiClient::RenameSpace(
    const SpaceModel& space, std::string name, SpaceModel* updated) const {
    Engine::Json::Value::Object body;
    body["name"] = std::move(name);
    ApiResult result = Decode(m_Http.Patch(
        "/api/v1/projects/" + space.projectId + "/spaces/" + space.id, body));
    if (result.ok && updated != nullptr) *updated = SpaceFromResponse(result.data);
    return result;
}

ApiResult CrystalApiClient::DuplicateSpace(
    const std::string& projectId, const std::string& spaceId,
    const std::string& name, SpaceModel* duplicated) const {
    Engine::Json::Value::Object body;
    body["name"] = name;
    ApiResult result = Decode(m_Http.Post(
        "/api/v1/projects/" + projectId + "/spaces/" + spaceId + "/duplicate", body));
    if (result.ok && duplicated != nullptr) *duplicated = SpaceFromResponse(result.data);
    return result;
}

ApiResult CrystalApiClient::DeleteSpace(const SpaceModel& space) const {
    return Decode(m_Http.Delete(
        "/api/v1/projects/" + space.projectId + "/spaces/" + space.id));
}

ApiResult CrystalApiClient::ListSchemas(
    const std::string& projectId, std::vector<CrystalEntitySchema>& output) const {
    ApiResult result = Decode(m_Http.Get("/api/v1/projects/" + projectId + "/schemas"));
    if (!result.ok) return result;
    output.clear();
    if (const Engine::Json::Value* schemas = result.data.Find("schemas"))
        for (const auto& item : schemas->AsArray()) output.push_back(EntitySchemaFromJson(item));
    return result;
}

ApiResult CrystalApiClient::CreateSchema(
    const std::string& projectId, const CrystalEntitySchema& schema,
    CrystalEntitySchema* created) const {
    ApiResult result = Decode(m_Http.Post(
        "/api/v1/projects/" + projectId + "/schemas", ToJson(schema)));
    if (result.ok && created != nullptr) *created = EntitySchemaFromJson(result.data);
    return result;
}

ApiResult CrystalApiClient::UpdateSchema(
    const CrystalEntitySchema& schema, CrystalEntitySchema* updated) const {
    ApiResult result = Decode(m_Http.Patch("/api/v1/schemas/" + schema.id, ToJson(schema)));
    if (result.ok && updated != nullptr) *updated = EntitySchemaFromJson(result.data);
    return result;
}

ApiResult CrystalApiClient::DeleteSchema(const std::string& schemaId) const {
    return Decode(m_Http.Delete("/api/v1/schemas/" + schemaId));
}

ApiResult CrystalApiClient::CreateSchemaProperty(
    const CrystalPropertyDefinition& property, CrystalPropertyDefinition* created) const {
    Engine::Json::Value::Object body;
    body["name"] = property.name;
    body["display_name"] = property.displayName.empty() ? property.name : property.displayName;
    body["property_type"] = PropertyTypeName(property.type);
    body["default_value"] = property.defaultValue;
    body["required"] = property.required;
    Engine::Json::Value::Array options;
    for (const std::string& option : property.options) options.emplace_back(option);
    body["options"] = std::move(options);
    if (property.hasMinValue) body["min_value"] = property.minValue;
    if (property.hasMaxValue) body["max_value"] = property.maxValue;
    if (property.hasStepValue) body["step_value"] = property.stepValue;
    body["sort_order"] = property.sortOrder;
    ApiResult result = Decode(m_Http.Post(
        "/api/v1/schemas/" + property.schemaId + "/properties", body));
    if (result.ok && created != nullptr) *created = PropertyDefinitionFromJson(result.data);
    return result;
}

ApiResult CrystalApiClient::UpdateSchemaProperty(
    const CrystalPropertyDefinition& property, CrystalPropertyDefinition* updated) const {
    Engine::Json::Value::Object body;
    body["display_name"] = property.displayName.empty() ? property.name : property.displayName;
    body["property_type"] = PropertyTypeName(property.type);
    body["default_value"] = property.defaultValue;
    body["required"] = property.required;
    Engine::Json::Value::Array options;
    for (const std::string& option : property.options) options.emplace_back(option);
    body["options"] = std::move(options);
    if (property.hasMinValue) body["min_value"] = property.minValue;
    if (property.hasMaxValue) body["max_value"] = property.maxValue;
    if (property.hasStepValue) body["step_value"] = property.stepValue;
    body["sort_order"] = property.sortOrder;
    ApiResult result = Decode(m_Http.Patch(
        "/api/v1/property-definitions/" + property.id, body));
    if (result.ok && updated != nullptr) *updated = PropertyDefinitionFromJson(result.data);
    return result;
}

ApiResult CrystalApiClient::DeleteSchemaProperty(const std::string& propertyId) const {
    return Decode(m_Http.Delete("/api/v1/property-definitions/" + propertyId));
}

ApiResult CrystalApiClient::SaveCamera(const SpaceModel& space) const {
    Engine::Json::Value::Object body;
    body["camera_x"] = space.cameraX;
    body["camera_y"] = space.cameraY;
    body["zoom"] = space.zoom;
    return Decode(m_Http.Patch(
        "/api/v1/projects/" + space.projectId + "/spaces/" + space.id + "/camera", body));
}

ApiResult CrystalApiClient::CreateEntity(
    const CrystalEntity& entity, CrystalEntity& created) const {
    ApiResult result = Decode(m_Http.Post("/api/v1/entities", ToJson(entity)));
    if (result.ok) created = EntityFromJson(result.data);
    return result;
}

ApiResult CrystalApiClient::UpdateEntity(
    const CrystalEntity& entity, int expectedRevision, CrystalEntity* updated) const {
    Engine::Json::Value body = ToJson(entity);
    if (expectedRevision >= 0) body["expected_revision"] = expectedRevision;
    ApiResult result = Decode(m_Http.Patch("/api/v1/entities/" + entity.id, body));
    if (result.ok && updated != nullptr) *updated = EntityFromJson(result.data);
    return result;
}

ApiResult CrystalApiClient::UpdateEntityTitle(
    const std::string& entityId, const std::string& title, int expectedRevision,
    CrystalEntity* updated) const {
    Engine::Json::Value::Object body;
    body["title"] = title;
    if (expectedRevision >= 0) body["expected_revision"] = expectedRevision;
    ApiResult result = Decode(m_Http.Patch("/api/v1/entities/" + entityId, body));
    if (result.ok && updated != nullptr) *updated = EntityFromJson(result.data);
    return result;
}

ApiResult CrystalApiClient::DeleteEntity(const std::string& entityId) const {
    return Decode(m_Http.Delete("/api/v1/entities/" + entityId));
}

ApiResult CrystalApiClient::SaveView(
    const EntityView& view, int expectedRevision, EntityView* saved) const {
    Engine::Json::Value body = ToJson(view);
    if (expectedRevision >= 0) body["expected_revision"] = expectedRevision;
    ApiResult result = Decode(m_Http.Put(
        "/api/v1/entity-views/" + view.entityId + "/" + view.spaceId, body));
    if (result.ok && saved != nullptr) *saved = EntityViewFromJson(result.data);
    return result;
}

ApiResult CrystalApiClient::DeleteView(
    const std::string& entityId, const std::string& spaceId) const {
    return Decode(m_Http.Delete("/api/v1/entity-views/" + entityId + "/" + spaceId));
}

ApiResult CrystalApiClient::CreateBoard(const BoardModel& board, BoardModel* created) const {
    ApiResult result = Decode(m_Http.Post(
        "/api/v1/projects/" + board.projectId + "/boards", ToJson(board)));
    if (result.ok && created != nullptr) *created = BoardFromJson(result.data);
    return result;
}

ApiResult CrystalApiClient::UpdateBoard(const BoardModel& board, BoardModel* updated) const {
    ApiResult result = Decode(m_Http.Patch("/api/v1/boards/" + board.id, ToJson(board)));
    if (result.ok && updated != nullptr) *updated = BoardFromJson(result.data);
    return result;
}

ApiResult CrystalApiClient::DeleteBoard(const std::string& boardId) const {
    return Decode(m_Http.Delete("/api/v1/boards/" + boardId));
}

ApiResult CrystalApiClient::AttachViewToBoard(
    const std::string& boardId, const std::string& viewId,
    Engine::Vec2F localPosition, EntityView* updated) const {
    Engine::Json::Value::Object body;
    body["local_x"] = localPosition.x;
    body["local_y"] = localPosition.y;
    ApiResult result = Decode(m_Http.Put(
        "/api/v1/boards/" + boardId + "/views/" + viewId, body));
    if (result.ok && updated != nullptr) *updated = EntityViewFromJson(result.data);
    return result;
}

ApiResult CrystalApiClient::DetachViewFromBoard(
    const std::string& boardId, const std::string& viewId) const {
    return Decode(m_Http.Delete("/api/v1/boards/" + boardId + "/views/" + viewId));
}

ApiResult CrystalApiClient::CreateLayer(const UserLayerModel& layer, UserLayerModel* created) const {
    ApiResult result = Decode(m_Http.Post(
        "/api/v1/projects/" + layer.projectId + "/layers", ToJson(layer)));
    if (result.ok && created != nullptr) *created = UserLayerFromJson(result.data);
    return result;
}

ApiResult CrystalApiClient::UpdateLayer(const UserLayerModel& layer, UserLayerModel* updated) const {
    ApiResult result = Decode(m_Http.Patch("/api/v1/layers/" + layer.id, ToJson(layer)));
    if (result.ok && updated != nullptr) *updated = UserLayerFromJson(result.data);
    return result;
}

ApiResult CrystalApiClient::DeleteLayer(const std::string& layerId, bool deleteVisuals) const {
    Engine::Json::Value::Object body;
    body["mode"] = deleteVisuals ? "delete_visuals" : "move_to_main";
    return Decode(m_Http.Delete("/api/v1/layers/" + layerId, body));
}

ApiResult CrystalApiClient::CreateQueryView(const QueryViewModel& query, QueryViewModel* created) const {
    ApiResult result = Decode(m_Http.Post(
        "/api/v1/projects/" + query.projectId + "/query-views", ToJson(query)));
    if (result.ok && created != nullptr) *created = QueryViewFromJson(result.data);
    return result;
}

ApiResult CrystalApiClient::UpdateQueryView(const QueryViewModel& query, QueryViewModel* updated) const {
    ApiResult result = Decode(m_Http.Patch("/api/v1/query-views/" + query.id, ToJson(query)));
    if (result.ok && updated != nullptr) *updated = QueryViewFromJson(result.data);
    return result;
}

ApiResult CrystalApiClient::DeleteQueryView(const std::string& queryId) const {
    return Decode(m_Http.Delete("/api/v1/query-views/" + queryId));
}

ApiResult CrystalApiClient::CreateDashboardWidget(
    const DashboardWidgetModel& widget, DashboardWidgetModel* created) const {
    ApiResult result = Decode(m_Http.Post(
        "/api/v1/projects/" + widget.projectId + "/dashboard-widgets", ToJson(widget)));
    if (result.ok && created != nullptr) *created = DashboardWidgetFromJson(result.data);
    return result;
}

ApiResult CrystalApiClient::UpdateDashboardWidget(
    const DashboardWidgetModel& widget, DashboardWidgetModel* updated) const {
    ApiResult result = Decode(m_Http.Patch("/api/v1/dashboard-widgets/" + widget.id, ToJson(widget)));
    if (result.ok && updated != nullptr) *updated = DashboardWidgetFromJson(result.data);
    return result;
}

ApiResult CrystalApiClient::DeleteDashboardWidget(const std::string& widgetId) const {
    return Decode(m_Http.Delete("/api/v1/dashboard-widgets/" + widgetId));
}

ApiResult CrystalApiClient::ListRelationships(
    const std::string& projectId, std::vector<CrystalRelationship>& output) const {
    ApiResult result = Decode(m_Http.Get("/api/v1/projects/" + projectId + "/relationships"));
    if (!result.ok) return result;
    output.clear();
    if (const Engine::Json::Value* items = result.data.Find("relationships"))
        for (const Engine::Json::Value& item : items->AsArray()) output.push_back(RelationshipFromJson(item));
    return result;
}

ApiResult CrystalApiClient::CreateRelationship(
    const CrystalRelationship& relationship, CrystalRelationship& created) const {
    ApiResult result = Decode(m_Http.Post("/api/v1/relationships", ToJson(relationship)));
    if (result.ok) created = RelationshipFromJson(result.data);
    return result;
}

ApiResult CrystalApiClient::UpdateRelationshipVisibility(
    const std::string& relationshipId, bool visible, CrystalRelationship* updated) const {
    Engine::Json::Value::Object body;
    body["visible"] = visible;
    ApiResult result = Decode(m_Http.Patch("/api/v1/relationships/" + relationshipId, body));
    if (result.ok && updated != nullptr) *updated = RelationshipFromJson(result.data);
    return result;
}

ApiResult CrystalApiClient::DeleteRelationship(const std::string& relationshipId) const {
    return Decode(m_Http.Delete("/api/v1/relationships/" + relationshipId));
}

ApiResult CrystalApiClient::Search(
    const std::string& projectId, const std::string& query,
    std::vector<CrystalEntity>& output) const {
    ApiResult result = Decode(m_Http.Get(
        "/api/v1/projects/" + projectId + "/search?q=" + UrlEncode(query)));
    if (!result.ok) return result;
    output.clear();
    if (const Engine::Json::Value* items = result.data.Find("entities"))
        for (const Engine::Json::Value& item : items->AsArray()) output.push_back(EntityFromJson(item));
    return result;
}

ApiResult CrystalApiClient::CreateStroke(
    const std::string& projectId, const std::string& spaceId,
    const Engine::UI2D::InkStroke& stroke, Engine::UI2D::InkStroke* saved) const {
    Engine::Json::Value body = ToJson(stroke);
    body["project_id"] = projectId;
    body["space_id"] = spaceId;
    ApiResult result = Decode(m_Http.Post("/api/v1/ink-strokes", body));
    if (result.ok && saved != nullptr) *saved = StrokeFromJson(result.data);
    return result;
}

ApiResult CrystalApiClient::DeleteStroke(const std::string& strokeId) const {
    return Decode(m_Http.Delete("/api/v1/ink-strokes/" + strokeId));
}

ApiResult CrystalApiClient::UpdateStroke(
    const Engine::UI2D::InkStroke& stroke, Engine::UI2D::InkStroke* updated) const {
    Engine::Json::Value body = ToJson(stroke);
    body["anchor_entity_id"] = stroke.anchorEntityId.empty()
        ? Engine::Json::Value(nullptr) : Engine::Json::Value(stroke.anchorEntityId);
    body["anchor_board_id"] = stroke.anchorBoardId.empty()
        ? Engine::Json::Value(nullptr) : Engine::Json::Value(stroke.anchorBoardId);
    ApiResult result = Decode(m_Http.Patch("/api/v1/ink-strokes/" + stroke.id, body));
    if (result.ok && updated != nullptr) *updated = StrokeFromJson(result.data);
    return result;
}

ApiResult CrystalApiClient::UpdateStrokeAnchor(
    const Engine::UI2D::InkStroke& stroke) const {
    Engine::Json::Value::Object body;
    body["anchor_entity_id"] = stroke.anchorEntityId.empty()
        ? Engine::Json::Value(nullptr) : Engine::Json::Value(stroke.anchorEntityId);
    body["anchor_board_id"] = stroke.anchorBoardId.empty()
        ? Engine::Json::Value(nullptr) : Engine::Json::Value(stroke.anchorBoardId);
    body["local_offset_x"] = stroke.localOffset.x;
    body["local_offset_y"] = stroke.localOffset.y;
    return Decode(m_Http.Patch("/api/v1/ink-strokes/" + stroke.id, body));
}

ApiResult CrystalApiClient::GroupInkStrokes(
    const std::string& projectId, const std::string& spaceId,
    const std::vector<std::string>& strokeIds, const std::string& name) const {
    Engine::Json::Value::Array ids;
    for (const std::string& strokeId : strokeIds) ids.emplace_back(strokeId);
    Engine::Json::Value::Object body;
    body["project_id"] = projectId;
    body["space_id"] = spaceId;
    body["stroke_ids"] = std::move(ids);
    body["name"] = name;
    return Decode(m_Http.Post(
        "/api/v1/projects/" + projectId + "/ink-groups", body));
}

ApiResult CrystalApiClient::AcquireScene(
    const std::string& sceneId, const std::string& workerId,
    const std::string& workerName, const std::string& task,
    const std::string& branch, SceneLockModel& output,
    const std::string& taskId, const std::string& taskNote) const {
    Engine::Json::Value::Object body;
    body["worker_id"] = workerId;
    body["worker_name"] = workerName;
    body["task"] = task;
    body["branch"] = branch;
    body["task_id"] = taskId;
    body["task_note"] = taskNote;
    ApiResult result = Decode(m_Http.Post("/api/v1/scene-locks/" + sceneId + "/acquire", body));
    if (result.ok) output = LockFromResponse(result.data);
    return result;
}

ApiResult CrystalApiClient::HeartbeatScene(
    const std::string& sceneId, const std::string& workerId, SceneLockModel& output) const {
    Engine::Json::Value::Object body;
    body["worker_id"] = workerId;
    ApiResult result = Decode(m_Http.Post("/api/v1/scene-locks/" + sceneId + "/heartbeat", body));
    if (result.ok) output = LockFromResponse(result.data);
    return result;
}

ApiResult CrystalApiClient::ReleaseScene(
    const std::string& sceneId, const std::string& workerId, SceneLockModel& output) const {
    Engine::Json::Value::Object body;
    body["worker_id"] = workerId;
    ApiResult result = Decode(m_Http.Post("/api/v1/scene-locks/" + sceneId + "/release", body));
    if (result.ok) output = LockFromResponse(result.data);
    return result;
}

} // namespace Crystal
