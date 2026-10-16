#include "UI2D/Input/UIPressTracker.h"

#include "UI2D/Core/UIScene.h"

#include <algorithm>

namespace Engine::UI2D {

std::uint64_t UIPressTracker::Key(UIPressKey key) noexcept {
    return (static_cast<std::uint64_t>(key.pointerId) << 8u) |
        static_cast<std::uint8_t>(key.button);
}

bool UIPressTracker::BeginPress(
    UIPressKey key, UINodeHandle node, UINodeHandle interactionOwner,
    bool hasInteractionOwner, Engine::Vec2F position, double timestamp) {
    if (!node.IsValid() || key.button == Engine::PointerButton::None) return false;
    if (!interactionOwner.IsValid()) interactionOwner = node;
    m_Presses[Key(key)] = {
        node, interactionOwner, hasInteractionOwner,
        position, timestamp, false};
    return true;
}

const UIPressRecord* UIPressTracker::TryGet(UIPressKey key) const {
    const auto it = m_Presses.find(Key(key));
    return it == m_Presses.end() ? nullptr : &it->second;
}

UIPressRecord* UIPressTracker::TryGet(UIPressKey key) {
    const auto it = m_Presses.find(Key(key));
    return it == m_Presses.end() ? nullptr : &it->second;
}

bool UIPressTracker::Cancel(UIPressKey key) {
    UIPressRecord* record = TryGet(key);
    if (!record || record->canceled) return false;
    record->canceled = true;
    return true;
}

bool UIPressTracker::EndPress(UIPressKey key) { return m_Presses.erase(Key(key)) != 0; }

void UIPressTracker::CancelPointer(Engine::PointerId pointerId) {
    for (auto& [key, record] : m_Presses) {
        if ((key >> 8u) == pointerId) record.canceled = true;
    }
}

std::vector<Engine::PointerId> UIPressTracker::ActivePointers() const {
    std::vector<Engine::PointerId> result;
    for (const auto& [key, record] : m_Presses) {
        (void)record;
        const auto id = static_cast<Engine::PointerId>(key >> 8u);
        if (std::find(result.begin(), result.end(), id) == result.end()) result.push_back(id);
    }
    return result;
}

void UIPressTracker::OnNodeInvalidated(UINodeHandle node) {
    for (auto it = m_Presses.begin(); it != m_Presses.end();) {
        if (it->second.pressedNode == node ||
            it->second.interactionOwner == node) {
            it = m_Presses.erase(it);
        } else ++it;
    }
}

std::vector<UINodeHandle> UIPressTracker::Sanitize(const UIScene& scene) {
    std::vector<UINodeHandle> canceled;
    for (auto& [key, record] : m_Presses) {
        (void)key;
        const UINodeRecord* node = scene.TryGet(record.pressedNode);
        const UINodeRecord* owner = scene.TryGet(record.interactionOwner);
        if (!node || !owner || !node->visible || !node->enabled ||
            !node->layoutState.effectiveVisible ||
            !node->layoutState.effectiveEnabled || !owner->visible ||
            !owner->enabled || !owner->layoutState.effectiveVisible ||
            !owner->layoutState.effectiveEnabled) {
            if (!record.canceled) canceled.push_back(record.interactionOwner);
            record.canceled = true;
        }
    }
    return canceled;
}

void UIPressTracker::ClearAll() { m_Presses.clear(); }

bool UIPressTracker::IsPressed(UINodeHandle node, Engine::PointerButton button) const {
    for (const auto& [key, record] : m_Presses) {
        if (!record.canceled && record.interactionOwner == node &&
            (button == Engine::PointerButton::None ||
             static_cast<std::uint8_t>(key & 0xffu) == static_cast<std::uint8_t>(button))) return true;
    }
    return false;
}

bool UIPressTracker::HasActivePress() const {
    return std::any_of(m_Presses.begin(), m_Presses.end(), [](const auto& entry) {
        return !entry.second.canceled;
    });
}

} // namespace Engine::UI2D
