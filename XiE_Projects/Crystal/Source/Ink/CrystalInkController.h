#pragma once

#include "UI2D/InkStroke.h"

#include <string>
#include <utility>
#include <vector>

namespace Crystal {

class CrystalInkController {
public:
    void Load(std::vector<Engine::UI2D::InkStroke> strokes);
    void SetTool(Engine::UI2D::StrokeTool tool) noexcept { m_Tool = tool; }
    void SetShape(Engine::UI2D::StrokeShape shape) noexcept { m_Shape = shape; }
    void SetColor(std::string color) { m_Color = std::move(color); }
    void SetWidth(float width) noexcept { m_Width = width; }
    [[nodiscard]] Engine::UI2D::StrokeTool Tool() const noexcept { return m_Tool; }
    [[nodiscard]] Engine::UI2D::StrokeShape Shape() const noexcept { return m_Shape; }
    [[nodiscard]] const std::string& Color() const noexcept { return m_Color; }
    [[nodiscard]] float Width() const noexcept { return m_Width; }
    void BeginStroke(Engine::Vec2F worldPoint, float width, std::string layer);
    void AppendPoint(Engine::Vec2F worldPoint);
    [[nodiscard]] bool EndStroke(Engine::UI2D::InkStroke* completed = nullptr);
    [[nodiscard]] bool EraseStroke(const std::string& id);
    [[nodiscard]] bool AttachToEntity(
        const std::string& strokeId, const std::string& entityId, Engine::Vec2F entityPosition);
    [[nodiscard]] bool Undo() noexcept { return m_History.Undo(); }
    [[nodiscard]] bool Redo() noexcept { return m_History.Redo(); }
    [[nodiscard]] bool Drawing() const noexcept { return m_Drawing; }
    [[nodiscard]] const std::vector<Engine::UI2D::InkStroke>& Strokes() const noexcept {
        return m_History.Strokes();
    }

private:
    Engine::UI2D::StrokeHistory m_History;
    Engine::UI2D::InkStroke m_Active;
    Engine::UI2D::StrokeTool m_Tool = Engine::UI2D::StrokeTool::Pen;
    Engine::UI2D::StrokeShape m_Shape = Engine::UI2D::StrokeShape::Freehand;
    std::string m_Color = "#9ed5ff";
    float m_Width = 3.0f;
    bool m_Drawing = false;
};

} // namespace Crystal
