#include "Workspace/ProjectController.h"

#include <utility>

namespace Crystal {

ApiResult ProjectController::RefreshProjects() { return m_Api.ListProjects(m_Projects); }

ApiResult ProjectController::SwitchProject(const std::string& projectId) {
    ProjectWorkspaceModel next;
    const std::string spaceId = projectId + ":Overview";
    ApiResult result = m_Api.LoadWorkspace(projectId, spaceId, next);
    if (!result.ok) return result;
    // Commit the new workspace only after the complete request succeeded;
    // this prevents project A's graph/ink/camera from leaking into project B.
    m_Workspace = std::move(next);
    if (!m_Workspace.currentSpace.id.empty()) m_Space.Load(m_Workspace.currentSpace);
    return result;
}

ApiResult ProjectController::SwitchSpace(const std::string& spaceId) {
    if (m_Workspace.project.id.empty()) return {false, 0, {}, "no_project_selected"};
    ProjectWorkspaceModel next;
    ApiResult result = m_Api.LoadWorkspace(m_Workspace.project.id, spaceId, next);
    if (!result.ok) return result;
    m_Workspace = std::move(next);
    if (!m_Workspace.currentSpace.id.empty()) m_Space.Load(m_Workspace.currentSpace);
    return result;
}

ApiResult ProjectController::ReloadCurrentWorkspace() {
    if (m_Workspace.project.id.empty() || m_Space.Current().id.empty())
        return {false, 0, {}, "no_project_selected"};
    ProjectWorkspaceModel next;
    ApiResult result = m_Api.LoadWorkspace(m_Workspace.project.id, m_Space.Current().id, next);
    if (!result.ok) return result;
    m_Workspace = std::move(next);
    if (!m_Workspace.currentSpace.id.empty()) m_Space.Load(m_Workspace.currentSpace);
    return result;
}

void ProjectController::AdoptWorkspace(ProjectWorkspaceModel workspace) {
    m_Workspace = std::move(workspace);
    if (!m_Workspace.currentSpace.id.empty()) m_Space.Load(m_Workspace.currentSpace);
}

} // namespace Crystal
