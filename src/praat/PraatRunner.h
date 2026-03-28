#pragma once

#include <juce_core/juce_core.h>
#include <functional>

// Launches Praat as a child process in batch mode and waits for it to finish.
//
// Invocation:
//   <praatExecutable> --run <scriptFile> [arg1 arg2 ...]
//
// The `--run` flag puts Praat into headless batch mode:
//   - No GUI window is opened
//   - writeInfoLine: output goes to stdout (captured here)
//   - Error messages go to stderr (captured here)
//
// All methods are synchronous and BLOCKING.
// Call only from the worker thread (JobDispatcher).
//
// See docs/adr/ADR-002 for the IPC strategy.
class PraatRunner
{
public:
    // The outcome of a single Praat invocation.
    struct RunOutcome
    {
        bool         exitedSuccessfully;  // true if exit code was 0
        int          exitCode;
        juce::String standardOutput;      // everything Praat printed via writeInfoLine:
        juce::String standardError;       // Praat error messages, if any
    };

    explicit PraatRunner (juce::File praatExecutableFile);

    // Launches Praat and waits for it to complete (up to processTimeoutMilliseconds).
    // scriptArguments are appended after the script file path on the command line.
    // shouldCancel is polled every 100 ms — if it returns true the process is killed
    // and a cancelled outcome is returned (exitedSuccessfully=false, exitCode=-2).
    RunOutcome launchPraatWithScript (const juce::File& scriptFile,
                                      const juce::StringArray& scriptArguments = {},
                                      std::function<bool()> shouldCancel = nullptr);

    // Returns true if the stored executable path points to a file that exists
    // and appears executable. Does not actually launch Praat.
    bool isPraatExecutableReachable() const;

    // How long we wait for Praat before killing the process and returning an error.
    static constexpr int processTimeoutMilliseconds { 30'000 };

private:
    juce::File praatExecutableFile_;
};
