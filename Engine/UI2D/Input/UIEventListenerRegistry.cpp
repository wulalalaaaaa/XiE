#include "UI2D/Input/UIEventListenerRegistry.h"

#include "UI2D/Input/UIEventContext.h"

#include <algorithm>

namespace Engine::UI2D {

UIEventListenerHandle UIEventListenerRegistry::AddListener(
    UINodeHandle node,
    UIEventType eventType,
    UIEventPhaseMask phases,
    UIEventCallback callback) {
    if (!node.IsValid() || phases == UIEventPhaseMask::None || !callback) return {};
    auto it = std::find_if(m_Slots.begin(), m_Slots.end(), [](const Slot& slot) {
        return !slot.occupied && !slot.pendingRemoval;
    });
    if (it == m_Slots.end()) {
        m_Slots.push_back({});
        it = m_Slots.end() - 1;
    }
    it->node = node;
    it->type = eventType;
    it->phases = phases;
    it->callback = std::move(callback);
    it->occupied = true;
    it->pendingRemoval = false;
    if (it->generation == 0) it->generation = 1;
    return {static_cast<std::uint32_t>(std::distance(m_Slots.begin(), it) + 1), it->generation};
}

bool UIEventListenerRegistry::RemoveListener(UIEventListenerHandle handle) {
    if (!handle.IsValid() || handle.index > m_Slots.size()) return false;
    Slot& slot = m_Slots[handle.index - 1];
    if (!slot.occupied || slot.generation != handle.generation || slot.pendingRemoval) return false;
    if (DispatchInProgress()) {
        slot.pendingRemoval = true;
    } else {
        slot = Slot{};
        slot.generation = handle.generation + 1;
        if (slot.generation == 0) slot.generation = 1;
    }
    return true;
}

void UIEventListenerRegistry::RemoveAllForNode(UINodeHandle node) {
    for (std::size_t i = 0; i < m_Slots.size(); ++i) {
        Slot& slot = m_Slots[i];
        if (!slot.occupied || slot.node != node) continue;
        (void)RemoveListener({static_cast<std::uint32_t>(i + 1), slot.generation});
    }
}

void UIEventListenerRegistry::BeginDispatch() {
    if (m_DispatchDepth++ == 0) m_DispatchSlotLimit = m_Slots.size();
}

void UIEventListenerRegistry::EndDispatch() {
    if (m_DispatchDepth == 0) return;
    if (--m_DispatchDepth == 0) FlushPendingRemovals();
}

std::uint32_t UIEventListenerRegistry::Invoke(
    UINodeHandle node,
    UIEventType type,
    UIEventPhase phase,
    UIEventContext& context) {
    std::uint32_t invoked = 0;
    const std::size_t limit = DispatchInProgress() ? std::min(m_DispatchSlotLimit, m_Slots.size()) : m_Slots.size();
    for (std::size_t i = 0; i < limit; ++i) {
        Slot& slot = m_Slots[i];
        if (!slot.occupied || slot.node != node || slot.type != type ||
            !IncludesPhase(slot.phases, phase)) continue;
        slot.callback(context);
        ++invoked;
        if (context.Event().immediatePropagationStopped) break;
    }
    return invoked;
}

void UIEventListenerRegistry::FlushPendingRemovals() {
    for (Slot& slot : m_Slots) {
        if (!slot.pendingRemoval) continue;
        const std::uint32_t next = slot.generation + 1 == 0 ? 1 : slot.generation + 1;
        slot = Slot{};
        slot.generation = next;
    }
}

} // namespace Engine::UI2D
