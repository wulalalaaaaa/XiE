#pragma once

#include "DrawTypes2D.h"
#include "Foundation/Math/Matrix3.h"

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace Engine {

struct SpriteCommand {
    RectF dst{};
    RectF uv{0.0f, 0.0f, 1.0f, 1.0f};
    TextureHandle texture{};
    Color4f color{};
    AlphaMode alphaMode = AlphaMode::Straight;
    BlendMode blendMode = BlendMode::Alpha;
    Mat3F transform{};
};

struct SolidRectCommand {
    RectF rect{};
    Color4f color{};
    AlphaMode alphaMode = AlphaMode::Premultiplied;
    BlendMode blendMode = BlendMode::Alpha;
    Mat3F transform{};
};

struct RoundedRectCommand {
    RectF rect{};
    float radius = 0.0f;
    Color4f color{};
    AlphaMode alphaMode = AlphaMode::Premultiplied;
    BlendMode blendMode = BlendMode::Alpha;
    Mat3F transform{};
};

struct LineCommand {
    Vec2F from{};
    Vec2F to{};
    float thickness = 1.0f;
    Color4f color{};
    AlphaMode alphaMode = AlphaMode::Premultiplied;
    BlendMode blendMode = BlendMode::Alpha;
    Mat3F transform{};
};

struct CircleCommand {
    Vec2F center{};
    float radius = 1.0f;
    Color4f color{};
    AlphaMode alphaMode = AlphaMode::Premultiplied;
    BlendMode blendMode = BlendMode::Alpha;
    Mat3F transform{};
};

struct RingCommand {
    Vec2F center{};
    float radius = 1.0f;
    float thickness = 1.0f;
    Color4f color{};
    AlphaMode alphaMode = AlphaMode::Premultiplied;
    BlendMode blendMode = BlendMode::Alpha;
    Mat3F transform{};
};

struct NineSliceCommand {
    RectF dst{};
    RectF uv{0.0f, 0.0f, 1.0f, 1.0f};
    float left = 0.0f;
    float right = 0.0f;
    float top = 0.0f;
    float bottom = 0.0f;
    TextureHandle texture{};
    Color4f color{};
    AlphaMode alphaMode = AlphaMode::Premultiplied;
    BlendMode blendMode = BlendMode::Alpha;
    Mat3F transform{};
    float sourceLeftUv = 0.33333334f;
    float sourceRightUv = 0.33333334f;
    float sourceTopUv = 0.33333334f;
    float sourceBottomUv = 0.33333334f;
};

struct TextCommand {
    Vec2F position{};
    TextLayoutHandle layout{};
    Color4f color{};
    AlphaMode alphaMode = AlphaMode::Premultiplied;
    BlendMode blendMode = BlendMode::Alpha;
    Mat3F transform{};
};

struct PushClipRectCommand {
    RectF rect{};
};

struct PopClipCommand {};

struct CustomMeshCommand {
    std::vector<float> vertices;
    int vertexDimension = 2;
    std::vector<float> uvs;
    std::vector<std::uint32_t> indices;
    TextureHandle texture{};
    AlphaMode alphaMode = AlphaMode::Straight;
    BlendMode blendMode = BlendMode::Alpha;
    Mat3F transform{};
};

using DrawCommand2D = std::variant<
    SpriteCommand,
    SolidRectCommand,
    RoundedRectCommand,
    LineCommand,
    CircleCommand,
    RingCommand,
    NineSliceCommand,
    TextCommand,
    PushClipRectCommand,
    PopClipCommand,
    CustomMeshCommand>;

} // namespace Engine
