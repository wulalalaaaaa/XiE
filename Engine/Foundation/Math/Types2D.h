#pragma once

namespace Engine {

struct Vec2F {
    float x = 0.0f;
    float y = 0.0f;

    friend constexpr bool operator==(Vec2F, Vec2F) = default;
};

struct RectF {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;

    friend constexpr bool operator==(RectF, RectF) = default;
};

struct InsetsF {
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;

    friend constexpr bool operator==(InsetsF, InsetsF) = default;
};

struct Color4f {
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;

    friend constexpr bool operator==(Color4f, Color4f) = default;
};

enum class AlphaMode {
    Straight,
    Premultiplied
};

} // namespace Engine
