#pragma once

#include "World2D.h"

#include <filesystem>
#include <optional>
#include <string>

namespace Test2D {

class SceneRuntime2D {
public:
    void SetScenePath(const std::filesystem::path& scenePath);
    bool Initialize(World2D& world);
    bool TickHotReload(World2D& world);
    const std::string& LastError() const;

private:
    std::filesystem::path m_ScenePath{};
    std::optional<std::filesystem::file_time_type> m_LastWriteTime{};
    std::optional<std::filesystem::file_time_type> m_LastFailedWriteTime{};
    bool m_MissingReported = false;
    std::string m_LastError{};
};

} // namespace Test2D
