#include "StoryTypes.h"

namespace Engine::Story {

const char* ToString(const StoryPhase phase) {
    switch (phase) {
    case StoryPhase::Playing: return "Playing";
    case StoryPhase::Finished: return "Finished";
    case StoryPhase::Aborted: return "Aborted";
    }
    return "Unknown";
}

const char* ToString(const StoryEventType type) {
    switch (type) {
    case StoryEventType::Started: return "Started";
    case StoryEventType::LineChanged: return "LineChanged";
    case StoryEventType::Finished: return "Finished";
    case StoryEventType::Aborted: return "Aborted";
    case StoryEventType::StateSynchronized: return "StateSynchronized";
    case StoryEventType::Error: return "Error";
    }
    return "Unknown";
}

const char* ToString(const StoryResultCode code) {
    switch (code) {
    case StoryResultCode::Accepted: return "Accepted";
    case StoryResultCode::InvalidRequest: return "InvalidRequest";
    case StoryResultCode::WrongRuntimeRole: return "WrongRuntimeRole";
    case StoryResultCode::SessionAlreadyExists: return "SessionAlreadyExists";
    case StoryResultCode::SessionNotFound: return "SessionNotFound";
    case StoryResultCode::StoryNotFound: return "StoryNotFound";
    case StoryResultCode::EmptyStory: return "EmptyStory";
    case StoryResultCode::ContentVersionMismatch: return "ContentVersionMismatch";
    case StoryResultCode::RevisionMismatch: return "RevisionMismatch";
    case StoryResultCode::PermissionDenied: return "PermissionDenied";
    case StoryResultCode::DuplicateCommand: return "DuplicateCommand";
    case StoryResultCode::StoryAlreadyEnded: return "StoryAlreadyEnded";
    case StoryResultCode::InvalidState: return "InvalidState";
    }
    return "Unknown";
}

} // namespace Engine::Story
