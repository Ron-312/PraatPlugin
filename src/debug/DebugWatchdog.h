#pragma once

// ─── DebugWatchdog ────────────────────────────────────────────────────────────
//
// Enable with the CMake option:  -DPRAATPLUGIN_DEBUG_LOGGING=ON
//
// What it does:
//   • Posts a "ping" to the message thread every 50 ms from a background thread.
//     If the message thread doesn't respond within stallThresholdMs, a STALL
//     entry is recorded and written to the log file.
//   • Keeps a rolling buffer of the 100 most recent entries for the in-plugin
//     DevPanel (toggle via the DEV button in the plugin header).
//   • Mirrors every entry to  %TEMP%\PraatPlugin\debug.log  (Windows) or
//     $TMPDIR/PraatPlugin/debug.log  (macOS) for attaching to bug reports.
//
// Usage — C++ side:
//   1. Own one DebugWatchdog as the LAST private member of PraatPluginProcessor
//      (so it is constructed last and its automatic destructor runs first).
//   2. Call start() at the end of the processor constructor.
//   3. Call stop() as the VERY FIRST line of ~PraatPluginProcessor().
//   4. Use PRAAT_LOG / PRAAT_LOG_ERR / PRAAT_TIME_SCOPE at relevant sites.
//
// Usage — QA side:
//   Click the DEV button in the plugin header to show the DevPanel.
//   It shows live message-thread latency and a colour-coded event log.
//   Hit "copy log" to paste into a bug report.
// ─────────────────────────────────────────────────────────────────────────────

#ifdef PRAATPLUGIN_DEBUG_LOGGING

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <memory>

class DebugWatchdog : private juce::Thread
{
public:
    explicit DebugWatchdog (int stallThresholdMs = 250);
    ~DebugWatchdog() override;

    // Call at the end of the owning processor's constructor.
    void start();

    // Call as the VERY FIRST line of the owning processor's destructor.
    // Safe to call from the message thread.
    void stop();

    // ── Data for the DevPanel (message thread only) ───────────────────────────
    int  lastLatencyMs() const noexcept { return lastLatencyMs_.load(); }

    // Returns a juce::Array<juce::var>, newest entry first.
    // Each element: { t: string, msg: string, kind: "info"|"slow"|"stall"|"error" }.
    juce::var recentEntriesForUI() const;

    // ── Logging (any thread) ──────────────────────────────────────────────────
    static void          log (const juce::String& msg,
                               const juce::String& kind = "info");
    static juce::File    logFilePath();
    static DebugWatchdog* instance() noexcept { return instance_.load(); }

private:
    static constexpr int kMaxEntries = 100;
    static constexpr int kPollMs     = 50;

    void run() override;
    void postPing();
    void addEntry (const juce::String& msg, const juce::String& kind);
    void writeLine (const juce::String& line);

    int stallThresholdMs_;
    // Shared with callAsync lambdas so they can safely check whether the
    // watchdog is still alive before dereferencing 'this'.
    std::shared_ptr<std::atomic<bool>> alive_;

    std::atomic<bool>    pingInFlight_    { false };
    std::atomic<int64_t> pingDispatchedMs_{ 0 };
    std::atomic<bool>    currentlyStalled_{ false };
    std::atomic<int64_t> stallStartMs_    { 0 };
    std::atomic<int>     lastLatencyMs_   { 0 };

    struct Entry { juce::String t, msg, kind; };
    mutable juce::CriticalSection entriesLock_;
    juce::Array<Entry> entries_;

    mutable juce::CriticalSection fileLock_;
    std::unique_ptr<juce::FileLogger> logger_;

    static std::atomic<DebugWatchdog*> instance_;
};

// ─── RAII scope timer ─────────────────────────────────────────────────────────
// Records a "slow" (or "stall") entry when the scope exceeds thresholdMs.
// Drop PRAAT_TIME_SCOPE("label") at the top of any function or block.
struct ScopedBlockTimer
{
    explicit ScopedBlockTimer (const char* label, int thresholdMs = 30) noexcept
        : label_ (label), thresholdMs_ (thresholdMs),
          startMs_ (juce::Time::getCurrentTime().toMilliseconds()) {}

    ~ScopedBlockTimer()
    {
        const auto elapsed = static_cast<int> (
            juce::Time::getCurrentTime().toMilliseconds() - startMs_);
        if (elapsed >= thresholdMs_)
            DebugWatchdog::log (juce::String (label_) + "  " + juce::String (elapsed) + " ms",
                                elapsed >= 100 ? "stall" : "slow");
    }

    const char* label_;
    int         thresholdMs_;
    int64_t     startMs_;
};

// ─── Convenience macros ───────────────────────────────────────────────────────
#define PRAAT_LOG(msg)          DebugWatchdog::log (msg)
#define PRAAT_LOG_ERR(msg)      DebugWatchdog::log (msg, "error")
#define PRAAT_TIME_SCOPE(label) ScopedBlockTimer _st_##__LINE__ (label)

// ─────────────────────────────────────────────────────────────────────────────
#else  // !PRAATPLUGIN_DEBUG_LOGGING

#define PRAAT_LOG(msg)          do {} while (0)
#define PRAAT_LOG_ERR(msg)      do {} while (0)
#define PRAAT_TIME_SCOPE(label) do {} while (0)

#endif // PRAATPLUGIN_DEBUG_LOGGING
