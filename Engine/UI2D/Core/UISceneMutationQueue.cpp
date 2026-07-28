#include "UI2D/Core/UISceneMutationQueue.h"

#include <type_traits>

namespace Engine::UI2D {

void UISceneMutationQueue::Enqueue(UISceneMutation mutation) {
    std::scoped_lock lock(m_Mutex);
    m_Pending.push_back(std::move(mutation));
}

UISceneMutationResult UISceneMutationQueue::Flush(UIScene& scene) {
    std::vector<UISceneMutation> pending;
    {
        std::scoped_lock lock(m_Mutex);
        pending.swap(m_Pending);
    }
    UISceneMutationResult result;
    for (std::size_t i = 0; i < pending.size(); ++i) {
        bool applied = std::visit([&](auto& mutation) -> bool {
            using T = std::decay_t<decltype(mutation)>;
            if constexpr (std::is_same_v<T, CreateNodeMutation>) {
                UINodeHandle created = scene.CreateNode(std::move(mutation.debugName), mutation.parent);
                if (created.IsValid()) result.createdNodes.push_back(created);
                return created.IsValid();
            } else if constexpr (std::is_same_v<T, DestroyNodeMutation>) {
                return scene.DestroyNode(mutation.node);
            } else if constexpr (std::is_same_v<T, ReparentNodeMutation>) {
                return scene.ReparentNode(mutation.node, mutation.parent);
            } else {
                UINodeRecord* node = scene.TryGet(mutation.node);
                if (!node) return false;
                if constexpr (std::is_same_v<T, SetVisibilityMutation>) {
                    return scene.SetVisibility(mutation.node, mutation.visible);
                } else if constexpr (std::is_same_v<T, SetEnabledMutation>) {
                    return scene.SetEnabled(mutation.node, mutation.enabled);
                } else if constexpr (std::is_same_v<T, SetVisualMutation>) {
                    return scene.SetVisual(mutation.node, std::move(mutation.visual));
                } else if constexpr (std::is_same_v<T, SetLayoutMutation>) {
                    return scene.SetLayout(mutation.node, mutation.layout);
                }
                return true;
            }
        }, pending[i]);
        if (applied) {
            ++result.appliedCount;
        } else {
            const bool relationship = std::holds_alternative<ReparentNodeMutation>(pending[i]);
            result.errors.push_back({i, relationship ? UISceneMutationErrorCode::RejectedRelationship : UISceneMutationErrorCode::InvalidHandle});
        }
    }
    return result;
}

bool UISceneMutationQueue::Empty() const { std::scoped_lock lock(m_Mutex); return m_Pending.empty(); }
std::size_t UISceneMutationQueue::Size() const { std::scoped_lock lock(m_Mutex); return m_Pending.size(); }

} // namespace Engine::UI2D
