#include "UI2D/ToolController.h"

namespace Engine::UI2D {

ToolAction ToolController::PointerDown(
    Engine::Vec2F screenPosition,
    Engine::PointerButton button,
    std::string hitEntityId,
    std::string hitStrokeId) {
    ToolAction action{ToolActionType::None, screenPosition, {}};
    if (button == Engine::PointerButton::Middle) {
        m_Captured = true;
        m_Panning = true;
        action.type = ToolActionType::PanCanvas;
        return action;
    }
    if (button != Engine::PointerButton::Primary) return action;

    switch (m_Tool) {
    case CanvasTool::Select:
        if (!hitEntityId.empty()) {
            m_Captured = true;
            action.type = ToolActionType::SelectEntity;
            action.targetId = std::move(hitEntityId);
        } else if (!hitStrokeId.empty()) {
            action.type = ToolActionType::SelectStroke;
            action.targetId = std::move(hitStrokeId);
        }
        break;
    case CanvasTool::Node:
        m_Captured = true;
        action.type = ToolActionType::BeginNode;
        break;
    case CanvasTool::Pen:
        m_Captured = true;
        action.type = ToolActionType::BeginStroke;
        break;
    case CanvasTool::Eraser:
        if (!hitStrokeId.empty()) {
            action.type = ToolActionType::EraseStroke;
            action.targetId = std::move(hitStrokeId);
        }
        break;
    case CanvasTool::Connect:
        // Connect uses the same entity hit routing as Select; the Crystal
        // project owns the relationship type/commit policy.
        if (!hitEntityId.empty()) {
            m_Captured = true;
            action.type = ToolActionType::SelectEntity;
            action.targetId = std::move(hitEntityId);
        }
        break;
    }
    return action;
}

ToolAction ToolController::PointerMove(Engine::Vec2F screenPosition) {
    if (!m_Captured) return {};
    ToolAction action{ToolActionType::None, screenPosition, {}};
    if (m_Panning) action.type = ToolActionType::PanCanvas;
    else if (m_Tool == CanvasTool::Pen) action.type = ToolActionType::AppendStroke;
    else if (m_Tool == CanvasTool::Select || m_Tool == CanvasTool::Node)
        action.type = ToolActionType::SelectEntity;
    else if (m_Tool == CanvasTool::Connect)
        action.type = ToolActionType::SelectEntity;
    else if (m_Tool == CanvasTool::Eraser) action.type = ToolActionType::EraseStroke;
    else action.type = ToolActionType::PanCanvas;
    return action;
}

ToolAction ToolController::PointerUp(Engine::Vec2F screenPosition) {
    if (!m_Captured) return {};
    m_Captured = false;
    if (m_Panning) {
        m_Panning = false;
        return {ToolActionType::None, screenPosition, {}};
    }
    ToolAction action{ToolActionType::EndStroke, screenPosition, {}};
    if (m_Tool != CanvasTool::Pen) action.type = ToolActionType::None;
    return action;
}

} // namespace Engine::UI2D
