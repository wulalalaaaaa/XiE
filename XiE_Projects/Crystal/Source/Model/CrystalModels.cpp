#include "Model/CrystalModels.h"

#include <type_traits>
#include <utility>

namespace Crystal {
namespace {

Engine::Vec2F PointFromJson(const Engine::Json::Value& value) {
    return {static_cast<float>(value.GetNumber("x")),
            static_cast<float>(value.GetNumber("y"))};
}

bool BoolFromJson(const Engine::Json::Value& value, std::string_view key, bool fallback) {
    if (const Engine::Json::Value* item = value.Find(key)) {
        if (item->IsBool()) return item->AsBool();
        if (item->IsNumber()) return item->AsNumber() != 0.0;
    }
    return fallback;
}

std::vector<std::string> StringArrayFromJson(const Engine::Json::Value& value, std::string_view key) {
    std::vector<std::string> result;
    const Engine::Json::Value* array = value.Find(key);
    if (array == nullptr || !array->IsArray()) return result;
    for (const Engine::Json::Value& item : array->AsArray())
        if (item.IsString()) result.push_back(item.AsString());
    return result;
}

Engine::Json::Value StringArrayToJson(const std::vector<std::string>& values) {
    Engine::Json::Value::Array result;
    result.reserve(values.size());
    for (const std::string& value : values) result.emplace_back(value);
    return result;
}

SpaceModel SpaceFromJson(const Engine::Json::Value& value) {
    SpaceModel space;
    space.id = value.GetString("id");
    space.projectId = value.GetString("project_id");
    space.name = value.GetString("name");
    space.cameraX = value.GetNumber("camera_x");
    space.cameraY = value.GetNumber("camera_y");
    space.zoom = value.GetNumber("zoom", 1.0);
    space.viewType = SpaceViewTypeFromString(value.GetString("view_type", "Graph"));
    space.sortOrder = static_cast<int>(value.GetNumber("sort_order", 0));
    return space;
}

} // namespace

SceneLockModel SceneLockFromJson(const Engine::Json::Value& value) {
    SceneLockModel lock;
    lock.sceneId = value.GetString("scene_id");
    lock.projectId = value.GetString("project_id");
    lock.status = value.GetString("status", "FREE");
    lock.ownerId = value.GetString("owner_id");
    lock.ownerName = value.GetString("owner_name");
    lock.workerId = value.GetString("worker_id", lock.ownerId);
    lock.taskId = value.GetString("task_id");
    lock.task = value.GetString("task");
    lock.taskNote = value.GetString("task_note", lock.task);
    lock.branch = value.GetString("branch");
    lock.acquiredAt = value.GetString("acquired_at");
    lock.heartbeatAt = value.GetString("heartbeat_at");
    return lock;
}

namespace {

TaskDetails TaskDetailsFromJson(const Engine::Json::Value& value) {
    TaskDetails result;
    result.status = TaskStatusFromString(value.GetString("status", "Backlog"));
    result.priority = TaskPriorityFromString(value.GetString("priority", "Normal"));
    result.assignedWorkerId = value.GetString("assigned_worker_id");
    result.milestoneId = value.GetString("milestone_id");
    result.systemId = value.GetString("system_id");
    result.branch = value.GetString("branch");
    result.allowedPaths = StringArrayFromJson(value, "allowed_paths");
    result.forbiddenPaths = StringArrayFromJson(value, "forbidden_paths");
    result.acceptanceCriteria = value.GetString("acceptance_criteria");
    result.startedAt = value.GetString("started_at");
    result.completedAt = value.GetString("completed_at");
    return result;
}

SystemDetails SystemDetailsFromJson(const Engine::Json::Value& value) {
    SystemDetails result;
    result.status = SystemStatusFromString(value.GetString("status", "Planned"));
    result.ownerWorkerId = value.GetString("owner_worker_id");
    result.progress = value.GetNumber("progress", 0.0);
    return result;
}

WorkerDetails WorkerDetailsFromJson(const Engine::Json::Value& value) {
    WorkerDetails result;
    result.workerId = value.GetString("worker_id");
    result.type = WorkerTypeFromString(value.GetString("worker_type", "Automation"));
    result.status = WorkerStatusFromString(value.GetString("status", "Offline"));
    result.currentTaskId = value.GetString("current_task_id");
    result.currentTask = value.GetString("current_task");
    result.currentTaskStatus = value.GetString("current_task_status");
    result.branch = value.GetString("branch");
    result.lastActivityAt = value.GetString("last_activity_at");
    if (const Engine::Json::Value* locks = value.Find("current_locks"); locks != nullptr && locks->IsArray()) {
        for (const Engine::Json::Value& lock : locks->AsArray())
            result.currentLockIds.push_back(lock.GetString("scene_id"));
    }
    return result;
}

MilestoneDetails MilestoneDetailsFromJson(const Engine::Json::Value& value) {
    MilestoneDetails result;
    result.status = MilestoneStatusFromString(value.GetString("status", "Planned"));
    result.targetDate = value.GetString("target_date");
    result.progress = value.GetNumber("progress", 0.0);
    result.doneTaskCount = static_cast<int>(value.GetNumber("done_task_count", 0));
    result.totalRelatedTaskCount = static_cast<int>(value.GetNumber("total_related_task_count", 0));
    result.acceptanceCriteria = value.GetString("acceptance_criteria");
    return result;
}

SceneDetails SceneDetailsFromJson(const Engine::Json::Value& value) {
    SceneDetails result;
    result.assetPath = value.GetString("asset_path");
    return result;
}

Engine::Json::Value DetailsToJson(const CrystalEntity& value) {
    Engine::Json::Value::Object result;
    std::visit([&](const auto& details) {
        using T = std::decay_t<decltype(details)>;
        if constexpr (std::is_same_v<T, TaskDetails>) {
            result["status"] = TaskStatusName(details.status);
            result["priority"] = TaskPriorityName(details.priority);
            result["assigned_worker_id"] = details.assignedWorkerId;
            result["milestone_id"] = details.milestoneId;
            result["system_id"] = details.systemId;
            result["branch"] = details.branch;
            result["allowed_paths"] = StringArrayToJson(details.allowedPaths);
            result["forbidden_paths"] = StringArrayToJson(details.forbiddenPaths);
            result["acceptance_criteria"] = details.acceptanceCriteria;
            result["started_at"] = details.startedAt;
            result["completed_at"] = details.completedAt;
        } else if constexpr (std::is_same_v<T, SystemDetails>) {
            result["status"] = SystemStatusName(details.status);
            result["owner_worker_id"] = details.ownerWorkerId;
            result["progress"] = details.progress;
        } else if constexpr (std::is_same_v<T, WorkerDetails>) {
            result["worker_id"] = details.workerId;
            result["worker_type"] = WorkerTypeName(details.type);
            result["status"] = WorkerStatusName(details.status);
            result["current_task_id"] = details.currentTaskId;
            result["current_task"] = details.currentTask;
            result["current_task_status"] = details.currentTaskStatus;
            result["branch"] = details.branch;
            result["last_activity_at"] = details.lastActivityAt;
        } else if constexpr (std::is_same_v<T, MilestoneDetails>) {
            result["status"] = MilestoneStatusName(details.status);
            result["target_date"] = details.targetDate;
            result["progress"] = details.progress;
            result["done_task_count"] = details.doneTaskCount;
            result["total_related_task_count"] = details.totalRelatedTaskCount;
            result["acceptance_criteria"] = details.acceptanceCriteria;
        } else if constexpr (std::is_same_v<T, SceneDetails>) {
            result["asset_path"] = details.assetPath;
        }
    }, value.details);
    return result;
}

} // namespace

EntityType EntityTypeFromString(std::string_view value) noexcept {
    if (value == "text") return EntityType::Text;
    if (value == "note") return EntityType::Note;
    if (value == "task") return EntityType::Task;
    if (value == "system") return EntityType::System;
    if (value == "worker") return EntityType::Worker;
    if (value == "milestone") return EntityType::Milestone;
    if (value == "scene") return EntityType::Scene;
    return EntityType::Unknown;
}

PropertyType PropertyTypeFromString(std::string_view value) noexcept {
    if (value == "Text") return PropertyType::Text;
    if (value == "MultilineText") return PropertyType::MultilineText;
    if (value == "Number") return PropertyType::Number;
    if (value == "Bool") return PropertyType::Bool;
    if (value == "Enum") return PropertyType::Enum;
    if (value == "Date") return PropertyType::Date;
    if (value == "EntityReference") return PropertyType::EntityReference;
    if (value == "EntityReferenceList") return PropertyType::EntityReferenceList;
    if (value == "Tags") return PropertyType::Tags;
    if (value == "Progress") return PropertyType::Progress;
    if (value == "URL") return PropertyType::URL;
    return PropertyType::Unknown;
}

const char* PropertyTypeName(PropertyType value) noexcept {
    switch (value) {
    case PropertyType::Text: return "Text";
    case PropertyType::MultilineText: return "MultilineText";
    case PropertyType::Number: return "Number";
    case PropertyType::Bool: return "Bool";
    case PropertyType::Enum: return "Enum";
    case PropertyType::Date: return "Date";
    case PropertyType::EntityReference: return "EntityReference";
    case PropertyType::EntityReferenceList: return "EntityReferenceList";
    case PropertyType::Tags: return "Tags";
    case PropertyType::Progress: return "Progress";
    case PropertyType::URL: return "URL";
    default: return "Text";
    }
}

const char* EntityTypeName(EntityType value) noexcept {
    switch (value) {
    case EntityType::Text: return "text";
    case EntityType::Note: return "note";
    case EntityType::Task: return "task";
    case EntityType::System: return "system";
    case EntityType::Worker: return "worker";
    case EntityType::Milestone: return "milestone";
    case EntityType::Scene: return "scene";
    default: return "unknown";
    }
}

TaskStatus TaskStatusFromString(std::string_view value) noexcept {
    if (value == "Ready") return TaskStatus::Ready;
    if (value == "InProgress") return TaskStatus::InProgress;
    if (value == "Review") return TaskStatus::Review;
    if (value == "Testing") return TaskStatus::Testing;
    if (value == "Done") return TaskStatus::Done;
    if (value == "Blocked") return TaskStatus::Blocked;
    return TaskStatus::Backlog;
}

const char* TaskStatusName(TaskStatus value) noexcept {
    switch (value) {
    case TaskStatus::Ready: return "Ready";
    case TaskStatus::InProgress: return "InProgress";
    case TaskStatus::Review: return "Review";
    case TaskStatus::Testing: return "Testing";
    case TaskStatus::Done: return "Done";
    case TaskStatus::Blocked: return "Blocked";
    default: return "Backlog";
    }
}

TaskPriority TaskPriorityFromString(std::string_view value) noexcept {
    if (value == "Low") return TaskPriority::Low;
    if (value == "High") return TaskPriority::High;
    if (value == "Critical") return TaskPriority::Critical;
    return TaskPriority::Normal;
}

const char* TaskPriorityName(TaskPriority value) noexcept {
    switch (value) {
    case TaskPriority::Low: return "Low";
    case TaskPriority::High: return "High";
    case TaskPriority::Critical: return "Critical";
    default: return "Normal";
    }
}

SystemStatus SystemStatusFromString(std::string_view value) noexcept {
    if (value == "Developing") return SystemStatus::Developing;
    if (value == "Testing") return SystemStatus::Testing;
    if (value == "Stable") return SystemStatus::Stable;
    if (value == "Deprecated") return SystemStatus::Deprecated;
    return SystemStatus::Planned;
}

const char* SystemStatusName(SystemStatus value) noexcept {
    switch (value) {
    case SystemStatus::Developing: return "Developing";
    case SystemStatus::Testing: return "Testing";
    case SystemStatus::Stable: return "Stable";
    case SystemStatus::Deprecated: return "Deprecated";
    default: return "Planned";
    }
}

WorkerType WorkerTypeFromString(std::string_view value) noexcept {
    if (value == "Human") return WorkerType::Human;
    if (value == "Codex") return WorkerType::Codex;
    return WorkerType::Automation;
}

const char* WorkerTypeName(WorkerType value) noexcept {
    switch (value) {
    case WorkerType::Human: return "Human";
    case WorkerType::Codex: return "Codex";
    default: return "Automation";
    }
}

WorkerStatus WorkerStatusFromString(std::string_view value) noexcept {
    if (value == "Idle") return WorkerStatus::Idle;
    if (value == "Working") return WorkerStatus::Working;
    return WorkerStatus::Offline;
}

const char* WorkerStatusName(WorkerStatus value) noexcept {
    switch (value) {
    case WorkerStatus::Idle: return "Idle";
    case WorkerStatus::Working: return "Working";
    default: return "Offline";
    }
}

MilestoneStatus MilestoneStatusFromString(std::string_view value) noexcept {
    if (value == "Active") return MilestoneStatus::Active;
    if (value == "Completed") return MilestoneStatus::Completed;
    if (value == "Blocked") return MilestoneStatus::Blocked;
    return MilestoneStatus::Planned;
}

const char* MilestoneStatusName(MilestoneStatus value) noexcept {
    switch (value) {
    case MilestoneStatus::Active: return "Active";
    case MilestoneStatus::Completed: return "Completed";
    case MilestoneStatus::Blocked: return "Blocked";
    default: return "Planned";
    }
}

SpaceViewType SpaceViewTypeFromString(std::string_view value) noexcept {
    return value == "TaskBoard" ? SpaceViewType::TaskBoard : SpaceViewType::Graph;
}

const char* SpaceViewTypeName(SpaceViewType value) noexcept {
    return value == SpaceViewType::TaskBoard ? "TaskBoard" : "Graph";
}

ViewType ViewTypeFromString(std::string_view value) noexcept {
    if (value == "CompactNode") return ViewType::CompactNode;
    if (value == "Card") return ViewType::Card;
    if (value == "StatusCard") return ViewType::StatusCard;
    if (value == "Badge") return ViewType::Badge;
    return ViewType::Node;
}

const char* ViewTypeName(ViewType value) noexcept {
    switch (value) {
    case ViewType::CompactNode: return "CompactNode";
    case ViewType::Card: return "Card";
    case ViewType::StatusCard: return "StatusCard";
    case ViewType::Badge: return "Badge";
    default: return "Node";
    }
}

const char* DisplayModeName(DisplayMode value) noexcept {
    switch (value) {
    case DisplayMode::Graph: return "Graph";
    case DisplayMode::Draw: return "Draw";
    case DisplayMode::Presentation: return "Presentation";
    default: return "All";
    }
}

RelationshipType RelationshipTypeFromString(std::string_view value) noexcept {
    if (value == "depends_on") return RelationshipType::DependsOn;
    if (value == "blocks") return RelationshipType::Blocks;
    if (value == "belongs_to_system") return RelationshipType::BelongsToSystem;
    if (value == "assigned_to") return RelationshipType::AssignedTo;
    if (value == "belongs_to_milestone") return RelationshipType::BelongsToMilestone;
    if (value == "integrates_into") return RelationshipType::IntegratesInto;
    if (value == "related_to") return RelationshipType::RelatedTo;
    return RelationshipType::Unknown;
}

const char* RelationshipTypeName(RelationshipType value) noexcept {
    switch (value) {
    case RelationshipType::DependsOn: return "depends_on";
    case RelationshipType::Blocks: return "blocks";
    case RelationshipType::BelongsToSystem: return "belongs_to_system";
    case RelationshipType::AssignedTo: return "assigned_to";
    case RelationshipType::BelongsToMilestone: return "belongs_to_milestone";
    case RelationshipType::IntegratesInto: return "integrates_into";
    case RelationshipType::RelatedTo: return "related_to";
    default: return "unknown";
    }
}

const char* StrokeToolName(Engine::UI2D::StrokeTool value) noexcept {
    if (value == Engine::UI2D::StrokeTool::Eraser) return "eraser";
    if (value == Engine::UI2D::StrokeTool::Highlighter) return "highlighter";
    return "pen";
}

Engine::UI2D::StrokeTool StrokeToolFromString(std::string_view value) noexcept {
    if (value == "eraser") return Engine::UI2D::StrokeTool::Eraser;
    if (value == "highlighter") return Engine::UI2D::StrokeTool::Highlighter;
    return Engine::UI2D::StrokeTool::Pen;
}

const char* StrokeShapeName(Engine::UI2D::StrokeShape value) noexcept {
    switch (value) {
    case Engine::UI2D::StrokeShape::StraightLine: return "line";
    case Engine::UI2D::StrokeShape::Arrow: return "arrow";
    case Engine::UI2D::StrokeShape::Rectangle: return "rectangle";
    case Engine::UI2D::StrokeShape::Ellipse: return "ellipse";
    default: return "freehand";
    }
}

Engine::UI2D::StrokeShape StrokeShapeFromString(std::string_view value) noexcept {
    if (value == "line") return Engine::UI2D::StrokeShape::StraightLine;
    if (value == "arrow") return Engine::UI2D::StrokeShape::Arrow;
    if (value == "rectangle") return Engine::UI2D::StrokeShape::Rectangle;
    if (value == "ellipse") return Engine::UI2D::StrokeShape::Ellipse;
    return Engine::UI2D::StrokeShape::Freehand;
}

Engine::Json::Value ToJson(const CrystalEntity& value) {
    Engine::Json::Value::Object result;
    result["id"] = value.id;
    result["project_id"] = value.projectId;
    result["entity_type"] = value.type == EntityType::Unknown && !value.typeName.empty()
        ? value.typeName : EntityTypeName(value.type);
    result["schema_id"] = value.schemaId;
    result["title"] = value.title;
    result["description"] = value.description;
    result["revision"] = value.revision;
    result["data"] = value.data;
    result["properties"] = value.properties;
    result["details"] = DetailsToJson(value);
    return result;
}

Engine::Json::Value ToJson(const CrystalPropertyDefinition& value) {
    Engine::Json::Value::Array options;
    for (const std::string& option : value.options) options.emplace_back(option);
    Engine::Json::Value::Object result;
    result["id"] = value.id;
    result["schema_id"] = value.schemaId;
    result["name"] = value.name;
    result["display_name"] = value.displayName;
    result["property_type"] = PropertyTypeName(value.type);
    result["default_value"] = value.defaultValue;
    result["required"] = value.required;
    result["options"] = std::move(options);
    if (value.hasMinValue) result["min_value"] = value.minValue;
    if (value.hasMaxValue) result["max_value"] = value.maxValue;
    if (value.hasStepValue) result["step_value"] = value.stepValue;
    result["sort_order"] = value.sortOrder;
    return result;
}

Engine::Json::Value ToJson(const CrystalEntitySchema& value) {
    Engine::Json::Value::Array properties;
    for (const auto& property : value.properties) properties.emplace_back(ToJson(property));
    Engine::Json::Value::Object result;
    result["id"] = value.id;
    result["project_id"] = value.projectId;
    result["name"] = value.name;
    result["display_name"] = value.displayName;
    result["icon"] = value.icon;
    result["description"] = value.description;
    result["properties"] = std::move(properties);
    result["created_at"] = value.createdAt;
    result["updated_at"] = value.updatedAt;
    return result;
}

Engine::Json::Value ToJson(const EntityView& value) {
    Engine::Json::Value::Object result;
    result["id"] = value.id;
    result["entity_id"] = value.entityId;
    result["space_id"] = value.spaceId;
    result["x"] = value.position.x;
    result["y"] = value.position.y;
    result["width"] = value.size.x;
    result["height"] = value.size.y;
    result["view_type"] = ViewTypeName(value.viewType);
    result["layer_id"] = value.layerId;
    result["visible"] = value.visible;
    result["visible_properties"] = StringArrayToJson(value.visibleProperties);
    result["board_id"] = value.boardId;
    result["board_local_x"] = value.boardLocalPosition.x;
    result["board_local_y"] = value.boardLocalPosition.y;
    result["revision"] = value.revision;
    return result;
}

Engine::Json::Value ToJson(const BoardModel& value) {
    Engine::Json::Value::Object result;
    result["id"] = value.id;
    result["project_id"] = value.projectId;
    result["space_id"] = value.spaceId;
    result["title"] = value.title;
    result["x"] = value.position.x;
    result["y"] = value.position.y;
    result["width"] = value.size.x;
    result["height"] = value.size.y;
    result["background"] = value.background;
    result["border"] = value.border;
    result["locked"] = value.locked;
    result["collapsed"] = value.collapsed;
    result["layer_id"] = value.layerId;
    result["z_order"] = value.zOrder;
    result["created_at"] = value.createdAt;
    result["updated_at"] = value.updatedAt;
    return result;
}

Engine::Json::Value ToJson(const UserLayerModel& value) {
    Engine::Json::Value::Object result;
    result["id"] = value.id;
    result["project_id"] = value.projectId;
    result["name"] = value.name;
    result["visible"] = value.visible;
    result["space_id"] = value.spaceId;
    result["locked"] = value.locked;
    result["sort_order"] = value.sortOrder;
    return result;
}

Engine::Json::Value ToJson(const QueryViewModel& value) {
    Engine::Json::Value::Array filters;
    for (const auto& filter : value.filters) {
        filters.emplace_back(Engine::Json::Value::Object{
            {"property", filter.property}, {"operator", filter.op}, {"value", filter.value}});
    }
    Engine::Json::Value::Object result;
    result["id"] = value.id;
    result["project_id"] = value.projectId;
    result["space_id"] = value.spaceId;
    result["name"] = value.name;
    result["entity_schema"] = value.entitySchema;
    result["filters"] = std::move(filters);
    result["sort"] = value.sort;
    result["result_limit"] = value.resultLimit;
    Engine::Json::Value::Array results;
    for (const auto& entity : value.results) results.emplace_back(ToJson(entity));
    result["results"] = std::move(results);
    return result;
}

Engine::Json::Value ToJson(const DashboardWidgetModel& value) {
    Engine::Json::Value::Object result;
    result["id"] = value.id;
    result["project_id"] = value.projectId;
    result["space_id"] = value.spaceId;
    result["widget_type"] = value.widgetType;
    result["title"] = value.title;
    result["config"] = value.config;
    result["x"] = value.position.x;
    result["y"] = value.position.y;
    result["width"] = value.size.x;
    result["height"] = value.size.y;
    result["layer_id"] = value.layerId;
    result["visible"] = value.visible;
    result["revision"] = value.revision;
    return result;
}

Engine::Json::Value ToJson(const ActivityModel& value) {
    Engine::Json::Value::Object result;
    result["id"] = value.id;
    result["project_id"] = value.projectId;
    result["actor_id"] = value.actorId;
    result["actor_name"] = value.actorName;
    result["event_type"] = value.eventType;
    result["entity_id"] = value.entityId;
    result["summary"] = value.summary;
    result["payload"] = value.payload;
    result["created_at"] = value.createdAt;
    return result;
}

Engine::Json::Value ToJson(const CrystalRelationship& value) {
    Engine::Json::Value::Object result;
    result["id"] = value.id;
    result["project_id"] = value.projectId;
    result["source_entity_id"] = value.sourceEntityId;
    result["target_entity_id"] = value.targetEntityId;
    result["relationship_type"] = RelationshipTypeName(value.type);
    result["visible"] = value.visible;
    result["created_at"] = value.createdAt;
    return result;
}

Engine::Json::Value ToJson(const Engine::UI2D::InkStroke& value) {
    Engine::Json::Value::Array points;
    points.reserve(value.points.size());
    for (const Engine::Vec2F point : value.points) {
        Engine::Json::Value::Object item;
        item["x"] = point.x;
        item["y"] = point.y;
        points.emplace_back(std::move(item));
    }
    Engine::Json::Value::Object result;
    result["id"] = value.id;
    result["points"] = std::move(points);
    result["width"] = value.width;
    result["tool"] = StrokeToolName(value.tool);
    result["shape"] = StrokeShapeName(value.shape);
    result["color"] = value.color;
    result["layer_id"] = value.layer;
    result["group_id"] = value.groupId;
    result["anchor_entity_id"] = value.anchorEntityId;
    result["anchor_board_id"] = value.anchorBoardId;
    result["local_offset_x"] = value.localOffset.x;
    result["local_offset_y"] = value.localOffset.y;
    result["created_at"] = value.createdAt;
    return result;
}

CrystalEntity EntityFromJson(const Engine::Json::Value& value) {
    CrystalEntity result;
    result.id = value.GetString("id");
    result.projectId = value.GetString("project_id");
    result.typeName = value.GetString("entity_type");
    result.type = EntityTypeFromString(result.typeName);
    result.schemaId = value.GetString("schema_id");
    result.title = value.GetString("title");
    result.description = value.GetString("description");
    result.revision = static_cast<int>(value.GetNumber("revision", 1));
    if (const Engine::Json::Value* data = value.Find("data")) result.data = *data;
    if (const Engine::Json::Value* properties = value.Find("properties")) result.properties = *properties;
    const Engine::Json::Value* details = value.Find("details");
    if (details == nullptr) details = value.Find("data");
    if (details != nullptr) {
        switch (result.type) {
        case EntityType::Task: result.details = TaskDetailsFromJson(*details); break;
        case EntityType::System: result.details = SystemDetailsFromJson(*details); break;
        case EntityType::Worker: result.details = WorkerDetailsFromJson(*details); break;
        case EntityType::Milestone: result.details = MilestoneDetailsFromJson(*details); break;
        case EntityType::Scene: result.details = SceneDetailsFromJson(*details); break;
        default: break;
        }
    }
    return result;
}

CrystalPropertyDefinition PropertyDefinitionFromJson(const Engine::Json::Value& value) {
    CrystalPropertyDefinition result;
    result.id = value.GetString("id");
    result.schemaId = value.GetString("schema_id");
    result.name = value.GetString("name");
    result.displayName = value.GetString("display_name", result.name);
    result.type = PropertyTypeFromString(value.GetString("property_type", "Text"));
    if (const Engine::Json::Value* defaultValue = value.Find("default_value")) result.defaultValue = *defaultValue;
    result.required = BoolFromJson(value, "required", false);
    result.options = StringArrayFromJson(value, "options");
    if (const Engine::Json::Value* minimum = value.Find("min_value"); minimum != nullptr && minimum->IsNumber()) {
        result.minValue = minimum->AsNumber(); result.hasMinValue = true;
    }
    if (const Engine::Json::Value* maximum = value.Find("max_value"); maximum != nullptr && maximum->IsNumber()) {
        result.maxValue = maximum->AsNumber(); result.hasMaxValue = true;
    }
    if (const Engine::Json::Value* step = value.Find("step_value"); step != nullptr && step->IsNumber()) {
        result.stepValue = step->AsNumber(); result.hasStepValue = true;
    }
    result.sortOrder = static_cast<int>(value.GetNumber("sort_order", 0));
    return result;
}

CrystalEntitySchema EntitySchemaFromJson(const Engine::Json::Value& value) {
    CrystalEntitySchema result;
    result.id = value.GetString("id");
    result.projectId = value.GetString("project_id");
    result.name = value.GetString("name");
    result.displayName = value.GetString("display_name", result.name);
    result.icon = value.GetString("icon");
    result.description = value.GetString("description");
    result.createdAt = value.GetString("created_at");
    result.updatedAt = value.GetString("updated_at");
    if (const Engine::Json::Value* properties = value.Find("properties"); properties != nullptr && properties->IsArray())
        for (const Engine::Json::Value& property : properties->AsArray())
            result.properties.push_back(PropertyDefinitionFromJson(property));
    return result;
}

EntityView EntityViewFromJson(const Engine::Json::Value& value) {
    EntityView result;
    result.id = value.GetString("id");
    result.entityId = value.GetString("entity_id");
    result.spaceId = value.GetString("space_id");
    result.position = {static_cast<float>(value.GetNumber("x")),
                       static_cast<float>(value.GetNumber("y"))};
    result.size = {static_cast<float>(value.GetNumber("width", 220)),
                   static_cast<float>(value.GetNumber("height", 120))};
    result.viewType = ViewTypeFromString(value.GetString("view_type", "Node"));
    result.layerId = value.GetString("layer_id", "Main");
    result.visible = BoolFromJson(value, "visible", true);
    result.visibleProperties = StringArrayFromJson(value, "visible_properties");
    result.boardId = value.GetString("board_id");
    result.boardLocalPosition = {static_cast<float>(value.GetNumber("board_local_x")),
                                 static_cast<float>(value.GetNumber("board_local_y"))};
    result.revision = static_cast<int>(value.GetNumber("revision", 1));
    return result;
}

BoardModel BoardFromJson(const Engine::Json::Value& value) {
    BoardModel result;
    result.id = value.GetString("id");
    result.projectId = value.GetString("project_id");
    result.spaceId = value.GetString("space_id");
    result.title = value.GetString("title");
    result.position = {static_cast<float>(value.GetNumber("x")), static_cast<float>(value.GetNumber("y"))};
    result.size = {static_cast<float>(value.GetNumber("width", 800)),
                   static_cast<float>(value.GetNumber("height", 500))};
    result.background = value.GetString("background", "#172235");
    result.border = value.GetString("border", "#4d7198");
    result.locked = BoolFromJson(value, "locked", false);
    result.collapsed = BoolFromJson(value, "collapsed", false);
    result.layerId = value.GetString("layer_id", "Main");
    result.zOrder = static_cast<int>(value.GetNumber("z_order", 0));
    result.createdAt = value.GetString("created_at");
    result.updatedAt = value.GetString("updated_at");
    return result;
}

UserLayerModel UserLayerFromJson(const Engine::Json::Value& value) {
    UserLayerModel result;
    result.id = value.GetString("id");
    result.projectId = value.GetString("project_id");
    result.name = value.GetString("name");
    result.visible = BoolFromJson(value, "visible", true);
    result.spaceId = value.GetString("space_id");
    result.locked = BoolFromJson(value, "locked", false);
    result.sortOrder = static_cast<int>(value.GetNumber("sort_order", 0));
    return result;
}

QueryViewModel QueryViewFromJson(const Engine::Json::Value& value) {
    QueryViewModel result;
    result.id = value.GetString("id");
    result.projectId = value.GetString("project_id");
    result.spaceId = value.GetString("space_id");
    result.name = value.GetString("name");
    result.entitySchema = value.GetString("entity_schema");
    result.resultLimit = static_cast<int>(value.GetNumber("result_limit", 50));
    if (const Engine::Json::Value* sort = value.Find("sort")) result.sort = *sort;
    if (const Engine::Json::Value* filters = value.Find("filters"); filters != nullptr && filters->IsArray()) {
        for (const Engine::Json::Value& item : filters->AsArray()) {
            QueryFilterModel filter;
            filter.property = item.GetString("property", item.GetString("field"));
            filter.op = item.GetString("operator", "Equals");
            if (const Engine::Json::Value* filterValue = item.Find("value")) filter.value = *filterValue;
            result.filters.push_back(std::move(filter));
        }
    }
    if (const Engine::Json::Value* results = value.Find("results"); results != nullptr && results->IsArray())
        for (const Engine::Json::Value& item : results->AsArray()) result.results.push_back(EntityFromJson(item));
    return result;
}

DashboardWidgetModel DashboardWidgetFromJson(const Engine::Json::Value& value) {
    DashboardWidgetModel result;
    result.id = value.GetString("id");
    result.projectId = value.GetString("project_id");
    result.spaceId = value.GetString("space_id");
    result.widgetType = value.GetString("widget_type");
    result.title = value.GetString("title");
    if (const Engine::Json::Value* config = value.Find("config")) result.config = *config;
    result.position = {static_cast<float>(value.GetNumber("x")), static_cast<float>(value.GetNumber("y"))};
    result.size = {static_cast<float>(value.GetNumber("width", 320)),
                   static_cast<float>(value.GetNumber("height", 180))};
    result.layerId = value.GetString("layer_id", "Main");
    result.visible = BoolFromJson(value, "visible", true);
    result.revision = static_cast<int>(value.GetNumber("revision", 1));
    return result;
}

ActivityModel ActivityFromJson(const Engine::Json::Value& value) {
    ActivityModel result;
    result.id = value.GetString("id");
    result.projectId = value.GetString("project_id");
    result.actorId = value.GetString("actor_id");
    result.actorName = value.GetString("actor_name");
    result.eventType = value.GetString("event_type");
    result.entityId = value.GetString("entity_id");
    result.summary = value.GetString("summary");
    if (const Engine::Json::Value* payload = value.Find("payload")) result.payload = *payload;
    result.createdAt = value.GetString("created_at");
    return result;
}

CrystalRelationship RelationshipFromJson(const Engine::Json::Value& value) {
    CrystalRelationship result;
    result.id = value.GetString("id");
    result.projectId = value.GetString("project_id");
    result.sourceEntityId = value.GetString("source_entity_id");
    result.targetEntityId = value.GetString("target_entity_id");
    result.type = RelationshipTypeFromString(value.GetString("relationship_type"));
    result.visible = BoolFromJson(value, "visible", true);
    result.createdAt = value.GetString("created_at");
    return result;
}

Engine::UI2D::InkStroke StrokeFromJson(const Engine::Json::Value& value) {
    Engine::UI2D::InkStroke result;
    result.id = value.GetString("id");
    result.width = static_cast<float>(value.GetNumber("width", 2));
    result.tool = StrokeToolFromString(value.GetString("tool", "pen"));
    result.shape = StrokeShapeFromString(value.GetString("shape", "freehand"));
    result.color = value.GetString("color", "#9ed5ff");
    result.layer = value.GetString("layer_id", "Main");
    result.groupId = value.GetString("group_id");
    result.anchorEntityId = value.GetString("anchor_entity_id");
    result.anchorBoardId = value.GetString("anchor_board_id");
    result.localOffset = {static_cast<float>(value.GetNumber("local_offset_x")),
                          static_cast<float>(value.GetNumber("local_offset_y"))};
    result.createdAt = value.GetString("created_at");
    if (const Engine::Json::Value* points = value.Find("points")) {
        for (const Engine::Json::Value& point : points->AsArray())
            result.points.push_back(PointFromJson(point));
    }
    return result;
}

ProjectWorkspaceModel WorkspaceFromJson(const Engine::Json::Value& value) {
    ProjectWorkspaceModel result;
    if (const Engine::Json::Value* project = value.Find("project")) {
        result.project.id = project->GetString("id");
        result.project.name = project->GetString("name");
    }
    if (const Engine::Json::Value* spaces = value.Find("spaces"))
        for (const Engine::Json::Value& space : spaces->AsArray()) result.spaces.push_back(SpaceFromJson(space));
    if (const Engine::Json::Value* current = value.Find("current_space"); current != nullptr && !current->IsNull())
        result.currentSpace = SpaceFromJson(*current);
    if (const Engine::Json::Value* entities = value.Find("entities"))
        for (const Engine::Json::Value& entity : entities->AsArray()) result.entities.push_back(EntityFromJson(entity));
    if (const Engine::Json::Value* views = value.Find("views"))
        for (const Engine::Json::Value& view : views->AsArray()) result.views.push_back(EntityViewFromJson(view));
    if (const Engine::Json::Value* relationships = value.Find("relationships"))
        for (const Engine::Json::Value& relationship : relationships->AsArray())
            result.relationships.push_back(RelationshipFromJson(relationship));
    if (const Engine::Json::Value* strokes = value.Find("strokes"))
        for (const Engine::Json::Value& stroke : strokes->AsArray()) result.strokes.push_back(StrokeFromJson(stroke));
    if (const Engine::Json::Value* locks = value.Find("scene_locks"))
        for (const Engine::Json::Value& lock : locks->AsArray()) result.sceneLocks.push_back(SceneLockFromJson(lock));
    if (const Engine::Json::Value* layers = value.Find("user_layers")) {
        for (const Engine::Json::Value& layer : layers->AsArray())
            result.userLayers.push_back(UserLayerFromJson(layer));
    }
    if (const Engine::Json::Value* schemas = value.Find("schemas"); schemas != nullptr && schemas->IsArray())
        for (const Engine::Json::Value& schema : schemas->AsArray()) result.schemas.push_back(EntitySchemaFromJson(schema));
    if (const Engine::Json::Value* boards = value.Find("boards"); boards != nullptr && boards->IsArray())
        for (const Engine::Json::Value& board : boards->AsArray()) result.boards.push_back(BoardFromJson(board));
    if (const Engine::Json::Value* queries = value.Find("query_views"); queries != nullptr && queries->IsArray())
        for (const Engine::Json::Value& query : queries->AsArray()) result.queryViews.push_back(QueryViewFromJson(query));
    if (const Engine::Json::Value* widgets = value.Find("dashboard_widgets"); widgets != nullptr && widgets->IsArray())
        for (const Engine::Json::Value& widget : widgets->AsArray()) result.dashboardWidgets.push_back(DashboardWidgetFromJson(widget));
    if (const Engine::Json::Value* activities = value.Find("activities"); activities != nullptr && activities->IsArray())
        for (const Engine::Json::Value& activity : activities->AsArray()) result.activities.push_back(ActivityFromJson(activity));
    return result;
}

} // namespace Crystal
