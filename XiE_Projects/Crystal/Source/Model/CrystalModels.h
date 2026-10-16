#pragma once

#include "Foundation/Math/Types2D.h"
#include "Network/Json.h"
#include "UI2D/InkStroke.h"

#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace Crystal {

enum class EntityType { Text, Note, Task, System, Worker, Milestone, Scene, Unknown };
enum class PropertyType {
    Text, MultilineText, Number, Bool, Enum, Date, EntityReference,
    EntityReferenceList, Tags, Progress, URL, Unknown
};
enum class TaskStatus { Backlog, Ready, InProgress, Review, Testing, Done, Blocked };
enum class TaskPriority { Low, Normal, High, Critical };
enum class SystemStatus { Planned, Developing, Testing, Stable, Deprecated };
enum class WorkerType { Human, Codex, Automation };
enum class WorkerStatus { Offline, Idle, Working };
enum class MilestoneStatus { Planned, Active, Completed, Blocked };
enum class SpaceViewType { Graph, TaskBoard };
enum class ViewType { Node, CompactNode, Card, StatusCard, Badge };
enum class DisplayMode { All, Graph, Draw, Presentation };
enum class RelationshipType {
    DependsOn, Blocks, BelongsToSystem, AssignedTo, BelongsToMilestone,
    IntegratesInto, RelatedTo, Unknown
};

struct ProjectModel {
    std::string id;
    std::string name;
};

struct SpaceModel {
    std::string id;
    std::string projectId;
    std::string name;
    double cameraX = 0.0;
    double cameraY = 0.0;
    double zoom = 1.0;
    SpaceViewType viewType = SpaceViewType::Graph;
    int sortOrder = 0;
};

struct CrystalPropertyDefinition {
    std::string id;
    std::string schemaId;
    std::string name;
    std::string displayName;
    PropertyType type = PropertyType::Text;
    Engine::Json::Value defaultValue;
    bool required = false;
    std::vector<std::string> options;
    double minValue = 0.0;
    double maxValue = 0.0;
    double stepValue = 0.0;
    bool hasMinValue = false;
    bool hasMaxValue = false;
    bool hasStepValue = false;
    int sortOrder = 0;
};

struct CrystalEntitySchema {
    std::string id;
    std::string projectId;
    std::string name;
    std::string displayName;
    std::string icon;
    std::string description;
    std::vector<CrystalPropertyDefinition> properties;
    std::string createdAt;
    std::string updatedAt;
};

struct TaskDetails {
    TaskStatus status = TaskStatus::Backlog;
    TaskPriority priority = TaskPriority::Normal;
    std::string assignedWorkerId;
    std::string milestoneId;
    std::string systemId;
    std::string branch;
    std::vector<std::string> allowedPaths;
    std::vector<std::string> forbiddenPaths;
    std::string acceptanceCriteria;
    std::string startedAt;
    std::string completedAt;
};

struct SystemDetails {
    SystemStatus status = SystemStatus::Planned;
    std::string ownerWorkerId;
    double progress = 0.0;
};

struct WorkerDetails {
    std::string workerId;
    WorkerType type = WorkerType::Automation;
    WorkerStatus status = WorkerStatus::Offline;
    std::string currentTaskId;
    std::string currentTask;
    std::string currentTaskStatus;
    std::string branch;
    std::string lastActivityAt;
    std::vector<std::string> currentLockIds;
};

struct MilestoneDetails {
    MilestoneStatus status = MilestoneStatus::Planned;
    std::string targetDate;
    double progress = 0.0;
    int doneTaskCount = 0;
    int totalRelatedTaskCount = 0;
    std::string acceptanceCriteria;
};

struct SceneDetails {
    std::string assetPath;
};

using EntityDetails = std::variant<std::monostate, TaskDetails, SystemDetails,
                                   WorkerDetails, MilestoneDetails, SceneDetails>;

struct CrystalEntity {
    std::string id;
    std::string projectId;
    EntityType type = EntityType::Unknown;
    std::string typeName;
    std::string schemaId;
    std::string title;
    std::string description;
    int revision = 1;
    Engine::Json::Value data{Engine::Json::Value::Object{}};
    Engine::Json::Value properties{Engine::Json::Value::Object{}};
    EntityDetails details;
};

struct EntityView {
    std::string id;
    std::string entityId;
    std::string spaceId;
    Engine::Vec2F position{};
    Engine::Vec2F size{220.0f, 120.0f};
    ViewType viewType = ViewType::Node;
    std::string layerId = "Main";
    bool visible = true;
    std::vector<std::string> visibleProperties;
    std::string boardId;
    Engine::Vec2F boardLocalPosition{};
    int revision = 1;
};

struct BoardModel {
    std::string id;
    std::string projectId;
    std::string spaceId;
    std::string title;
    Engine::Vec2F position{};
    Engine::Vec2F size{800.0f, 500.0f};
    std::string background = "#172235";
    std::string border = "#4d7198";
    bool locked = false;
    bool collapsed = false;
    std::string layerId = "Main";
    int zOrder = 0;
    std::string createdAt;
    std::string updatedAt;
};

struct CrystalRelationship {
    std::string id;
    std::string projectId;
    std::string sourceEntityId;
    std::string targetEntityId;
    RelationshipType type = RelationshipType::Unknown;
    bool visible = true;
    std::string createdAt;
};

struct SceneLockModel {
    std::string sceneId;
    std::string projectId;
    std::string status = "FREE";
    std::string ownerId;
    std::string ownerName;
    std::string workerId;
    std::string taskId;
    std::string task;
    std::string taskNote;
    std::string branch;
    std::string acquiredAt;
    std::string heartbeatAt;
};

struct UserLayerModel {
    std::string id;
    std::string projectId;
    std::string name;
    bool visible = true;
    std::string spaceId;
    bool locked = false;
    int sortOrder = 0;
};

struct QueryFilterModel {
    std::string property;
    std::string op = "Equals";
    Engine::Json::Value value;
};

struct QueryViewModel {
    std::string id;
    std::string projectId;
    std::string spaceId;
    std::string name;
    std::string entitySchema;
    std::vector<QueryFilterModel> filters;
    Engine::Json::Value sort{Engine::Json::Value::Object{}};
    int resultLimit = 50;
    std::vector<CrystalEntity> results;
};

struct DashboardWidgetModel {
    std::string id;
    std::string projectId;
    std::string spaceId;
    std::string widgetType;
    std::string title;
    Engine::Json::Value config{Engine::Json::Value::Object{}};
    Engine::Vec2F position{};
    Engine::Vec2F size{320.0f, 180.0f};
    std::string layerId = "Main";
    bool visible = true;
    int revision = 1;
};

struct ActivityModel {
    std::string id;
    std::string projectId;
    std::string actorId;
    std::string actorName;
    std::string eventType;
    std::string entityId;
    std::string summary;
    Engine::Json::Value payload{Engine::Json::Value::Object{}};
    std::string createdAt;
};

struct ProjectWorkspaceModel {
    ProjectModel project;
    std::vector<SpaceModel> spaces;
    SpaceModel currentSpace;
    std::vector<CrystalEntity> entities;
    std::vector<EntityView> views;
    std::vector<CrystalRelationship> relationships;
    std::vector<Engine::UI2D::InkStroke> strokes;
    std::vector<SceneLockModel> sceneLocks;
    std::vector<UserLayerModel> userLayers;
    std::vector<CrystalEntitySchema> schemas;
    std::vector<BoardModel> boards;
    std::vector<QueryViewModel> queryViews;
    std::vector<DashboardWidgetModel> dashboardWidgets;
    std::vector<ActivityModel> activities;
};

[[nodiscard]] EntityType EntityTypeFromString(std::string_view value) noexcept;
[[nodiscard]] const char* EntityTypeName(EntityType value) noexcept;
[[nodiscard]] PropertyType PropertyTypeFromString(std::string_view value) noexcept;
[[nodiscard]] const char* PropertyTypeName(PropertyType value) noexcept;
[[nodiscard]] TaskStatus TaskStatusFromString(std::string_view value) noexcept;
[[nodiscard]] const char* TaskStatusName(TaskStatus value) noexcept;
[[nodiscard]] TaskPriority TaskPriorityFromString(std::string_view value) noexcept;
[[nodiscard]] const char* TaskPriorityName(TaskPriority value) noexcept;
[[nodiscard]] SystemStatus SystemStatusFromString(std::string_view value) noexcept;
[[nodiscard]] const char* SystemStatusName(SystemStatus value) noexcept;
[[nodiscard]] WorkerType WorkerTypeFromString(std::string_view value) noexcept;
[[nodiscard]] const char* WorkerTypeName(WorkerType value) noexcept;
[[nodiscard]] WorkerStatus WorkerStatusFromString(std::string_view value) noexcept;
[[nodiscard]] const char* WorkerStatusName(WorkerStatus value) noexcept;
[[nodiscard]] MilestoneStatus MilestoneStatusFromString(std::string_view value) noexcept;
[[nodiscard]] const char* MilestoneStatusName(MilestoneStatus value) noexcept;
[[nodiscard]] SpaceViewType SpaceViewTypeFromString(std::string_view value) noexcept;
[[nodiscard]] const char* SpaceViewTypeName(SpaceViewType value) noexcept;
[[nodiscard]] ViewType ViewTypeFromString(std::string_view value) noexcept;
[[nodiscard]] const char* ViewTypeName(ViewType value) noexcept;
[[nodiscard]] const char* DisplayModeName(DisplayMode value) noexcept;
[[nodiscard]] RelationshipType RelationshipTypeFromString(std::string_view value) noexcept;
[[nodiscard]] const char* RelationshipTypeName(RelationshipType value) noexcept;
[[nodiscard]] const char* StrokeToolName(Engine::UI2D::StrokeTool value) noexcept;
[[nodiscard]] Engine::UI2D::StrokeTool StrokeToolFromString(std::string_view value) noexcept;
[[nodiscard]] const char* StrokeShapeName(Engine::UI2D::StrokeShape value) noexcept;
[[nodiscard]] Engine::UI2D::StrokeShape StrokeShapeFromString(std::string_view value) noexcept;

[[nodiscard]] Engine::Json::Value ToJson(const CrystalEntity& value);
[[nodiscard]] Engine::Json::Value ToJson(const CrystalPropertyDefinition& value);
[[nodiscard]] Engine::Json::Value ToJson(const CrystalEntitySchema& value);
[[nodiscard]] Engine::Json::Value ToJson(const EntityView& value);
[[nodiscard]] Engine::Json::Value ToJson(const BoardModel& value);
[[nodiscard]] Engine::Json::Value ToJson(const UserLayerModel& value);
[[nodiscard]] Engine::Json::Value ToJson(const QueryViewModel& value);
[[nodiscard]] Engine::Json::Value ToJson(const DashboardWidgetModel& value);
[[nodiscard]] Engine::Json::Value ToJson(const ActivityModel& value);
[[nodiscard]] Engine::Json::Value ToJson(const CrystalRelationship& value);
[[nodiscard]] Engine::Json::Value ToJson(const Engine::UI2D::InkStroke& value);
[[nodiscard]] CrystalEntity EntityFromJson(const Engine::Json::Value& value);
[[nodiscard]] CrystalPropertyDefinition PropertyDefinitionFromJson(const Engine::Json::Value& value);
[[nodiscard]] CrystalEntitySchema EntitySchemaFromJson(const Engine::Json::Value& value);
[[nodiscard]] EntityView EntityViewFromJson(const Engine::Json::Value& value);
[[nodiscard]] BoardModel BoardFromJson(const Engine::Json::Value& value);
[[nodiscard]] UserLayerModel UserLayerFromJson(const Engine::Json::Value& value);
[[nodiscard]] QueryViewModel QueryViewFromJson(const Engine::Json::Value& value);
[[nodiscard]] DashboardWidgetModel DashboardWidgetFromJson(const Engine::Json::Value& value);
[[nodiscard]] ActivityModel ActivityFromJson(const Engine::Json::Value& value);
[[nodiscard]] CrystalRelationship RelationshipFromJson(const Engine::Json::Value& value);
[[nodiscard]] SceneLockModel SceneLockFromJson(const Engine::Json::Value& value);
[[nodiscard]] Engine::UI2D::InkStroke StrokeFromJson(const Engine::Json::Value& value);
[[nodiscard]] ProjectWorkspaceModel WorkspaceFromJson(const Engine::Json::Value& value);

} // namespace Crystal
