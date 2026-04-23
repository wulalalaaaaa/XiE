#pragma once

#include <any>
#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>

namespace Engine {

enum class AssetType {
    Unknown = 0,
    Shader,
    Texture,
    Mesh,
};

enum class AssetStatus {
    Unloaded = 0,
    Loaded,
    Dirty,
    Failed,
};

struct AssetRecord {
    std::filesystem::path path;
    AssetType type = AssetType::Unknown;
    std::filesystem::file_time_type lastWriteTime{};
    AssetStatus status = AssetStatus::Unloaded;
    std::string failureReason;
    std::any handle;
};

class AssetManager {
public:
    template <typename T>
    bool Load(const std::filesystem::path& path, AssetType type, const std::function<bool(const std::filesystem::path&, T&)>& loader);

    template <typename T>
    bool ReloadIfChanged(const std::filesystem::path& path, const std::function<bool(const std::filesystem::path&, T&)>& loader);

    template <typename T>
    const T* GetHandle(const std::filesystem::path& path) const;

    const AssetRecord* GetRecord(const std::filesystem::path& path) const;
    const std::string* GetFailureReason(const std::filesystem::path& path) const;

private:
    static std::string ToKey(const std::filesystem::path& path);

private:
    std::unordered_map<std::string, AssetRecord> m_Records;
};

template <typename T>
bool AssetManager::Load(const std::filesystem::path& path, AssetType type, const std::function<bool(const std::filesystem::path&, T&)>& loader) {
    auto& record = m_Records[ToKey(path)];
    record.path = path;
    record.type = type;

    if (!std::filesystem::exists(path)) {
        record.status = AssetStatus::Failed;
        record.failureReason = "File does not exist";
        return false;
    }

    T loaded{};
    if (!loader(path, loaded)) {
        record.status = AssetStatus::Failed;
        record.failureReason = "Loader failed";
        return false;
    }

    record.lastWriteTime = std::filesystem::last_write_time(path);
    record.status = AssetStatus::Loaded;
    record.failureReason.clear();
    record.handle = std::move(loaded);
    return true;
}

template <typename T>
bool AssetManager::ReloadIfChanged(const std::filesystem::path& path, const std::function<bool(const std::filesystem::path&, T&)>& loader) {
    auto it = m_Records.find(ToKey(path));
    if (it == m_Records.end()) {
        return false;
    }

    AssetRecord& record = it->second;

    if (!std::filesystem::exists(path)) {
        record.status = AssetStatus::Failed;
        record.failureReason = "File does not exist";
        return false;
    }

    const auto now = std::filesystem::last_write_time(path);
    if (now == record.lastWriteTime) {
        return false;
    }

    record.status = AssetStatus::Dirty;

    T loaded{};
    if (!loader(path, loaded)) {
        record.status = AssetStatus::Failed;
        record.failureReason = "Reload failed";
        return false;
    }

    record.lastWriteTime = now;
    record.status = AssetStatus::Loaded;
    record.failureReason.clear();
    record.handle = std::move(loaded);
    return true;
}

template <typename T>
const T* AssetManager::GetHandle(const std::filesystem::path& path) const {
    const auto it = m_Records.find(ToKey(path));
    if (it == m_Records.end()) {
        return nullptr;
    }
    return std::any_cast<T>(&it->second.handle);
}

} // namespace Engine
