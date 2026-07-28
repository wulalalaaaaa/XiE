#pragma once

#include "Renderer2D/DrawTypes2D.h"

#include <variant>

namespace Engine::UI2D {

struct UIPanelVisual {
    Engine::Color4f color{};
    float cornerRadius = 0.0f;
    bool useRoundedRect = false;
};

enum class UIImageFit { Stretch, Contain, Cover, None };
struct UIImageVisual {
    Engine::TextureHandle texture{};
    Engine::RectF uv{0, 0, 1, 1};
    Engine::Color4f tint{};
    UIImageFit fit = UIImageFit::Stretch;
    bool preserveAspectRatio = true;
};

enum class UITextHorizontalAlignment { Start, Center, End };
enum class UITextVerticalAlignment { Start, Center, End };
struct UITextVisual {
    Engine::TextLayoutHandle layout{};
    Engine::Color4f color{};
    UITextHorizontalAlignment horizontalAlignment = UITextHorizontalAlignment::Start;
    UITextVerticalAlignment verticalAlignment = UITextVerticalAlignment::Start;
};

struct UINineSliceVisual {
    Engine::TextureHandle texture{};
    Engine::RectF uv{0, 0, 1, 1};
    Engine::InsetsF borders{};
    Engine::Color4f tint{};
    Engine::InsetsF sourceBorders{};
    Engine::InsetsF destinationBorders{};
};

enum class UIShapeType { SolidRect, RoundedRect, Circle, Ring, Line };
struct UIShapeVisual {
    UIShapeType type = UIShapeType::SolidRect;
    Engine::Color4f color{};
    float radius = 0.0f;
    float thickness = 1.0f;
    Engine::Vec2F lineTo{};
    float cornerRadius = 0.0f;
    Engine::Vec2F lineStart{};
    Engine::Vec2F lineEnd{};
    float innerRadius = 0.0f;
    float outerRadius = 0.0f;
};

using UIVisual = std::variant<std::monostate, UIPanelVisual, UIImageVisual, UITextVisual, UINineSliceVisual, UIShapeVisual>;

} // namespace Engine::UI2D
