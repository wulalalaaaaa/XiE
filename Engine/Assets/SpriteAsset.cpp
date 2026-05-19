#include "SpriteAsset.h"

#include "Core/Log.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>

namespace Engine {

namespace {

std::string TrimCopy(const std::string& value) {
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return {};
    }

    const auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

std::string ToLowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool StartsWithToken(const std::string& value, const std::string& token) {
    if (value.size() <= token.size()) {
        return false;
    }

    if (value.rfind(token, 0) != 0) {
        return false;
    }

    return std::isspace(static_cast<unsigned char>(value[token.size()])) != 0;
}

bool ParseFloatCount(const std::string& value, int expectedCount, float* outValues) {
    std::istringstream ss(value);
    for (int i = 0; i < expectedCount; ++i) {
        if (!(ss >> outValues[i])) {
            return false;
        }
    }

    std::string extra;
    return !(ss >> extra);
}

} // namespace

bool SpriteAsset::LoadFromFile(const std::filesystem::path& path) {
    m_DescriptorPath.clear();
    m_TexturePath.clear();
    m_AtlasPath.clear();
    m_AtlasRegionName.clear();
    m_HasTexture = false;
    m_UsesAtlasRegion = false;
    m_UVRect = {0.0f, 0.0f, 1.0f, 1.0f};
    m_Pivot = {0.5f, 0.5f};
    m_PixelsPerUnit = 100.0f;

    if (!std::filesystem::exists(path)) {
        XLOG_ERROR((std::string("Sprite file not found: ") + path.string()).c_str());
        return false;
    }

    if (ToLowerCopy(path.extension().string()) != ".xsprite") {
        XLOG_ERROR((std::string("Unsupported sprite extension: ") + path.string()).c_str());
        return false;
    }

    std::ifstream input(path);
    if (!input.is_open()) {
        XLOG_ERROR((std::string("Failed to open xsprite file: ") + path.string()).c_str());
        return false;
    }

    std::stringstream buffer;
    buffer << input.rdbuf();
    const std::string content = buffer.str();

    std::istringstream stream(content);
    std::string line;
    bool sawHeader = false;

    while (std::getline(stream, line)) {
        const std::string trimmed = TrimCopy(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }

        if (!sawHeader) {
            if (trimmed != "xie_sprite 1") {
                XLOG_ERROR("xsprite header must be: xie_sprite 1");
                return false;
            }
            sawHeader = true;
            continue;
        }

        if (StartsWithToken(trimmed, "texture")) {
            std::filesystem::path texturePath = TrimCopy(trimmed.substr(std::string("texture").size()));
            if (texturePath.empty()) {
                XLOG_ERROR("xsprite texture line must be: texture <path>");
                return false;
            }

            if (texturePath.is_relative()) {
                texturePath = path.parent_path() / texturePath;
            }

            m_TexturePath = texturePath.lexically_normal();
            m_HasTexture = true;
            continue;
        }

        if (StartsWithToken(trimmed, "atlas")) {
            std::filesystem::path atlasPath = TrimCopy(trimmed.substr(std::string("atlas").size()));
            if (atlasPath.empty()) {
                XLOG_ERROR("xsprite atlas line must be: atlas <path>");
                return false;
            }

            if (atlasPath.is_relative()) {
                atlasPath = path.parent_path() / atlasPath;
            }

            m_AtlasPath = atlasPath.lexically_normal();
            continue;
        }

        if (StartsWithToken(trimmed, "region")) {
            const std::string regionName = TrimCopy(trimmed.substr(std::string("region").size()));
            if (regionName.empty()) {
                XLOG_ERROR("xsprite region line must be: region <name>");
                return false;
            }
            m_AtlasRegionName = regionName;
            continue;
        }

        if (StartsWithToken(trimmed, "uv_rect")) {
            float values[4] = {};
            if (!ParseFloatCount(trimmed.substr(std::string("uv_rect").size()), 4, values)) {
                XLOG_ERROR("xsprite uv_rect line must be: uv_rect <u_min> <v_min> <u_max> <v_max>");
                return false;
            }
            m_UVRect = {values[0], values[1], values[2], values[3]};
            continue;
        }

        if (StartsWithToken(trimmed, "pivot")) {
            float values[2] = {};
            if (!ParseFloatCount(trimmed.substr(std::string("pivot").size()), 2, values)) {
                XLOG_ERROR("xsprite pivot line must be: pivot <x> <y>");
                return false;
            }
            m_Pivot = {values[0], values[1]};
            continue;
        }

        if (StartsWithToken(trimmed, "pixels_per_unit")) {
            float ppu = 0.0f;
            if (!ParseFloatCount(trimmed.substr(std::string("pixels_per_unit").size()), 1, &ppu) || ppu <= 0.0f) {
                XLOG_ERROR("xsprite pixels_per_unit line must be: pixels_per_unit <positive-value>");
                return false;
            }
            m_PixelsPerUnit = ppu;
            continue;
        }

        XLOG_ERROR("xsprite only supports lines: texture/atlas/region/uv_rect/pivot/pixels_per_unit/comments");
        return false;
    }

    if (!sawHeader) {
        XLOG_ERROR("xsprite header missing");
        return false;
    }

    if ((!m_AtlasPath.empty() && m_AtlasRegionName.empty()) || (m_AtlasPath.empty() && !m_AtlasRegionName.empty())) {
        XLOG_ERROR("xsprite atlas binding requires both lines: atlas <path> and region <name>");
        return false;
    }

    m_UsesAtlasRegion = !m_AtlasPath.empty() && !m_AtlasRegionName.empty();
    m_DescriptorPath = path;
    return true;
}

bool SpriteAsset::HasTexture() const {
    return m_HasTexture;
}

const std::filesystem::path& SpriteAsset::GetTexturePath() const {
    return m_TexturePath;
}

bool SpriteAsset::UsesAtlasRegion() const {
    return m_UsesAtlasRegion;
}

const std::filesystem::path& SpriteAsset::GetAtlasPath() const {
    return m_AtlasPath;
}

const std::string& SpriteAsset::GetAtlasRegionName() const {
    return m_AtlasRegionName;
}

const float* SpriteAsset::GetUVRect() const {
    return m_UVRect.data();
}

const float* SpriteAsset::GetPivot() const {
    return m_Pivot.data();
}

float SpriteAsset::GetPixelsPerUnit() const {
    return m_PixelsPerUnit;
}

const std::filesystem::path& SpriteAsset::GetDescriptorPath() const {
    return m_DescriptorPath;
}

} // namespace Engine
