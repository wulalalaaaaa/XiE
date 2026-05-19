#include "MeshAsset.h"

#include "Core/Log.h"
#include "Utils/MemoryPool.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
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

bool ParseVertexValues(const std::string& line, std::vector<float>& outValues) {
    std::istringstream ss(line);
    float value = 0.0f;
    while (ss >> value) {
        outValues.push_back(value);
    }

    if (!ss.eof()) {
        return false;
    }

    return outValues.size() == 2 || outValues.size() == 3;
}

bool ParseUVValues(const std::string& line, float& outU, float& outV) {
    std::istringstream ss(line);
    if (!(ss >> outU >> outV)) {
        return false;
    }

    std::string extra;
    return !(ss >> extra);
}

bool ParseIntegerStrict(const std::string& token, int& outValue) {
    std::istringstream ss(token);
    ss >> outValue;
    return !(ss.fail() || !ss.eof());
}

struct FaceVertexIndex {
    unsigned int position = 0;
    bool hasUV = false;
    unsigned int uv = 0;
};

bool ParseFaceTokens(
    const std::string& line,
    bool oneBasedIndex,
    bool allowSlashToken,
    bool allowUVIndex,
    std::vector<FaceVertexIndex>& outFace
) {
    std::istringstream ss(line);
    std::string token;
    while (ss >> token) {
        std::string positionToken = token;
        std::string uvToken;
        if (allowSlashToken && token.find('/') != std::string::npos) {
            const std::size_t firstSlash = token.find('/');
            const std::size_t secondSlash = token.find('/', firstSlash + 1);

            positionToken = token.substr(0, firstSlash);
            if (secondSlash == std::string::npos) {
                uvToken = token.substr(firstSlash + 1);
            } else {
                uvToken = token.substr(firstSlash + 1, secondSlash - firstSlash - 1);
            }
        }

        if (positionToken.empty()) {
            return false;
        }

        int parsedPositionIndex = 0;
        if (!ParseIntegerStrict(positionToken, parsedPositionIndex)) {
            return false;
        }

        FaceVertexIndex faceVertex{};
        if (oneBasedIndex) {
            if (parsedPositionIndex <= 0) {
                return false;
            }
            faceVertex.position = static_cast<unsigned int>(parsedPositionIndex - 1);
        } else {
            if (parsedPositionIndex < 0) {
                return false;
            }
            faceVertex.position = static_cast<unsigned int>(parsedPositionIndex);
        }

        if (!uvToken.empty()) {
            if (!allowUVIndex) {
                return false;
            }

            int parsedUVIndex = 0;
            if (!ParseIntegerStrict(uvToken, parsedUVIndex)) {
                return false;
            }

            if (oneBasedIndex) {
                if (parsedUVIndex <= 0) {
                    return false;
                }
                faceVertex.uv = static_cast<unsigned int>(parsedUVIndex - 1);
            } else {
                if (parsedUVIndex < 0) {
                    return false;
                }
                faceVertex.uv = static_cast<unsigned int>(parsedUVIndex);
            }
            faceVertex.hasUV = true;
        }

        outFace.push_back(faceVertex);
    }

    return outFace.size() == 3 || outFace.size() == 4;
}

void AppendFaceAsTriangles(const std::vector<FaceVertexIndex>& face, std::vector<FaceVertexIndex>& outIndices) {
    if (face.size() == 3) {
        outIndices.insert(outIndices.end(), {face[0], face[1], face[2]});
        return;
    }

    if (face.size() == 4) {
        outIndices.insert(outIndices.end(), {face[0], face[1], face[2], face[0], face[2], face[3]});
    }
}

void PromoteVertices2DTo3D(std::vector<float>& ioVertices2D) {
    if (ioVertices2D.empty()) {
        return;
    }

    const std::size_t vertexCount = ioVertices2D.size() / 2;
    std::vector<float> promoted;
    promoted.reserve(vertexCount * 3);
    for (std::size_t i = 0; i < vertexCount; ++i) {
        promoted.push_back(ioVertices2D[2 * i]);
        promoted.push_back(ioVertices2D[2 * i + 1]);
        promoted.push_back(0.0f);
    }

    ioVertices2D.swap(promoted);
}

bool AppendVertexWithDimension(
    const std::vector<float>& parsedValues,
    int& ioVertexDimension,
    std::vector<float>& outVertices,
    std::string& outError
) {
    const int parsedDimension = static_cast<int>(parsedValues.size());
    if (parsedDimension != 2 && parsedDimension != 3) {
        outError = "vertex must have 2 or 3 components";
        return false;
    }

    if (ioVertexDimension == 0) {
        ioVertexDimension = parsedDimension;
    }

    if (ioVertexDimension == 2 && parsedDimension == 3) {
        PromoteVertices2DTo3D(outVertices);
        ioVertexDimension = 3;
    }

    if (ioVertexDimension == 3 && parsedDimension == 2) {
        outVertices.push_back(parsedValues[0]);
        outVertices.push_back(parsedValues[1]);
        outVertices.push_back(0.0f);
        return true;
    }

    if (parsedDimension != ioVertexDimension) {
        outError = "vertex dimension is inconsistent";
        return false;
    }

    outVertices.insert(outVertices.end(), parsedValues.begin(), parsedValues.end());
    return true;
}

bool ValidateMeshData(const std::vector<float>& vertices, int vertexDimension, const std::vector<unsigned int>& indices) {
    if (vertexDimension != 2 && vertexDimension != 3) {
        return false;
    }

    if (vertices.size() < static_cast<std::size_t>(vertexDimension * 3) || (vertices.size() % static_cast<std::size_t>(vertexDimension)) != 0) {
        return false;
    }

    if (indices.size() < 3 || (indices.size() % 3) != 0) {
        return false;
    }

    const unsigned int vertexCount = static_cast<unsigned int>(vertices.size() / static_cast<std::size_t>(vertexDimension));
    for (const unsigned int idx : indices) {
        if (idx >= vertexCount) {
            return false;
        }
    }

    return true;
}

bool ValidateUVData(const std::vector<float>& uvs, unsigned int vertexCount) {
    if (uvs.empty()) {
        return false;
    }
    return (uvs.size() % 2) == 0 && (uvs.size() / 2) == vertexCount;
}

bool BuildExpandedMeshWithExplicitUVs(
    const std::vector<float>& sourceVertices,
    int vertexDimension,
    const std::vector<float>& sourceUVs,
    const std::vector<FaceVertexIndex>& triangleVertices,
    std::vector<float>& outVertices,
    std::vector<float>& outUVs,
    std::vector<unsigned int>& outIndices,
    std::string& outError
) {
    struct PairHash {
        std::size_t operator()(const std::uint64_t key) const noexcept {
            return static_cast<std::size_t>(key);
        }
    };

    const unsigned int sourceVertexCount = static_cast<unsigned int>(sourceVertices.size() / static_cast<std::size_t>(vertexDimension));
    const unsigned int sourceUVCount = static_cast<unsigned int>(sourceUVs.size() / 2);

    std::unordered_map<std::uint64_t, unsigned int, PairHash> remap;
    remap.reserve(triangleVertices.size());

    outVertices.clear();
    outUVs.clear();
    outIndices.clear();
    outIndices.reserve(triangleVertices.size());

    const std::uint64_t positionShift = 32ULL;
    for (const FaceVertexIndex& fv : triangleVertices) {
        if (!fv.hasUV) {
            outError = "face mixes UV and non-UV indices";
            return false;
        }
        if (fv.position >= sourceVertexCount) {
            outError = "face position index out of range";
            return false;
        }
        if (fv.uv >= sourceUVCount) {
            outError = "face uv index out of range";
            return false;
        }

        const std::uint64_t key = (static_cast<std::uint64_t>(fv.position) << positionShift) | static_cast<std::uint64_t>(fv.uv);
        const auto it = remap.find(key);
        if (it != remap.end()) {
            outIndices.push_back(it->second);
            continue;
        }

        const unsigned int newIndex = static_cast<unsigned int>(outVertices.size() / static_cast<std::size_t>(vertexDimension));
        remap.emplace(key, newIndex);
        outIndices.push_back(newIndex);

        const std::size_t vertexBase = static_cast<std::size_t>(fv.position) * static_cast<std::size_t>(vertexDimension);
        for (int c = 0; c < vertexDimension; ++c) {
            outVertices.push_back(sourceVertices[vertexBase + static_cast<std::size_t>(c)]);
        }

        const std::size_t uvBase = static_cast<std::size_t>(fv.uv) * 2;
        outUVs.push_back(sourceUVs[uvBase]);
        outUVs.push_back(sourceUVs[uvBase + 1]);
    }

    return true;
}

bool ParseXMeshContent(
    const std::string& text,
    std::vector<float>& outVertices,
    int& outVertexDimension,
    std::vector<float>& outUVs,
    bool& outHasExplicitUV,
    std::vector<unsigned int>& outIndices,
    std::string& outError
) {
    std::istringstream stream(text);
    std::string line;
    bool sawHeader = false;
    bool sawVertexLine = false;
    std::vector<float> sourceVertices;
    std::vector<float> sourceUVs;
    std::vector<FaceVertexIndex> triangleVertices;
    outVertexDimension = 0;
    outHasExplicitUV = false;
    outUVs.clear();

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

        if (trimmed.rfind("vd", 0) == 0) {
            if (sawVertexLine) {
                outError = "xmesh vd line must appear before any vertex";
                return false;
            }

            const std::string dimToken = TrimCopy(trimmed.substr(2));
            int parsedDimension = 0;
            if (!ParseIntegerStrict(dimToken, parsedDimension) || (parsedDimension != 2 && parsedDimension != 3)) {
                outError = "xmesh vd line must be: vd 2 or vd 3";
                return false;
            }

            outVertexDimension = parsedDimension;
            continue;
        }

        if (trimmed.size() > 2 && trimmed[0] == 'v' && trimmed[1] == 't' && std::isspace(static_cast<unsigned char>(trimmed[2])) != 0) {
            float u = 0.0f;
            float v = 0.0f;
            if (!ParseUVValues(trimmed.substr(2), u, v)) {
                outError = "xmesh uv line must be: vt <u> <v>";
                return false;
            }
            sourceUVs.push_back(u);
            sourceUVs.push_back(v);
            continue;
        }

        if (StartsWithToken(trimmed, 'v')) {
            sawVertexLine = true;
            std::vector<float> values;
            if (!ParseVertexValues(trimmed.substr(1), values)) {
                outError = "xmesh vertex line must be: v <x> <y> [z]";
                return false;
            }

            if (outVertexDimension != 0 && static_cast<int>(values.size()) != outVertexDimension) {
                outError = "xmesh vertex dimension does not match vd";
                return false;
            }

            if (!AppendVertexWithDimension(values, outVertexDimension, sourceVertices, outError)) {
                return false;
            }

            continue;
        }

        if (StartsWithToken(trimmed, 'f')) {
            std::vector<FaceVertexIndex> face;
            if (!ParseFaceTokens(trimmed.substr(1), false, true, true, face)) {
                outError = "xmesh face line must be: f i[/ui] j[/uj] k[/uk] [l[/ul]], zero-based";
                return false;
            }

            bool hasUVInFace = false;
            bool hasNonUVInFace = false;
            for (const FaceVertexIndex& fv : face) {
                hasUVInFace = hasUVInFace || fv.hasUV;
                hasNonUVInFace = hasNonUVInFace || !fv.hasUV;
            }
            if (hasUVInFace && hasNonUVInFace) {
                outError = "xmesh face cannot mix UV and non-UV vertices";
                return false;
            }

            AppendFaceAsTriangles(face, triangleVertices);
            continue;
        }

        outError = "xmesh only supports lines: vd/v/vt/f/comments";
        return false;
    }

    if (!sawHeader) {
        outError = "xmesh header missing";
        return false;
    }

    if (outVertexDimension == 0) {
        outError = "xmesh requires at least one vertex";
        return false;
    }

    std::vector<unsigned int> sourceIndices;
    sourceIndices.reserve(triangleVertices.size());
    bool hasAnyUV = false;
    for (const FaceVertexIndex& fv : triangleVertices) {
        sourceIndices.push_back(fv.position);
        hasAnyUV = hasAnyUV || fv.hasUV;
    }

    if (!ValidateMeshData(sourceVertices, outVertexDimension, sourceIndices)) {
        outError = "xmesh data is incomplete or has invalid indices";
        return false;
    }

    if (hasAnyUV) {
        if ((sourceUVs.size() % 2) != 0 || sourceUVs.empty()) {
            outError = "xmesh has UV face indices but vt data is missing or invalid";
            return false;
        }

        if (!BuildExpandedMeshWithExplicitUVs(
                sourceVertices,
                outVertexDimension,
                sourceUVs,
                triangleVertices,
                outVertices,
                outUVs,
                outIndices,
                outError)) {
            return false;
        }

        const unsigned int vertexCount = static_cast<unsigned int>(outVertices.size() / static_cast<std::size_t>(outVertexDimension));
        if (!ValidateUVData(outUVs, vertexCount) || !ValidateMeshData(outVertices, outVertexDimension, outIndices)) {
            outError = "xmesh explicit uv data is invalid";
            return false;
        }

        outHasExplicitUV = true;
        return true;
    }

    outVertices = std::move(sourceVertices);
    outIndices = std::move(sourceIndices);
    outUVs.clear();
    outHasExplicitUV = false;
    return true;
}

bool ParseObjContent(
    const std::string& text,
    std::vector<float>& outVertices,
    int& outVertexDimension,
    std::vector<float>& outUVs,
    bool& outHasExplicitUV,
    std::vector<unsigned int>& outIndices,
    std::string& outError
) {
    std::istringstream stream(text);
    std::string line;
    std::vector<float> sourceVertices;
    std::vector<float> sourceUVs;
    std::vector<FaceVertexIndex> triangleVertices;
    outVertexDimension = 0;
    outHasExplicitUV = false;
    outUVs.clear();

    while (std::getline(stream, line)) {
        const std::string trimmed = TrimCopy(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }

        if (trimmed.size() > 2 && trimmed[0] == 'v' && trimmed[1] == 't' && std::isspace(static_cast<unsigned char>(trimmed[2])) != 0) {
            float u = 0.0f;
            float v = 0.0f;
            if (!ParseUVValues(trimmed.substr(2), u, v)) {
                outError = "obj uv line must be: vt u v";
                return false;
            }
            sourceUVs.push_back(u);
            sourceUVs.push_back(v);
            continue;
        }

        if (StartsWithToken(trimmed, 'v')) {
            std::vector<float> values;
            if (!ParseVertexValues(trimmed.substr(1), values)) {
                outError = "obj vertex line must be: v x y [z]";
                return false;
            }

            if (!AppendVertexWithDimension(values, outVertexDimension, sourceVertices, outError)) {
                return false;
            }

            continue;
        }

        if (StartsWithToken(trimmed, 'f')) {
            std::vector<FaceVertexIndex> face;
            if (!ParseFaceTokens(trimmed.substr(1), true, true, true, face)) {
                outError = "obj face line must be triangle or quad";
                return false;
            }

            bool hasUVInFace = false;
            bool hasNonUVInFace = false;
            for (const FaceVertexIndex& fv : face) {
                hasUVInFace = hasUVInFace || fv.hasUV;
                hasNonUVInFace = hasNonUVInFace || !fv.hasUV;
            }
            if (hasUVInFace && hasNonUVInFace) {
                outError = "obj face cannot mix UV and non-UV vertices";
                return false;
            }

            AppendFaceAsTriangles(face, triangleVertices);
            continue;
        }
    }

    std::vector<unsigned int> sourceIndices;
    sourceIndices.reserve(triangleVertices.size());
    bool hasAnyUV = false;
    for (const FaceVertexIndex& fv : triangleVertices) {
        sourceIndices.push_back(fv.position);
        hasAnyUV = hasAnyUV || fv.hasUV;
    }

    if (!ValidateMeshData(sourceVertices, outVertexDimension, sourceIndices)) {
        outError = "obj data is incomplete or has invalid indices";
        return false;
    }

    if (hasAnyUV) {
        if ((sourceUVs.size() % 2) != 0 || sourceUVs.empty()) {
            outError = "obj has UV face indices but vt data is missing or invalid";
            return false;
        }

        if (!BuildExpandedMeshWithExplicitUVs(
                sourceVertices,
                outVertexDimension,
                sourceUVs,
                triangleVertices,
                outVertices,
                outUVs,
                outIndices,
                outError)) {
            return false;
        }

        const unsigned int vertexCount = static_cast<unsigned int>(outVertices.size() / static_cast<std::size_t>(outVertexDimension));
        if (!ValidateUVData(outUVs, vertexCount) || !ValidateMeshData(outVertices, outVertexDimension, outIndices)) {
            outError = "obj explicit uv data is invalid";
            return false;
        }

        outHasExplicitUV = true;
        return true;
    }

    outVertices = std::move(sourceVertices);
    outIndices = std::move(sourceIndices);
    outUVs.clear();
    outHasExplicitUV = false;
    return true;
}

bool ParseLegacyTriangleContent(
    const std::string& text,
    std::vector<float>& outVertices,
    int& outVertexDimension,
    std::vector<float>& outUVs,
    bool& outHasExplicitUV,
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
    outVertexDimension = 2;
    outUVs.clear();
    outHasExplicitUV = false;
    outIndices = {0, 1, 2};
    return true;
}

} // namespace

MeshAsset::~MeshAsset() {
    ReleaseMeshData();
}

MeshAsset::MeshAsset(const MeshAsset& other) {
    if (other.m_Vertices == nullptr || other.m_Indices == nullptr || other.m_VertexFloatCount <= 0 || other.m_IndexCount <= 0) {
        return;
    }

    const std::vector<float> vertices(other.m_Vertices, other.m_Vertices + other.m_VertexFloatCount);
    const std::vector<float> uvs = (other.m_UVs != nullptr && other.m_UVFloatCount > 0)
        ? std::vector<float>(other.m_UVs, other.m_UVs + other.m_UVFloatCount)
        : std::vector<float>{};
    const std::vector<unsigned int> indices(other.m_Indices, other.m_Indices + other.m_IndexCount);
    if (!SetMeshData(vertices, other.m_VertexDimension, uvs, other.m_HasExplicitUVs, indices)) {
        XLOG_ERROR("MeshAsset copy construction failed");
    }
}

MeshAsset& MeshAsset::operator=(const MeshAsset& other) {
    if (this == &other) {
        return *this;
    }

    if (other.m_Vertices == nullptr || other.m_Indices == nullptr || other.m_VertexFloatCount <= 0 || other.m_IndexCount <= 0) {
        ReleaseMeshData();
        return *this;
    }

    const std::vector<float> vertices(other.m_Vertices, other.m_Vertices + other.m_VertexFloatCount);
    const std::vector<float> uvs = (other.m_UVs != nullptr && other.m_UVFloatCount > 0)
        ? std::vector<float>(other.m_UVs, other.m_UVs + other.m_UVFloatCount)
        : std::vector<float>{};
    const std::vector<unsigned int> indices(other.m_Indices, other.m_Indices + other.m_IndexCount);
    if (!SetMeshData(vertices, other.m_VertexDimension, uvs, other.m_HasExplicitUVs, indices)) {
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
    int vertexDimension = 0;
    std::vector<float> uvs;
    bool hasExplicitUVs = false;
    std::vector<unsigned int> indices;
    std::string error;

    const std::string ext = ToLowerCopy(path.extension().string());
    bool ok = false;

    if (ext == ".xmesh") {
        ok = ParseXMeshContent(content, vertices, vertexDimension, uvs, hasExplicitUVs, indices, error);
    } else if (ext == ".obj") {
        ok = ParseObjContent(content, vertices, vertexDimension, uvs, hasExplicitUVs, indices, error);
    } else {
        ok = ParseLegacyTriangleContent(content, vertices, vertexDimension, uvs, hasExplicitUVs, indices, error);
        if (!ok) {
            error.clear();
            ok = ParseObjContent(content, vertices, vertexDimension, uvs, hasExplicitUVs, indices, error);
        }
    }

    if (!ok) {
        const std::string message = "Failed to parse mesh file: " + path.string() + " (" + error + ")";
        XLOG_ERROR(message.c_str());
        return false;
    }

    if (!SetMeshData(vertices, vertexDimension, uvs, hasExplicitUVs, indices)) {
        XLOG_ERROR((std::string("Failed to allocate mesh data: ") + path.string()).c_str());
        return false;
    }

    return true;
}

const float* MeshAsset::GetVertices() const {
    return m_Vertices;
}

const float* MeshAsset::GetVertices2D() const {
    return (m_VertexDimension == 2) ? m_Vertices : nullptr;
}

int MeshAsset::GetVertexDimension() const {
    return m_VertexDimension;
}

int MeshAsset::GetVertexCount() const {
    if (m_VertexDimension <= 0) {
        return 0;
    }
    return m_VertexFloatCount / m_VertexDimension;
}

int MeshAsset::GetVertexFloatCount() const {
    return m_VertexFloatCount;
}

bool MeshAsset::HasExplicitUVs() const {
    return m_HasExplicitUVs;
}

const float* MeshAsset::GetUVs() const {
    return m_UVs;
}

int MeshAsset::GetUVCount() const {
    return m_UVFloatCount / 2;
}

const unsigned int* MeshAsset::GetIndices() const {
    return m_Indices;
}

int MeshAsset::GetIndexCount() const {
    return m_IndexCount;
}

bool MeshAsset::SetMeshData(
    const std::vector<float>& vertices,
    int vertexDimension,
    const std::vector<float>& uvs,
    bool hasExplicitUVs,
    const std::vector<unsigned int>& indices
) {
    if (!ValidateMeshData(vertices, vertexDimension, indices)) {
        return false;
    }
    const unsigned int vertexCount = static_cast<unsigned int>(vertices.size() / static_cast<std::size_t>(vertexDimension));
    if (hasExplicitUVs && !ValidateUVData(uvs, vertexCount)) {
        return false;
    }
    if (!hasExplicitUVs && !uvs.empty()) {
        return false;
    }

    ReleaseMeshData();

    FixedBlockMemoryPool& pool = GetMeshMemoryPool();

    const std::size_t vertexBytes = sizeof(float) * vertices.size();
    const std::size_t uvBytes = sizeof(float) * uvs.size();
    const std::size_t indexBytes = sizeof(unsigned int) * indices.size();

    void* vertexMemory = nullptr;
    void* uvMemory = nullptr;
    void* indexMemory = nullptr;

    try {
        vertexMemory = pool.Allocate(vertexBytes);
        if (uvBytes > 0) {
            uvMemory = pool.Allocate(uvBytes);
        }
        indexMemory = pool.Allocate(indexBytes);
    } catch (...) {
        if (vertexMemory != nullptr) {
            pool.Deallocate(vertexMemory, vertexBytes);
        }
        if (uvMemory != nullptr) {
            pool.Deallocate(uvMemory, uvBytes);
        }
        if (indexMemory != nullptr) {
            pool.Deallocate(indexMemory, indexBytes);
        }
        return false;
    }

    std::memcpy(vertexMemory, vertices.data(), vertexBytes);
    if (uvMemory != nullptr) {
        std::memcpy(uvMemory, uvs.data(), uvBytes);
    }
    std::memcpy(indexMemory, indices.data(), indexBytes);

    m_Vertices = static_cast<float*>(vertexMemory);
    m_VertexFloatCount = static_cast<int>(vertices.size());
    m_VertexDimension = vertexDimension;
    m_UVs = static_cast<float*>(uvMemory);
    m_UVFloatCount = static_cast<int>(uvs.size());
    m_HasExplicitUVs = hasExplicitUVs;
    m_Indices = static_cast<unsigned int*>(indexMemory);
    m_IndexCount = static_cast<int>(indices.size());
    return true;
}

void MeshAsset::ReleaseMeshData() noexcept {
    FixedBlockMemoryPool& pool = GetMeshMemoryPool();
    if (m_Vertices != nullptr) {
        pool.Deallocate(m_Vertices, sizeof(float) * static_cast<std::size_t>(m_VertexFloatCount));
        m_Vertices = nullptr;
        m_VertexFloatCount = 0;
        m_VertexDimension = 0;
    }

    if (m_UVs != nullptr) {
        pool.Deallocate(m_UVs, sizeof(float) * static_cast<std::size_t>(m_UVFloatCount));
        m_UVs = nullptr;
        m_UVFloatCount = 0;
    }
    m_HasExplicitUVs = false;

    if (m_Indices != nullptr) {
        pool.Deallocate(m_Indices, sizeof(unsigned int) * static_cast<std::size_t>(m_IndexCount));
        m_Indices = nullptr;
        m_IndexCount = 0;
    }
}

void MeshAsset::MoveFrom(MeshAsset&& other) noexcept {
    m_Vertices = other.m_Vertices;
    m_VertexFloatCount = other.m_VertexFloatCount;
    m_VertexDimension = other.m_VertexDimension;
    m_UVs = other.m_UVs;
    m_UVFloatCount = other.m_UVFloatCount;
    m_HasExplicitUVs = other.m_HasExplicitUVs;
    m_Indices = other.m_Indices;
    m_IndexCount = other.m_IndexCount;

    other.m_Vertices = nullptr;
    other.m_VertexFloatCount = 0;
    other.m_VertexDimension = 0;
    other.m_UVs = nullptr;
    other.m_UVFloatCount = 0;
    other.m_HasExplicitUVs = false;
    other.m_Indices = nullptr;
    other.m_IndexCount = 0;
}

} // namespace Engine
