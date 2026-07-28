#pragma once

#include "IStoryTransport.h"

#include <map>
#include <utility>

namespace Engine::Story {

class LoopbackStoryTransport final : public IStoryTransport {
public:
    void SetAuthorityReceiver(StoryCommandReceiver receiver) override;
    bool ConnectReplica(StoryPeerId peerId, StoryStateReceiver receiver) override;
    void DisconnectReplica(StoryPeerId peerId) override;
    StoryResult SendCommand(StoryPeerId sourcePeerId, const StoryCommand& command) override;
    void PublishState(const StorySessionState& state) override;

    std::size_t ReplicaCount() const;

private:
    StoryCommandReceiver m_AuthorityReceiver;
    std::map<StoryPeerId, StoryStateReceiver> m_ReplicaReceivers;
    std::map<StorySessionId, StorySessionState> m_LatestStates;
};

} // namespace Engine::Story
