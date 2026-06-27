#include "SceneRuntime2D.h"

#include "SceneLoader2D.h"

#include <system_error>

namespace Test2D {

void SceneRuntime2D::SetScenePath(const std::filesystem::path& scenePath) {
    m_ScenePath = scenePath;
}

bool SceneRuntime2D::Initialize(World2D& world) {
    m_LastError.clear();
    m_LastFailedWriteTime.reset();
    m_MissingReported = false;

    if (m_ScenePath.empty()) {
        m_LastError = "scene path is empty";
        return false;
    }

    std::string loadError;
    if (!LoadScene2D(m_ScenePath, world, loadError)) {
        m_LastError = "initial scene load failed: " + loadError;
        return false;
    }

    std::error_code ec;
    const auto writeTime = std::filesystem::last_write_time(m_ScenePath, ec);
    if (ec) {
        m_LastWriteTime.reset();
    } else {
        m_LastWriteTime = writeTime;
    }

    return true;
}

bool SceneRuntime2D::TickHotReload(World2D& world) {
    if (m_ScenePath.empty()) {
        if (m_LastError != "scene path is empty") {
            m_LastError = "scene path is empty";
        }
        return false;
    }

    std::error_code existsEc;
    const bool exists = std::filesystem::exists(m_ScenePath, existsEc);
    if (existsEc) {
        const std::string message = "failed to check scene file existence: " + existsEc.message();
        if (m_LastError != message) {
            m_LastError = message;
        }
        return false;
    }

    if (!exists) {
        if (!m_MissingReported) {
            m_LastError = "scene file missing: " + m_ScenePath.generic_string();
            m_MissingReported = true;
        }
        return false;
    }
    m_MissingReported = false;

    std::error_code timeEc;
    const auto writeTime = std::filesystem::last_write_time(m_ScenePath, timeEc);
    if (timeEc) {
        const std::string message = "failed to get scene file timestamp: " + timeEc.message();
        if (m_LastError != message) {
            m_LastError = message;
        }
        return false;
    }

    if (!m_LastWriteTime.has_value()) {
        m_LastWriteTime = writeTime;
        return false;
    }

    if (writeTime == *m_LastWriteTime) {
        return false;
    }

    World2D loadedWorld{};
    std::string loadError;
    if (!LoadScene2D(m_ScenePath, loadedWorld, loadError)) {
        if (!m_LastFailedWriteTime.has_value() || writeTime != *m_LastFailedWriteTime) {
            m_LastError = "scene hot-reload failed: " + loadError;
            m_LastFailedWriteTime = writeTime;
        }
        return false;
    }

    world = std::move(loadedWorld);
    m_LastWriteTime = writeTime;
    m_LastFailedWriteTime.reset();
    m_LastError.clear();
    return true;
}

const std::string& SceneRuntime2D::LastError() const {
    return m_LastError;
}

} // namespace Test2D
