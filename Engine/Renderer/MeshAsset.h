#pragma once

#include <filesystem>
#include <vector>

namespace Engine {

class MeshAsset {
public:
    MeshAsset() = default;
    ~MeshAsset();

    MeshAsset(const MeshAsset& other);
    MeshAsset& operator=(const MeshAsset& other);

    MeshAsset(MeshAsset&& other) noexcept;
    MeshAsset& operator=(MeshAsset&& other) noexcept;

    bool LoadFromFile(const std::filesystem::path& path);

    const float* GetVertices() const;
    const float* GetVertices2D() const;
    int GetVertexDimension() const;
    int GetVertexCount() const;
    int GetVertexFloatCount() const;
    bool HasExplicitUVs() const;
    const float* GetUVs() const;
    int GetUVCount() const;
    const unsigned int* GetIndices() const;
    int GetIndexCount() const;

private:
    bool SetMeshData(
        const std::vector<float>& vertices,
        int vertexDimension,
        const std::vector<float>& uvs,
        bool hasExplicitUVs,
        const std::vector<unsigned int>& indices
    );
    void ReleaseMeshData() noexcept;
    void MoveFrom(MeshAsset&& other) noexcept;

private:
    float* m_Vertices = nullptr;
    int m_VertexFloatCount = 0;
    int m_VertexDimension = 0;
    float* m_UVs = nullptr;
    int m_UVFloatCount = 0;
    bool m_HasExplicitUVs = false;
    unsigned int* m_Indices = nullptr;
    int m_IndexCount = 0;
};

} // namespace Engine
