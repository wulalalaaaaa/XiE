#pragma once

#include <array>
#include <filesystem>
#include <string>

namespace Engine {

class SpriteAsset {
public:
    bool LoadFromFile(const std::filesystem::path& path);

    bool HasTexture() const;
    const std::filesystem::path& GetTexturePath() const;
    bool UsesAtlasRegion() const;
    const std::filesystem::path& GetAtlasPath() const;
    const std::string& GetAtlasRegionName() const;
    const float* GetUVRect() const;
    const float* GetPivot() const;
    float GetPixelsPerUnit() const;
    const std::filesystem::path& GetDescriptorPath() const;

private:
    std::filesystem::path m_DescriptorPath;
    std::filesystem::path m_TexturePath;
    std::filesystem::path m_AtlasPath;
    std::string m_AtlasRegionName;
    bool m_HasTexture = false;
    bool m_UsesAtlasRegion = false;
    std::array<float, 4> m_UVRect = {0.0f, 0.0f, 1.0f, 1.0f};
    std::array<float, 2> m_Pivot = {0.5f, 0.5f};
    float m_PixelsPerUnit = 100.0f;
};

} // namespace Engine
