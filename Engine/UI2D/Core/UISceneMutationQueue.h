#pragma once

#include "UI2D/Core/UIScene.h"

#include <cstddef>
#include <mutex>
#include <string>
#include <variant>
#include <vector>

namespace Engine::UI2D {

struct CreateNodeMutation { std::string debugName; UINodeHandle parent{}; };
struct DestroyNodeMutation { UINodeHandle node{}; };
struct ReparentNodeMutation { UINodeHandle node{}; UINodeHandle parent{}; };
struct SetVisibilityMutation { UINodeHandle node{}; bool visible = true; };
struct SetEnabledMutation { UINodeHandle node{}; bool enabled = true; };
struct SetVisualMutation { UINodeHandle node{}; UIVisual visual{}; };
struct SetLayoutMutation { UINodeHandle node{}; UILayoutParams layout{}; };

using UISceneMutation = std::variant<
    CreateNodeMutation,
    DestroyNodeMutation,
    ReparentNodeMutation,
    SetVisibilityMutation,
    SetEnabledMutation,
    SetVisualMutation,
    SetLayoutMutation>;

enum class UISceneMutationErrorCode { InvalidHandle, RejectedRelationship };
struct UISceneMutationError { std::size_t mutationIndex = 0; UISceneMutationErrorCode code{}; };
struct UISceneMutationResult {
    std::size_t appliedCount = 0;
    std::vector<UINodeHandle> createdNodes;
    std::vector<UISceneMutationError> errors;
    [[nodiscard]] bool Succeeded() const noexcept { return errors.empty(); }
};

class UISceneMutationQueue {
public:
    void Enqueue(UISceneMutation mutation);
    UISceneMutationResult Flush(UIScene& scene);
    [[nodiscard]] bool Empty() const;
    [[nodiscard]] std::size_t Size() const;

private:
    mutable std::mutex m_Mutex;
    std::vector<UISceneMutation> m_Pending;
};

} // namespace Engine::UI2D
