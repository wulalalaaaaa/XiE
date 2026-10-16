#pragma once

#include "Model/CrystalModels.h"
#include "Net/CrystalApiClient.h"
#include "Workspace/SpaceController.h"

#include <string>
#include <vector>

namespace Crystal {

class ProjectController {
public:
    explicit ProjectController(CrystalApiClient& api) : m_Api(api) {}

    [[nodiscard]] ApiResult RefreshProjects();
    [[nodiscard]] ApiResult SwitchProject(const std::string& projectId);
    [[nodiscard]] ApiResult SwitchSpace(const std::string& spaceId);
    [[nodiscard]] ApiResult ReloadCurrentWorkspace();
    void AdoptWorkspace(ProjectWorkspaceModel workspace);

    [[nodiscard]] const std::vector<ProjectModel>& Projects() const noexcept { return m_Projects; }
    [[nodiscard]] ProjectWorkspaceModel& Workspace() noexcept { return m_Workspace; }
    [[nodiscard]] const ProjectWorkspaceModel& Workspace() const noexcept { return m_Workspace; }
    [[nodiscard]] const ProjectModel& CurrentProject() const noexcept { return m_Workspace.project; }
    [[nodiscard]] const SpaceController& CurrentSpace() const noexcept { return m_Space; }
    [[nodiscard]] SpaceController& CurrentSpace() noexcept { return m_Space; }

private:
    CrystalApiClient& m_Api;
    std::vector<ProjectModel> m_Projects;
    ProjectWorkspaceModel m_Workspace;
    SpaceController m_Space;
};

} // namespace Crystal
