#include "Net/CrystalRealtimeEvent.h"

namespace Crystal {

CrystalRealtimeEventType RealtimeEventTypeFromWire(std::string_view type) noexcept {
    if (type == "workspace.state") return CrystalRealtimeEventType::WorkspaceSnapshot;
    if (type == "entity.created") return CrystalRealtimeEventType::EntityCreated;
    if (type == "entity.updated") return CrystalRealtimeEventType::EntityUpdated;
    if (type == "entity.deleted") return CrystalRealtimeEventType::EntityDeleted;
    if (type == "view.created") return CrystalRealtimeEventType::ViewCreated;
    if (type == "view.updated") return CrystalRealtimeEventType::ViewUpdated;
    if (type == "view.deleted") return CrystalRealtimeEventType::ViewDeleted;
    if (type == "relationship.created") return CrystalRealtimeEventType::RelationshipCreated;
    if (type == "relationship.updated") return CrystalRealtimeEventType::RelationshipUpdated;
    if (type == "relationship.deleted") return CrystalRealtimeEventType::RelationshipDeleted;
    if (type == "space.created") return CrystalRealtimeEventType::SpaceCreated;
    if (type == "space.updated") return CrystalRealtimeEventType::SpaceUpdated;
    if (type == "space.deleted") return CrystalRealtimeEventType::SpaceDeleted;
    if (type == "schema.created") return CrystalRealtimeEventType::SchemaCreated;
    if (type == "schema.updated") return CrystalRealtimeEventType::SchemaUpdated;
    if (type == "schema.deleted") return CrystalRealtimeEventType::SchemaDeleted;
    if (type == "board.created") return CrystalRealtimeEventType::BoardCreated;
    if (type == "board.updated") return CrystalRealtimeEventType::BoardUpdated;
    if (type == "board.deleted") return CrystalRealtimeEventType::BoardDeleted;
    if (type == "layer.created") return CrystalRealtimeEventType::LayerCreated;
    if (type == "layer.updated") return CrystalRealtimeEventType::LayerUpdated;
    if (type == "layer.deleted") return CrystalRealtimeEventType::LayerDeleted;
    if (type == "query_view.created") return CrystalRealtimeEventType::QueryViewCreated;
    if (type == "query_view.updated") return CrystalRealtimeEventType::QueryViewUpdated;
    if (type == "query_view.deleted") return CrystalRealtimeEventType::QueryViewDeleted;
    if (type == "dashboard_widget.created") return CrystalRealtimeEventType::DashboardWidgetCreated;
    if (type == "dashboard_widget.updated") return CrystalRealtimeEventType::DashboardWidgetUpdated;
    if (type == "dashboard_widget.deleted") return CrystalRealtimeEventType::DashboardWidgetDeleted;
    if (type == "activity.created") return CrystalRealtimeEventType::ActivityCreated;
    if (type == "worker.updated") return CrystalRealtimeEventType::WorkerUpdated;
    if (type == "scene_lock.acquired") return CrystalRealtimeEventType::SceneLockAcquired;
    if (type == "scene_lock.heartbeat") return CrystalRealtimeEventType::SceneLockHeartbeat;
    if (type == "scene_lock.released") return CrystalRealtimeEventType::SceneLockReleased;
    if (type == "scene_lock.stale") return CrystalRealtimeEventType::SceneLockStale;
    if (type == "ink.created") return CrystalRealtimeEventType::InkCreated;
    if (type == "ink.updated") return CrystalRealtimeEventType::InkUpdated;
    if (type == "ink.deleted") return CrystalRealtimeEventType::InkDeleted;
    return CrystalRealtimeEventType::Unknown;
}

} // namespace Crystal
