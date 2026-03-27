#include "jobs/JobDispatcher.h"
#include "audio/WavFileWriter.h"
#include "praat/PraatRunner.h"
#include "results/ResultParser.h"

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

void JobDispatcher::run()
{
    // Worker thread loop — runs until signalConsumerToStop() is called.
    while (! threadShouldExit())
    {
        AnalysisJob nextJob;

        if (! queue_.waitForNextJobAndDequeue (nextJob))
            break;   // Queue was stopped

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
    }
}

void JobDispatcher::runPraatScript (AnalysisJob& job)
{
    PraatRunner praatRunner (job.praatExecutableFile);
    ResultParser resultParser;

    // Build the argument list: inputFile and outputFile are always first,
    // followed by any user-controlled script parameters in form-block order.
    // Praat reads all arguments positionally, so order must match exactly.
    juce::StringArray scriptArgs;
    scriptArgs.add (job.capturedAudioWavFile.getFullPathName());
    scriptArgs.add (job.outputAudioWavFile.getFullPathName());
    for (int i = 0; i < job.scriptParameters.size(); ++i)
        scriptArgs.add (job.scriptParameters.getAllValues()[i]);

    const auto runOutcome = praatRunner.launchPraatWithScript (
        job.praatScriptFile, scriptArgs);

    if (runOutcome.exitedSuccessfully)
    {
        job.result       = resultParser.parseOutputFromSuccessfulRun (runOutcome.standardOutput);
        job.currentState = JobState::CompletedSuccessfully;
    }
    else
    {
        job.result       = resultParser.buildFailureResult (runOutcome.standardError,
                                                             runOutcome.standardError);
        job.currentState = JobState::FailedWithError;
        job.errorDescription = runOutcome.standardError.isEmpty()
                                   ? "Praat exited with code " + juce::String (runOutcome.exitCode)
                                   : runOutcome.standardError;
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
