#include "LoopbackStoryTransport.h"

#include <utility>

namespace Engine::Story {

void LoopbackStoryTransport::SetAuthorityReceiver(StoryCommandReceiver receiver) {
    m_AuthorityReceiver = std::move(receiver);
}

bool LoopbackStoryTransport::ConnectReplica(
    const StoryPeerId peerId,
    StoryStateReceiver receiver
) {
    if (peerId == 0 || !receiver) {
        return false;
    }

    m_ReplicaReceivers[peerId] = std::move(receiver);
    StoryStateReceiver& connectedReceiver = m_ReplicaReceivers.at(peerId);
    for (const auto& [sessionId, state] : m_LatestStates) {
        (void)sessionId;
        connectedReceiver(state);
    }
    return true;
}

void LoopbackStoryTransport::DisconnectReplica(const StoryPeerId peerId) {
    m_ReplicaReceivers.erase(peerId);
}

StoryResult LoopbackStoryTransport::SendCommand(
    const StoryPeerId sourcePeerId,
    const StoryCommand& command
) {
    if (!m_AuthorityReceiver) {
        return StoryResult::Rejected(
            StoryResultCode::InvalidState,
            command.sessionId,
            0,
            "Loopback transport has no authority receiver");
    }

    StoryCommandContext context{};
    context.sourcePeerId = sourcePeerId;
    context.fromAuthority = false;
    return m_AuthorityReceiver(command, context);
}

void LoopbackStoryTransport::PublishState(const StorySessionState& state) {
    m_LatestStates[state.sessionId] = state;
    for (auto& [peerId, receiver] : m_ReplicaReceivers) {
        (void)peerId;
        receiver(state);
    }
}

std::size_t LoopbackStoryTransport::ReplicaCount() const {
    return m_ReplicaReceivers.size();
}

} // namespace Engine::Story
