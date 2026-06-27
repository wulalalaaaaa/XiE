#pragma once

#include "World2D.h"

#include "Assets/AssetManager.h"
#include "Assets/AtlasAsset.h"
#include "Assets/MaterialAsset.h"
#include "Assets/SpriteAsset.h"
#include "Assets/TextureAsset.h"
#include "Renderer/MeshAsset.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

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
    std::unordered_map<std::string, const Engine::SpriteAsset*> spritesByPath;
    std::unordered_map<std::string, const Engine::AtlasAsset*> atlasesByPath;
    std::uint64_t revision = 0;

    const Engine::SpriteAsset* FindSprite(const std::filesystem::path& path) const;
    const Engine::AtlasAsset* FindAtlas(const std::filesystem::path& path) const;
};

class AssetRuntime2D {
public:
    void SetAssetRoot(const std::filesystem::path& assetRoot);
    bool Initialize();
    void TickHotReload();
    void EnsureSpriteRefs(const World2D& world);

    const AssetRuntime2DSnapshot& GetSnapshot() const;
    bool ChangedThisFrame() const;

private:
    bool LoadMesh();
    bool LoadMaterial();
    bool LoadSprite();
    bool LoadSprite(const std::filesystem::path& path);
    bool LoadAtlas(const std::filesystem::path& path);
    bool LoadTexture(const std::filesystem::path& path);
    bool TrackSpritePath(const std::filesystem::path& path);
    bool TrackAtlasPath(const std::filesystem::path& path);
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
    std::vector<std::filesystem::path> m_TrackedSpritePaths;
    std::vector<std::filesystem::path> m_TrackedAtlasPaths;
    std::unordered_set<std::string> m_TrackedSpriteKeys;
    std::unordered_set<std::string> m_TrackedAtlasKeys;
    bool m_Initialized = false;
    bool m_ChangedThisFrame = false;
};

} // namespace Test2D
