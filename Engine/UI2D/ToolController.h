#pragma once

#include "Foundation/Math/Types2D.h"
#include "Input/InputEvents.h"

#include <string>

namespace Engine::UI2D {

enum class CanvasTool {
    Select,
    Node,
    Connect,
    Pen,
    Eraser
};

enum class ToolActionType {
    None,
    SelectEntity,
    SelectStroke,
    BeginNode,
    BeginStroke,
    AppendStroke,
    EndStroke,
    EraseStroke,
    PanCanvas
};

struct ToolAction {
    ToolActionType type = ToolActionType::None;
    Engine::Vec2F screenPosition{};
    std::string targetId;
};

// Routes canvas pointer input in one place. In particular, Pen and Eraser
// never produce node-selection actions, so drawing cannot accidentally drag a
// graph node.
class ToolController {
public:
    void SetTool(CanvasTool tool) noexcept { m_Tool = tool; }
    [[nodiscard]] CanvasTool Tool() const noexcept { return m_Tool; }
    [[nodiscard]] bool Captured() const noexcept { return m_Captured; }

    ToolAction PointerDown(
        Engine::Vec2F screenPosition,
        Engine::PointerButton button,
        std::string hitEntityId = {},
        std::string hitStrokeId = {});
    ToolAction PointerMove(Engine::Vec2F screenPosition);
    ToolAction PointerUp(Engine::Vec2F screenPosition);
    void Cancel() noexcept { m_Captured = false; m_Panning = false; }

private:
    CanvasTool m_Tool = CanvasTool::Select;
    bool m_Captured = false;
    bool m_Panning = false;
};

} // namespace Engine::UI2D
