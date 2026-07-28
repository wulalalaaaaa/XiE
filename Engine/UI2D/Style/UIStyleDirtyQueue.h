#pragma once

#include "UI2D/Core/UINodeHandle.h"

#include <span>
#include <unordered_set>
#include <vector>

namespace Engine::UI2D {

class UIScene;

class UIStyleDirtyQueue {
public:
    void MarkDirty(UINodeHandle node);
    void MarkSubtreeDirty(const UIScene& scene, UINodeHandle root);
    void MarkAllDirty(const UIScene& scene);
    std::vector<UINodeHandle> Consume();
    void OnNodeInvalidated(UINodeHandle node);
    [[nodiscard]] bool Empty() const noexcept { return m_Nodes.empty(); }
private:
    struct Hash {
        std::size_t operator()(UINodeHandle value) const noexcept {
            return static_cast<std::size_t>(value.index) * 16777619u ^ value.generation;
        }
    };
    std::vector<UINodeHandle> m_Nodes;
    std::unordered_set<UINodeHandle, Hash> m_Set;
};

} // namespace Engine::UI2D
