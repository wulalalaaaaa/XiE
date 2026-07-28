#pragma once

#include "StoryTypes.h"

#include <functional>

namespace Engine::Story {

using StoryCommandReceiver =
    std::function<StoryResult(const StoryCommand&, const StoryCommandContext&)>;
using StoryStateReceiver =
    std::function<StoryResult(const StorySessionState&)>;

class IStoryTransport {
public:
    virtual ~IStoryTransport() = default;

    virtual void SetAuthorityReceiver(StoryCommandReceiver receiver) = 0;
    virtual bool ConnectReplica(StoryPeerId peerId, StoryStateReceiver receiver) = 0;
    virtual void DisconnectReplica(StoryPeerId peerId) = 0;
    virtual StoryResult SendCommand(StoryPeerId sourcePeerId, const StoryCommand& command) = 0;
    virtual void PublishState(const StorySessionState& state) = 0;
};

} // namespace Engine::Story
