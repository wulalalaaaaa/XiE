#pragma once

#include "UI2D/Core/UINodeHandle.h"
#include "UI2D/Input/UIEventTypes.h"

#include <cstdint>
#include <functional>
#include <vector>

namespace Engine::UI2D {

class UIEventContext;

struct UIEventListenerHandle {
    std::uint32_t index = 0;
    std::uint32_t generation = 0;
    [[nodiscard]] bool IsValid() const noexcept { return index != 0 && generation != 0; }
    friend bool operator==(UIEventListenerHandle, UIEventListenerHandle) = default;
};

using UIEventCallback = std::function<void(UIEventContext&)>;

class UIEventListenerRegistry {
public:
    UIEventListenerHandle AddListener(
        UINodeHandle node,
        UIEventType eventType,
        UIEventPhaseMask phases,
        UIEventCallback callback);
    bool RemoveListener(UIEventListenerHandle handle);
    void RemoveAllForNode(UINodeHandle node);

    void BeginDispatch();
    void EndDispatch();
    [[nodiscard]] bool DispatchInProgress() const noexcept { return m_DispatchDepth != 0; }
    std::uint32_t Invoke(
        UINodeHandle node,
        UIEventType type,
        UIEventPhase phase,
        UIEventContext& context);

private:
    struct Slot {
        UINodeHandle node{};
        UIEventType type = UIEventType::PointerMove;
        UIEventPhaseMask phases = UIEventPhaseMask::None;
        UIEventCallback callback;
        std::uint32_t generation = 1;
        bool occupied = false;
        bool pendingRemoval = false;
    };

    void FlushPendingRemovals();

    std::vector<Slot> m_Slots;
    std::uint32_t m_DispatchDepth = 0;
    std::size_t m_DispatchSlotLimit = 0;
};

} // namespace Engine::UI2D
