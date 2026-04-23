#include "AssetManager.h"

#include <system_error>

namespace Engine {

const AssetRecord* AssetManager::GetRecord(const std::filesystem::path& path) const {
    const auto it = m_Records.find(ToKey(path));
    if (it == m_Records.end()) {
        return nullptr;
    }
    return &it->second;
}

const std::string* AssetManager::GetFailureReason(const std::filesystem::path& path) const {
    const AssetRecord* record = GetRecord(path);
    if (record == nullptr) {
        return nullptr;
    }

    return &record->failureReason;
}

std::string AssetManager::ToKey(const std::filesystem::path& path) {
    std::error_code ec;
    const auto canonical = std::filesystem::weakly_canonical(path, ec);
    if (!ec) {
        return canonical.generic_string();
    }
    return path.generic_string();
}

} // namespace Engine
