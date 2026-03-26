# ADR-003: Isolate the Audio Thread Behind a Lock-Free Ring Buffer

**Date:** 2026-03-25
**Status:** Accepted
**Deciders:** Project founder

---

## Context

JUCE plugins process audio in `AudioProcessor::processBlock()`, which is called on a dedicated **audio thread** by the DAW. This thread has a hard real-time deadline: it must return before the current audio buffer is consumed by the audio hardware, or the DAW will produce an audible glitch (dropout, crackle, silence).

The audio thread must **never**:
- Acquire a non-trivial mutex (may block indefinitely)
- Allocate or free heap memory (system allocator is not real-time safe)
- Perform file I/O (syscall, may block on disk)
- Spawn processes or make system calls (expensive, unpredictable latency)
- Call into JUCE's MessageManager (runs on a different thread)

PraatPlugin needs audio data from `processBlock` to eventually reach `PraatRunner`, which needs a WAV file on disk. The challenge is crossing the thread boundary safely.

## Decision

We will use a **lock-free ring buffer** (`juce::AbstractFifo` + `juce::AudioBuffer<float>`) in `AudioCapture` as the sole data path from the audio thread to the rest of the system.

```
Audio thread                    Message/Worker threads
────────────                    ──────────────────────
processBlock()
  │
  └─ AudioCapture::appendSamplesFromProcessBlock()
         │
         └─ AbstractFifo::prepareToWrite()   ← lock-free, single-producer
              write samples to ring buffer
              AbstractFifo::finishedWrite()
```

When the user clicks **Analyze**, the message thread signals the worker thread to flush the ring buffer to a WAV file. The worker thread then uses `AbstractFifo::prepareToRead()` to drain the buffer — also lock-free from the producer's perspective (single-producer, single-consumer).

`processBlock` itself contains **no branching on analysis state**, no function calls that may block, and no memory allocation. It calls `AudioCapture::appendSamplesFromProcessBlock()` unconditionally.

## Rationale

`juce::AbstractFifo` is explicitly designed for lock-free, single-producer single-consumer use. JUCE's documentation and community usage confirm it is safe to write from the audio thread and read from a different thread without a mutex, as long as only one producer and one consumer exist — which is our case.

Alternative data structures (e.g. a `std::mutex`-protected `std::vector`, `juce::CriticalSection`) would be semantically simpler but would occasionally block the audio thread when the consumer happens to hold the lock, causing dropouts. Even a "try_lock" approach is insufficient: if the lock is contended, we lose data or we spin, both of which are unacceptable.

## Alternatives Considered

| Option | Why not chosen |
|--------|---------------|
| `std::mutex` + `std::vector` (message thread drains) | Acquiring the mutex in processBlock can block the audio thread if the message thread holds it. Non-deterministic. |
| `juce::CriticalSection` (JUCE mutex) | Same problem — not real-time safe on contention |
| Allocating a new buffer each processBlock and posting it to a queue | Allocation in processBlock is itself a real-time violation. |
| Direct WAV write from processBlock | File I/O is real-time unsafe. Never acceptable. |

## Consequences

**Positive:**
- Audio thread remains deterministic and deadline-safe
- No dropouts or glitches from plugin logic
- Clean, well-understood data flow with a single ownership boundary

**Negative / Trade-offs:**
- The ring buffer has a fixed capacity. If it fills faster than it's drained, new samples overwrite old ones. For a 30-second capture window at 48 kHz stereo, the buffer needs ~11 million float samples (~44 MB). This is pre-allocated in `prepareToPlay`.
- The "capture window" is always a rolling 30-second tail of audio, not the entire session. This is intentional and should be documented in the UI.

**Risks and mitigations:**
- Risk: Developer adds blocking code to `processBlock` in the future (e.g. a convenient log statement).
  Mitigation: All code paths in `processBlock` and `appendSamplesFromProcessBlock` are labeled with `// AUDIO-THREAD SAFE` comments. Code review must flag any call that isn't explicitly marked safe.
- Risk: Buffer overflow if `prepareToPlay` hasn't been called before first `processBlock`.
  Mitigation: `AudioCapture` initializes the ring buffer with a default size and is re-initialized in `prepareToPlay`.
