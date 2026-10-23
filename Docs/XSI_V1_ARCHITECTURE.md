# XiE Sensory Interface V1 Architecture

Date: 2026-10-21

## XiE inspection and placement

XiE is a CMake project using C++20 (`cmake_minimum_required(VERSION 3.20)`). `Engine/CMakeLists.txt` exports reusable modules through named `xie_*` targets; `xie_engine` is a shared-library aggregate, while platform-neutral modules such as `xie_image`, `xie_input`, and `xie_application` are separate static targets. Project directories under `XiE_Projects` add their own targets and tests.

The current repository already has:

- `Engine/Core/MainThreadEventQueue`: mutex-protected FIFO with callback and drain, but unbounded, nonblocking, and without close semantics.
- `Engine/Input/InputEventQueue`: bounded for pointer input with pointer-specific coalescing and soft growth for state events. It is not a strict bound and has no blocking close/read API.
- `Engine/Audio/Core/AudioBufferPool`: a prepared, mutable audio DSP pool; it is not a general immutable media resource store.
- `Engine/Image/ImageBuffer`: owns CPU image bytes and copies its vector on copy. It is image-specific and does not provide shared immutable audio/video payloads.
- `Engine/Foundation/Handles/GenerationalHandle`: an owner-local index/generation handle, not a persistent resource identity.
- `Engine/Core/Log` and bundled `spdlog`; CTest contract-test executables; standard `std::thread` and condition-variable use. No general application thread pool or queue with XSI's required semantics was found.

None of those facilities already provides strict bounded MPMC blocking receive, stop-token cancellation, reliable-full reporting, video latest-only eviction, and wake-on-close. XSI therefore adds the smallest event-specific channel in the cognition-facing project layer. It uses `std::mutex`, `std::condition_variable_any`, `std::jthread`, and `std::shared_ptr<const ...>`; it does not alter UI/input queues or add a competing general Engine event bus. `MediaResourceRef` is a small immutable byte owner rather than a new cache/resource manager. XSI reports typed errors and queue statistics rather than logging from the hot path.

## Module and data flow

    external source
          |
      Adapter                 maps source identity, times, parts, stream sequence
          |
      ISensoryEventPublisher
          |
          SensoryEventChannel     validates -> fixed-capacity slot queue -> wake receiver
          |
      ISensoryEventReceiver
          |
          cognition consumer      receives prevalidated stream ordering; consumes read-only bytes

The code lives under `XiE_Projects/XSI`, not `Engine`: XiE remains responsible for reusable low-level infrastructure, while XSI protocol and Adapter behavior remain project-level. The target is a small static library with no renderer, window, UI, network, Python, JSON, or third-party dependency.

## Thread and lifetime model

- Any number of producers and consumers may use one channel. One mutex protects fixed-slot FIFO links, the free list, video-frame index, queue state, and statistics. Unlinking an eligible slot is O(1), without shifting later events; when tracking is saturated, candidate search examines no more than 1,024 video slots, including the oldest candidate.
- Stream sequence state is updated inside `Receive` before the queue lock is released. This defines processing order by successful enqueue order, so MPMC consumers cannot race Start and Data into a false `StreamNotStarted`. Consumers can process the returned payloads in parallel after the small ordered state transition. Multiple producers sharing a stream must coordinate sequence assignment to match the channel's enqueue linearization order.
- Each adapter owns its worker lifetime. The simulated video adapter uses `std::jthread` and a stop-aware condition-variable timer; it does not poll in a tight loop. Its worker entry catches all exceptions, records a thread-safe `AdapterRuntimeStatus`, and attempts to finalize its stream before returning on failure.
- Video frame byte sizes and intervals are validated before Start. `StopAndReport` waits up to 100 ms for `StreamEnded`; cancellation or a full/closed publisher returns an explicit abnormal result. On failure the adapter invokes `AbandonStream` to clear queued events and sequence state. If the worker already finalized itself, a later `StopAndReport` joins it and returns the stored stop result. The old `void Stop()` call remains as a compatibility wrapper; callers can read `RuntimeStatus()` or use `StopAndReport()` for the stop result.
- `Close` wakes all blocked channel operations. Accepted events remain available for drain. Stop tokens can cancel a blocked operation independently of channel closure.
- A failed publish keeps the original event in `PublishResult::rejectedEvent`, so explicit backpressure does not destroy the producer's only copy.
- An adapter receives only `ISensoryEventPublisher`; a consumer receives only `ISensoryEventReceiver`. Both must stop/join before their referenced channel is destroyed.
- The media resource storage is independently reference-counted and read-only. Dropping a queue item releases its reference; no cache keeps it alive.
- The channel preallocates `optional<SensoryEvent>` slots, FIFO links, a pure-video candidate index, and a free-slot stack up to its configured capacity. Event strings, parts, and media payloads can allocate according to input limits.

## Queue policy choice

The XiE main-thread queue cannot be made XSI's queue without changing its unbounded UI/application semantics. The input queue has a different contract: it coalesces pointer motion, allows soft growth to preserve state transitions, and is associated with a window. XSI's event queue is therefore specialized to its event types and policies. `LatestOnly` removes only events made entirely of video media parts. Mixed text/media events and stream boundaries remain intact; if no legal frame can be replaced, publication reports `Full` and returns the rejected event.

The channel records exact, merged dropped-sequence intervals per stream. A forward gap is accepted only when every missing sequence matches a recorded LatestOnly eviction; duplicates, backward sequences, gaps without an eviction record, and invalid lifecycle events remain errors. Tracking is bounded by `QueueOptions::maxActiveStreams` and 64 disjoint intervals per stream. Normal replacement uses the head of a preallocated video index in O(1). When that candidate cannot be recorded because tracking state is saturated, the channel checks at most 1,023 later video candidates while holding the queue mutex. If none of the first 1,024 candidates can be tracked, it returns backpressure with the incoming event intact, even when a later candidate might be trackable. This fixed search budget prevents a full-queue scan. `StreamSequenceValidator::ForgetStream` and publisher `AbandonStream` provide cleanup after abnormal termination.

`PublishWait` and `PublishFor` can wait for queue capacity. However, when a LatestOnly publish sees queued video candidates but its bounded search cannot find a trackable eviction, it returns `Full` immediately rather than waiting for its timeout. This preserves the incoming event and lets the caller decide whether to retry after tracking state changes.

## Versioning boundary

`ISensoryEventCodecV1` and `IPersistentMediaResourceResolver` define where a future codec maps native events to a versioned envelope and stable resource references. Media byte transport/storage is a separate operation. This boundary has no implementation in V1; current delivery is in-process only.

## Build, run, and package

The XiE root CMake project keeps `XiEVoiceMixer` as its Visual Studio startup target. XSI is independently selectable and installable:

    cmake --preset vs2022-x64-debug
    cmake --build --preset build-debug --target xie_sensory_demo xie_sensory_tests
    ctest --test-dir out/build/vs2022-x64-debug -C Debug -R xie_sensory_tests --output-on-failure
    cmake --build --preset build-release --target xie_sensory_benchmarks
    out/build/vs2022-x64-release/XiE_Projects/XSI/Release/xie_sensory_benchmarks.exe
    out/build/vs2022-x64-debug/XiE_Projects/XSI/Debug/xie_sensory_demo.exe 5
    cmake --preset vs2022-x64-release
    cmake --build --preset build-release --target xie_sensory_demo xie_sensory_tests
    cmake --install out/build/vs2022-x64-release --config Release --component XSI --prefix out/package/xsi-v1

The standalone `XiE_Projects/XSI` CMake entry is also usable for focused builds. The XiE root uses the same project target through `add_subdirectory`; no new top-level application framework is introduced.

In this checkout, the root's `Engine/External/glad` directory is empty. A normal full-project configure attempted to fetch GLAD and failed because network access to GitHub was unavailable. This review-fix turn used `XiE_Projects/XSI` as a standalone CMake project for Debug and Release verification; no temporary GLAD stub was used and no full-engine build is claimed.

## Known limits

There is no real QQ adapter, camera/audio capture, codec implementation, persistent media store, cross-process transport, network protocol, exactly-once guarantee, or LLM integration. Event IDs are practically unique across process restarts, not coordinated across devices. Resource metrics count media payload bytes, not total process heap usage.

The full XiE renderer/application build and root install graph were not verified because GLAD source is absent and could not be fetched in this environment. The XSI library, demo, tests, and benchmark were verified through the standalone XSI CMake entry point.
