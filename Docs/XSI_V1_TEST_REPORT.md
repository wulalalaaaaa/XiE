# XiE Sensory Interface V1 Review-Fix Test Report

Date: 2026-10-21

## Environment and commands

- Windows x64, Visual Studio 17 2022 generator, MSVC 19.36.32548, CMake 4.3.1.
- C++20. No third-party package was installed.
- The final build was run from the actual checkout at `E:\Project_c++\XiE\XiE_Projects\XSI` and used output directory `E:\LLM\xiling2\.xsi-v1-final-actual`.

Focused configure and build:

```powershell
cmake -S E:\Project_c++\XiE\XiE_Projects\XSI -B E:\LLM\xiling2\.xsi-v1-final-actual -G "Visual Studio 17 2022" -A x64
cmake --build E:\LLM\xiling2\.xsi-v1-final-actual --config Debug --target xie_sensory_tests xie_sensory_demo xie_sensory_benchmarks --parallel 6
cmake --build E:\LLM\xiling2\.xsi-v1-final-actual --config Release --target xie_sensory_tests xie_sensory_demo xie_sensory_benchmarks --parallel 6
ctest --test-dir E:\LLM\xiling2\.xsi-v1-final-actual -C Debug --output-on-failure --repeat until-fail:5
ctest --test-dir E:\LLM\xiling2\.xsi-v1-final-actual -C Release --output-on-failure --repeat until-fail:5
```

The XSI standalone configure and all three targets built successfully in Debug and Release. CTest passed 5/5 consecutive repetitions in each configuration. The direct test executable reported `XSI checks=10103 failures=0` in both configurations.

## Sol review reproductions

The baseline report recorded these failures from the original `review.cpp`: mixed text/video frames were replaced (`second=1`, sequence 2 received, one stale drop); full-queue stop delivered Start but no End; a 513-byte stream ID passed queue publication but failed a default sequence validator; two consumers caused a legal Data event to be checked before Start; and `SIZE_MAX` frame size started successfully before the Release process exited 3.

I rebuilt and ran the unmodified `E:\LLM\XSI\xsi_review\review.cpp` against the final actual XSI library in Debug and Release. Both produced:

```text
mixed_drop first=0 second=2 received_sequence=1 dropped=0
stop_full start_ok=1 starts=0 ends=0
custom_limit published=1 validator_accepted=0 error=0
consumer_reorder published=11 data_accepted=0 start_accepted=1
```

The separate `huge` invocation printed `huge_frame start=0` and exited 0 in both configurations.

The unchanged reproducer still calls the legacy `void Stop()`, so it cannot display `StopAndReport`'s failure result. Its two remaining negative checks also exercise the old usage: it gives a default-limit standalone validator to a custom-limit event, and manually calls a separate validator after MPMC dequeue. The repaired production path uses `ReceiveResult.streamSequence`, which processes state under the channel dequeue lock; a standalone validator can now be constructed with the same `ValidationLimits`. The focused tests below assert these supported paths and the bounded stop status directly. The empty Start/End counts in the no-consumer reproducer are expected after a timed-out End: the queue reports abnormal termination and clears the undeliverable stream entries instead of claiming End was delivered.

A temporary copy of the reproducer changed only those call sites to use `StopAndReport`, matching custom limits, and `ReceiveResult.streamSequence`. It exited 0 in Debug and Release with:

```text
mixed_drop first=0 second=2 received_sequence=1 dropped=0
stop_full start_ok=1 starts=0 ends=0 endStatus=5 endPublished=0 cleaned=1
custom_limit published=1 validator_accepted=1 error=-1
consumer_reorder published=11 data_accepted=1 start_accepted=1
```

`endStatus=5` is `TimedOut`; the result explicitly reports no End delivery and successful abnormal stream cleanup.

## Six review findings

| Finding | Change and regression evidence |
|---|---|
| P1-1 mixed LatestOnly eviction | Only non-empty `StreamData` whose every part is video is indexed as replaceable. Mixed text/video events at capacities 1, 2, and 4 return `Full`, retain the rejected event, and preserve all queued text and media parts. |
| P1-2 failed StreamEnded | `StopAndReport` joins the worker, waits at most 100 ms for End (cancellable), and reports delivery/abnormal status. On timeout, cancellation, or channel close it records the error and calls `AbandonStream`; tests check queue-full, stopped consumer, closed channel, cancellation, normal End delivery, and state cleanup. |
| P1-3 stream order under MPMC | `Receive` runs the channel-owned sequence transition under the same lock that removes the FIFO head, then returns its result. A deterministic two-consumer gate delays Start's consumer after dequeue while another consumer receives Data; Start/Data/End all validate. Different streams' payload handling still runs in parallel after receive. Two producers on one stream are tested both when sequence assignment follows queue order and when it does not; queue lock acquisition order is the contract and invalid order is reported. |
| P1-4 adapter worker exceptions | Frame sizes 0, `SIZE_MAX`, and greater than 64 MiB plus invalid intervals fail before worker start. The `noexcept` worker boundary catches all exceptions, updates thread-safe runtime status, and self-finalizes the stream. A fake publisher throwing after Start signals End before the test calls Stop, then verifies the stored abnormal StopAndReport result and no process termination. |
| P2-1 shared limits | The channel constructs its sequence validator with the same `ValidationLimits`; the standalone validator accepts those limits too. Regression uses `maxStringBytes=600` and a 513-byte stream ID. |
| P2-2 LatestOnly lock work | A preallocated video candidate index and linked FIFO slots make normal replacement/unlink O(1). If the oldest candidate cannot be recorded because dropped-frame tracking is saturated, the channel examines at most 1,024 video candidates. If none can be tracked in that window it returns `Full`, even if a later candidate exists. A dedicated capacity-65,536 saturation benchmark measures this bounded worst-case path along with high-water capacities, mixed-event ratios, and four-producer contention. |

Additional tests cover separated drops around retained stream data (drop sequences 1 and 3 while retaining data 2), exact dropped-frame gap acceptance, duplicate/backward/unrecorded sequence rejection, non-droppable Start/End, drop-tracking table saturation with a later trackable candidate, the 1,024-candidate backpressure limit with the rejected event preserved, explicit `ForgetStream`, media reference copy/move/thread lifetime and byte identity, bounded long runs, MPMC delivery, channel close, and concurrent adapter stop/resource release.

## Functional, demo, and bounded-memory results

| Measurement | Debug | Release |
|---|---:|---:|
| Direct test checks | 10,103 passed, 0 failed | 10,103 passed, 0 failed |
| CTest repetitions | 5/5 passed | 5/5 passed |
| 4P/4C reliable stress | 6,000 delivered; 49,360 events/s; peak depth 28/128 | 6,000 delivered; 982,045 events/s; peak depth 128/128 |
| 10,000 LatestOnly video events | 9,996 dropped; peak depth 4/4 | 9,996 dropped; peak depth 4/4 |
| Sampled peak media bytes in bounded run | 20,480 | 20,480 |
| Live media bytes after channel close | 0 | 0 |
| One-second simulated demo | 33 events; 32 media refs verified; 0 lifecycle errors | 33 events; 32 media refs verified; 0 lifecycle errors |
| Demo average / max receive latency | 0.087 / 0.438 ms | 0.030 / 0.074 ms |
| Demo peak media payload / live bytes at exit | 71,680 / 0 | 65,536 / 0 |

The demo's two simulated sources send mixed text/images and synthetic video frames through the channel. It delivers StreamEnded on normal shutdown. These measurements are single-run observations, not performance guarantees. Media byte counters exclude event strings, vector/control-block overhead, thread stacks, and other process allocations.

## LatestOnly benchmark results

The benchmark starts with a full queue. Control events are immutable text events mixed into the queue at the listed ratio; each measured publish is a pure-video frame. P95/P99 time the `TryPublish` call itself. Throughput includes frame construction and publication. Each single-producer case performs 512 operations (128 at capacity 65,536). Results below are one Release run on this host.

| Capacity | Control mix | Throughput (events/s) | P95 publish (µs) | P99 publish (µs) |
|---:|---:|---:|---:|---:|
| 128 | 0% | 1,267,010 | 0.6 | 0.6 |
| 128 | 10% | 1,282,890 | 0.6 | 0.6 |
| 128 | 50% | 1,284,170 | 0.6 | 0.6 |
| 1,024 | 0% | 1,226,640 | 0.6 | 0.8 |
| 1,024 | 10% | 1,251,830 | 0.6 | 0.7 |
| 1,024 | 50% | 1,257,680 | 0.6 | 0.7 |
| 8,192 | 0% | 1,109,670 | 0.9 | 1.0 |
| 8,192 | 10% | 1,025,030 | 1.1 | 1.4 |
| 8,192 | 50% | 938,933 | 1.1 | 1.7 |
| 65,536 | 0% | 590,406 | 1.8 | 2.3 |
| 65,536 | 10% | 1,077,440 | 0.9 | 1.0 |
| 65,536 | 50% | 1,073,830 | 0.9 | 1.1 |

At capacity 65,536, a separate four-producer saturated run performed 8,192 operations per ratio with zero rejected publications:

| Control mix | Throughput (events/s) | P50 (µs) | P95 (µs) | P99 (µs) |
|---:|---:|---:|---:|---:|
| 0% | 474,489 | 2.8 | 24.4 | 45.9 |
| 10% | 498,725 | 2.9 | 23.2 | 49.1 |
| 50% | 464,339 | 3.1 | 25.3 | 47.4 |

In the Debug single-producer high-water cases, P95 was 10.7–16.5 µs and P99 was 12.1–24.9 µs. Debug four-producer P95 was 130.5–142.2 µs and P99 was 286.4–343.6 µs. These are host-specific contention observations. The one-producer run does not show a capacity-proportional latency increase; the four-producer results quantify mutex contention at maximum capacity. They do not constitute a service-level target or a cross-machine comparison.

The tracking-saturation benchmark sets `maxActiveStreams=1`, fills a 65,536-slot queue with 1,024 frames from an untracked stream ahead of a candidate from the tracked stream, and measures the bounded 1,024-candidate search. The later trackable candidate is intentionally beyond the search limit, so all eight incoming events return `Full` and are retained. Release measured 5,843 events/s with P50/P95/P99 of 163.8/169/169 µs; Debug measured 393 events/s with 2,526.8/2,552.5/2,552.5 µs. This path is substantially slower than normal O(1) replacement, but it cannot hold the mutex while scanning all 65,536 slots. Consumers can drain the queue or callers can increase the active/drop tracking limit to avoid this saturation case.

## Sanitizers and full XiE build

An MSVC `/fsanitize=address` Debug build of `xie_sensory_tests` compiled and linked. The executable could not start: native exit was `-1073741515` (`0xC0000135`), with no test output, because `clang_rt.asan_dbg_dynamic-x86_64.dll` is not installed. This is not an ASan test pass. No ThreadSanitizer runtime is available here.

I also configured the XiE root without a GLAD stub:

```powershell
cmake -S E:\Project_c++\XiE -B E:\LLM\xiling2\.xsi-root-final-no-glad -G "Visual Studio 17 2022" -A x64 -DXIE_USE_SYSTEM_GLFW=OFF -DFETCHCONTENT_SOURCE_DIR_GLFW=E:\Project_c++\XiE\Engine\External\glfw
```

The configure failed while FetchContent attempted to clone GLAD from GitHub; `Engine\External\glad` is empty and network access was unavailable. No temporary stub was used. Therefore no full XiE configure/build/CTest or root install result is claimed. The successful Debug/Release results above are from the standalone XSI project only.

## Limits

No real QQ, camera, audio device, codec, cross-process transport, shared memory, persistent media resolver, or remote inference service is included. Reliable publication remains bounded in-process backpressure, not durable delivery or exactly-once processing. `review.cpp` retains the two legacy manual-validator checks described above; updated channel/limit behavior is covered in the deterministic XSI tests. Sanitizer runtime and full root build remain unavailable in this environment.
