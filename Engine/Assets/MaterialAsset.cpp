#include "MaterialAsset.h"

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

bool ParseTintLine(const std::string& value, std::array<float, 4>& outTint) {
    std::istringstream ss(value);

    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;

    if (!(ss >> r >> g >> b)) {
        return false;
    }

    if (!(ss >> a)) {
        if (!ss.eof()) {
            return false;
        }
        a = 1.0f;
    } else {
        std::string extra;
        if (ss >> extra) {
            return false;
        }
    }

    outTint = {r, g, b, a};
    return true;
}

} // namespace

bool MaterialAsset::LoadFromFile(const std::filesystem::path& path) {
    m_DescriptorPath.clear();
    m_TexturePath.clear();
    m_HasTexture = false;
    m_Tint = {1.0f, 1.0f, 1.0f, 1.0f};

    if (!std::filesystem::exists(path)) {
        XLOG_ERROR((std::string("Material file not found: ") + path.string()).c_str());
        return false;
    }

    if (ToLowerCopy(path.extension().string()) != ".xmat") {
        XLOG_ERROR((std::string("Unsupported material extension: ") + path.string()).c_str());
        return false;
    }

    std::ifstream input(path);
    if (!input.is_open()) {
        XLOG_ERROR((std::string("Failed to open xmat file: ") + path.string()).c_str());
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
            if (trimmed != "xie_mat 1") {
                XLOG_ERROR("xmat header must be: xie_mat 1");
                return false;
            }
            sawHeader = true;
            continue;
        }

        if (StartsWithToken(trimmed, "texture")) {
            std::filesystem::path texturePath = TrimCopy(trimmed.substr(std::string("texture").size()));
            if (texturePath.empty()) {
                XLOG_ERROR("xmat texture line must be: texture <path>");
                return false;
            }

            if (texturePath.is_relative()) {
                texturePath = path.parent_path() / texturePath;
            }

            m_TexturePath = texturePath.lexically_normal();
            m_HasTexture = true;
            continue;
        }

        if (StartsWithToken(trimmed, "tint")) {
            if (!ParseTintLine(trimmed.substr(std::string("tint").size()), m_Tint)) {
                XLOG_ERROR("xmat tint line must be: tint <r> <g> <b> [a]");
                return false;
            }
            continue;
        }

        XLOG_ERROR("xmat only supports lines: texture/tint/comments");
        return false;
    }

    if (!sawHeader) {
        XLOG_ERROR("xmat header missing");
        return false;
    }

    m_DescriptorPath = path;
    return true;
}

bool MaterialAsset::HasTexture() const {
    return m_HasTexture;
}

const std::filesystem::path& MaterialAsset::GetTexturePath() const {
    return m_TexturePath;
}

const float* MaterialAsset::GetTint() const {
    return m_Tint.data();
}

const std::filesystem::path& MaterialAsset::GetDescriptorPath() const {
    return m_DescriptorPath;
}

} // namespace Engine
