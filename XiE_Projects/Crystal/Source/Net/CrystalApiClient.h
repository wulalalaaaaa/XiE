#pragma once

#include "Model/CrystalModels.h"
#include "Network/HttpClient.h"

#include <string>
#include <vector>

namespace Crystal {

struct ApiResult {
    bool ok = false;
    int statusCode = 0;
    Engine::Json::Value data;
    std::string error;
};

class CrystalApiClient {
public:
    explicit CrystalApiClient(std::string baseUrl = "http://127.0.0.1:8787")
        : m_Http(std::move(baseUrl)) {}

    void SetBaseUrl(std::string baseUrl) { m_Http.SetBaseUrl(std::move(baseUrl)); }
    [[nodiscard]] const std::string& BaseUrl() const noexcept { return m_Http.BaseUrl(); }

    [[nodiscard]] ApiResult Health() const;
    [[nodiscard]] ApiResult ListProjects(std::vector<ProjectModel>& output) const;
    [[nodiscard]] ApiResult LoadWorkspace(
        const std::string& projectId, const std::string& spaceId, ProjectWorkspaceModel& output) const;
    [[nodiscard]] ApiResult SaveCamera(const SpaceModel& space) const;
    [[nodiscard]] ApiResult RenameSpace(
        const SpaceModel& space, std::string name, SpaceModel* updated = nullptr) const;
    [[nodiscard]] ApiResult DuplicateSpace(
        const std::string& projectId, const std::string& spaceId,
        const std::string& name, SpaceModel* duplicated = nullptr) const;
    [[nodiscard]] ApiResult DeleteSpace(const SpaceModel& space) const;
    [[nodiscard]] ApiResult ListSchemas(
        const std::string& projectId, std::vector<CrystalEntitySchema>& output) const;
    [[nodiscard]] ApiResult CreateSchema(
        const std::string& projectId, const CrystalEntitySchema& schema,
        CrystalEntitySchema* created = nullptr) const;
    [[nodiscard]] ApiResult UpdateSchema(
        const CrystalEntitySchema& schema, CrystalEntitySchema* updated = nullptr) const;
    [[nodiscard]] ApiResult DeleteSchema(const std::string& schemaId) const;
    [[nodiscard]] ApiResult CreateSchemaProperty(
        const CrystalPropertyDefinition& property,
        CrystalPropertyDefinition* created = nullptr) const;
    [[nodiscard]] ApiResult UpdateSchemaProperty(
        const CrystalPropertyDefinition& property,
        CrystalPropertyDefinition* updated = nullptr) const;
    [[nodiscard]] ApiResult DeleteSchemaProperty(const std::string& propertyId) const;
    [[nodiscard]] ApiResult CreateEntity(
        const CrystalEntity& entity, CrystalEntity& created) const;
    [[nodiscard]] ApiResult UpdateEntity(
        const CrystalEntity& entity, int expectedRevision, CrystalEntity* updated = nullptr) const;
    [[nodiscard]] ApiResult UpdateEntityTitle(
        const std::string& entityId, const std::string& title, int expectedRevision = -1,
        CrystalEntity* updated = nullptr) const;
    [[nodiscard]] ApiResult DeleteEntity(const std::string& entityId) const;
    [[nodiscard]] ApiResult SaveView(
        const EntityView& view, int expectedRevision = -1,
        EntityView* saved = nullptr) const;
    [[nodiscard]] ApiResult DeleteView(
        const std::string& entityId, const std::string& spaceId) const;
    [[nodiscard]] ApiResult CreateBoard(const BoardModel& board, BoardModel* created = nullptr) const;
    [[nodiscard]] ApiResult UpdateBoard(const BoardModel& board, BoardModel* updated = nullptr) const;
    [[nodiscard]] ApiResult DeleteBoard(const std::string& boardId) const;
    [[nodiscard]] ApiResult AttachViewToBoard(
        const std::string& boardId, const std::string& viewId,
        Engine::Vec2F localPosition, EntityView* updated = nullptr) const;
    [[nodiscard]] ApiResult DetachViewFromBoard(
        const std::string& boardId, const std::string& viewId) const;
    [[nodiscard]] ApiResult CreateLayer(const UserLayerModel& layer, UserLayerModel* created = nullptr) const;
    [[nodiscard]] ApiResult UpdateLayer(const UserLayerModel& layer, UserLayerModel* updated = nullptr) const;
    [[nodiscard]] ApiResult DeleteLayer(const std::string& layerId, bool deleteVisuals = false) const;
    [[nodiscard]] ApiResult CreateQueryView(const QueryViewModel& query, QueryViewModel* created = nullptr) const;
    [[nodiscard]] ApiResult UpdateQueryView(const QueryViewModel& query, QueryViewModel* updated = nullptr) const;
    [[nodiscard]] ApiResult DeleteQueryView(const std::string& queryId) const;
    [[nodiscard]] ApiResult CreateDashboardWidget(
        const DashboardWidgetModel& widget, DashboardWidgetModel* created = nullptr) const;
    [[nodiscard]] ApiResult UpdateDashboardWidget(
        const DashboardWidgetModel& widget, DashboardWidgetModel* updated = nullptr) const;
    [[nodiscard]] ApiResult DeleteDashboardWidget(const std::string& widgetId) const;
    [[nodiscard]] ApiResult ListRelationships(
        const std::string& projectId, std::vector<CrystalRelationship>& output) const;
    [[nodiscard]] ApiResult CreateRelationship(
        const CrystalRelationship& relationship, CrystalRelationship& created) const;
    [[nodiscard]] ApiResult UpdateRelationshipVisibility(
        const std::string& relationshipId, bool visible,
        CrystalRelationship* updated = nullptr) const;
    [[nodiscard]] ApiResult DeleteRelationship(const std::string& relationshipId) const;
    [[nodiscard]] ApiResult Search(
        const std::string& projectId, const std::string& query,
        std::vector<CrystalEntity>& output) const;
    [[nodiscard]] ApiResult CreateStroke(
        const std::string& projectId, const std::string& spaceId,
        const Engine::UI2D::InkStroke& stroke, Engine::UI2D::InkStroke* saved = nullptr) const;
    [[nodiscard]] ApiResult UpdateStroke(
        const Engine::UI2D::InkStroke& stroke,
        Engine::UI2D::InkStroke* updated = nullptr) const;
    [[nodiscard]] ApiResult UpdateStrokeAnchor(
        const Engine::UI2D::InkStroke& stroke) const;
    [[nodiscard]] ApiResult GroupInkStrokes(
        const std::string& projectId, const std::string& spaceId,
        const std::vector<std::string>& strokeIds, const std::string& name = {}) const;
    [[nodiscard]] ApiResult DeleteStroke(const std::string& strokeId) const;
    [[nodiscard]] ApiResult AcquireScene(
        const std::string& sceneId, const std::string& workerId,
        const std::string& workerName, const std::string& task,
        const std::string& branch, SceneLockModel& output,
        const std::string& taskId = {}, const std::string& taskNote = {}) const;
    [[nodiscard]] ApiResult HeartbeatScene(
        const std::string& sceneId, const std::string& workerId, SceneLockModel& output) const;
    [[nodiscard]] ApiResult ReleaseScene(
        const std::string& sceneId, const std::string& workerId, SceneLockModel& output) const;

private:
    [[nodiscard]] ApiResult Decode(const Engine::Network::HttpResponse& response) const;
    Engine::Network::HttpClient m_Http;
};

} // namespace Crystal
