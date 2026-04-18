#pragma once

#include <array>
#include <filesystem>
#include <string>

namespace Engine {

class TriangleAsset {
public:
    bool LoadFromFile(const std::filesystem::path& path);
    bool ReloadIfChanged();

    const std::array<float, 6>& GetVerticesNdc(int framebufferWidth, int framebufferHeight) const;
    const std::filesystem::path& GetPath() const;

private:
    bool ParseFile(const std::string& content);
    static bool ParseLine(const std::string& line, float& outX, float& outY);

private:
    std::filesystem::path m_Path;
    std::filesystem::file_time_type m_LastWriteTime{};
    std::array<float, 6> m_PixelVertices{100.0f, 100.0f, 300.0f, 100.0f, 200.0f, 300.0f};

    mutable std::array<float, 6> m_NdcVertices{};
};

} // namespace Engine
