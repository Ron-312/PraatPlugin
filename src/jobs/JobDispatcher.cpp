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

// ── Form argument extractor ────────────────────────────────────────────────────
//
// Praat's --run mode maps CLI arguments to form fields positionally and requires
// ALL fields to be supplied.  If a script declares N fields but only 2 arguments
// are passed (inputFile, outputFile), Praat aborts with:
//   "Found 2 arguments but expected more"
//
// This helper reads the script's form block and extracts the default value of
// every field beyond the first two.  Those defaults are appended automatically
// so any user script with extra parameters (stretch factor, sample rate, etc.)
// runs with its declared defaults without requiring extra UI.
static juce::StringArray extractExtraFormDefaults (const juce::File& scriptFile)
{
    juce::StringArray extras;
    const juce::String content = scriptFile.loadFileAsString();

    bool inForm    = false;
    int  fieldIndex = 0;

    // All Praat form field type keywords (case-insensitive).
    static const char* const kFieldTypes[] = {
        "sentence", "word", "text", "real", "positive", "natural",
        "integer", "boolean", "choice", "optionmenu", nullptr
    };

    for (auto line : juce::StringArray::fromLines (content))
    {
        line = line.trimStart();

        if (line.startsWithIgnoreCase ("form ") || line.equalsIgnoreCase ("form"))
        {
            inForm = true;
            continue;
        }
        if (line.equalsIgnoreCase ("endform"))
            break;
        if (! inForm)
            continue;

        for (int t = 0; kFieldTypes[t] != nullptr; ++t)
        {
            const juce::String prefix = juce::String (kFieldTypes[t]) + " ";
            if (line.startsWithIgnoreCase (prefix))
            {
                ++fieldIndex;
                if (fieldIndex > 2)
                {
                    // Line format: "<type> <fieldName> [<default>...]"
                    // Everything after the field name is the default value.
                    const auto afterKeyword = line.substring ((int) prefix.length()).trimStart();
                    const int  nameEnd      = afterKeyword.indexOfChar (' ');
                    const auto defaultVal   = (nameEnd >= 0)
                                                 ? afterKeyword.substring (nameEnd + 1).trimStart()
                                                 : juce::String{};
                    extras.add (defaultVal);
                }
                break;
            }
        }
    }

    return extras;
}

void JobDispatcher::runPraatScript (AnalysisJob& job)
{
    PraatRunner praatRunner (job.praatExecutableFile);
    ResultParser resultParser;

    // Always supply inputFile (arg 1) and outputFile (arg 2).
    // If the user supplied extra args via the parameter panel, use those;
    // otherwise fall back to the script's declared defaults.
    juce::StringArray scriptArgs = {
        job.capturedAudioWavFile.getFullPathName(),
        job.outputAudioWavFile.getFullPathName()
    };
    if (job.extraScriptArgs.size() > 0)
        scriptArgs.addArray (job.extraScriptArgs);
    else
        scriptArgs.addArray (extractExtraFormDefaults (job.praatScriptFile));

    const auto runOutcome = praatRunner.launchPraatWithScript (
        job.praatScriptFile,
        scriptArgs);

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
