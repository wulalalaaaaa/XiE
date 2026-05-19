#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <unordered_map>

namespace Engine {

class AtlasAsset {
public:
    bool LoadFromFile(const std::filesystem::path& path);

    bool HasTexture() const;
    const std::filesystem::path& GetTexturePath() const;
    bool TryGetRegionUVRect(const std::string& regionName, std::array<float, 4>& outUVRect) const;
    const std::filesystem::path& GetDescriptorPath() const;

private:
    std::filesystem::path m_DescriptorPath;
    std::filesystem::path m_TexturePath;
    bool m_HasTexture = false;
    std::unordered_map<std::string, std::array<float, 4>> m_Regions;
};

} // namespace Engine

