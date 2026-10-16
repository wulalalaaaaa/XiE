#pragma once

#include "UI2D/ToolController.h"

#include <string>

namespace Crystal {

struct CrystalTopBarState {
    std::string projectName;
    std::string spaceName;
    std::string connectionStatus = "Disconnected";
    Engine::UI2D::CanvasTool tool = Engine::UI2D::CanvasTool::Select;
    double zoom = 1.0;
};

} // namespace Crystal
