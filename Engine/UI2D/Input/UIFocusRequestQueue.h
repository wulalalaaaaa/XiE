#pragma once

#include "UI2D/Input/UIFocusTypes.h"

#include <vector>

namespace Engine::UI2D {

class IUIFocusManager;
class UIScene;

class UIFocusRequestQueue {
public:
    void Enqueue(UIFocusRequest request);
    [[nodiscard]] bool Empty() const noexcept { return m_Pending.empty(); }
    [[nodiscard]] std::size_t Size() const noexcept { return m_Pending.size(); }
    UIFocusRequestResult Flush(
        UIScene& scene,
        IUIFocusManager& focusManager,
        UIFocusDispatchContext& context);
    void Clear() { m_Pending.clear(); }

private:
    std::vector<UIFocusRequest> m_Pending;
    bool m_FlushInProgress = false;
};

} // namespace Engine::UI2D
