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

    const float* GetVertices2D() const;
    int GetVertexFloatCount() const;
    const unsigned int* GetIndices() const;
    int GetIndexCount() const;

private:
    bool SetMeshData(const std::vector<float>& vertices2D, const std::vector<unsigned int>& indices);
    void ReleaseMeshData() noexcept;
    void MoveFrom(MeshAsset&& other) noexcept;

private:
    float* m_Vertices2D = nullptr;
    int m_VertexFloatCount = 0;
    unsigned int* m_Indices = nullptr;
    int m_IndexCount = 0;
};

} // namespace Engine
