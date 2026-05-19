#include "AtlasAsset.h"

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

} // namespace

bool AtlasAsset::LoadFromFile(const std::filesystem::path& path) {
    m_DescriptorPath.clear();
    m_TexturePath.clear();
    m_HasTexture = false;
    m_Regions.clear();

    if (!std::filesystem::exists(path)) {
        XLOG_ERROR((std::string("Atlas file not found: ") + path.string()).c_str());
        return false;
    }

    if (ToLowerCopy(path.extension().string()) != ".xatlas") {
        XLOG_ERROR((std::string("Unsupported atlas extension: ") + path.string()).c_str());
        return false;
    }

    std::ifstream input(path);
    if (!input.is_open()) {
        XLOG_ERROR((std::string("Failed to open xatlas file: ") + path.string()).c_str());
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
            if (trimmed != "xie_atlas 1") {
                XLOG_ERROR("xatlas header must be: xie_atlas 1");
                return false;
            }
            sawHeader = true;
            continue;
        }

        if (StartsWithToken(trimmed, "texture")) {
            std::filesystem::path texturePath = TrimCopy(trimmed.substr(std::string("texture").size()));
            if (texturePath.empty()) {
                XLOG_ERROR("xatlas texture line must be: texture <path>");
                return false;
            }

            if (texturePath.is_relative()) {
                texturePath = path.parent_path() / texturePath;
            }

            m_TexturePath = texturePath.lexically_normal();
            m_HasTexture = true;
            continue;
        }

        if (StartsWithToken(trimmed, "region")) {
            std::istringstream ss(trimmed.substr(std::string("region").size()));
            std::string regionName;
            float uMin = 0.0f;
            float vMin = 0.0f;
            float uMax = 1.0f;
            float vMax = 1.0f;
            if (!(ss >> regionName >> uMin >> vMin >> uMax >> vMax)) {
                XLOG_ERROR("xatlas region line must be: region <name> <u_min> <v_min> <u_max> <v_max>");
                return false;
            }
            std::string extra;
            if (ss >> extra) {
                XLOG_ERROR("xatlas region line has extra tokens");
                return false;
            }
            if (regionName.empty()) {
                XLOG_ERROR("xatlas region name cannot be empty");
                return false;
            }

            m_Regions[regionName] = {uMin, vMin, uMax, vMax};
            continue;
        }

        XLOG_ERROR("xatlas only supports lines: texture/region/comments");
        return false;
    }

    if (!sawHeader) {
        XLOG_ERROR("xatlas header missing");
        return false;
    }

    m_DescriptorPath = path;
    return true;
}

bool AtlasAsset::HasTexture() const {
    return m_HasTexture;
}

const std::filesystem::path& AtlasAsset::GetTexturePath() const {
    return m_TexturePath;
}

bool AtlasAsset::TryGetRegionUVRect(const std::string& regionName, std::array<float, 4>& outUVRect) const {
    const auto it = m_Regions.find(regionName);
    if (it == m_Regions.end()) {
        return false;
    }

    outUVRect = it->second;
    return true;
}

const std::filesystem::path& AtlasAsset::GetDescriptorPath() const {
    return m_DescriptorPath;
}

} // namespace Engine

