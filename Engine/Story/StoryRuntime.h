#pragma once

#include "IStoryCatalog.h"
#include "IStoryRuntime.h"

#include <map>
#include <set>
#include <utility>

namespace Engine::Story {

class StoryRuntime final : public IStoryRuntime {
public:
    StoryRuntime(const IStoryCatalog& catalog, StoryRuntimeRole role);

    StoryRuntimeRole Role() const override;
    StoryResult Start(
        const StoryStartRequest& request,
        const StoryCommandContext& context
    ) override;
    StoryResult Apply(
        const StoryCommand& command,
        const StoryCommandContext& context
    ) override;
    StoryResult Synchronize(const StorySessionState& state) override;
    const StorySessionState* FindSession(StorySessionId sessionId) const override;
    std::vector<StoryEvent> DrainEvents() override;

private:
    StoryResult Reject(
        StoryResultCode code,
        StorySessionId sessionId,
        StoryRevision revision,
        std::string message
    );
    void PushStateEvent(StoryEventType type, const StorySessionState& state);
    void PushCurrentLineEvent(const StorySessionState& state);
    bool CanControl(
        const StorySessionState& state,
        const StoryCommandContext& context
    ) const;

private:
    const IStoryCatalog& m_Catalog;
    StoryRuntimeRole m_Role = StoryRuntimeRole::Authority;
    std::map<StorySessionId, StorySessionState> m_Sessions;
    std::set<std::pair<StoryPeerId, StoryCommandId>> m_AppliedCommands;
    std::vector<StoryEvent> m_Events;
};

} // namespace Engine::Story
