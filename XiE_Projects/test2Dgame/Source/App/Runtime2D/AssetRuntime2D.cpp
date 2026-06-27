#include "AssetRuntime2D.h"

#include "Core/Log.h"
#include "World2D.h"

#include <functional>
#include <string>
#include <system_error>

namespace Test2D {

namespace {

std::string PathKey(const std::filesystem::path& path) {
    std::error_code ec;
    const std::filesystem::path canonical = std::filesystem::weakly_canonical(path, ec);
    if (!ec) {
        return canonical.generic_string();
    }
    return path.lexically_normal().generic_string();
}

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

const Engine::SpriteAsset* AssetRuntime2DSnapshot::FindSprite(const std::filesystem::path& path) const {
    const auto it = spritesByPath.find(PathKey(path));
    if (it == spritesByPath.end()) {
        return nullptr;
    }
    return it->second;
}

const Engine::AtlasAsset* AssetRuntime2DSnapshot::FindAtlas(const std::filesystem::path& path) const {
    const auto it = atlasesByPath.find(PathKey(path));
    if (it == atlasesByPath.end()) {
        return nullptr;
    }
    return it->second;
}

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
    (void)TrackSpritePath(m_SpritePath);

    bool loaded = true;
    loaded = LoadMesh() && loaded;
    loaded = LoadMaterial() && loaded;
    loaded = LoadSprite() && loaded;

    m_ResolvedAtlasPath = ResolveAtlasPath();
    if (!m_ResolvedAtlasPath.empty()) {
        (void)TrackAtlasPath(m_ResolvedAtlasPath);
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

    for (const std::filesystem::path& spritePath : m_TrackedSpritePaths) {
        if (spritePath == m_SpritePath) {
            continue;
        }

        if (m_AssetManager.GetHandle<Engine::SpriteAsset>(spritePath) == nullptr && std::filesystem::exists(spritePath)) {
            if (LoadSprite(spritePath)) {
                MarkChanged();
            }
        }

        if (ReloadAssetIfChanged<Engine::SpriteAsset>(
                m_AssetManager,
                spritePath,
                [](const std::filesystem::path& path, Engine::SpriteAsset& asset) {
                    return asset.LoadFromFile(path);
                })) {
            MarkChanged();
        }

        const Engine::SpriteAsset* sprite = m_AssetManager.GetHandle<Engine::SpriteAsset>(spritePath);
        if (sprite != nullptr && sprite->UsesAtlasRegion()) {
            if (TrackAtlasPath(sprite->GetAtlasPath()) && LoadAtlas(sprite->GetAtlasPath())) {
                MarkChanged();
            }
        }
    }

    const std::filesystem::path atlasPath = ResolveAtlasPath();
    if (atlasPath != m_ResolvedAtlasPath) {
        m_ResolvedAtlasPath = atlasPath;
        if (!m_ResolvedAtlasPath.empty()) {
            (void)TrackAtlasPath(m_ResolvedAtlasPath);
            (void)LoadAtlas(m_ResolvedAtlasPath);
        }
        MarkChanged();
    }

    for (const std::filesystem::path& atlasPath : m_TrackedAtlasPaths) {
        if (m_AssetManager.GetHandle<Engine::AtlasAsset>(atlasPath) == nullptr && std::filesystem::exists(atlasPath)) {
            if (LoadAtlas(atlasPath)) {
                MarkChanged();
            }
        }

        if (ReloadAssetIfChanged<Engine::AtlasAsset>(
                m_AssetManager,
                atlasPath,
                [](const std::filesystem::path& path, Engine::AtlasAsset& asset) {
                    return asset.LoadFromFile(path);
                })) {
            MarkChanged();
        }
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

    if (m_ChangedThisFrame) {
        std::string message = "AssetRuntime2D hot-reload revision=" + std::to_string(m_Snapshot.revision) +
            " mesh=" + m_Snapshot.meshPath.generic_string() +
            " sprite=" + m_Snapshot.spritePath.generic_string() +
            " atlas=" + m_Snapshot.atlasPath.generic_string() +
            " texture=" + m_Snapshot.texturePath.generic_string() +
            " material=" + m_Snapshot.materialPath.generic_string();
        XLOG_INFO(message.c_str());
    }
}

const AssetRuntime2DSnapshot& AssetRuntime2D::GetSnapshot() const {
    return m_Snapshot;
}

bool AssetRuntime2D::ChangedThisFrame() const {
    return m_ChangedThisFrame;
}

void AssetRuntime2D::EnsureSpriteRefs(const World2D& world) {
    bool changed = false;

    for (const Entity2D& entity : world.Entities()) {
        if (entity.sprite.spritePath == nullptr || entity.sprite.spritePath[0] == '\0') {
            continue;
        }

        const std::filesystem::path spritePath = std::filesystem::path(entity.sprite.spritePath).lexically_normal();
        if (!TrackSpritePath(spritePath)) {
            continue;
        }

        if (!LoadSprite(spritePath)) {
            const std::string message = "AssetRuntime2D: failed to load entity sprite: " + spritePath.generic_string();
            XLOG_WARN(message.c_str());
            continue;
        }

        changed = true;
        const Engine::SpriteAsset* sprite = m_AssetManager.GetHandle<Engine::SpriteAsset>(spritePath);
        if (sprite != nullptr && sprite->UsesAtlasRegion()) {
            const std::filesystem::path atlasPath = sprite->GetAtlasPath();
            if (TrackAtlasPath(atlasPath)) {
                if (!LoadAtlas(atlasPath)) {
                    const std::string message = "AssetRuntime2D: failed to load entity sprite atlas: " + atlasPath.generic_string();
                    XLOG_WARN(message.c_str());
                } else {
                    changed = true;
                }
            }
        }
    }

    if (changed) {
        MarkChanged();
        RefreshSnapshot();
    }
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
    return LoadSprite(m_SpritePath);
}

bool AssetRuntime2D::LoadSprite(const std::filesystem::path& path) {
    if (path.empty() || !std::filesystem::exists(path)) {
        return false;
    }

    return m_AssetManager.Load<Engine::SpriteAsset>(
        path,
        Engine::AssetType::Sprite,
        [](const std::filesystem::path& spritePath, Engine::SpriteAsset& asset) {
            return asset.LoadFromFile(spritePath);
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

bool AssetRuntime2D::TrackSpritePath(const std::filesystem::path& path) {
    if (path.empty()) {
        return false;
    }

    const std::string key = PathKey(path);
    if (!m_TrackedSpriteKeys.insert(key).second) {
        return false;
    }

    m_TrackedSpritePaths.push_back(path.lexically_normal());
    return true;
}

bool AssetRuntime2D::TrackAtlasPath(const std::filesystem::path& path) {
    if (path.empty()) {
        return false;
    }

    const std::string key = PathKey(path);
    if (!m_TrackedAtlasKeys.insert(key).second) {
        return false;
    }

    m_TrackedAtlasPaths.push_back(path.lexically_normal());
    return true;
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

    m_Snapshot.spritesByPath.clear();
    for (const std::filesystem::path& spritePath : m_TrackedSpritePaths) {
        const Engine::SpriteAsset* sprite = m_AssetManager.GetHandle<Engine::SpriteAsset>(spritePath);
        if (sprite != nullptr) {
            m_Snapshot.spritesByPath[PathKey(spritePath)] = sprite;
        }
    }

    m_Snapshot.atlasesByPath.clear();
    for (const std::filesystem::path& atlasPath : m_TrackedAtlasPaths) {
        const Engine::AtlasAsset* atlas = m_AssetManager.GetHandle<Engine::AtlasAsset>(atlasPath);
        if (atlas != nullptr) {
            m_Snapshot.atlasesByPath[PathKey(atlasPath)] = atlas;
        }
    }
}

void AssetRuntime2D::MarkChanged() {
    m_ChangedThisFrame = true;
    m_Snapshot.revision += 1;
}

} // namespace Test2D
