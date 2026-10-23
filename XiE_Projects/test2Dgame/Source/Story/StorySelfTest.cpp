#include "StorySelfTest.h"

#include "Story/IStoryCatalog.h"
#include "Story/LoopbackStoryTransport.h"
#include "Story/StoryRuntime.h"

#include <cstdint>

namespace Test2D {
namespace {

using namespace Engine::Story;

struct TestState {
    int checksPassed = 0;
    std::string failure;

    bool Check(const bool condition, const char* message) {
        if (!condition && failure.empty()) {
            failure = message;
        }
        if (condition) {
            ++checksPassed;
        }
        return condition;
    }
};

bool SameReplicatedState(const StorySessionState* lhs, const StorySessionState* rhs) {
    return lhs != nullptr &&
        rhs != nullptr &&
        lhs->sessionId == rhs->sessionId &&
        lhs->story == rhs->story &&
        lhs->position == rhs->position &&
        lhs->phase == rhs->phase &&
        lhs->revision == rhs->revision &&
        lhs->contentVersion == rhs->contentVersion &&
        lhs->controllerPeerId == rhs->controllerPeerId;
}

} // namespace

StorySelfTestResult RunStorySelfTest(const Engine::Story::IStoryCatalog& catalog) {
    using namespace Engine::Story;

    TestState test{};
    const std::vector<StoryKey> keys = catalog.StoryKeys();
    if (!test.Check(!keys.empty(), "Catalog did not discover any stories")) {
        return {false, test.checksPassed, test.failure};
    }

    const StoryKey storyKey = keys.front();
    const StoryDefinition* definition = catalog.FindStory(storyKey);
    if (!test.Check(definition != nullptr, "StoryKey lookup failed") ||
        !test.Check(!definition->lines.empty(), "Discovered story contains no lines")) {
        return {false, test.checksPassed, test.failure};
    }
    StoryPosition firstPosition{};
    test.Check(
        catalog.FirstPosition(storyKey, firstPosition),
        "Catalog did not expose the first story position");
    test.Check(
        catalog.FindLine(storyKey, firstPosition) != nullptr,
        "Catalog could not resolve its first story line");

    StoryRuntime authority(catalog, StoryRuntimeRole::Authority);
    StoryRuntime replicaOne(catalog, StoryRuntimeRole::Replica);
    StoryRuntime replicaTwo(catalog, StoryRuntimeRole::Replica);
    LoopbackStoryTransport transport;

    transport.SetAuthorityReceiver(
        [&authority, &transport](const StoryCommand& command, const StoryCommandContext& context) {
            StoryResult result = authority.Apply(command, context);
            if (result.Succeeded()) {
                if (const StorySessionState* state = authority.FindSession(command.sessionId)) {
                    transport.PublishState(*state);
                }
            }
            return result;
        });

    constexpr StoryPeerId kControllerPeer = 101;
    constexpr StoryPeerId kOtherPeer = 202;
    constexpr StorySessionId kSession = 9001;

    test.Check(
        transport.ConnectReplica(
            kControllerPeer,
            [&replicaOne](const StorySessionState& state) {
                return replicaOne.Synchronize(state);
            }),
        "First replica failed to connect");

    StoryCommand missingSession{};
    missingSession.commandId = 1;
    missingSession.type = StoryCommandType::Advance;
    missingSession.sessionId = kSession;
    missingSession.expectedRevision = 0;
    missingSession.contentVersion = catalog.ContentVersion();
    StoryResult result = transport.SendCommand(kControllerPeer, missingSession);
    test.Check(
        result.code == StoryResultCode::SessionNotFound,
        "Command for a missing session was not rejected");

    StoryStartRequest start{};
    start.sessionId = kSession;
    start.story = storyKey;
    start.contentVersion = catalog.ContentVersion() + 1;
    result = transport.SendCommand(
        kControllerPeer,
        StoryCommand::Start(2, start));
    test.Check(
        result.code == StoryResultCode::ContentVersionMismatch,
        "Mismatched content version was not rejected");

    start.contentVersion = catalog.ContentVersion();
    result = transport.SendCommand(
        kControllerPeer,
        StoryCommand::Start(3, start));
    test.Check(result.Succeeded(), "Valid Start command failed");
    test.Check(
        SameReplicatedState(
            authority.FindSession(kSession),
            replicaOne.FindSession(kSession)),
        "First replica did not receive Start state");

    test.Check(
        transport.ConnectReplica(
            kOtherPeer,
            [&replicaTwo](const StorySessionState& state) {
                return replicaTwo.Synchronize(state);
            }),
        "Late replica failed to connect");
    test.Check(
        SameReplicatedState(
            authority.FindSession(kSession),
            replicaTwo.FindSession(kSession)),
        "Late replica did not receive current state");

    StoryCommand badRevision{};
    badRevision.commandId = 4;
    badRevision.type = StoryCommandType::Advance;
    badRevision.sessionId = kSession;
    badRevision.expectedRevision = 0;
    badRevision.contentVersion = catalog.ContentVersion();
    result = transport.SendCommand(kControllerPeer, badRevision);
    test.Check(
        result.code == StoryResultCode::RevisionMismatch,
        "Mismatched revision was not rejected");

    const StorySessionState* authorityState = authority.FindSession(kSession);
    if (!test.Check(authorityState != nullptr, "Authority session disappeared")) {
        return {false, test.checksPassed, test.failure};
    }

    StoryCommand unauthorized = badRevision;
    unauthorized.commandId = 5;
    unauthorized.expectedRevision = authorityState->revision;
    result = transport.SendCommand(kOtherPeer, unauthorized);
    test.Check(
        result.code == StoryResultCode::PermissionDenied,
        "Unauthorized peer command was not rejected");

    StoryCommandId nextCommandId = 6;
    StoryCommand advance = unauthorized;
    advance.commandId = nextCommandId++;
    result = transport.SendCommand(kControllerPeer, advance);
    test.Check(result.Succeeded(), "Valid Advance command failed");

    const StoryRevision revisionAfterAdvance =
        authority.FindSession(kSession)->revision;
    result = transport.SendCommand(kControllerPeer, advance);
    test.Check(
        result.code == StoryResultCode::DuplicateCommand,
        "Duplicate Advance command was not rejected");
    test.Check(
        authority.FindSession(kSession)->revision == revisionAfterAdvance,
        "Duplicate Advance changed authority revision");

    while (authority.FindSession(kSession)->phase == StoryPhase::Playing) {
        const StorySessionState& current = *authority.FindSession(kSession);
        StoryCommand next{};
        next.commandId = nextCommandId++;
        next.type = StoryCommandType::Advance;
        next.sessionId = kSession;
        next.expectedRevision = current.revision;
        next.contentVersion = catalog.ContentVersion();
        result = transport.SendCommand(kControllerPeer, next);
        if (!test.Check(result.Succeeded(), "Advance-to-finish command failed")) {
            break;
        }
    }

    const StorySessionState* finalAuthority = authority.FindSession(kSession);
    test.Check(
        finalAuthority != nullptr && finalAuthority->phase == StoryPhase::Finished,
        "Story did not reach Finished state");
    test.Check(
        SameReplicatedState(finalAuthority, replicaOne.FindSession(kSession)),
        "First replica final state differs from authority");
    test.Check(
        SameReplicatedState(finalAuthority, replicaTwo.FindSession(kSession)),
        "Second replica final state differs from authority");

    if (finalAuthority != nullptr) {
        StoryCommand afterFinish{};
        afterFinish.commandId = nextCommandId;
        afterFinish.type = StoryCommandType::Advance;
        afterFinish.sessionId = kSession;
        afterFinish.expectedRevision = finalAuthority->revision;
        afterFinish.contentVersion = catalog.ContentVersion();
        result = transport.SendCommand(kControllerPeer, afterFinish);
        test.Check(
            result.code == StoryResultCode::StoryAlreadyEnded,
            "Advance after Finished was not rejected");
        ++nextCommandId;
    }

    StoryStartRequest abortStart{};
    abortStart.sessionId = kSession + 1;
    abortStart.story = storyKey;
    abortStart.contentVersion = catalog.ContentVersion();
    result = transport.SendCommand(
        kControllerPeer,
        StoryCommand::Start(nextCommandId++, abortStart));
    test.Check(result.Succeeded(), "Abort test session failed to start");
    if (const StorySessionState* abortState = authority.FindSession(abortStart.sessionId)) {
        StoryCommand abort{};
        abort.commandId = nextCommandId++;
        abort.type = StoryCommandType::Abort;
        abort.sessionId = abortState->sessionId;
        abort.expectedRevision = abortState->revision;
        abort.contentVersion = catalog.ContentVersion();
        result = transport.SendCommand(kControllerPeer, abort);
        test.Check(result.Succeeded(), "Abort command failed");
        test.Check(
            authority.FindSession(abortStart.sessionId)->phase == StoryPhase::Aborted,
            "Abort command did not set Aborted state");
    }

    StoryStartRequest finishStart{};
    finishStart.sessionId = kSession + 2;
    finishStart.story = storyKey;
    finishStart.contentVersion = catalog.ContentVersion();
    result = transport.SendCommand(
        kControllerPeer,
        StoryCommand::Start(nextCommandId++, finishStart));
    test.Check(result.Succeeded(), "Finish test session failed to start");
    if (const StorySessionState* finishState = authority.FindSession(finishStart.sessionId)) {
        StoryCommand finish{};
        finish.commandId = nextCommandId++;
        finish.type = StoryCommandType::Finish;
        finish.sessionId = finishState->sessionId;
        finish.expectedRevision = finishState->revision;
        finish.contentVersion = catalog.ContentVersion();
        result = transport.SendCommand(kControllerPeer, finish);
        test.Check(result.Succeeded(), "Finish command failed");
        test.Check(
            authority.FindSession(finishStart.sessionId)->phase == StoryPhase::Finished,
            "Finish command did not set Finished state");
    }

    StorySelfTestResult output{};
    output.passed = test.failure.empty();
    output.checksPassed = test.checksPassed;
    output.message = output.passed ? "All story checks passed" : test.failure;
    return output;
}

} // namespace Test2D
