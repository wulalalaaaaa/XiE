#pragma once

#include <filesystem>
#include <vector>

namespace Engine {

class TextureAsset {
public:
    bool LoadFromFile(const std::filesystem::path& path);

    const unsigned char* GetPixels() const;
    int GetWidth() const;
    int GetHeight() const;
    int GetChannelCount() const;

    const std::filesystem::path& GetDescriptorPath() const;
    const std::filesystem::path& GetSourceImagePath() const;

private:
    bool LoadFromImageFile(const std::filesystem::path& imagePath);
    bool ParseXTextureFile(const std::filesystem::path& descriptorPath, std::filesystem::path& outImagePath);

private:
    std::filesystem::path m_DescriptorPath;
    std::filesystem::path m_SourceImagePath;
    std::vector<unsigned char> m_Pixels;
    int m_Width = 0;
    int m_Height = 0;
    int m_ChannelCount = 4;
};

} // namespace Engine
