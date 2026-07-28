#pragma once

#include "Foundation/Math/Types2D.h"

namespace Engine::UI2D {

enum class UILayoutMode { Absolute, Anchor, HorizontalStack, VerticalStack, Overlay };
enum class UISizeMode { Fixed, Auto, Stretch };
enum class UIHorizontalAlignment { Start, Center, End, Stretch };
enum class UIVerticalAlignment { Start, Center, End, Stretch };

struct UILength {
    UISizeMode mode = UISizeMode::Auto;
    float value = 0.0f;

    friend constexpr bool operator==(UILength, UILength) = default;
};

struct UISizeRule {
    UILength width{};
    UILength height{};
    Engine::Vec2F minSize{};
    Engine::Vec2F maxSize{1000000.0f, 1000000.0f};

    friend constexpr bool operator==(UISizeRule, UISizeRule) = default;
};

struct UIAnchor {
    float minX = 0.0f;
    float minY = 0.0f;
    float maxX = 0.0f;
    float maxY = 0.0f;

    friend constexpr bool operator==(UIAnchor, UIAnchor) = default;
};

struct UILayoutParams {
    UILayoutMode mode = UILayoutMode::Absolute;
    UISizeRule sizeRule{};
    Engine::InsetsF margin{};
    Engine::InsetsF padding{};
    UIAnchor anchor{};
    Engine::Vec2F offsetMin{};
    Engine::Vec2F offsetMax{};
    float spacing = 0.0f;
    UIHorizontalAlignment horizontalAlignment = UIHorizontalAlignment::Start;
    UIVerticalAlignment verticalAlignment = UIVerticalAlignment::Start;

    friend constexpr bool operator==(UILayoutParams, UILayoutParams) = default;
};

} // namespace Engine::UI2D
