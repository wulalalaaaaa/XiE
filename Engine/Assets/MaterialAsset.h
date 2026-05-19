#pragma once

#include <array>
#include <filesystem>

namespace Engine {

class MaterialAsset {
public:
    bool LoadFromFile(const std::filesystem::path& path);

    bool HasTexture() const;
    const std::filesystem::path& GetTexturePath() const;
    const float* GetTint() const;
    const std::filesystem::path& GetDescriptorPath() const;

private:
    std::filesystem::path m_DescriptorPath;
    std::filesystem::path m_TexturePath;
    bool m_HasTexture = false;
    std::array<float, 4> m_Tint = {1.0f, 1.0f, 1.0f, 1.0f};
};

} // namespace Engine
