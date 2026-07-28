#pragma once

#include "StoryTypes.h"

#include <vector>

namespace Engine::Story {

class IStoryCatalog {
public:
    virtual ~IStoryCatalog() = default;

    virtual StoryContentVersion ContentVersion() const = 0;
    virtual const StoryDefinition* FindStory(const StoryKey& key) const = 0;
    virtual const StoryLine* FindLine(const StoryKey& key, StoryPosition position) const = 0;
    virtual bool FirstPosition(const StoryKey& key, StoryPosition& outPosition) const = 0;
    virtual bool NextPosition(
        const StoryKey& key,
        StoryPosition current,
        StoryPosition& outPosition
    ) const = 0;
    virtual std::vector<StoryKey> StoryKeys() const = 0;
};

} // namespace Engine::Story
