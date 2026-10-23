# XiE Sensory Interface V1 Changelog

Date: 2026-10-21

## Added

- `XiE_Projects/XSI/CMakeLists.txt`: standalone and XiE-subdirectory CMake targets, CTest registration, and install rules.
- `XiE_Projects/XSI/Include/XSI/SensoryEvent.h`: event identity, timestamps, source/external identity, ordered content parts, immutable media references, and validation API.
- `XiE_Projects/XSI/Include/XSI/SensoryEventChannel.h`: publisher/receiver interfaces, bounded MPMC channel contract, status and metrics.
- `XiE_Projects/XSI/Include/XSI/StreamSequenceValidator.h`: bounded sequence state for continuous streams.
- `XiE_Projects/XSI/Include/XSI/SensoryAdapter.h`: adapter lifecycle and simulated text/image and video adapters.
- `XiE_Projects/XSI/Include/XSI/CodecBoundary.h`: versioned codec and persistent media locator boundary, without a codec implementation.
- `XiE_Projects/XSI/Source/SensoryEvent.cpp`: event ID generation, immutable media storage metrics, UTF-8 and event validation.
- `XiE_Projects/XSI/Source/SensoryEventChannel.cpp`: fixed-capacity synchronized slot queue, backpressure, latest-frame replacement, close and stop-token wakeup.
- `XiE_Projects/XSI/Source/StreamSequenceValidator.cpp`: stream start/data/end ordering and bounded active-stream and exact dropped-interval tracking.
- `XiE_Projects/XSI/Source/SimulatedAdapters.cpp`: one mixed text/image source and a timed synthetic video source.
- `XiE_Projects/XSI/Demo/Main.cpp`: runnable event consumer, resource lifecycle checks, throughput, latency, and queue/media metrics.
- `XiE_Projects/XSI/Tests/SensoryInterfaceTests.cpp`: deterministic protocol, resource, queue, lifecycle, and concurrent stress checks.
- `XiE_Projects/XSI/Benchmarks/LatestOnlyBenchmark.cpp`: Release/Debug high-water LatestOnly benchmark across capacities and mixed-event ratios, including four-producer contention and a 65,536-slot bounded dropped-tracking saturation search with throughput/P50/P95/P99 publish latency.
- `Docs/XSI_V1_SPEC.md`, `Docs/XSI_V1_ARCHITECTURE.md`, `Docs/XSI_V1_TEST_REPORT.md`, and `Docs/XSI_V1_CHANGELOG.md`.

## Modified

- Root `CMakeLists.txt`: add `XiE_Projects/XSI` to the existing XiE project build.
- Root `.gitignore`: allow the new XSI project and four XSI delivery documents to be tracked alongside XiE's existing project-specific exceptions.

## Sol review fixes

- `SensoryEventChannel.cpp`: restrict LatestOnly eviction to pure-video stream data, preserve rejected mixed events, record dropped sequence intervals, sequence-process each stream event before dequeue unlock, and replace capacity-wide shifts with O(1) FIFO/video linked-index operations. A 1,024-candidate fallback handles tracking saturation without scanning the entire queue; if no candidate is trackable in that bounded window, it returns the original event with `Full`.
- `StreamSequenceValidator.h/.cpp`: share configurable validation limits with the channel, recognize only fully covered recorded dropped-frame gaps, reject duplicate/backward/unrecorded sequences, bound disjoint intervals per stream, and expose explicit stream-state cleanup.
- `SensoryEventChannel.h/.cpp`: add bounded `PublishFor`, explicit `AbandonStream`, and a stream-sequence result on received events. Preserve the existing `void Stop()` adapter call shape.
- `SensoryAdapter.h` and `SimulatedAdapters.cpp`: add `StopAndReport`, bounded StreamEnded delivery, abnormal termination and state cleanup reporting, pre-start frame/interval validation, checked timestamp arithmetic, worker exception containment and self-finalization, stored stop results, and thread-safe runtime status.
- `Demo/Main.cpp` and `Tests/SensoryInterfaceTests.cpp`: consume in-channel sequence results and cover the six review items, mixed capacities, deterministic MPMC ordering, adapter stop/error cases, concurrent close, and different-stream parallel payload handling.
- `XSI_V1_SPEC.md`, `XSI_V1_ARCHITECTURE.md`, and `XSI_V1_TEST_REPORT.md`: record the updated contracts, validation and benchmark evidence, and remaining limits.

No XiE rendering, window, UI, or existing queue interfaces were modified.
