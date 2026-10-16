#pragma once

#include "Foundation/Math/Types2D.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Engine::UI2D {

enum class StrokeTool { Pen, Highlighter, Eraser };
enum class StrokeShape { Freehand, StraightLine, Arrow, Rectangle, Ellipse };

struct InkStroke {
    std::string id;
    std::vector<Engine::Vec2F> points;
    float width = 2.0f;
    StrokeTool tool = StrokeTool::Pen;
    StrokeShape shape = StrokeShape::Freehand;
    std::string color = "#9ed5ff";
    std::string layer = "Main";
    std::string groupId;
    // Optional entity anchor. Empty means free ink; localOffset stores the
    // entity position at attach time so moving the EntityView moves the ink.
    std::string anchorEntityId;
    std::string anchorBoardId;
    Engine::Vec2F localOffset{};
    std::string createdAt;
};

// A small command history suitable for local drawing. Network persistence is
// intentionally performed only after a stroke is complete.
class StrokeHistory {
public:
    void Add(InkStroke stroke);
    bool Remove(const std::string& id);
    bool Undo();
    bool Redo();
    void Clear() noexcept;

    [[nodiscard]] const std::vector<InkStroke>& Strokes() const noexcept { return m_Strokes; }
    [[nodiscard]] std::vector<InkStroke>& Strokes() noexcept { return m_Strokes; }

private:
    std::vector<InkStroke> m_Strokes;
    std::vector<InkStroke> m_Undo;
    std::vector<InkStroke> m_Redo;
};

} // namespace Engine::UI2D
