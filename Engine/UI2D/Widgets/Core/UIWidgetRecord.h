#pragma once

#include "UI2D/Core/UINodeHandle.h"
#include "UI2D/Widgets/Core/UIWidgetTypes.h"
#include "UI2D/Widgets/Scrolling/UIScrollViewBehavior.h"
#include "UI2D/Widgets/Scrolling/UIScrollbarBehavior.h"

#include <memory>
#include <vector>

namespace Engine::UI2D {

struct UIWidgetRecord {
    UIWidgetHandle handle{};
    UIWidgetHandle parentWidget{};
    std::vector<UIWidgetHandle> childWidgets;
    UINodeHandle rootNode{};
    UINodeHandle childHostNode{};
    std::vector<UINodeHandle> ownedNodes;
    UIWidgetKind kind = UIWidgetKind::Panel;
    UIWidgetLifecycleState lifecycle = UIWidgetLifecycleState::Constructing;
    bool visible = true;
    bool enabled = true;
    std::uint64_t creationOrder = 0;

    UIButtonBehaviorDesc buttonBehavior{};
    bool spacePressed = false;
    bool activationInProgress = false;
    bool invalidationQueued = false;
    bool checked = false;
    bool allowUncheck = true;

    float value = 0.0f;
    float minimum = 0.0f;
    float maximum = 1.0f;
    UIProgressDirection progressDirection = UIProgressDirection::LeftToRight;
    UINodeHandle progressBackground{};
    UINodeHandle progressFill{};

    std::unique_ptr<UIScrollViewState> scrollView;
    std::unique_ptr<UIScrollbarState> scrollbar;
};

} // namespace Engine::UI2D
