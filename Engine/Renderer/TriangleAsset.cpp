#include "TriangleAsset.h"

#include "Core/Log.h"

#include <fstream>
#include <sstream>

namespace Engine {

bool TriangleAsset::LoadFromFile(const std::filesystem::path& path) {
    m_Path = path;

    if (!std::filesystem::exists(m_Path)) {
        XLOG_ERROR((std::string("Triangle file not found: ") + m_Path.string()).c_str());
        return false;
    }

    m_LastWriteTime = std::filesystem::last_write_time(m_Path);

    std::ifstream input(m_Path);
    if (!input.is_open()) {
        XLOG_ERROR((std::string("Failed to open triangle file: ") + m_Path.string()).c_str());
        return false;
    }

    std::stringstream buffer;
    buffer << input.rdbuf();

    if (!ParseFile(buffer.str())) {
        XLOG_ERROR((std::string("Failed to parse triangle file: ") + m_Path.string()).c_str());
        return false;
    }

    XLOG_INFO((std::string("Triangle loaded from: ") + m_Path.string()).c_str());
    return true;
}

bool TriangleAsset::ReloadIfChanged() {
    if (m_Path.empty() || !std::filesystem::exists(m_Path)) {
        return false;
    }

    const auto currentWriteTime = std::filesystem::last_write_time(m_Path);
    if (currentWriteTime == m_LastWriteTime) {
        return false;
    }

    m_LastWriteTime = currentWriteTime;

    std::ifstream input(m_Path);
    if (!input.is_open()) {
        XLOG_ERROR((std::string("Hot reload failed to open: ") + m_Path.string()).c_str());
        return false;
    }

    std::stringstream buffer;
    buffer << input.rdbuf();

    if (!ParseFile(buffer.str())) {
        XLOG_ERROR((std::string("Hot reload parse failed: ") + m_Path.string()).c_str());
        return false;
    }

    XLOG_INFO((std::string("Triangle hot reloaded: ") + m_Path.string()).c_str());
    return true;
}

const std::array<float, 6>& TriangleAsset::GetVerticesNdc(int framebufferWidth, int framebufferHeight) const {
    if (framebufferWidth <= 0 || framebufferHeight <= 0) {
        return m_NdcVertices;
    }

    for (int i = 0; i < 3; ++i) {
        const float x = m_PixelVertices[2 * i + 0];
        const float y = m_PixelVertices[2 * i + 1];

        const float ndcX = (x / static_cast<float>(framebufferWidth)) * 2.0f - 1.0f;
        const float ndcY = 1.0f - (y / static_cast<float>(framebufferHeight)) * 2.0f;

        m_NdcVertices[2 * i + 0] = ndcX;
        m_NdcVertices[2 * i + 1] = ndcY;
    }

    return m_NdcVertices;
}

const std::filesystem::path& TriangleAsset::GetPath() const {
    return m_Path;
}

bool TriangleAsset::ParseFile(const std::string& content) {
    std::istringstream stream(content);
    std::string line;

    std::array<float, 6> parsed{};
    bool found[3] = {false, false, false};

    while (std::getline(stream, line)) {
        if (line.empty()) {
            continue;
        }

        if (line.rfind("v0:", 0) == 0) {
            found[0] = ParseLine(line.substr(3), parsed[0], parsed[1]);
        } else if (line.rfind("v1:", 0) == 0) {
            found[1] = ParseLine(line.substr(3), parsed[2], parsed[3]);
        } else if (line.rfind("v2:", 0) == 0) {
            found[2] = ParseLine(line.substr(3), parsed[4], parsed[5]);
        }
    }

    if (!(found[0] && found[1] && found[2])) {
        return false;
    }

    m_PixelVertices = parsed;
    return true;
}

bool TriangleAsset::ParseLine(const std::string& line, float& outX, float& outY) {
    std::istringstream ss(line);
    ss >> outX >> outY;
    return !ss.fail();
}

} // namespace Engine
