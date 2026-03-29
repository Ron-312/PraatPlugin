#include "praat/PraatRunner.h"
#include "debug/DebugWatchdog.h"

PraatRunner::PraatRunner (juce::File praatExecutableFile)
    : praatExecutableFile_ (std::move (praatExecutableFile))
{
}

PraatRunner::RunOutcome PraatRunner::launchPraatWithScript (
    const juce::File& scriptFile,
    const juce::StringArray& scriptArguments,
    std::function<bool()> shouldCancel)
{
    // Build the full command line:
    //   /Applications/Praat.app/Contents/MacOS/Praat --run /path/to/script.praat [args...]
    juce::StringArray commandLine;
    commandLine.add (praatExecutableFile_.getFullPathName());
    commandLine.add ("--run");
    commandLine.add (scriptFile.getFullPathName());
    commandLine.addArray (scriptArguments);

    juce::ChildProcess praatProcess;

    const bool launchSucceeded = praatProcess.start (commandLine);

    if (! launchSucceeded)
    {
        PRAAT_LOG_ERR ("PraatRunner: failed to launch Praat at "
                       + praatExecutableFile_.getFullPathName());
        return { false, -1,
                 {},
                 "Failed to launch Praat. Check that the executable exists and is runnable." };
    }

    // Poll every 100 ms so we can honour cancellation requests and enforce
    // the overall timeout without blocking for up to 30 seconds at a time.
    int totalWaitedMs = 0;
    static constexpr int kPollIntervalMs = 100;

    while (! praatProcess.waitForProcessToFinish (kPollIntervalMs))
    {
        totalWaitedMs += kPollIntervalMs;

        if (shouldCancel && shouldCancel())
        {
            praatProcess.kill();
            return { false, -2, {}, "Analysis cancelled." };
        }

        if (totalWaitedMs >= processTimeoutMilliseconds)
        {
            praatProcess.kill();
            PRAAT_LOG_ERR ("PraatRunner: script timed out after "
                           + juce::String (processTimeoutMilliseconds / 1000) + " s — "
                           + scriptFile.getFileName());
            return { false, -1,
                     {},
                     "Praat timed out after " +
                         juce::String (processTimeoutMilliseconds / 1000) + " seconds." };
        }
    }

    const juce::String allOutput = praatProcess.readAllProcessOutput();
    const int          exitCode  = praatProcess.getExitCode();

    // Praat does not provide a separate stderr stream via ChildProcess on macOS;
    // error output is mixed into the combined output stream.
    // If exit code is non-zero, treat the full output as error output.
    if (exitCode != 0)
    {
        PRAAT_LOG_ERR ("PraatRunner: Praat exited with code " + juce::String (exitCode)
                       + " — " + allOutput.substring (0, 200));
        return { false, exitCode, {}, allOutput };
    }

    return { true, 0, allOutput, {} };
}

bool PraatRunner::isPraatExecutableReachable() const
{
    return praatExecutableFile_.existsAsFile();
}
