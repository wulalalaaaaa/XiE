#include "MeshAsset.h"

#include "Core/Log.h"
#include "Utils/MemoryPool.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

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

bool StartsWithToken(const std::string& value, char token) {
    return value.size() > 1 && value[0] == token && std::isspace(static_cast<unsigned char>(value[1])) != 0;
}

bool ParseTwoFloats(const std::string& line, float& outX, float& outY, bool allowTrailingValue) {
    std::istringstream ss(line);
    ss >> outX >> outY;
    if (ss.fail()) {
        return false;
    }

    std::string next;
    if (!(ss >> next)) {
        return true;
    }

    if (!allowTrailingValue) {
        return false;
    }

    return !(ss >> next);
}

bool ParseIntegerStrict(const std::string& token, int& outValue) {
    std::istringstream ss(token);
    ss >> outValue;
    return !(ss.fail() || !ss.eof());
}

bool ParseFaceTokens(const std::string& line, bool oneBasedIndex, bool allowSlashToken, std::vector<unsigned int>& outFace) {
    std::istringstream ss(line);
    std::string token;
    while (ss >> token) {
        std::string indexToken = token;
        if (allowSlashToken) {
            const std::size_t slashPos = token.find('/');
            indexToken = (slashPos == std::string::npos) ? token : token.substr(0, slashPos);
        }

        if (indexToken.empty()) {
            return false;
        }

        int parsedIndex = 0;
        if (!ParseIntegerStrict(indexToken, parsedIndex)) {
            return false;
        }

        if (oneBasedIndex) {
            if (parsedIndex <= 0) {
                return false;
            }
            outFace.push_back(static_cast<unsigned int>(parsedIndex - 1));
        } else {
            if (parsedIndex < 0) {
                return false;
            }
            outFace.push_back(static_cast<unsigned int>(parsedIndex));
        }
    }

    return outFace.size() == 3 || outFace.size() == 4;
}

void AppendFaceAsTriangles(const std::vector<unsigned int>& face, std::vector<unsigned int>& outIndices) {
    if (face.size() == 3) {
        outIndices.insert(outIndices.end(), {face[0], face[1], face[2]});
        return;
    }

    if (face.size() == 4) {
        outIndices.insert(outIndices.end(), {face[0], face[1], face[2], face[0], face[2], face[3]});
    }
}

bool ValidateMeshData(const std::vector<float>& vertices, const std::vector<unsigned int>& indices) {
    if (vertices.size() < 6 || (vertices.size() % 2) != 0) {
        return false;
    }

    if (indices.size() < 3 || (indices.size() % 3) != 0) {
        return false;
    }

    const unsigned int vertexCount = static_cast<unsigned int>(vertices.size() / 2);
    for (const unsigned int idx : indices) {
        if (idx >= vertexCount) {
            return false;
        }
    }

    return true;
}

bool ParseXMeshContent(
    const std::string& text,
    std::vector<float>& outVertices,
    std::vector<unsigned int>& outIndices,
    std::string& outError
) {
    std::istringstream stream(text);
    std::string line;
    bool sawHeader = false;

    while (std::getline(stream, line)) {
        const std::string trimmed = TrimCopy(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }

        if (!sawHeader) {
            if (trimmed != "xie_mesh 1") {
                outError = "xmesh header must be: xie_mesh 1";
                return false;
            }
            sawHeader = true;
            continue;
        }

        if (StartsWithToken(trimmed, 'v')) {
            float x = 0.0f;
            float y = 0.0f;
            if (!ParseTwoFloats(trimmed.substr(1), x, y, false)) {
                outError = "xmesh vertex line must be: v <x> <y>";
                return false;
            }
            outVertices.push_back(x);
            outVertices.push_back(y);
            continue;
        }

        if (StartsWithToken(trimmed, 'f')) {
            std::vector<unsigned int> face;
            if (!ParseFaceTokens(trimmed.substr(1), false, false, face)) {
                outError = "xmesh face line must be: f i j k [l], zero-based";
                return false;
            }
            AppendFaceAsTriangles(face, outIndices);
            continue;
        }

        outError = "xmesh only supports lines: v/f/comments";
        return false;
    }

    if (!sawHeader) {
        outError = "xmesh header missing";
        return false;
    }

    if (!ValidateMeshData(outVertices, outIndices)) {
        outError = "xmesh data is incomplete or has invalid indices";
        return false;
    }

    return true;
}

bool ParseObjContent(
    const std::string& text,
    std::vector<float>& outVertices,
    std::vector<unsigned int>& outIndices,
    std::string& outError
) {
    std::istringstream stream(text);
    std::string line;

    while (std::getline(stream, line)) {
        const std::string trimmed = TrimCopy(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }

        if (StartsWithToken(trimmed, 'v')) {
            float x = 0.0f;
            float y = 0.0f;
            if (!ParseTwoFloats(trimmed.substr(1), x, y, true)) {
                outError = "obj vertex line must be: v x y [z]";
                return false;
            }
            outVertices.push_back(x);
            outVertices.push_back(y);
            continue;
        }

        if (StartsWithToken(trimmed, 'f')) {
            std::vector<unsigned int> face;
            if (!ParseFaceTokens(trimmed.substr(1), true, true, face)) {
                outError = "obj face line must be triangle or quad";
                return false;
            }
            AppendFaceAsTriangles(face, outIndices);
            continue;
        }
    }

    if (!ValidateMeshData(outVertices, outIndices)) {
        outError = "obj data is incomplete or has invalid indices";
        return false;
    }

    return true;
}

bool ParseLegacyTriangleContent(
    const std::string& text,
    std::vector<float>& outVertices,
    std::vector<unsigned int>& outIndices,
    std::string& outError
) {
    std::istringstream stream(text);
    std::string line;
    std::array<float, 6> parsed{};
    bool found[3] = {false, false, false};

    while (std::getline(stream, line)) {
        const std::string trimmed = TrimCopy(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }

        if (trimmed.rfind("v0:", 0) == 0) {
            found[0] = ParseTwoFloats(trimmed.substr(3), parsed[0], parsed[1], false);
        } else if (trimmed.rfind("v1:", 0) == 0) {
            found[1] = ParseTwoFloats(trimmed.substr(3), parsed[2], parsed[3], false);
        } else if (trimmed.rfind("v2:", 0) == 0) {
            found[2] = ParseTwoFloats(trimmed.substr(3), parsed[4], parsed[5], false);
        }
    }

    if (!(found[0] && found[1] && found[2])) {
        outError = "legacy format requires v0/v1/v2 lines";
        return false;
    }

    outVertices.assign(parsed.begin(), parsed.end());
    outIndices = {0, 1, 2};
    return true;
}

} // namespace

MeshAsset::~MeshAsset() {
    ReleaseMeshData();
}

MeshAsset::MeshAsset(const MeshAsset& other) {
    if (other.m_Vertices2D == nullptr || other.m_Indices == nullptr || other.m_VertexFloatCount <= 0 || other.m_IndexCount <= 0) {
        return;
    }

    const std::vector<float> vertices(other.m_Vertices2D, other.m_Vertices2D + other.m_VertexFloatCount);
    const std::vector<unsigned int> indices(other.m_Indices, other.m_Indices + other.m_IndexCount);
    if (!SetMeshData(vertices, indices)) {
        XLOG_ERROR("MeshAsset copy construction failed");
    }
}

MeshAsset& MeshAsset::operator=(const MeshAsset& other) {
    if (this == &other) {
        return *this;
    }

    if (other.m_Vertices2D == nullptr || other.m_Indices == nullptr || other.m_VertexFloatCount <= 0 || other.m_IndexCount <= 0) {
        ReleaseMeshData();
        return *this;
    }

    const std::vector<float> vertices(other.m_Vertices2D, other.m_Vertices2D + other.m_VertexFloatCount);
    const std::vector<unsigned int> indices(other.m_Indices, other.m_Indices + other.m_IndexCount);
    if (!SetMeshData(vertices, indices)) {
        XLOG_ERROR("MeshAsset copy assignment failed");
    }

    return *this;
}

MeshAsset::MeshAsset(MeshAsset&& other) noexcept {
    MoveFrom(std::move(other));
}

MeshAsset& MeshAsset::operator=(MeshAsset&& other) noexcept {
    if (this != &other) {
        ReleaseMeshData();
        MoveFrom(std::move(other));
    }
    return *this;
}

bool MeshAsset::LoadFromFile(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        XLOG_ERROR((std::string("Mesh file not found: ") + path.string()).c_str());
        return false;
    }

    std::ifstream input(path);
    if (!input.is_open()) {
        XLOG_ERROR((std::string("Failed to open mesh file: ") + path.string()).c_str());
        return false;
    }

    std::stringstream buffer;
    buffer << input.rdbuf();
    const std::string content = buffer.str();

    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    std::string error;

    const std::string ext = ToLowerCopy(path.extension().string());
    bool ok = false;

    if (ext == ".xmesh") {
        ok = ParseXMeshContent(content, vertices, indices, error);
    } else if (ext == ".obj") {
        ok = ParseObjContent(content, vertices, indices, error);
    } else {
        ok = ParseLegacyTriangleContent(content, vertices, indices, error);
        if (!ok) {
            error.clear();
            ok = ParseObjContent(content, vertices, indices, error);
        }
    }

    if (!ok) {
        const std::string message = "Failed to parse mesh file: " + path.string() + " (" + error + ")";
        XLOG_ERROR(message.c_str());
        return false;
    }

    if (!SetMeshData(vertices, indices)) {
        XLOG_ERROR((std::string("Failed to allocate mesh data: ") + path.string()).c_str());
        return false;
    }

    return true;
}

const float* MeshAsset::GetVertices2D() const {
    return m_Vertices2D;
}

int MeshAsset::GetVertexFloatCount() const {
    return m_VertexFloatCount;
}

const unsigned int* MeshAsset::GetIndices() const {
    return m_Indices;
}

int MeshAsset::GetIndexCount() const {
    return m_IndexCount;
}

bool MeshAsset::SetMeshData(const std::vector<float>& vertices2D, const std::vector<unsigned int>& indices) {
    if (!ValidateMeshData(vertices2D, indices)) {
        return false;
    }

    ReleaseMeshData();

    FixedBlockMemoryPool& pool = GetMeshMemoryPool();

    const std::size_t vertexBytes = sizeof(float) * vertices2D.size();
    const std::size_t indexBytes = sizeof(unsigned int) * indices.size();

    void* vertexMemory = nullptr;
    void* indexMemory = nullptr;

    try {
        vertexMemory = pool.Allocate(vertexBytes);
        indexMemory = pool.Allocate(indexBytes);
    } catch (...) {
        if (vertexMemory != nullptr) {
            pool.Deallocate(vertexMemory, vertexBytes);
        }
        if (indexMemory != nullptr) {
            pool.Deallocate(indexMemory, indexBytes);
        }
        return false;
    }

    std::memcpy(vertexMemory, vertices2D.data(), vertexBytes);
    std::memcpy(indexMemory, indices.data(), indexBytes);

    m_Vertices2D = static_cast<float*>(vertexMemory);
    m_VertexFloatCount = static_cast<int>(vertices2D.size());
    m_Indices = static_cast<unsigned int*>(indexMemory);
    m_IndexCount = static_cast<int>(indices.size());
    return true;
}

void MeshAsset::ReleaseMeshData() noexcept {
    FixedBlockMemoryPool& pool = GetMeshMemoryPool();
    if (m_Vertices2D != nullptr) {
        pool.Deallocate(m_Vertices2D, sizeof(float) * static_cast<std::size_t>(m_VertexFloatCount));
        m_Vertices2D = nullptr;
        m_VertexFloatCount = 0;
    }

    if (m_Indices != nullptr) {
        pool.Deallocate(m_Indices, sizeof(unsigned int) * static_cast<std::size_t>(m_IndexCount));
        m_Indices = nullptr;
        m_IndexCount = 0;
    }
}

void MeshAsset::MoveFrom(MeshAsset&& other) noexcept {
    m_Vertices2D = other.m_Vertices2D;
    m_VertexFloatCount = other.m_VertexFloatCount;
    m_Indices = other.m_Indices;
    m_IndexCount = other.m_IndexCount;

    other.m_Vertices2D = nullptr;
    other.m_VertexFloatCount = 0;
    other.m_Indices = nullptr;
    other.m_IndexCount = 0;
}

} // namespace Engine
