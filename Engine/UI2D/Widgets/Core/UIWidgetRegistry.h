#pragma once

#include "UI2D/Input/UIEventListenerRegistry.h"
#include "UI2D/Widgets/Core/UIWidgetRecord.h"
#include "UI2D/Widgets/Scrolling/UIScrollTypes.h"

#include <optional>
#include <unordered_map>

namespace Engine::UI2D {

class UIWindowRuntime;

class UIWidgetRegistry {
public:
    explicit UIWidgetRegistry(UIWindowRuntime& runtime) : m_Runtime(&runtime) {}

    UIWidgetHandle Register(UIWidgetRecord record);
    bool Destroy(UIWidgetHandle widget, UIWindowRuntime& runtime);
    UIWidgetRecord* TryGet(UIWidgetHandle widget);
    const UIWidgetRecord* TryGet(UIWidgetHandle widget) const;
    bool Reparent(UIWidgetHandle child, UIWidgetHandle parent, UIWindowRuntime& runtime);
    void OnNodeInvalidated(UINodeHandle node);
    void Clear(UIWindowRuntime& runtime);

    UIWidgetConnectionHandle Connect(UIWidgetHandle widget, UINodeHandle node,
        UIEventType eventType, UIEventPhaseMask phases, UIEventCallback callback);
    UIWidgetConnectionHandle OnActivated(UIWidgetHandle widget, UIButtonActivatedCallback callback);
    UIWidgetConnectionHandle OnCheckedChanged(UIWidgetHandle widget, UIToggleCheckedChangedCallback callback);
    UIWidgetConnectionHandle OnScrollChanged(UIWidgetHandle widget, UIScrollChangedCallback callback);
    UIWidgetConnectionHandle OnScrollCompleted(UIWidgetHandle widget, UIScrollCompletedCallback callback);
    bool Disconnect(UIWidgetConnectionHandle connection);
    void DisconnectAll(UIWidgetHandle widget);
    void DispatchActivated(const UIButtonActivatedEvent& event);
    void DispatchCheckedChanged(UIWidgetHandle widget, bool checked);
    void DispatchScrollChanged(const UIScrollChangedEvent& event);
    void DispatchScrollCompleted(const UIScrollCompletedEvent& event);

    [[nodiscard]] UIWidgetHandle OwnerOf(UINodeHandle node) const;
    [[nodiscard]] std::size_t Size() const noexcept { return m_LiveCount; }
    [[nodiscard]] bool Empty() const noexcept { return m_LiveCount == 0; }

private:
    struct WidgetSlot { std::uint32_t generation = 0; std::optional<UIWidgetRecord> record; };
    enum class ConnectionKind { Event, Activated, CheckedChanged, ScrollChanged, ScrollCompleted };
    struct ConnectionSlot {
        std::uint32_t generation = 0;
        bool occupied = false;
        UIWidgetHandle widget{};
        ConnectionKind kind = ConnectionKind::Event;
        UIEventListenerHandle listener{};
        UIButtonActivatedCallback activated;
        UIToggleCheckedChangedCallback checkedChanged;
        UIScrollChangedCallback scrollChanged;
        UIScrollCompletedCallback scrollCompleted;
    };
    struct NodeHash {
        std::size_t operator()(UINodeHandle node) const noexcept {
            return static_cast<std::size_t>(node.index) * 16777619u ^ node.generation;
        }
    };

    UIWidgetConnectionHandle AllocateConnection(UIWidgetHandle widget, ConnectionKind kind);
    bool WouldCreateCycle(UIWidgetHandle child, UIWidgetHandle parent) const;
    void RemoveFromParent(UIWidgetRecord& record);
    void ReleaseWidget(UIWidgetHandle handle);

    UIWindowRuntime* m_Runtime = nullptr;
    std::vector<WidgetSlot> m_Slots{1};
    std::vector<std::uint32_t> m_Free;
    std::vector<ConnectionSlot> m_Connections{1};
    std::vector<std::uint32_t> m_FreeConnections;
    std::unordered_map<UINodeHandle, UIWidgetHandle, NodeHash> m_NodeOwners;
    std::uint64_t m_NextCreationOrder = 1;
    std::size_t m_LiveCount = 0;
};

} // namespace Engine::UI2D
