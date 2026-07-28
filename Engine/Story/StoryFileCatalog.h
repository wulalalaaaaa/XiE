#pragma once

#include "IStoryCatalog.h"

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace Engine::Story {

class StoryFileCatalog final : public IStoryCatalog {
public:
    bool Load(const std::filesystem::path& rootPath);

    StoryContentVersion ContentVersion() const override;
    const StoryDefinition* FindStory(const StoryKey& key) const override;
    const StoryLine* FindLine(const StoryKey& key, StoryPosition position) const override;
    bool FirstPosition(const StoryKey& key, StoryPosition& outPosition) const override;
    bool NextPosition(
        const StoryKey& key,
        StoryPosition current,
        StoryPosition& outPosition
    ) const override;
    std::vector<StoryKey> StoryKeys() const override;

    const std::filesystem::path& RootPath() const;
    const std::string& LastError() const;

private:
    std::filesystem::path m_RootPath;
    std::map<StoryKey, StoryDefinition> m_Stories;
    StoryContentVersion m_ContentVersion = 0;
    std::string m_LastError;
};

} // namespace Engine::Story
