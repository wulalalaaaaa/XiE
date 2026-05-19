#pragma once

#include "Assets/AssetManager.h"
#include "Assets/AtlasAsset.h"
#include "Assets/MaterialAsset.h"
#include "Assets/SpriteAsset.h"
#include "Assets/TextureAsset.h"
#include "Renderer/MeshAsset.h"

#include <cstdint>
#include <filesystem>

namespace Test2D {

struct AssetRuntime2DSnapshot {
    const Engine::MeshAsset* mesh = nullptr;
    const Engine::SpriteAsset* sprite = nullptr;
    const Engine::AtlasAsset* atlas = nullptr;
    const Engine::TextureAsset* texture = nullptr;
    const Engine::MaterialAsset* material = nullptr;
    std::filesystem::path meshPath;
    std::filesystem::path spritePath;
    std::filesystem::path atlasPath;
    std::filesystem::path texturePath;
    std::filesystem::path materialPath;
    std::uint64_t revision = 0;
};

class AssetRuntime2D {
public:
    void SetAssetRoot(const std::filesystem::path& assetRoot);
    bool Initialize();
    void TickHotReload();

    const AssetRuntime2DSnapshot& GetSnapshot() const;
    bool ChangedThisFrame() const;

private:
    bool LoadMesh();
    bool LoadMaterial();
    bool LoadSprite();
    bool LoadAtlas(const std::filesystem::path& path);
    bool LoadTexture(const std::filesystem::path& path);
    std::filesystem::path ResolveAtlasPath() const;
    std::filesystem::path ResolveTexturePath() const;
    void RefreshSnapshot();
    void MarkChanged();

private:
    std::filesystem::path m_AssetRoot = "Assets";
    std::filesystem::path m_MeshPath;
    std::filesystem::path m_TexturePath;
    std::filesystem::path m_MaterialPath;
    std::filesystem::path m_SpritePath;
    std::filesystem::path m_ResolvedAtlasPath;
    std::filesystem::path m_ResolvedTexturePath;

    Engine::AssetManager m_AssetManager;
    AssetRuntime2DSnapshot m_Snapshot{};
    bool m_Initialized = false;
    bool m_ChangedThisFrame = false;
};

} // namespace Test2D
