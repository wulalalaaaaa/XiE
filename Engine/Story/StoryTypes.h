#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Engine::Story {

using StorySessionId = std::uint64_t;
using StoryRevision = std::uint64_t;
using StoryContentVersion = std::uint64_t;
using StoryPeerId = std::uint64_t;
using StoryCommandId = std::uint64_t;

struct StoryKey {
    std::string category;
    std::string title;

    bool Empty() const {
        return category.empty() || title.empty();
    }

    std::string ToString() const {
        return category + "/" + title;
    }

    friend bool operator==(const StoryKey&, const StoryKey&) = default;

    friend bool operator<(const StoryKey& lhs, const StoryKey& rhs) {
        if (lhs.category != rhs.category) {
            return lhs.category < rhs.category;
        }
        return lhs.title < rhs.title;
    }
};

struct StoryPosition {
    std::uint32_t segmentOrder = 0;
    std::uint32_t lineIndex = 0;

    friend bool operator==(const StoryPosition&, const StoryPosition&) = default;
};

struct StoryLine {
    StoryPosition position{};
    std::string speaker;
    std::string content;
    std::string briefDescription;
    std::string segmentFileName;
    bool isNarration = true;
};

struct StoryDefinition {
    StoryKey key{};
    std::vector<StoryLine> lines;
};

enum class StoryPhase {
    Playing = 0,
    Finished,
    Aborted,
};

enum class StoryRuntimeRole {
    Authority = 0,
    Replica,
};

enum class StoryCommandType {
    Start = 0,
    Advance,
    Finish,
    Abort,
};

enum class StoryEventType {
    Started = 0,
    LineChanged,
    Finished,
    Aborted,
    StateSynchronized,
    Error,
};

enum class StoryResultCode {
    Accepted = 0,
    InvalidRequest,
    WrongRuntimeRole,
    SessionAlreadyExists,
    SessionNotFound,
    StoryNotFound,
    EmptyStory,
    ContentVersionMismatch,
    RevisionMismatch,
    PermissionDenied,
    DuplicateCommand,
    StoryAlreadyEnded,
    InvalidState,
};

struct StoryStartRequest {
    StorySessionId sessionId = 0;
    StoryKey story{};
    StoryContentVersion contentVersion = 0;
};

struct StoryCommand {
    StoryCommandId commandId = 0;
    StoryCommandType type = StoryCommandType::Advance;
    StorySessionId sessionId = 0;
    StoryRevision expectedRevision = 0;
    StoryContentVersion contentVersion = 0;
    std::optional<StoryStartRequest> startRequest;

    static StoryCommand Start(StoryCommandId commandId, const StoryStartRequest& request) {
        StoryCommand command{};
        command.commandId = commandId;
        command.type = StoryCommandType::Start;
        command.sessionId = request.sessionId;
        command.expectedRevision = 0;
        command.contentVersion = request.contentVersion;
        command.startRequest = request;
        return command;
    }
};

struct StoryCommandContext {
    StoryPeerId sourcePeerId = 0;
    bool fromAuthority = false;
};

struct StorySessionState {
    StorySessionId sessionId = 0;
    StoryKey story{};
    StoryPosition position{};
    StoryPhase phase = StoryPhase::Playing;
    StoryRevision revision = 0;
    StoryContentVersion contentVersion = 0;
    StoryPeerId controllerPeerId = 0;
    StoryCommandId lastAppliedCommandId = 0;
};

struct StoryResult {
    StoryResultCode code = StoryResultCode::InvalidRequest;
    StorySessionId sessionId = 0;
    StoryRevision revision = 0;
    std::string message;

    bool Succeeded() const {
        return code == StoryResultCode::Accepted;
    }

    static StoryResult Accepted(StorySessionId sessionId, StoryRevision revision, std::string message = {}) {
        return {StoryResultCode::Accepted, sessionId, revision, std::move(message)};
    }

    static StoryResult Rejected(
        StoryResultCode code,
        StorySessionId sessionId,
        StoryRevision revision,
        std::string message
    ) {
        return {code, sessionId, revision, std::move(message)};
    }
};

struct StoryEvent {
    StoryEventType type = StoryEventType::Error;
    StorySessionState state{};
    std::optional<StoryLine> line;
    StoryResult result{};
};

const char* ToString(StoryPhase phase);
const char* ToString(StoryEventType type);
const char* ToString(StoryResultCode code);

} // namespace Engine::Story
