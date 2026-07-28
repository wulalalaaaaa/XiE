#include "StoryRuntime.h"

#include <utility>

namespace Engine::Story {

StoryRuntime::StoryRuntime(const IStoryCatalog& catalog, const StoryRuntimeRole role)
    : m_Catalog(catalog)
    , m_Role(role) {}

StoryRuntimeRole StoryRuntime::Role() const {
    return m_Role;
}

StoryResult StoryRuntime::Start(
    const StoryStartRequest& request,
    const StoryCommandContext& context
) {
    if (m_Role != StoryRuntimeRole::Authority) {
        return Reject(
            StoryResultCode::WrongRuntimeRole,
            request.sessionId,
            0,
            "Only an authority runtime can start a story");
    }
    if (request.sessionId == 0 || request.story.Empty()) {
        return Reject(
            StoryResultCode::InvalidRequest,
            request.sessionId,
            0,
            "Start requires a non-zero session id and a complete StoryKey");
    }
    if (m_Sessions.contains(request.sessionId)) {
        return Reject(
            StoryResultCode::SessionAlreadyExists,
            request.sessionId,
            m_Sessions.at(request.sessionId).revision,
            "Story session already exists");
    }
    if (request.contentVersion != m_Catalog.ContentVersion()) {
        return Reject(
            StoryResultCode::ContentVersionMismatch,
            request.sessionId,
            0,
            "Start content version does not match authority catalog");
    }

    const StoryDefinition* definition = m_Catalog.FindStory(request.story);
    if (definition == nullptr) {
        return Reject(
            StoryResultCode::StoryNotFound,
            request.sessionId,
            0,
            "Story was not found: " + request.story.ToString());
    }
    if (definition->lines.empty()) {
        return Reject(
            StoryResultCode::EmptyStory,
            request.sessionId,
            0,
            "Story contains no lines: " + request.story.ToString());
    }

    StorySessionState state{};
    state.sessionId = request.sessionId;
    state.story = request.story;
    state.position = definition->lines.front().position;
    state.phase = StoryPhase::Playing;
    state.revision = 1;
    state.contentVersion = request.contentVersion;
    state.controllerPeerId = context.sourcePeerId;
    m_Sessions.emplace(state.sessionId, state);

    PushStateEvent(StoryEventType::Started, state);
    PushCurrentLineEvent(state);
    return StoryResult::Accepted(state.sessionId, state.revision, "Story started");
}

StoryResult StoryRuntime::Apply(
    const StoryCommand& command,
    const StoryCommandContext& context
) {
    if (m_Role != StoryRuntimeRole::Authority) {
        return Reject(
            StoryResultCode::WrongRuntimeRole,
            command.sessionId,
            0,
            "Only an authority runtime can apply commands");
    }
    if (command.commandId == 0 || command.sessionId == 0) {
        return Reject(
            StoryResultCode::InvalidRequest,
            command.sessionId,
            0,
            "Command and session ids must be non-zero");
    }

    const std::pair<StoryPeerId, StoryCommandId> commandKey{
        context.sourcePeerId,
        command.commandId};
    if (m_AppliedCommands.contains(commandKey)) {
        const StorySessionState* existing = FindSession(command.sessionId);
        return Reject(
            StoryResultCode::DuplicateCommand,
            command.sessionId,
            existing != nullptr ? existing->revision : 0,
            "Command was already applied");
    }

    if (command.type == StoryCommandType::Start) {
        if (!command.startRequest.has_value() ||
            command.startRequest->sessionId != command.sessionId ||
            command.startRequest->contentVersion != command.contentVersion) {
            return Reject(
                StoryResultCode::InvalidRequest,
                command.sessionId,
                0,
                "Start command payload is missing or inconsistent");
        }

        StoryResult result = Start(*command.startRequest, context);
        if (result.Succeeded()) {
            StorySessionState& state = m_Sessions.at(command.sessionId);
            state.lastAppliedCommandId = command.commandId;
            m_AppliedCommands.insert(commandKey);
        }
        return result;
    }

    auto found = m_Sessions.find(command.sessionId);
    if (found == m_Sessions.end()) {
        return Reject(
            StoryResultCode::SessionNotFound,
            command.sessionId,
            0,
            "Story session does not exist");
    }

    StorySessionState& state = found->second;
    if (command.contentVersion != m_Catalog.ContentVersion() ||
        command.contentVersion != state.contentVersion) {
        return Reject(
            StoryResultCode::ContentVersionMismatch,
            state.sessionId,
            state.revision,
            "Command content version does not match session and catalog");
    }
    if (command.expectedRevision != state.revision) {
        return Reject(
            StoryResultCode::RevisionMismatch,
            state.sessionId,
            state.revision,
            "Command expected revision does not match authority state");
    }
    if (!CanControl(state, context)) {
        return Reject(
            StoryResultCode::PermissionDenied,
            state.sessionId,
            state.revision,
            "Command source does not control this story session");
    }
    if (state.phase != StoryPhase::Playing) {
        return Reject(
            StoryResultCode::StoryAlreadyEnded,
            state.sessionId,
            state.revision,
            "Story session has already ended");
    }

    switch (command.type) {
    case StoryCommandType::Advance: {
        StoryPosition next{};
        ++state.revision;
        state.lastAppliedCommandId = command.commandId;
        if (m_Catalog.NextPosition(state.story, state.position, next)) {
            state.position = next;
            PushCurrentLineEvent(state);
        } else {
            state.phase = StoryPhase::Finished;
            PushStateEvent(StoryEventType::Finished, state);
        }
        break;
    }
    case StoryCommandType::Finish:
        ++state.revision;
        state.lastAppliedCommandId = command.commandId;
        state.phase = StoryPhase::Finished;
        PushStateEvent(StoryEventType::Finished, state);
        break;
    case StoryCommandType::Abort:
        ++state.revision;
        state.lastAppliedCommandId = command.commandId;
        state.phase = StoryPhase::Aborted;
        PushStateEvent(StoryEventType::Aborted, state);
        break;
    case StoryCommandType::Start:
        return Reject(
            StoryResultCode::InvalidRequest,
            state.sessionId,
            state.revision,
            "Unexpected Start command");
    }

    m_AppliedCommands.insert(commandKey);
    return StoryResult::Accepted(state.sessionId, state.revision, "Story command applied");
}

StoryResult StoryRuntime::Synchronize(const StorySessionState& state) {
    if (m_Role != StoryRuntimeRole::Replica) {
        return Reject(
            StoryResultCode::WrongRuntimeRole,
            state.sessionId,
            state.revision,
            "Only a replica runtime accepts synchronized state");
    }
    if (state.sessionId == 0 || state.story.Empty() || state.revision == 0) {
        return Reject(
            StoryResultCode::InvalidState,
            state.sessionId,
            state.revision,
            "Replicated story state is incomplete");
    }
    if (state.contentVersion != m_Catalog.ContentVersion()) {
        return Reject(
            StoryResultCode::ContentVersionMismatch,
            state.sessionId,
            state.revision,
            "Replicated state content version does not match local catalog");
    }
    if (m_Catalog.FindStory(state.story) == nullptr) {
        return Reject(
            StoryResultCode::StoryNotFound,
            state.sessionId,
            state.revision,
            "Replicated story does not exist locally");
    }
    if (state.phase == StoryPhase::Playing &&
        m_Catalog.FindLine(state.story, state.position) == nullptr) {
        return Reject(
            StoryResultCode::InvalidState,
            state.sessionId,
            state.revision,
            "Replicated story position does not exist locally");
    }

    auto found = m_Sessions.find(state.sessionId);
    if (found != m_Sessions.end()) {
        if (state.revision < found->second.revision) {
            return Reject(
                StoryResultCode::RevisionMismatch,
                state.sessionId,
                found->second.revision,
                "Replicated state is older than local state");
        }
        if (state.revision == found->second.revision) {
            return StoryResult::Rejected(
                StoryResultCode::DuplicateCommand,
                state.sessionId,
                state.revision,
                "Replicated state is already applied");
        }
    }

    const bool isNewSession = found == m_Sessions.end();
    m_Sessions[state.sessionId] = state;
    PushStateEvent(StoryEventType::StateSynchronized, state);
    if (state.phase == StoryPhase::Playing) {
        if (isNewSession) {
            PushStateEvent(StoryEventType::Started, state);
        }
        PushCurrentLineEvent(state);
    } else if (state.phase == StoryPhase::Finished) {
        PushStateEvent(StoryEventType::Finished, state);
    } else {
        PushStateEvent(StoryEventType::Aborted, state);
    }

    return StoryResult::Accepted(state.sessionId, state.revision, "Story state synchronized");
}

const StorySessionState* StoryRuntime::FindSession(const StorySessionId sessionId) const {
    const auto found = m_Sessions.find(sessionId);
    return found == m_Sessions.end() ? nullptr : &found->second;
}

std::vector<StoryEvent> StoryRuntime::DrainEvents() {
    std::vector<StoryEvent> events = std::move(m_Events);
    m_Events.clear();
    return events;
}

StoryResult StoryRuntime::Reject(
    const StoryResultCode code,
    const StorySessionId sessionId,
    const StoryRevision revision,
    std::string message
) {
    StoryResult result = StoryResult::Rejected(code, sessionId, revision, std::move(message));
    StoryEvent event{};
    event.type = StoryEventType::Error;
    event.result = result;
    if (const StorySessionState* state = FindSession(sessionId)) {
        event.state = *state;
    } else {
        event.state.sessionId = sessionId;
        event.state.revision = revision;
    }
    m_Events.push_back(std::move(event));
    return result;
}

void StoryRuntime::PushStateEvent(
    const StoryEventType type,
    const StorySessionState& state
) {
    StoryEvent event{};
    event.type = type;
    event.state = state;
    event.result = StoryResult::Accepted(state.sessionId, state.revision);
    m_Events.push_back(std::move(event));
}

void StoryRuntime::PushCurrentLineEvent(const StorySessionState& state) {
    StoryEvent event{};
    event.type = StoryEventType::LineChanged;
    event.state = state;
    event.result = StoryResult::Accepted(state.sessionId, state.revision);
    if (const StoryLine* line = m_Catalog.FindLine(state.story, state.position)) {
        event.line = *line;
    }
    m_Events.push_back(std::move(event));
}

bool StoryRuntime::CanControl(
    const StorySessionState& state,
    const StoryCommandContext& context
) const {
    return context.fromAuthority || context.sourcePeerId == state.controllerPeerId;
}

} // namespace Engine::Story
