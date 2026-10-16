#pragma once

#include "Model/CrystalModels.h"

#include <vector>

namespace Crystal {
struct ProjectSwitcher {
    std::vector<ProjectModel> projects;
    std::string selectedProjectId;
};
}
