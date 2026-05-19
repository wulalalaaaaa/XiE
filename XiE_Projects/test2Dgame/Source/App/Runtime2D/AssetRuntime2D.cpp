#include "AssetRuntime2D.h"

#include "Core/Log.h"

#include <functional>
#include <string>

namespace Test2D {

namespace {

template <typename TAsset>
bool ReloadAssetIfChanged(
    Engine::AssetManager& manager,
    const std::filesystem::path& path,
    const std::function<bool(const std::filesystem::path&, TAsset&)>& loader
) {
    if (path.empty() || !std::filesystem::exists(path)) {
        return false;
    }
    return manager.ReloadIfChanged<TAsset>(path, loader);
}

} // namespace

void AssetRuntime2D::SetAssetRoot(const std::filesystem::path& assetRoot) {
    if (assetRoot.empty()) {
        return;
    }
    m_AssetRoot = assetRoot;
}

bool AssetRuntime2D::Initialize() {
    m_MeshPath = m_AssetRoot / "Mesh" / "main.xmesh";
    m_TexturePath = m_AssetRoot / "Texture" / "main.xtexture";
    m_MaterialPath = m_AssetRoot / "Material" / "main.xmat";
    m_SpritePath = m_AssetRoot / "Sprite" / "main.xsprite";

    bool loaded = true;
    loaded = LoadMesh() && loaded;
    loaded = LoadMaterial() && loaded;
    loaded = LoadSprite() && loaded;

    m_ResolvedAtlasPath = ResolveAtlasPath();
    if (!m_ResolvedAtlasPath.empty()) {
        loaded = LoadAtlas(m_ResolvedAtlasPath) && loaded;
    }

    loaded = LoadTexture(m_TexturePath) && loaded;
    m_ResolvedTexturePath = ResolveTexturePath();
    if (!m_ResolvedTexturePath.empty() && m_ResolvedTexturePath != m_TexturePath) {
        loaded = LoadTexture(m_ResolvedTexturePath) && loaded;
    }

    m_Initialized = true;
    m_ChangedThisFrame = true;
    RefreshSnapshot();
    return loaded;
}

void AssetRuntime2D::TickHotReload() {
    if (!m_Initialized) {
        Initialize();
        return;
    }

    m_ChangedThisFrame = false;

    if (ReloadAssetIfChanged<Engine::MeshAsset>(
            m_AssetManager,
            m_MeshPath,
            [](const std::filesystem::path& path, Engine::MeshAsset& asset) {
                return asset.LoadFromFile(path);
            })) {
        MarkChanged();
    }

    if (ReloadAssetIfChanged<Engine::MaterialAsset>(
            m_AssetManager,
            m_MaterialPath,
            [](const std::filesystem::path& path, Engine::MaterialAsset& asset) {
                return asset.LoadFromFile(path);
            })) {
        MarkChanged();
    }

    if (!m_SpritePath.empty() && std::filesystem::exists(m_SpritePath) &&
        m_AssetManager.GetHandle<Engine::SpriteAsset>(m_SpritePath) == nullptr) {
        if (LoadSprite()) {
            MarkChanged();
        }
    }

    if (ReloadAssetIfChanged<Engine::SpriteAsset>(
            m_AssetManager,
            m_SpritePath,
            [](const std::filesystem::path& path, Engine::SpriteAsset& asset) {
                return asset.LoadFromFile(path);
            })) {
        MarkChanged();
    }

    const std::filesystem::path atlasPath = ResolveAtlasPath();
    if (atlasPath != m_ResolvedAtlasPath) {
        m_ResolvedAtlasPath = atlasPath;
        if (!m_ResolvedAtlasPath.empty()) {
            (void)LoadAtlas(m_ResolvedAtlasPath);
        }
        MarkChanged();
    }

    if (ReloadAssetIfChanged<Engine::AtlasAsset>(
            m_AssetManager,
            m_ResolvedAtlasPath,
            [](const std::filesystem::path& path, Engine::AtlasAsset& asset) {
                return asset.LoadFromFile(path);
            })) {
        MarkChanged();
    }

    if (ReloadAssetIfChanged<Engine::TextureAsset>(
            m_AssetManager,
            m_TexturePath,
            [](const std::filesystem::path& path, Engine::TextureAsset& asset) {
                return asset.LoadFromFile(path);
            })) {
        MarkChanged();
    }

    const std::filesystem::path resolvedTexturePath = ResolveTexturePath();
    if (resolvedTexturePath != m_ResolvedTexturePath) {
        m_ResolvedTexturePath = resolvedTexturePath;
        if (!m_ResolvedTexturePath.empty()) {
            (void)LoadTexture(m_ResolvedTexturePath);
        }
        MarkChanged();
    }

    if (ReloadAssetIfChanged<Engine::TextureAsset>(
            m_AssetManager,
            m_ResolvedTexturePath,
            [](const std::filesystem::path& path, Engine::TextureAsset& asset) {
                return asset.LoadFromFile(path);
            })) {
        MarkChanged();
    }

    RefreshSnapshot();
}

const AssetRuntime2DSnapshot& AssetRuntime2D::GetSnapshot() const {
    return m_Snapshot;
}

bool AssetRuntime2D::ChangedThisFrame() const {
    return m_ChangedThisFrame;
}

bool AssetRuntime2D::LoadMesh() {
    return m_AssetManager.Load<Engine::MeshAsset>(
        m_MeshPath,
        Engine::AssetType::Mesh,
        [](const std::filesystem::path& path, Engine::MeshAsset& asset) {
            return asset.LoadFromFile(path);
        });
}

bool AssetRuntime2D::LoadMaterial() {
    return m_AssetManager.Load<Engine::MaterialAsset>(
        m_MaterialPath,
        Engine::AssetType::Material,
        [](const std::filesystem::path& path, Engine::MaterialAsset& asset) {
            return asset.LoadFromFile(path);
        });
}

bool AssetRuntime2D::LoadSprite() {
    if (m_SpritePath.empty() || !std::filesystem::exists(m_SpritePath)) {
        return false;
    }

    return m_AssetManager.Load<Engine::SpriteAsset>(
        m_SpritePath,
        Engine::AssetType::Sprite,
        [](const std::filesystem::path& path, Engine::SpriteAsset& asset) {
            return asset.LoadFromFile(path);
        });
}

bool AssetRuntime2D::LoadAtlas(const std::filesystem::path& path) {
    if (path.empty()) {
        return false;
    }

    return m_AssetManager.Load<Engine::AtlasAsset>(
        path,
        Engine::AssetType::Atlas,
        [](const std::filesystem::path& atlasPath, Engine::AtlasAsset& asset) {
            return asset.LoadFromFile(atlasPath);
        });
}

bool AssetRuntime2D::LoadTexture(const std::filesystem::path& path) {
    if (path.empty()) {
        return false;
    }

    return m_AssetManager.Load<Engine::TextureAsset>(
        path,
        Engine::AssetType::Texture,
        [](const std::filesystem::path& texturePath, Engine::TextureAsset& asset) {
            return asset.LoadFromFile(texturePath);
        });
}

std::filesystem::path AssetRuntime2D::ResolveAtlasPath() const {
    if (m_SpritePath.empty()) {
        return {};
    }

    const Engine::SpriteAsset* sprite = m_AssetManager.GetHandle<Engine::SpriteAsset>(m_SpritePath);
    if (sprite == nullptr || !sprite->UsesAtlasRegion()) {
        return {};
    }

    return sprite->GetAtlasPath();
}

std::filesystem::path AssetRuntime2D::ResolveTexturePath() const {
    if (!m_ResolvedAtlasPath.empty()) {
        const Engine::AtlasAsset* atlas = m_AssetManager.GetHandle<Engine::AtlasAsset>(m_ResolvedAtlasPath);
        if (atlas != nullptr && atlas->HasTexture()) {
            return atlas->GetTexturePath();
        }
    }

    if (!m_SpritePath.empty()) {
        const Engine::SpriteAsset* sprite = m_AssetManager.GetHandle<Engine::SpriteAsset>(m_SpritePath);
        if (sprite != nullptr && sprite->HasTexture()) {
            return sprite->GetTexturePath();
        }
    }

    if (!m_MaterialPath.empty()) {
        const Engine::MaterialAsset* material = m_AssetManager.GetHandle<Engine::MaterialAsset>(m_MaterialPath);
        if (material != nullptr && material->HasTexture()) {
            return material->GetTexturePath();
        }
    }

    return m_TexturePath;
}

void AssetRuntime2D::RefreshSnapshot() {
    m_Snapshot.mesh = m_AssetManager.GetHandle<Engine::MeshAsset>(m_MeshPath);
    m_Snapshot.sprite = m_AssetManager.GetHandle<Engine::SpriteAsset>(m_SpritePath);
    m_Snapshot.atlas = m_ResolvedAtlasPath.empty() ? nullptr : m_AssetManager.GetHandle<Engine::AtlasAsset>(m_ResolvedAtlasPath);

    const std::filesystem::path activeTexturePath = m_ResolvedTexturePath.empty() ? m_TexturePath : m_ResolvedTexturePath;
    m_Snapshot.texture = activeTexturePath.empty() ? nullptr : m_AssetManager.GetHandle<Engine::TextureAsset>(activeTexturePath);
    m_Snapshot.material = m_AssetManager.GetHandle<Engine::MaterialAsset>(m_MaterialPath);

    m_Snapshot.meshPath = m_MeshPath;
    m_Snapshot.spritePath = m_SpritePath;
    m_Snapshot.atlasPath = m_ResolvedAtlasPath;
    m_Snapshot.texturePath = activeTexturePath;
    m_Snapshot.materialPath = m_MaterialPath;
}

void AssetRuntime2D::MarkChanged() {
    m_ChangedThisFrame = true;
    m_Snapshot.revision += 1;
}

} // namespace Test2D
