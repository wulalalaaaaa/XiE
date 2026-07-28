#pragma once

#include "StoryTypes.h"

#include <vector>

namespace Engine::Story {

class IStoryRuntime {
public:
    virtual ~IStoryRuntime() = default;

    virtual StoryRuntimeRole Role() const = 0;
    virtual StoryResult Start(
        const StoryStartRequest& request,
        const StoryCommandContext& context
    ) = 0;
    virtual StoryResult Apply(
        const StoryCommand& command,
        const StoryCommandContext& context
    ) = 0;
    virtual StoryResult Synchronize(const StorySessionState& state) = 0;
    virtual const StorySessionState* FindSession(StorySessionId sessionId) const = 0;
    virtual std::vector<StoryEvent> DrainEvents() = 0;
};

} // namespace Engine::Story
