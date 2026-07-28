#pragma once

#include "Foundation/Math/Types2D.h"
#include "Input/InputEvents.h"
#include "UI2D/Core/UINodeHandle.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace Engine::UI2D {

class UIScene;

struct UIPressKey {
    Engine::PointerId pointerId = 0;
    Engine::PointerButton button = Engine::PointerButton::None;
    friend bool operator==(UIPressKey, UIPressKey) = default;
};

struct UIPressRecord {
    UINodeHandle pressedNode{};
    Engine::Vec2F downScenePosition{};
    double downTimestampSeconds = 0.0;
    bool canceled = false;
};

class UIPressTracker {
public:
    bool BeginPress(UIPressKey key, UINodeHandle node, Engine::Vec2F position, double timestamp);
    [[nodiscard]] const UIPressRecord* TryGet(UIPressKey key) const;
    [[nodiscard]] UIPressRecord* TryGet(UIPressKey key);
    bool Cancel(UIPressKey key);
    bool EndPress(UIPressKey key);
    void CancelPointer(Engine::PointerId pointerId);
    [[nodiscard]] std::vector<Engine::PointerId> ActivePointers() const;
    void OnNodeInvalidated(UINodeHandle node);
    std::vector<UINodeHandle> Sanitize(const UIScene& scene);
    void ClearAll();
    [[nodiscard]] bool IsPressed(UINodeHandle node, Engine::PointerButton button = Engine::PointerButton::None) const;
    [[nodiscard]] bool HasActivePress() const;

private:
    static std::uint64_t Key(UIPressKey key) noexcept;
    std::unordered_map<std::uint64_t, UIPressRecord> m_Presses;
};

} // namespace Engine::UI2D
