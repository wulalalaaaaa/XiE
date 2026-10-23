# XiE Sensory Interface V1 Specification

Date: 2026-10-21

## Purpose and boundary

XSI converts input from an external source into a validated native C++ event and delivers it to a consumer. It does not interpret meaning, choose attention, run an LLM, manage memory, connect to QQ, or decode media.

The in-process API uses C++ objects. JSON is not used on the event path. This V1 provides no network transport and makes no cross-process reliability or exactly-once guarantee.

## Event header

`SensoryEvent` contains:

| Field | Type | Meaning |
|---|---|---|
| `id` | `EventId { high, low }` | 128-bit event identity. A process-session nonce and atomic counter make IDs practically collision-resistant across restarts; this is not a cryptographic identity or distributed deduplication service. |
| `source.adapter` | UTF-8 string | Adapter/platform namespace, such as `qq` or `camera`. Required. |
| `source.instance` | UTF-8 string | Concrete account/device instance. Required. |
| `channel` | UTF-8 string | Session or input channel. Required. |
| `actor` | optional UTF-8 string | Entity responsible for the input. Absence is valid. |
| `externalId` | optional `{ nameSpace, value }` | Original platform ID and its namespace. Absence is valid; the `value` alone is never treated as globally unique. |
| `kind` | `EventKind` | `Discrete`, `StreamStarted`, `StreamData`, or `StreamEnded`. |
| `occurredTime` | signed nanoseconds plus `TimeDomain` | Source occurrence time. `UnixEpochNanoseconds` is UTC wall time; `SourceMonotonicNanoseconds` is monotonic time scoped to the source instance. |
| `receivedTime.utcNanoseconds` | signed nanoseconds | System UTC wall time stamped at successful publication. |
| `receivedTime.monotonicNanoseconds` | signed nanoseconds | Process-local steady clock stamped at successful publication. Use for latency only; never interpret it as UTC or compare it with another process. |
| `streamId` | optional UTF-8 string | Source stream identity. Scoped by source adapter, source instance, and channel. |
| `sequence` | optional `uint64` | Stream ordering number. Start is 0; data begins at 1; end carries the next expected number. |
| `parts` | ordered `vector<ContentPart>` | Original order of text and media pieces is preserved. |

`StreamStarted` and `StreamEnded` have no parts. `StreamData` must have at least one part. Discrete events cannot carry stream fields. `SensoryEventChannel::Receive` returns a `StreamSequenceResult` for every stream event; it updates the validator while still holding the dequeue lock, so multiple MPMC consumers cannot process a later stream state before an earlier one. Payload work can run in parallel after `Receive` returns. The channel's publish lock defines the enqueue order for concurrent producers; producers sharing one stream must coordinate sequence assignment to match that order. An out-of-order sequence is returned with `UnexpectedSequence`.

## Content parts and media ownership

V1 defines two part structures:

- `TextPart` stores valid UTF-8 bytes.
- `MediaPart` stores `MediaKind`, an immutable resource reference, a format ID, and optional stream-relative position and duration in nanoseconds.

`MediaKind` is `Image`, `Audio`, `Video`, `Haptic`, or `Extension`. Format identifiers such as `image/jpeg`, `audio/pcm-s16le`, and application-specific identifiers distinguish encodings; formats do not add new event kinds. Audio and video `StreamData` parts require a position. Position and duration are stream-relative, not wall-clock values.

`MediaResourceRef` owns a `shared_ptr<const Storage>`. Copying a reference only increments the shared reference count. `Adopt(vector&&)` transfers the vector allocation; `Copy(span)` intentionally copies bytes once. The public view is `span<const byte>`, so consumers cannot modify another consumer's payload. The final reference releases storage. Process-local pointers and shared references are not persistent resource IDs.

## Validation limits

`ValidateEvent` rejects malformed fields with a `ValidationErrorCode`; it does not repair or silently truncate input. Defaults are:

| Limit | Default |
|---|---:|
| General identity string | 512 bytes |
| Text part | 1 MiB |
| Format ID | 256 bytes |
| Parts per event | 64 |
| Bytes per media resource | 512 MiB |

Applications can provide different limits. Validation checks the event ID, required source/channel, UTF-8, event/stream field combinations, sequence shape, part count, media kind, live resource reference, media size, and format ID. `StreamSequenceValidator` accepts the same `ValidationLimits` object as the channel and caps active stream and pending dropped-frame stream state (default 1,024 each). It retains at most 64 merged, disjoint dropped-sequence intervals per stream; a new eviction that cannot be recorded is refused and its incoming event is returned as `Full`. `ForgetStream` explicitly removes state after abnormal termination.

## Channel semantics

`SensoryEventChannel` is a fixed-capacity, mutex-protected MPMC slot queue. Event slots, free-slot indices, FIFO links, and the video-eviction index are allocated up front. The normal legal LatestOnly replacement and queue unlink are O(1) while holding the queue mutex. If the dropped-frame tracking table cannot record the oldest video candidate, the channel checks at most the first 1,024 video candidates, including the oldest. This exceptional fallback is bounded by a fixed limit; a trackable candidate beyond that search window may remain queued and the new publication returns `Full`. The original incoming event is always returned on backpressure. Capacity must be from 1 through 65,536.

- `Reliable`: `TryPublish` returns `Full` when capacity is exhausted. Every failed publish returns the original event in `PublishResult::rejectedEvent` so a moved event can be retried or persisted. `PublishWait` waits for room and can be cancelled with a stop token.
- `LatestOnly`: only `StreamData` events whose non-empty `parts` list contains video media parts exclusively are eligible for eviction. An event containing text, audio, images, or any other part is preserved as a whole. The channel checks at most the first 1,024 video candidates, including the oldest, if the tracking state cannot record an eviction. If none within that bounded search is trackable, publication returns `Full` with the original event in `rejectedEvent`, even if a later candidate exists outside the search window. `PublishWait` and `PublishFor` also return this `Full` result immediately; the caller can retry after stream-tracking state changes. `droppedStaleFrames` counts each replacement. The channel records exact evicted sequence intervals and accepts a forward gap only when every missing sequence was dropped; duplicate, backward, and unrecorded gaps remain errors. Start and End events are never eviction candidates.
- `Close`: wakes blocked readers and publishers. Readers drain events already accepted, then receive `Closed`. Publishers receive `Closed`. Stop tokens return `Cancelled`.

`ISensoryEventPublisher::PublishFor` provides a bounded wait for final control events. If a LatestOnly publish sees a video candidate but the bounded search cannot find a trackable eviction, `PublishWait` and `PublishFor` return `Full` immediately rather than waiting for their timeout. The simulated video adapter's `StopAndReport` waits at most 100 ms for `StreamEnded`, unless the caller cancels the wait. If the end cannot be accepted, it returns an abnormal result and calls `AbandonStream`, which removes queued events for that stream and clears sequence state. A worker exception records failure and attempts this same finalization itself; a later `StopAndReport` joins the worker and returns the stored stop result. The legacy `Stop()` remains available and performs bounded cleanup; `RuntimeStatus()` exposes failures to callers that use that API.

`Reliable` means explicit in-process backpressure only. It is not durable storage or cross-process delivery. Callers must stop/join worker threads before destroying their channel.

## Versioned codec and transport boundary

`CodecBoundary.h` defines `ISensoryEventCodecV1`, schema version 1, and `IPersistentMediaResourceResolver`. An implementation maps process-local `MediaResourceRef` values to stable namespaced resource IDs before encoding. Event descriptors can be encoded independently from resource bytes; bytes need a separate storage/transfer mechanism and must be available to the resolver before decoding.

This boundary is declarations only. V1 does not implement binary encoding, serialization compatibility negotiation, shared memory, a resource store, or network transport. No process pointer may be serialized.
