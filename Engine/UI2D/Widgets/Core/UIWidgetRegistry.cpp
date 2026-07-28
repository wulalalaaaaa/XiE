#include "UI2D/Widgets/Core/UIWidgetRegistry.h"

#include "UI2D/Runtime/UIWindowRuntime.h"

#include <algorithm>
#include <atomic>

namespace Engine::UI2D {
namespace {
std::atomic<std::uint32_t> g_WidgetGeneration{1};
std::atomic<std::uint32_t> g_ConnectionGeneration{1};
std::uint32_t Next(std::atomic<std::uint32_t>& value) {
    std::uint32_t result = value.fetch_add(1, std::memory_order_relaxed);
    while (result == 0) result = value.fetch_add(1, std::memory_order_relaxed);
    return result;
}
} // namespace

UIWidgetRecord* UIWidgetRegistry::TryGet(UIWidgetHandle handle) {
    if (!handle.IsValid() || handle.index >= m_Slots.size()) return nullptr;
    WidgetSlot& slot = m_Slots[handle.index];
    return slot.generation == handle.generation && slot.record ? &*slot.record : nullptr;
}
const UIWidgetRecord* UIWidgetRegistry::TryGet(UIWidgetHandle handle) const {
    if (!handle.IsValid() || handle.index >= m_Slots.size()) return nullptr;
    const WidgetSlot& slot = m_Slots[handle.index];
    return slot.generation == handle.generation && slot.record ? &*slot.record : nullptr;
}

UIWidgetHandle UIWidgetRegistry::Register(UIWidgetRecord record) {
    if (!record.rootNode.IsValid() || record.ownedNodes.empty() ||
        std::find(record.ownedNodes.begin(), record.ownedNodes.end(), record.rootNode) == record.ownedNodes.end())
        return {};
    for (UINodeHandle node : record.ownedNodes) {
        if (!m_Runtime->Scene().TryGet(node) || m_NodeOwners.contains(node)) return {};
    }
    std::uint32_t index = 0;
    if (!m_Free.empty()) { index = m_Free.back(); m_Free.pop_back(); }
    else { index = static_cast<std::uint32_t>(m_Slots.size()); m_Slots.push_back({}); }
    WidgetSlot& slot = m_Slots[index];
    slot.generation = Next(g_WidgetGeneration);
    const UIWidgetHandle handle{index, slot.generation};
    record.handle = handle;
    record.lifecycle = UIWidgetLifecycleState::Mounted;
    record.creationOrder = m_NextCreationOrder++;
    slot.record = std::move(record);
    for (UINodeHandle node : slot.record->ownedNodes) m_NodeOwners.emplace(node, handle);
    if (UIWidgetRecord* parent = TryGet(slot.record->parentWidget)) parent->childWidgets.push_back(handle);
    ++m_LiveCount;
    return handle;
}

void UIWidgetRegistry::RemoveFromParent(UIWidgetRecord& record) {
    if (UIWidgetRecord* parent = TryGet(record.parentWidget))
        std::erase(parent->childWidgets, record.handle);
    record.parentWidget = {};
}
void UIWidgetRegistry::ReleaseWidget(UIWidgetHandle handle) {
    WidgetSlot& slot = m_Slots[handle.index];
    if (!slot.record) return;
    for (UINodeHandle node : slot.record->ownedNodes) m_NodeOwners.erase(node);
    slot.record.reset();
    m_Free.push_back(handle.index);
    --m_LiveCount;
}

bool UIWidgetRegistry::Destroy(UIWidgetHandle handle, UIWindowRuntime& runtime) {
    if (runtime.EventListeners().DispatchInProgress()) return false;
    UIWidgetRecord* record = TryGet(handle);
    if (!record || record->lifecycle == UIWidgetLifecycleState::Destroying ||
        record->lifecycle == UIWidgetLifecycleState::Destroyed) return false;
    record->lifecycle = UIWidgetLifecycleState::Destroying;
    const std::vector<UIWidgetHandle> children = record->childWidgets;
    for (UIWidgetHandle child : children) (void)Destroy(child, runtime);
    record = TryGet(handle);
    if (!record) return false;
    DisconnectAll(handle);
    RemoveFromParent(*record);
    const UINodeHandle root = record->rootNode;
    if (runtime.Scene().TryGet(root)) (void)runtime.Scene().DestroyNode(root);
    record = TryGet(handle);
    if (record) {
        record->lifecycle = UIWidgetLifecycleState::Destroyed;
        ReleaseWidget(handle);
    }
    return true;
}

bool UIWidgetRegistry::WouldCreateCycle(UIWidgetHandle child, UIWidgetHandle parent) const {
    for (UIWidgetHandle current = parent; current.IsValid();) {
        if (current == child) return true;
        const UIWidgetRecord* record = TryGet(current);
        current = record ? record->parentWidget : UIWidgetHandle{};
    }
    return false;
}
bool UIWidgetRegistry::Reparent(UIWidgetHandle childHandle, UIWidgetHandle parentHandle,
    UIWindowRuntime& runtime) {
    if (runtime.EventListeners().DispatchInProgress()) return false;
    UIWidgetRecord* child = TryGet(childHandle);
    UIWidgetRecord* parent = parentHandle.IsValid() ? TryGet(parentHandle) : nullptr;
    if (!child || childHandle == parentHandle || (parentHandle.IsValid() && !parent) ||
        WouldCreateCycle(childHandle, parentHandle)) return false;
    const UINodeHandle sceneParent = parent
        ? (parent->childHostNode.IsValid() ? parent->childHostNode : parent->rootNode)
        : runtime.Scene().Root();
    if (!runtime.Scene().ReparentNode(child->rootNode, sceneParent)) return false;
    RemoveFromParent(*child);
    child->parentWidget = parentHandle;
    if (parent) parent->childWidgets.push_back(childHandle);
    return true;
}

void UIWidgetRegistry::OnNodeInvalidated(UINodeHandle node) { m_NodeOwners.erase(node); }
UIWidgetHandle UIWidgetRegistry::OwnerOf(UINodeHandle node) const {
    const auto it = m_NodeOwners.find(node);
    return it == m_NodeOwners.end() ? UIWidgetHandle{} : it->second;
}
void UIWidgetRegistry::Clear(UIWindowRuntime& runtime) {
    std::vector<UIWidgetHandle> roots;
    for (std::size_t i = 1; i < m_Slots.size(); ++i) {
        if (m_Slots[i].record && !m_Slots[i].record->parentWidget.IsValid())
            roots.push_back(m_Slots[i].record->handle);
    }
    for (UIWidgetHandle root : roots) (void)Destroy(root, runtime);
    for (std::size_t i = 1; i < m_Slots.size(); ++i)
        if (m_Slots[i].record) (void)Destroy(m_Slots[i].record->handle, runtime);
}

UIWidgetConnectionHandle UIWidgetRegistry::AllocateConnection(
    UIWidgetHandle widget, ConnectionKind kind) {
    const UIWidgetRecord* record = TryGet(widget);
    if (!record || record->lifecycle != UIWidgetLifecycleState::Mounted ||
        record->invalidationQueued) return {};
    std::uint32_t index = 0;
    if (!m_FreeConnections.empty()) { index = m_FreeConnections.back(); m_FreeConnections.pop_back(); }
    else { index = static_cast<std::uint32_t>(m_Connections.size()); m_Connections.push_back({}); }
    ConnectionSlot& slot = m_Connections[index];
    slot = {};
    slot.generation = Next(g_ConnectionGeneration);
    slot.occupied = true; slot.widget = widget; slot.kind = kind;
    return {index, slot.generation};
}

UIWidgetConnectionHandle UIWidgetRegistry::Connect(UIWidgetHandle widget, UINodeHandle node,
    UIEventType eventType, UIEventPhaseMask phases, UIEventCallback callback) {
    UIWidgetRecord* record = TryGet(widget);
    if (!record || record->lifecycle != UIWidgetLifecycleState::Mounted || !callback) return {};
    const UIWidgetConnectionHandle connection = AllocateConnection(widget, ConnectionKind::Event);
    if (!connection.IsValid()) return {};
    UIEventListenerHandle listener = m_Runtime->AddEventListener(node, eventType, phases,
        [this, widget, callback = std::move(callback)](UIEventContext& context) mutable {
            const UIWidgetRecord* current = TryGet(widget);
            if (current && current->lifecycle == UIWidgetLifecycleState::Mounted &&
                !current->invalidationQueued) callback(context);
        });
    if (!listener.IsValid()) { (void)Disconnect(connection); return {}; }
    m_Connections[connection.index].listener = listener;
    return connection;
}

UIWidgetConnectionHandle UIWidgetRegistry::OnActivated(
    UIWidgetHandle widget, UIButtonActivatedCallback callback) {
    if (!callback) return {};
    const UIWidgetConnectionHandle result = AllocateConnection(widget, ConnectionKind::Activated);
    if (result.IsValid()) m_Connections[result.index].activated = std::move(callback);
    return result;
}
UIWidgetConnectionHandle UIWidgetRegistry::OnCheckedChanged(
    UIWidgetHandle widget, UIToggleCheckedChangedCallback callback) {
    if (!callback) return {};
    const UIWidgetConnectionHandle result = AllocateConnection(widget, ConnectionKind::CheckedChanged);
    if (result.IsValid()) m_Connections[result.index].checkedChanged = std::move(callback);
    return result;
}
UIWidgetConnectionHandle UIWidgetRegistry::OnScrollChanged(
    UIWidgetHandle widget, UIScrollChangedCallback callback) {
    if (!callback) return {};
    const UIWidgetConnectionHandle result = AllocateConnection(widget, ConnectionKind::ScrollChanged);
    if (result.IsValid()) m_Connections[result.index].scrollChanged = std::move(callback);
    return result;
}
UIWidgetConnectionHandle UIWidgetRegistry::OnScrollCompleted(
    UIWidgetHandle widget, UIScrollCompletedCallback callback) {
    if (!callback) return {};
    const UIWidgetConnectionHandle result = AllocateConnection(widget, ConnectionKind::ScrollCompleted);
    if (result.IsValid()) m_Connections[result.index].scrollCompleted = std::move(callback);
    return result;
}

bool UIWidgetRegistry::Disconnect(UIWidgetConnectionHandle handle) {
    if (!handle.IsValid() || handle.index >= m_Connections.size()) return false;
    ConnectionSlot& slot = m_Connections[handle.index];
    if (!slot.occupied || slot.generation != handle.generation) return false;
    if (slot.listener.IsValid()) (void)m_Runtime->RemoveEventListener(slot.listener);
    const std::uint32_t generation = slot.generation;
    slot = {}; slot.generation = generation;
    m_FreeConnections.push_back(handle.index);
    return true;
}
void UIWidgetRegistry::DisconnectAll(UIWidgetHandle widget) {
    std::vector<UIWidgetConnectionHandle> handles;
    for (std::size_t i = 1; i < m_Connections.size(); ++i) {
        const ConnectionSlot& slot = m_Connections[i];
        if (slot.occupied && slot.widget == widget)
            handles.push_back({static_cast<std::uint32_t>(i), slot.generation});
    }
    for (UIWidgetConnectionHandle handle : handles) (void)Disconnect(handle);
}

void UIWidgetRegistry::DispatchActivated(const UIButtonActivatedEvent& event) {
    const std::size_t limit = m_Connections.size();
    for (std::size_t i = 1; i < limit; ++i) {
        const ConnectionSlot& slot = m_Connections[i];
        if (!slot.occupied || slot.widget != event.widget || slot.kind != ConnectionKind::Activated) continue;
        const UIButtonActivatedCallback callback = slot.activated;
        if (callback) callback(event);
    }
}
void UIWidgetRegistry::DispatchCheckedChanged(UIWidgetHandle widget, bool checked) {
    const std::size_t limit = m_Connections.size();
    for (std::size_t i = 1; i < limit; ++i) {
        const ConnectionSlot& slot = m_Connections[i];
        if (!slot.occupied || slot.widget != widget || slot.kind != ConnectionKind::CheckedChanged) continue;
        const UIToggleCheckedChangedCallback callback = slot.checkedChanged;
        if (callback) callback(checked);
    }
}
void UIWidgetRegistry::DispatchScrollChanged(const UIScrollChangedEvent& event) {
    const std::size_t limit = m_Connections.size();
    for (std::size_t i = 1; i < limit; ++i) {
        const ConnectionSlot& slot = m_Connections[i];
        if (!slot.occupied || slot.widget != event.scrollView || slot.kind != ConnectionKind::ScrollChanged) continue;
        const UIScrollChangedCallback callback = slot.scrollChanged;
        if (callback) callback(event);
    }
}
void UIWidgetRegistry::DispatchScrollCompleted(const UIScrollCompletedEvent& event) {
    const std::size_t limit = m_Connections.size();
    for (std::size_t i = 1; i < limit; ++i) {
        const ConnectionSlot& slot = m_Connections[i];
        if (!slot.occupied || slot.widget != event.scrollView || slot.kind != ConnectionKind::ScrollCompleted) continue;
        const UIScrollCompletedCallback callback = slot.scrollCompleted;
        if (callback) callback(event);
    }
}

} // namespace Engine::UI2D
