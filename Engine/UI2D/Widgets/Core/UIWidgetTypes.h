#pragma once

#include "Renderer2D/DrawTypes2D.h"
#include "UI2D/Core/UITransform.h"
#include "UI2D/Core/UIVisual.h"
#include "UI2D/Layout/UILayoutTypes.h"
#include "UI2D/Style/UIStyleTypes.h"
#include "UI2D/Widgets/Core/UIWidgetHandle.h"

#include <cstdint>
#include <functional>

namespace Engine::UI2D {

enum class UIWidgetLifecycleState { Constructing, Mounted, Destroying, Destroyed };
enum class UIWidgetKind { Panel, Image, Text, Button, ToggleButton, ProgressBar, ScrollView, Scrollbar };

struct UIWidgetCommonProperties {
    bool visible = true;
    bool enabled = true;
    bool focusable = false;
    std::int32_t tabIndex = -1;
    UILayoutParams layout{};
    UITransform transform{};
    UIStyleClassId styleClass{};
};
struct UIPanelWidgetDesc {
    UIWidgetCommonProperties common;
    Engine::Color4f backgroundColor{};
    float cornerRadius = 0.0f;
    Engine::TextureHandle nineSliceTexture{};
    Engine::InsetsF nineSliceBorders{};
    bool clipChildren = false;
};
struct UIImageWidgetDesc {
    UIWidgetCommonProperties common;
    Engine::TextureHandle texture{};
    UIImageFit fit = UIImageFit::Contain;
    Engine::Color4f tint{};
};
struct UITextWidgetDesc {
    UIWidgetCommonProperties common;
    Engine::TextLayoutHandle textLayout{};
    Engine::Color4f color{};
    UITextHorizontalAlignment horizontalAlignment = UITextHorizontalAlignment::Start;
    UITextVerticalAlignment verticalAlignment = UITextVerticalAlignment::Start;
};
struct UIButtonBehaviorDesc {
    bool focusOnPointerDown = true;
    bool activateOnPointerClick = true;
    bool activateOnEnter = true;
    bool activateOnSpace = true;
};
struct UIButtonWidgetDesc {
    UIPanelWidgetDesc panel;
    UIButtonBehaviorDesc behavior;
    UIButtonWidgetDesc() {
        panel.common.focusable = true;
        panel.common.tabIndex = 0;
    }
};
struct UIToggleButtonWidgetDesc {
    UIButtonWidgetDesc button;
    bool checked = false;
    bool allowUncheck = true;
};
enum class UIProgressDirection { LeftToRight, RightToLeft, BottomToTop, TopToBottom };
struct UIProgressBarWidgetDesc {
    UIWidgetCommonProperties common;
    float value = 0.0f;
    float minimum = 0.0f;
    float maximum = 1.0f;
    UIStyleClassId backgroundStyle{};
    UIStyleClassId fillStyle{};
    Engine::Color4f backgroundColor{0.18f, 0.20f, 0.24f, 1.0f};
    Engine::Color4f fillColor{0.25f, 0.60f, 1.0f, 1.0f};
    UIProgressDirection direction = UIProgressDirection::LeftToRight;
};

struct UIButtonActivatedEvent {
    enum class Source { Pointer, KeyboardEnter, KeyboardSpace, Programmatic };
    UIWidgetHandle widget{};
    Source source = Source::Programmatic;
};
using UIButtonActivatedCallback = std::function<void(const UIButtonActivatedEvent&)>;
using UIToggleCheckedChangedCallback = std::function<void(bool)>;

struct UIWidgetUpdateResult {
    std::uint32_t appliedMutationCount = 0;
    std::uint32_t rejectedMutationCount = 0;
    std::uint32_t destroyedWidgetCount = 0;
    bool sceneChanged = false;
};

} // namespace Engine::UI2D
