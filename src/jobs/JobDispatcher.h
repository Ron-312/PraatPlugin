#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include "jobs/JobQueue.h"
#include "jobs/AnalysisJob.h"
#include "praat/PraatInstallationLocator.h"
#include <functional>
#include <mutex>

// Single background worker thread that processes AnalysisJobs from a JobQueue.
//
// For each job it:
//   1. Writes job.audioToAnalyze to a temp WAV file via WavFileWriter
//   2. Spawns Praat via PraatRunner with the job's script
//   3. Parses stdout via ResultParser
//   4. Marks the job CompletedSuccessfully or FailedWithError
//   5. Calls the completion callback (which fires AsyncUpdater on the message thread)
//   6. Cleans up temp files
//
// Audio samples arrive pre-copied into the job's audioToAnalyze buffer
// (see ADR-008).  The dispatcher no longer needs AudioCapture.
//
// Owns the worker thread lifecycle.
// Start with startDispatchingJobs(), stop with stopAndWaitForCurrentJobToFinish().
//
// See docs/adr/ADR-003 and ADR-008.
class JobDispatcher : private juce::Thread
{
public:
    // Called on the message thread when a job finishes (success or failure).
    using JobCompletionCallback = std::function<void (AnalysisJob completedJob)>;

    JobDispatcher (JobQueue& queue,
                   PraatInstallationLocator& praatLocator);

    ~JobDispatcher() override;

    // Starts the background thread. Safe to call multiple times (no-op if already running).
    void startDispatchingJobs();

    // Signals the worker thread to stop after the current job finishes,
    // then blocks until the thread exits. Safe to call from the message thread.
    void stopAndWaitForCurrentJobToFinish();

    // Registers the callback invoked (via juce::MessageManager) when each job completes.
    // Replace at any time from the message thread.
    void onJobCompleted (JobCompletionCallback callback);

    bool isCurrentlyProcessingAJob() const noexcept;

    // Signals the currently running Praat process to be killed.
    // No-op if no job is running.  Safe to call from any thread.
    void cancelCurrentJob();

    // How long stopAndWaitForCurrentJobToFinish() waits before giving up.
    static constexpr int processTimeoutForStopMs_ { 35'000 };

private:
    // juce::Thread entry point — runs on the worker thread.
    void run() override;

    void executeJob (AnalysisJob& job);
    void writeAudioToTempFile (AnalysisJob& job);
    void runPraatScript (AnalysisJob& job);

    // If the script wrote an output WAV, moves it out of the UUID temp dir so
    // cleanUpTempFilesForJob() doesn't delete it.  Updates job.outputAudioWavFile
    // to the stable path.
    void promoteOutputAudioFileIfPresent (AnalysisJob& job);

    void parseJobResult (AnalysisJob& job, const juce::String& praatStdout);
    void cleanUpTempFilesForJob (const AnalysisJob& job);
    void deliverCompletedJobToMessageThread (AnalysisJob completedJob);

    JobQueue&                    queue_;
    PraatInstallationLocator&    praatLocator_;

    mutable std::mutex           callbackMutex_;
    JobCompletionCallback        completionCallback_;

    std::atomic<bool>            isProcessingAJob_  { false };
    std::atomic<bool>            cancelRequested_   { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JobDispatcher)
};
