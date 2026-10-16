#pragma once

#include "Network/Json.h"
#include "Network/WebSocketClient.h"

#include <string>
#include <string_view>

namespace Crystal {

enum class CrystalRealtimeEventType {
    ConnectionState,
    WorkspaceSnapshot,
    EntityCreated,
    EntityUpdated,
    EntityDeleted,
    ViewCreated,
    ViewUpdated,
    ViewDeleted,
    RelationshipCreated,
    RelationshipUpdated,
    RelationshipDeleted,
    SpaceCreated,
    SpaceUpdated,
    SpaceDeleted,
    SchemaCreated,
    SchemaUpdated,
    SchemaDeleted,
    BoardCreated,
    BoardUpdated,
    BoardDeleted,
    LayerCreated,
    LayerUpdated,
    LayerDeleted,
    QueryViewCreated,
    QueryViewUpdated,
    QueryViewDeleted,
    DashboardWidgetCreated,
    DashboardWidgetUpdated,
    DashboardWidgetDeleted,
    ActivityCreated,
    WorkerUpdated,
    SceneLockAcquired,
    SceneLockHeartbeat,
    SceneLockReleased,
    SceneLockStale,
    InkCreated,
    InkUpdated,
    InkDeleted,
    Unknown,
};

struct CrystalRealtimeEvent {
    CrystalRealtimeEventType type = CrystalRealtimeEventType::Unknown;
    std::string wireType;
    Engine::Json::Value payload{Engine::Json::Value::Object{}};
    Engine::Network::WebSocketState connectionState =
        Engine::Network::WebSocketState::Disconnected;
};

[[nodiscard]] CrystalRealtimeEventType RealtimeEventTypeFromWire(
    std::string_view type) noexcept;

} // namespace Crystal
