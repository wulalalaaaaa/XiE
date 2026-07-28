#pragma once

#include "UI2D/Animation/IUIAnimator.h"
#include "UI2D/Input/UIInteractionState.h"
#include "UI2D/Theme/ResolvedUITheme.h"

namespace Engine::UI2D {

class UIScene;

struct UIStyleResolveContext {
    const ResolvedUITheme& theme;
    const UIInteractionSnapshot& interaction;
    bool windowFocused = true;
    bool forceImmediate = false;
};
struct UIStyleUpdateResult {
    std::uint32_t checkedNodeCount = 0;
    std::uint32_t changedNodeCount = 0;
    std::uint32_t animationsStarted = 0;
    bool transformInvalidated = false;
    bool visualInvalidated = false;
    bool hitTestInvalidated = false;
};

[[nodiscard]] UIInteractionStateMask ResolveInteractionState(
    const UIScene& scene, const UIInteractionSnapshot& snapshot, UINodeHandle node);
[[nodiscard]] bool MatchesStyleSelector(
    const UIStyleSelector& selector, UIStyleClassId styleClass, UIInteractionStateMask state) noexcept;
[[nodiscard]] UIResolvedStyle ResolveNodeStyle(
    const UIResolvedStyleClass& styleClass, UIStyleClassId id, UIInteractionStateMask state);

class IUIInteractionStyleResolver {
public:
    virtual ~IUIInteractionStyleResolver() = default;
    virtual UIStyleUpdateResult Update(UIScene& scene, const UIStyleResolveContext& context,
        IUIAnimator& animator) = 0;
    virtual void OnNodeInvalidated(UINodeHandle node) = 0;
};

} // namespace Engine::UI2D
