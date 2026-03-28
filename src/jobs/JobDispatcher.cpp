#include "jobs/JobDispatcher.h"
#include "audio/WavFileWriter.h"
#include "praat/PraatRunner.h"
#include "praat/PraatFormParser.h"
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
    }
}

// ── Script argument builder ────────────────────────────────────────────────────
//
// Builds the complete positional CLI argument list for a Praat --run invocation.
//
// Praat maps CLI arguments to form fields positionally.  We parse ALL form
// fields in order (via PraatFormParser::parseAllFields) and emit one argument
// per field:
//   • isInputFilePath  → actual captured WAV path
//   • isOutputFilePath → actual output WAV path
//   • everything else  → user-supplied value (from scriptParameters) or field default
//
// This correctly handles scripts where optionMenu or other fields appear BEFORE
// the sentence inputFile / sentence outputFile fields.
static juce::StringArray buildScriptArgs (const juce::File&           scriptFile,
                                          const juce::File&           inputWav,
                                          const juce::File&           outputWav,
                                          const juce::StringPairArray& userParams)
{
    juce::StringArray args;

    for (const auto& field : PraatFormParser::parseAllFields (scriptFile))
    {
        if (field.isInputFilePath)
        {
            args.add (inputWav.getFullPathName());
        }
        else if (field.isOutputFilePath)
        {
            args.add (outputWav.getFullPathName());
        }
        else
        {
            const auto userVal = userParams.getValue (field.name, {});
            auto argVal = userVal.isNotEmpty() ? userVal : field.defaultValue;

            // Praat --run expects the option NAME string for optionMenu/choice
            // fields, not a numeric index.  The UI stores values as 1-based
            // indices ("1", "2", …), so convert here before passing to Praat.
            if (field.type == "optionmenu" || field.type == "choice")
            {
                const int zeroBasedIdx = argVal.getIntValue() - 1;
                if (juce::isPositiveAndBelow (zeroBasedIdx, field.options.size()))
                    argVal = field.options[zeroBasedIdx];
            }

            args.add (argVal);
        }
    }

    return args;
}

// ── Sound-object wrapper builder ───────────────────────────────────────────────
//
// Community scripts expect a Sound object to already be selected in Praat's
// object list (they start with `sound = selected("Sound")`).  They have no
// sentence inputFile / outputFile form fields — audio comes from the GUI selection.
//
// To bridge this with our headless file-based model, we generate a tiny wrapper
// script at runtime that:
//   1. Loads the captured WAV into Praat as a Sound object.
//   2. Calls the community script via runScript with the user's param values.
//   3. Saves whatever Sound is selected after the script finishes to outputFile.
//
// Detection: a script needs the wrapper if parseAllFields() finds no isInputFilePath
// field — i.e. no sentence/word field is the 1st of its type (so no file-arg model).
static juce::String praatStringEscape (const juce::String& s)
{
    return s.replace ("\\", "\\\\").replace ("\"", "\\\"");
}

static juce::String buildWrapperScript (const juce::File&            inputWav,
                                         const juce::File&            outputWav,
                                         const juce::File&            communityScript,
                                         const juce::Array<FormParam>& params,
                                         const juce::StringPairArray& userValues)
{
    juce::String w;

    // Step 1 — load the plugin's captured audio as a Praat Sound object
    // and immediately select it so the community script finds it selected.
    w += "inputSound__ = Read from file: \"" + praatStringEscape (inputWav.getFullPathName()) + "\"\n";
    w += "selectObject: inputSound__\n\n";

    // Step 2 — call the community script with all its form params in order.
    // comment-type form fields are automatically skipped by Praat's runScript;
    // parseExtraParams already excludes them, so the count is correct.
    w += "runScript: \"" + praatStringEscape (communityScript.getFullPathName()) + "\"";

    for (const auto& param : params)
    {
        const auto userVal = userValues.getValue (param.name, {});
        auto val = userVal.isNotEmpty() ? userVal : param.defaultValue;

        // Convert 1-based optionMenu index to the option name string.
        if (param.type == "optionmenu" || param.type == "choice")
        {
            const int idx = val.getIntValue() - 1;
            if (juce::isPositiveAndBelow (idx, param.options.size()))
                val = param.options[idx];
            w += ", \"" + praatStringEscape (val) + "\"";
        }
        else if (param.type == "sentence" || param.type == "word" || param.type == "text")
        {
            w += ", \"" + praatStringEscape (val) + "\"";
        }
        else    // boolean, real, positive, natural, integer
        {
            w += ", " + (val.isNotEmpty() ? val : juce::String ("0"));
        }
    }

    w += "\n\n";

    // Step 3 — save the resulting Sound (if any) to the output path.
    w += "if numberOfSelected (\"Sound\") > 0\n";
    w += "    selectObject: selected (\"Sound\")\n";
    w += "    Save as WAV file: \"" + praatStringEscape (outputWav.getFullPathName()) + "\"\n";
    w += "endif\n";

    return w;
}

void JobDispatcher::runPraatScript (AnalysisJob& job)
{
    PraatRunner praatRunner (job.praatExecutableFile);
    ResultParser resultParser;

    // Detect which calling model the script uses.
    // Scripts with a sentence inputFile form field → standard file-arg model.
    // Scripts with no sentence inputFile (community scripts) → Sound-object model,
    // requires a generated wrapper.
    const auto allFields = PraatFormParser::parseAllFields (job.praatScriptFile);

    bool needsWrapper = true;
    for (const auto& f : allFields)
        if (f.isInputFilePath) { needsWrapper = false; break; }

    juce::File  scriptToRun = job.praatScriptFile;
    juce::StringArray scriptArgs;

    if (needsWrapper)
    {
        // Generate a wrapper script that loads audio into Praat's object list,
        // calls the community script with the user's param values, then saves
        // the result Sound to our output path.
        const auto wrapperContent = buildWrapperScript (
            job.capturedAudioWavFile,
            job.outputAudioWavFile,
            job.praatScriptFile,
            PraatFormParser::parseExtraParams (job.praatScriptFile),
            job.scriptParameters);

        const auto wrapperFile = job.capturedAudioWavFile.getParentDirectory()
                                     .getChildFile ("wrapper.praat");
        wrapperFile.replaceWithText (wrapperContent);
        scriptToRun = wrapperFile;
        // No CLI args — the wrapper is self-contained.
    }
    else
    {
        // Standard model: build positional args from the form's sentence fields.
        scriptArgs = buildScriptArgs (
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
    }
    else if (runOutcome.exitedSuccessfully)
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
