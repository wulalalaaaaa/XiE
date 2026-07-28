#include "UI2D/Style/UIStyleDirtyQueue.h"

#include "UI2D/Core/UIScene.h"

namespace Engine::UI2D {

void UIStyleDirtyQueue::MarkDirty(UINodeHandle node) {
    if (node.IsValid() && m_Set.insert(node).second) m_Nodes.push_back(node);
}
void UIStyleDirtyQueue::MarkSubtreeDirty(const UIScene& scene, UINodeHandle root) {
    if (!scene.TryGet(root)) return;
    std::vector<UINodeHandle> stack{root};
    while (!stack.empty()) {
        const UINodeHandle node = stack.back(); stack.pop_back();
        MarkDirty(node);
        for (UINodeHandle child : scene.Children(node)) stack.push_back(child);
    }
}
void UIStyleDirtyQueue::MarkAllDirty(const UIScene& scene) {
    for (UINodeHandle node : scene.LiveNodes()) MarkDirty(node);
}
std::vector<UINodeHandle> UIStyleDirtyQueue::Consume() {
    std::vector<UINodeHandle> result;
    result.swap(m_Nodes);
    m_Set.clear();
    return result;
}
void UIStyleDirtyQueue::OnNodeInvalidated(UINodeHandle node) {
    if (!m_Set.erase(node)) return;
    std::erase(m_Nodes, node);
}

} // namespace Engine::UI2D
