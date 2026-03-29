#ifdef PRAATPLUGIN_DEBUG_LOGGING

#include "debug/DebugWatchdog.h"

DebugWatchdog* DebugWatchdog::instance_ = nullptr;

static juce::String nowStr()
{
    return juce::Time::getCurrentTime().toString (false, true, true, true);
}

// ─────────────────────────────────────────────────────────────────────────────

DebugWatchdog::DebugWatchdog (int stallThresholdMs)
    : juce::Thread ("PraatPlugin::Watchdog"),
      stallThresholdMs_ (stallThresholdMs),
      alive_ (std::make_shared<std::atomic<bool>> (true))
{
    instance_ = this;

    logFilePath().getParentDirectory().createDirectory();

    // 5 MB rolling cap — keeps a few sessions worth of history without
    // growing unbounded.
    logger_ = std::make_unique<juce::FileLogger> (
        logFilePath(), "PraatPlugin debug log", 5 * 1024 * 1024);

    const auto header = "=== SESSION START  "
                        + juce::Time::getCurrentTime().toString (true, true)
                        + " ===\n    log: "
                        + logFilePath().getFullPathName();
    writeLine (header);
    addEntry (header, "info");
}

DebugWatchdog::~DebugWatchdog()
{
    stop();
    writeLine ("=== SESSION END ===\n");
    instance_ = nullptr;
}

void DebugWatchdog::start()
{
    pingDispatchedMs_ = juce::Time::getCurrentTime().toMilliseconds();
    startThread (juce::Thread::Priority::background);
    postPing();
}

void DebugWatchdog::stop()
{
    if (! isThreadRunning())
        return;

    // Mark dead first so any callAsync lambda that fires during stopThread's
    // wait window will safely skip accessing 'this'.
    *alive_ = false;
    signalThreadShouldExit();
    stopThread (500);
}

void DebugWatchdog::log (const juce::String& msg, const juce::String& kind)
{
    if (instance_ == nullptr)
        return;

    instance_->writeLine (nowStr() + "  " + msg);
    instance_->addEntry  (msg, kind);
}

juce::File DebugWatchdog::logFilePath()
{
    return juce::File::getSpecialLocation (juce::File::tempDirectory)
               .getChildFile ("PraatPlugin")
               .getChildFile ("debug.log");
}

juce::var DebugWatchdog::recentEntriesForUI() const
{
    juce::ScopedLock sl (entriesLock_);

    juce::Array<juce::var> out;
    out.ensureStorageAllocated (entries_.size());

    // Return newest-first so the DevPanel shows recent events at the top.
    for (int i = entries_.size() - 1; i >= 0; --i)
    {
        auto obj = std::make_unique<juce::DynamicObject>();
        obj->setProperty ("t",    entries_[i].t);
        obj->setProperty ("msg",  entries_[i].msg);
        obj->setProperty ("kind", entries_[i].kind);
        out.add (juce::var (obj.release()));
    }

    return juce::var (out);
}

// ─────────────────────────────────────────────────────────────────────────────
// Background thread
// ─────────────────────────────────────────────────────────────────────────────

void DebugWatchdog::run()
{
    while (! threadShouldExit())
    {
        wait (kPollMs);

        if (! pingInFlight_.load())
        {
            postPing();
            continue;
        }

        const auto waitedMs = static_cast<int> (
            juce::Time::getCurrentTime().toMilliseconds() - pingDispatchedMs_.load());

        if (waitedMs < stallThresholdMs_)
            continue;

        if (! currentlyStalled_.exchange (true))
        {
            stallStartMs_ = pingDispatchedMs_.load();
            const auto msg = "STALL BEGIN — message thread unresponsive for "
                             + juce::String (waitedMs) + " ms";
            writeLine ("!!! " + nowStr() + "  " + msg);
            addEntry  (msg, "stall");
        }
        else
        {
            // Continue logging while stalled so the panel shows the growing count.
            const auto msg = "STALL ONGOING — " + juce::String (waitedMs) + " ms blocked";
            writeLine ("!!! " + nowStr() + "  " + msg);
            addEntry  (msg, "stall");
        }
    }
}

void DebugWatchdog::postPing()
{
    pingInFlight_     = true;
    pingDispatchedMs_ = juce::Time::getCurrentTime().toMilliseconds();

    // Capture by value so the lambda doesn't dangle if the watchdog is stopped
    // between the post and the execution.
    auto aliveRef = alive_;

    juce::MessageManager::callAsync ([this, aliveRef]
    {
        if (! aliveRef->load())
            return;

        const auto latencyMs = static_cast<int> (
            juce::Time::getCurrentTime().toMilliseconds() - pingDispatchedMs_.load());

        lastLatencyMs_ = latencyMs;

        if (currentlyStalled_.exchange (false))
        {
            const auto totalMs = static_cast<int> (
                juce::Time::getCurrentTime().toMilliseconds() - stallStartMs_.load());
            const auto msg = "STALL END — total duration " + juce::String (totalMs) + " ms";
            writeLine ("!!! " + nowStr() + "  " + msg);
            addEntry  (msg, "stall");
        }
        else if (latencyMs >= 30)
        {
            const auto msg = "latency  " + juce::String (latencyMs) + " ms";
            writeLine ("  " + nowStr() + "  " + msg);
            addEntry  (msg, latencyMs >= 100 ? "stall" : "slow");
        }

        pingInFlight_ = false;
    });
}

// ─────────────────────────────────────────────────────────────────────────────

void DebugWatchdog::addEntry (const juce::String& msg, const juce::String& kind)
{
    juce::ScopedLock sl (entriesLock_);
    entries_.add ({ nowStr(), msg, kind });
    while (entries_.size() > kMaxEntries)
        entries_.remove (0);
}

void DebugWatchdog::writeLine (const juce::String& line)
{
    juce::ScopedLock sl (fileLock_);
    if (logger_)
        logger_->logMessage (line);
}

#endif // PRAATPLUGIN_DEBUG_LOGGING
