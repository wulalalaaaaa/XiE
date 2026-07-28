#include "UI2D/Core/UINodeStore.h"

namespace Engine::UI2D {

UINodeHandle UINodeStore::Create(std::string debugName) {
    std::size_t slotIndex = 0;
    while (slotIndex < m_Slots.size() && m_Slots[slotIndex].occupied) ++slotIndex;
    if (slotIndex == m_Slots.size()) m_Slots.emplace_back();
    Slot& slot = m_Slots[slotIndex];
    if (slot.generation == 0) slot.generation = 1;
    slot.occupied = true;
    slot.record = {};
    slot.record.handle = {static_cast<std::uint32_t>(slotIndex + 1), slot.generation};
    slot.record.debugName = std::move(debugName);
    ++m_Size;
    return slot.record.handle;
}

bool UINodeStore::Destroy(UINodeHandle handle) {
    if (TryGet(handle) == nullptr) return false;
    Slot& slot = m_Slots[handle.index - 1];
    slot.record = {};
    slot.occupied = false;
    ++slot.generation;
    if (slot.generation == 0) slot.generation = 1;
    --m_Size;
    return true;
}

UINodeRecord* UINodeStore::TryGet(UINodeHandle handle) {
    return const_cast<UINodeRecord*>(static_cast<const UINodeStore*>(this)->TryGet(handle));
}

const UINodeRecord* UINodeStore::TryGet(UINodeHandle handle) const {
    if (!handle.IsValid() || handle.index > m_Slots.size()) return nullptr;
    const Slot& slot = m_Slots[handle.index - 1];
    if (!slot.occupied || slot.generation != handle.generation) return nullptr;
    return &slot.record;
}

std::vector<UINodeHandle> UINodeStore::LiveHandles() const {
    std::vector<UINodeHandle> handles;
    handles.reserve(m_Size);
    for (const Slot& slot : m_Slots) if (slot.occupied) handles.push_back(slot.record.handle);
    return handles;
}

} // namespace Engine::UI2D
