#include "jobs/JobDispatcher.h"
#include "audio/WavFileWriter.h"
#include "praat/PraatRunner.h"
#include "praat/PraatFormParser.h"
#include "praat/ScriptInvocation.h"
#include "scripts/ScriptCapabilities.h"
#include "results/ResultParser.h"
#include "debug/DebugWatchdog.h"

JobDispatcher::JobDispatcher (JobQueue& queue,
                               PraatInstallationLocator& praatLocator)
    : juce::Thread ("PraatPlugin::JobDispatcher"),
      queue_        (queue),
      praatLocator_ (praatLocator)
{
}

JobDispatcher::~JobDispatcher()
{
    stopAndWaitForCurrentJobToFinish();
}

void JobDispatcher::startDispatchingJobs()
{
    if (! isThreadRunning())
        startThread();
}

void JobDispatcher::stopAndWaitForCurrentJobToFinish()
{
    queue_.signalConsumerToStop();
    stopThread (processTimeoutForStopMs_);
}

void JobDispatcher::onJobCompleted (JobCompletionCallback callback)
{
    std::lock_guard<std::mutex> lock (callbackMutex_);
    completionCallback_ = std::move (callback);
}

bool JobDispatcher::isCurrentlyProcessingAJob() const noexcept
{
    return isProcessingAJob_.load();
}

void JobDispatcher::cancelCurrentJob()
{
    cancelRequested_ = true;
}

void JobDispatcher::run()
{
    // Worker thread loop — runs until signalConsumerToStop() is called.
    while (! threadShouldExit())
    {
        AnalysisJob nextJob;

        if (! queue_.waitForNextJobAndDequeue (nextJob))
            break;   // Queue was stopped

        cancelRequested_  = false;
        isProcessingAJob_ = true;
        executeJob (nextJob);
        isProcessingAJob_ = false;
    }
}

void JobDispatcher::executeJob (AnalysisJob& job)
{
    job.currentState = JobState::Running;

    writeAudioToTempFile (job);

    if (job.currentState == JobState::Running)   // write succeeded
        runPraatScript (job);

    // Move output WAV out of the temp dir BEFORE cleanup so it survives.
    promoteOutputAudioFileIfPresent (job);

    cleanUpTempFilesForJob (job);
    deliverCompletedJobToMessageThread (job);
}

void JobDispatcher::writeAudioToTempFile (AnalysisJob& job)
{
    const auto tempDirectory = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                   .getChildFile ("PraatPlugin")
                                   .getChildFile (job.jobIdentifier.toDashedString());

    tempDirectory.createDirectory();
    job.capturedAudioWavFile  = tempDirectory.getChildFile ("input.wav");
    job.outputAudioWavFile    = tempDirectory.getChildFile ("output.wav");

    // Write the pre-copied audio buffer (see ADR-008: file-based audio workflow).
    const auto writeResult = WavFileWriter::writeAudioBufferToWavFile (
        job.audioToAnalyze, job.audioSampleRate, job.capturedAudioWavFile);

    if (writeResult != WavFileWriter::WriteResult::Success)
    {
        job.currentState     = JobState::FailedWithError;
        job.errorDescription = "Could not write audio to temp file ("
                             + WavFileWriter::describeWriteResult (writeResult)
                             + "): "
                             + job.capturedAudioWavFile.getFullPathName();
        PRAAT_LOG_ERR ("writeAudioToTempFile failed: " + job.errorDescription);
    }
}

void JobDispatcher::runPraatScript (AnalysisJob& job)
{
    if (! job.praatExecutableFile.existsAsFile())
    {
        job.currentState     = JobState::FailedWithError;
        job.errorDescription = "Praat executable not found. Click \"PRAAT NOT FOUND\" in the plugin header to locate it.";
        PRAAT_LOG_ERR ("runPraatScript: executable not found at "
                       + job.praatExecutableFile.getFullPathName());
        return;
    }

    PRAAT_LOG ("Running script: " + job.praatScriptFile.getFileName());

    // Refuse up front what would fail or hang (unsupported script, a file
    // dialog headless Praat can't show) instead of waiting for the timeout.
    if (const auto problem = ScriptCapabilities::analyse (job.praatScriptFile)
                                 .checkBeforeRun (job.scriptParameters);
        problem.isNotEmpty())
    {
        job.currentState     = JobState::FailedWithError;
        job.errorDescription = problem;
        job.result           = ResultParser().buildFailureResult (problem, problem);
        return;
    }

    // Praat 7 blocks file writes from --run scripts without --FULL-TRUST, and
    // every script must save its result WAV.  The user chose the script.
    PraatRunner praatRunner (job.praatExecutableFile,
                             PraatRunner::executableNeedsFullTrust (job.praatExecutableFile));
    ResultParser resultParser;

    // Detect which calling model the script uses.
    // Scripts with a sentence inputFile form field → standard file-arg model.
    // Scripts with no sentence inputFile (community scripts) → Sound-object model,
    // requires a generated wrapper.
    const auto allFields = PraatFormParser::parseAllFields (job.praatScriptFile);

    const bool needsWrapper = ScriptInvocation::needsSoundObjectWrapper (allFields);

    juce::File  scriptToRun = job.praatScriptFile;
    juce::StringArray scriptArgs;

    if (needsWrapper)
    {
        // Generate a wrapper script that loads audio into Praat's object list,
        // calls the community script with the user's param values, then saves
        // the result Sound to our output path.
        const auto wrapperContent = ScriptInvocation::buildWrapperScript (
            job.capturedAudioWavFile,
            job.outputAudioWavFile,
            job.praatScriptFile,
            PraatFormParser::parseExtraParams (job.praatScriptFile),
            job.scriptParameters,
            job.secondaryAudioFile.existsAsFile() ? job.secondaryAudioFile : juce::File());

        const auto wrapperFile = job.capturedAudioWavFile.getParentDirectory()
                                     .getChildFile ("wrapper.praat");
        wrapperFile.replaceWithText (wrapperContent);
        scriptToRun = wrapperFile;
        // No CLI args — the wrapper is self-contained.
    }
    else
    {
        // Standard model: build positional args from the form's sentence fields.
        scriptArgs = ScriptInvocation::buildScriptArgs (
            job.praatScriptFile,
            job.capturedAudioWavFile,
            job.outputAudioWavFile,
            job.scriptParameters);
    }

    const auto runOutcome = praatRunner.launchPraatWithScript (
        scriptToRun, scriptArgs, [this]() { return cancelRequested_.load(); });

    if (runOutcome.exitCode == -2)   // -2 = our sentinel for user-cancelled
    {
        job.currentState     = JobState::Cancelled;
        job.errorDescription = "Analysis cancelled.";
        PRAAT_LOG ("Script cancelled: " + job.praatScriptFile.getFileName());
    }
    else if (runOutcome.exitedSuccessfully)
    {
        job.result       = resultParser.parseOutputFromSuccessfulRun (runOutcome.standardOutput);
        job.currentState = JobState::CompletedSuccessfully;
        PRAAT_LOG ("Script completed: " + job.praatScriptFile.getFileName());
    }
    else
    {
        job.result       = resultParser.buildFailureResult (runOutcome.standardError,
                                                             runOutcome.standardError);
        job.currentState = JobState::FailedWithError;
        job.errorDescription = runOutcome.standardError.isEmpty()
                                   ? "Praat exited with code " + juce::String (runOutcome.exitCode)
                                   : runOutcome.standardError;
        PRAAT_LOG_ERR ("Script failed (" + job.praatScriptFile.getFileName() + "): "
                       + job.errorDescription);
    }
}

void JobDispatcher::promoteOutputAudioFileIfPresent (AnalysisJob& job)
{
    if (! job.outputAudioWavFile.existsAsFile())
        return;

    // Move the output WAV to a stable location that survives temp dir cleanup.
    // Named after the job UUID so concurrent jobs never collide.
    const auto stableOutputDir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                     .getChildFile ("PraatPlugin")
                                     .getChildFile ("output");

    stableOutputDir.createDirectory();

    const auto stablePath = stableOutputDir.getChildFile (
        job.jobIdentifier.toDashedString() + "_output.wav");

    job.outputAudioWavFile.moveFileTo (stablePath);
    job.outputAudioWavFile = stablePath;
}

void JobDispatcher::cleanUpTempFilesForJob (const AnalysisJob& job)
{
    // Delete the UUID temp directory and everything in it.
    // macOS will also clean TMPDIR on reboot as a fallback (see ADR-004).
    if (job.capturedAudioWavFile.existsAsFile())
        job.capturedAudioWavFile.getParentDirectory().deleteRecursively();
}

void JobDispatcher::deliverCompletedJobToMessageThread (AnalysisJob completedJob)
{
    // Copy the callback to avoid holding the lock during invocation.
    JobCompletionCallback callbackCopy;
    {
        std::lock_guard<std::mutex> lock (callbackMutex_);
        callbackCopy = completionCallback_;
    }

    if (callbackCopy)
    {
        // Post to the message thread — safe to call from any thread.
        juce::MessageManager::callAsync ([callback = std::move (callbackCopy),
                                          job      = std::move (completedJob)] () mutable
        {
            callback (std::move (job));
        });
    }
}
