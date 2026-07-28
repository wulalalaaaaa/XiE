#pragma once

namespace Engine::UI2D {

enum class UIHitShapeType {
    None,
    Rect,
    RoundedRect,
    Circle,
    Ring
};

// Shape parameters are expressed in node-local logical pixels. A non-positive
// Circle outerRadius uses half the node's shortest side. A Ring with both
// radii unspecified uses 50% and 100% of that default radius.
struct UIHitShape {
    UIHitShapeType type = UIHitShapeType::Rect;
    float cornerRadius = 0.0f;
    float innerRadius = 0.0f;
    float outerRadius = 0.0f;

    friend constexpr bool operator==(UIHitShape, UIHitShape) = default;
};

} // namespace Engine::UI2D
