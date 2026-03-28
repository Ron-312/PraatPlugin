#include "plugin/PraatPluginProcessor.h"
#include "plugin/PraatPluginEditor.h"
#include "audio/WavFileWriter.h"
#include <BinaryData.h>

PraatPluginProcessor::PraatPluginProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      jobDispatcher_ (jobQueue_, praatLocator_)
{
    audioFormatManager_.registerBasicFormats();   // WAV, AIFF, FLAC, etc.

    // Wire up the job completion callback.
    // JobDispatcher delivers this on the message thread when each job finishes.
    jobDispatcher_.onJobCompleted ([this] (AnalysisJob completedJob)
    {
        receiveCompletedJob (std::move (completedJob));
    });

    jobDispatcher_.startDispatchingJobs();

    // Write bundled .praat scripts to app-support folder and auto-load them.
    installBundledScriptsIfNeeded();
}

PraatPluginProcessor::~PraatPluginProcessor()
{
    jobDispatcher_.stopAndWaitForCurrentJobToFinish();
    transportSource_.setSource (nullptr);

    // Clean up temp files kept alive for the transport reader.
    if (processedAudioFile_.existsAsFile())
        processedAudioFile_.deleteFile();
    if (liveCaptureFile_.existsAsFile())
        liveCaptureFile_.deleteFile();
}

//==============================================================================
// AudioProcessor

void PraatPluginProcessor::prepareToPlay (double sampleRate, int maximumExpectedBlockSize)
{
    transportSource_.prepareToPlay (maximumExpectedBlockSize, sampleRate);

    captureSampleRate_ = sampleRate;
    audioCapture_.prepareForCapture (sampleRate,
                                     juce::jmax (1, getTotalNumInputChannels()));
}

void PraatPluginProcessor::releaseResources()
{
    transportSource_.releaseResources();
    audioCapture_.clearCaptureBuffer();
}

void PraatPluginProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer& /*midiMessages*/)
{
    // Capture the incoming track audio FIRST, before renderNextAudioBlock()
    // clears the buffer.  AUDIO-THREAD SAFE: lock-free AbstractFifo.  See ADR-003.
    if (isCapturing_.load (std::memory_order_relaxed))
        audioCapture_.appendSamplesFromProcessBlock (buffer);

    // Then fill the output with playback audio (or silence).
    renderNextAudioBlock (buffer);
}

void PraatPluginProcessor::renderNextAudioBlock (juce::AudioBuffer<float>& buffer)
{
    // During live capture the input buffer already holds the track audio.
    // Returning without clearing it passes audio through transparently so the
    // user can hear what is being recorded in real time.
    if (isCapturing_.load (std::memory_order_relaxed))
        return;

    buffer.clear();

    if (! transportSource_.isPlaying())
        return;

    // Stop automatically when we reach the end of the selected region.
    if (transportSource_.getCurrentPosition() >= playbackRegionEndSeconds_.load (std::memory_order_relaxed))
    {
        transportSource_.stop();
        return;
    }

    juce::AudioSourceChannelInfo info (&buffer, 0, buffer.getNumSamples());
    transportSource_.getNextAudioBlock (info);
}

//==============================================================================
// Bundled scripts

void PraatPluginProcessor::installBundledScriptsIfNeeded()
{
    // Scripts are embedded as BinaryData and extracted to a user-accessible
    // folder on first run.  Users can edit the files in that folder; we only
    // write a file if it does not already exist so edits are never overwritten.
    const auto scriptsDir = juce::File::getSpecialLocation (
                                juce::File::userApplicationDataDirectory)
                                .getChildFile ("PraatPlugin")
                                .getChildFile ("scripts");

    scriptsDir.createDirectory();

    // Helper: write bundled data to disk only when the file is absent.
    auto extractIfMissing = [&] (const char* data, int size, const juce::String& filename)
    {
        const auto dest = scriptsDir.getChildFile (filename);
        if (! dest.existsAsFile())
            dest.replaceWithData (data, static_cast<size_t> (size));
    };

    extractIfMissing (BinaryData::pitch_analysis_praat,
                      BinaryData::pitch_analysis_praatSize,
                      "pitch_analysis.praat");

    extractIfMissing (BinaryData::formant_analysis_praat,
                      BinaryData::formant_analysis_praatSize,
                      "formant_analysis.praat");

    extractIfMissing (BinaryData::fuzz_distortion_praat,
                      BinaryData::fuzz_distortion_praatSize,
                      "fuzz_distortion.praat");

    extractIfMissing (BinaryData::robot_voice_praat,
                      BinaryData::robot_voice_praatSize,
                      "robot_voice.praat");

    extractIfMissing (BinaryData::pitch_shift_praat,
                      BinaryData::pitch_shift_praatSize,
                      "pitch_shift.praat");

    extractIfMissing (BinaryData::wavefolder_praat,
                      BinaryData::wavefolder_praatSize,
                      "wavefolder.praat");

    extractIfMissing (BinaryData::stereo_phaser_praat,
                      BinaryData::stereo_phaser_praatSize,
                      "stereo_phaser.praat");

    extractIfMissing (BinaryData::spectral_reverb_praat,
                      BinaryData::spectral_reverb_praatSize,
                      "spectral_reverb.praat");

    extractIfMissing (BinaryData::paulstretch_praat,
                      BinaryData::paulstretch_praatSize,
                      "paulstretch.praat");

    // Auto-load so the UI shows scripts immediately without the user
    // having to click "Load Scripts...".
    scriptManager_.loadScriptsFromDirectory (scriptsDir);
}

//==============================================================================
// Live capture

void PraatPluginProcessor::startLiveCapture()
{
    audioCapture_.clearCaptureBuffer();
    isCapturing_ = true;
}

void PraatPluginProcessor::stopLiveCapture()
{
    isCapturing_ = false;

    if (! audioCapture_.hasAudioReadyForAnalysis())
        return;

    // Use a UUID in the filename so each recording gets a unique path.
    // This guarantees the editor's file-path change-detection always sees a new
    // file, even when re-recording (same path would produce a false "no change").
    const auto captureFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("PraatPlugin")
                                 .getChildFile ("capture_" + juce::Uuid().toDashedString() + ".wav");

    captureFile.getParentDirectory().createDirectory();

    juce::Thread::launch ([this, captureFile] () mutable
    {
        audioCapture_.writeCapturedAudioToWavFile (captureFile);

        juce::MessageManager::callAsync ([this, captureFile] ()
        {
            // Delete the previous capture file now that we have a new one.
            // The old transport reader held it open; releasing it here is safe
            // because loadAudioFromFile() disconnects the transport first.
            if (liveCaptureFile_.existsAsFile())
                liveCaptureFile_.deleteFile();

            // Keep the new file alive — the transport reader needs it for playback.
            liveCaptureFile_ = captureFile;

            loadAudioFromFile (liveCaptureFile_);
            audioCapture_.clearCaptureBuffer();
        });
    });
}

bool PraatPluginProcessor::isCapturing() const noexcept
{
    return isCapturing_.load();
}

float PraatPluginProcessor::captureInputPeakLevel() const noexcept
{
    return audioCapture_.latestPeakLevel();
}

void PraatPluginProcessor::copyLiveCaptureSnapshotTo (juce::AudioBuffer<float>& dest) const
{
    audioCapture_.copyCurrentContentsTo (dest);
}

double PraatPluginProcessor::captureSampleRate() const noexcept
{
    return captureSampleRate_;
}

//==============================================================================
// File-based audio workflow (ADR-008)

bool PraatPluginProcessor::loadAudioFromFile (const juce::File& audioFile)
{
    // Stop any current playback before replacing the source.
    transportSource_.stop();
    transportSource_.setSource (nullptr);
    audioReaderSource_.reset();
    processedReaderSource_.reset();

    // Clear the processed slot — it belongs to the previous original file.
    if (processedAudioFile_.existsAsFile())
        processedAudioFile_.deleteFile();
    processedAudioFile_ = juce::File();
    processedAudioBuffer_.setSize (0, 0, false, true, false);
    hasProcessedAudio_  = false;

    // Create a format reader for the file.
    std::unique_ptr<juce::AudioFormatReader> reader (
        audioFormatManager_.createReaderFor (audioFile));

    if (reader == nullptr)
        return false;

    // Read the entire file into the in-memory buffer (for waveform display and
    // region extraction at analysis time).
    const int numChannels = static_cast<int> (reader->numChannels);
    const int numSamples  = static_cast<int> (reader->lengthInSamples);

    loadedAudioBuffer_.setSize (numChannels, numSamples, false, true, false);
    reader->read (&loadedAudioBuffer_, 0, numSamples, 0, true, true);

    loadedAudioSampleRate_ = reader->sampleRate;
    loadedAudioFile_       = audioFile;
    selectedRegionSeconds_ = {};   // clear old selection

    // Set up the transport source with a fresh reader for playback.
    // (We create a new reader here because the first one may have its position
    // advanced after the read() call above.)
    std::unique_ptr<juce::AudioFormatReader> playbackReader (
        audioFormatManager_.createReaderFor (audioFile));

    if (playbackReader != nullptr)
    {
        audioReaderSource_ = std::make_unique<juce::AudioFormatReaderSource> (
            playbackReader.release(), true /* takes ownership */);

        transportSource_.setSource (audioReaderSource_.get(),
                                    0, nullptr,
                                    loadedAudioSampleRate_);
    }

    audioIsLoaded_ = true;
    ++loadedAudioVersion_;   // signal to editor that the buffer content has changed
    return true;
}

bool PraatPluginProcessor::hasAudioLoaded() const noexcept
{
    return audioIsLoaded_.load();
}

const juce::AudioBuffer<float>& PraatPluginProcessor::loadedAudioBuffer() const noexcept
{
    return loadedAudioBuffer_;
}

double PraatPluginProcessor::loadedAudioSampleRate() const noexcept
{
    return loadedAudioSampleRate_;
}

juce::File PraatPluginProcessor::loadedAudioFile() const noexcept
{
    return loadedAudioFile_;
}

uint32_t PraatPluginProcessor::loadedAudioVersion() const noexcept
{
    return loadedAudioVersion_.load();
}

//==============================================================================
// Processed audio

bool PraatPluginProcessor::loadProcessedAudioFromFile (const juce::File& audioFile)
{
    // If a previous processed source exists the transport may still be pointing
    // at it (e.g. the user played it and stopped, but never switched back to
    // the original).  Destroy the old reader source only AFTER disconnecting
    // the transport so the audio thread cannot read from the freed memory.
    if (processedReaderSource_ != nullptr)
    {
        transportSource_.stop();
        transportSource_.setSource (nullptr);
    }

    // Read the processed audio into an in-memory buffer for waveform display.
    std::unique_ptr<juce::AudioFormatReader> reader (
        audioFormatManager_.createReaderFor (audioFile));

    if (reader == nullptr)
        return false;

    const int numChannels = static_cast<int> (reader->numChannels);
    const int numSamples  = static_cast<int> (reader->lengthInSamples);

    processedAudioBuffer_.setSize (numChannels, numSamples, false, true, false);
    reader->read (&processedAudioBuffer_, 0, numSamples, 0, true, true);
    processedAudioSampleRate_ = reader->sampleRate;

    // Replace the stable processed file (delete the old one first).
    if (processedAudioFile_.existsAsFile())
        processedAudioFile_.deleteFile();
    processedAudioFile_ = audioFile;   // take ownership of the stable temp path

    // Create a fresh reader for the transport source (the buffer read above
    // may have advanced the first reader's internal position).
    std::unique_ptr<juce::AudioFormatReader> playbackReader (
        audioFormatManager_.createReaderFor (audioFile));

    processedReaderSource_.reset();
    if (playbackReader != nullptr)
    {
        processedReaderSource_ = std::make_unique<juce::AudioFormatReaderSource> (
            playbackReader.release(), true);
    }

    hasProcessedAudio_ = true;
    return true;
}

bool PraatPluginProcessor::hasProcessedAudio() const noexcept
{
    return hasProcessedAudio_.load();
}

juce::File PraatPluginProcessor::processedAudioFile() const noexcept
{
    return processedAudioFile_;
}

const juce::AudioBuffer<float>& PraatPluginProcessor::processedAudioBuffer() const noexcept
{
    return processedAudioBuffer_;
}

double PraatPluginProcessor::processedAudioSampleRate() const noexcept
{
    return processedAudioSampleRate_;
}

//==============================================================================
// Region selection

void PraatPluginProcessor::setAnalysisRegionInSeconds (juce::Range<double> regionInSeconds)
{
    selectedRegionSeconds_ = regionInSeconds;
}

juce::Range<double> PraatPluginProcessor::selectedRegionInSeconds() const noexcept
{
    return selectedRegionSeconds_;
}

//==============================================================================
// Playback

void PraatPluginProcessor::startPlaybackOfOriginalRegion()
{
    if (! hasAudioLoaded() || audioReaderSource_ == nullptr)
        return;

    // Reconnect the transport to the original reader in case it was last pointing
    // at the processed source after a startPlaybackOfProcessedOutput() call.
    transportSource_.stop();
    transportSource_.setSource (audioReaderSource_.get(), 0, nullptr, loadedAudioSampleRate_);

    const double totalDuration = loadedAudioSampleRate_ > 0.0
        ? static_cast<double> (loadedAudioBuffer_.getNumSamples()) / loadedAudioSampleRate_
        : 0.0;

    const double startSec = selectedRegionSeconds_.getLength() > 0.0
        ? selectedRegionSeconds_.getStart() : 0.0;
    const double endSec   = selectedRegionSeconds_.getLength() > 0.0
        ? selectedRegionSeconds_.getEnd()   : totalDuration;

    isPlayingOriginal_.store (true, std::memory_order_relaxed);
    playbackRegionEndSeconds_.store (endSec, std::memory_order_relaxed);
    transportSource_.setPosition (startSec);
    transportSource_.start();
}

void PraatPluginProcessor::startPlaybackOfProcessedOutput()
{
    if (! hasProcessedAudio_.load() || processedReaderSource_ == nullptr)
        return;

    // Switch the transport source to the processed reader.
    transportSource_.stop();
    transportSource_.setSource (processedReaderSource_.get(), 0, nullptr, processedAudioSampleRate_);

    const double duration = processedAudioSampleRate_ > 0.0
        ? static_cast<double> (processedAudioBuffer_.getNumSamples()) / processedAudioSampleRate_
        : 0.0;

    isPlayingOriginal_.store (false, std::memory_order_relaxed);
    playbackRegionEndSeconds_.store (duration, std::memory_order_relaxed);
    transportSource_.setPosition (0.0);
    transportSource_.start();
}

void PraatPluginProcessor::stopPlayback()
{
    transportSource_.stop();
}

bool PraatPluginProcessor::isPlayingBack() const noexcept
{
    return transportSource_.isPlaying();
}

double PraatPluginProcessor::currentPlaybackPosition() const noexcept
{
    return transportSource_.getCurrentPosition();
}

bool PraatPluginProcessor::isPlayingOriginal() const noexcept
{
    return isPlayingOriginal_.load (std::memory_order_relaxed);
}

//==============================================================================
// Analysis

void PraatPluginProcessor::beginAnalysisOfSelectedRegion (const juce::StringArray& extraScriptArgs)
{
    if (! isPraatAvailable())
        return;

    if (! hasAudioLoaded())
        return;

    if (! scriptManager_.hasActiveScriptSelected())
        return;

    // Determine which samples to analyse.
    const double totalDuration = loadedAudioSampleRate_ > 0.0
        ? static_cast<double> (loadedAudioBuffer_.getNumSamples()) / loadedAudioSampleRate_
        : 0.0;

    const double startSec = selectedRegionSeconds_.getLength() > 0.0
        ? selectedRegionSeconds_.getStart()
        : 0.0;

    const double endSec   = selectedRegionSeconds_.getLength() > 0.0
        ? selectedRegionSeconds_.getEnd()
        : totalDuration;

    const int startSample = static_cast<int> (startSec * loadedAudioSampleRate_);
    const int numSamples  = static_cast<int> ((endSec - startSec) * loadedAudioSampleRate_);

    if (numSamples <= 0)
        return;

    // Copy selected region into the job (message-thread to worker-thread hand-off).
    AnalysisJob job;
    job.jobIdentifier     = juce::Uuid();
    job.praatScriptFile   = scriptManager_.activeScript();
    job.praatExecutableFile = praatLocator_.locatePraatExecutable();
    job.audioSampleRate   = loadedAudioSampleRate_;
    job.audioToAnalyze.setSize (loadedAudioBuffer_.getNumChannels(), numSamples, false, true, false);

    for (int ch = 0; ch < loadedAudioBuffer_.getNumChannels(); ++ch)
    {
        job.audioToAnalyze.copyFrom (ch, 0,
                                     loadedAudioBuffer_, ch,
                                     juce::jmin (startSample, loadedAudioBuffer_.getNumSamples() - 1),
                                     numSamples);
    }

    job.extraScriptArgs = extraScriptArgs;
    job.currentState    = JobState::Pending;

    analysisIsInProgress_ = true;
    jobQueue_.enqueueAnalysisJob (std::move (job));
}

//==============================================================================
// Script parameters

juce::StringPairArray PraatPluginProcessor::currentScriptParameters() const
{
    const juce::File activeScript = scriptManager_.activeScript();
    if (! activeScript.existsAsFile())
        return {};

    // Parse the form block to get the canonical parameter list (names + defaults).
    const auto parameterDefinitions = ScriptParameterParser::parse (activeScript);
    if (parameterDefinitions.isEmpty())
        return {};

    // Build a StringPairArray ordered to match the form block.
    // Use stored user values where available; fall back to defaults otherwise.
    juce::StringPairArray result;
    result.setIgnoresCase (false);

    const auto storedIt = scriptParameterValues_.find (activeScript.getFullPathName());
    const bool hasStoredValues = (storedIt != scriptParameterValues_.end());

    for (const auto& paramDef : parameterDefinitions)
    {
        juce::String value;

        if (hasStoredValues)
        {
            const juce::String storedValue = storedIt->second.getValue (paramDef.name, {});
            value = storedValue.isNotEmpty() ? storedValue
                                             : juce::String (paramDef.defaultValue);
        }
        else
        {
            value = juce::String (paramDef.defaultValue);
        }

        result.set (paramDef.name, value);
    }

    return result;
}

void PraatPluginProcessor::setScriptParameter (const juce::String& parameterName,
                                                const juce::String& value)
{
    const juce::File activeScript = scriptManager_.activeScript();
    if (! activeScript.existsAsFile())
        return;

    scriptParameterValues_[activeScript.getFullPathName()].set (parameterName, value);
}

//==============================================================================
// State queries

bool PraatPluginProcessor::isPraatAvailable() const noexcept
{
    return praatLocator_.isPraatInstalled();
}

bool PraatPluginProcessor::isAnalysisInProgress() const noexcept
{
    return analysisIsInProgress_.load();
}

AnalysisResult PraatPluginProcessor::mostRecentAnalysisResult() const
{
    std::lock_guard<std::mutex> lock (latestResultMutex_);
    return latestResult_;
}

juce::String PraatPluginProcessor::currentStatusMessage() const
{
    if (! isPraatAvailable())
        return praatLocator_.describeSearchResult();

    if (isCapturing())
        return "Recording from track — press Stop Rec when done";

    if (isPlayingBack())
        return "Playing...";

    if (isAnalysisInProgress())
        return "Analyzing...";

    const auto result = mostRecentAnalysisResult();

    if (result.failureReason.isNotEmpty())
        return "Error: " + result.failureReason;

    if (result.parsedSuccessfully)
        return "Analysis complete";

    if (! hasAudioLoaded())
        return "Load an audio file to begin";

    return "Ready \xe2\x80\x94 select a region and press Analyze";
}

//==============================================================================
// Async result delivery

void PraatPluginProcessor::receiveCompletedJob (AnalysisJob completedJob)
{
    // Called on the message thread via MessageManager::callAsync (see JobDispatcher).
    {
        std::lock_guard<std::mutex> lock (latestResultMutex_);
        latestResult_ = completedJob.result;
    }
    analysisIsInProgress_ = false;

    // If the script produced audio output, load it into the PROCESSED slot.
    // The original stays intact so the user can A/B compare.
    // NOTE: do NOT delete the file here — it stays on disk for the transport
    //       reader.  It will be cleaned up when a new script runs, a new
    //       original is loaded, or the processor is destroyed.
    if (completedJob.outputAudioWavFile.existsAsFile())
    {
        loadProcessedAudioFromFile (completedJob.outputAudioWavFile);
        audioOutputWasReceived_ = true;
    }

    // Notify the editor (if open) via AsyncUpdater.
    triggerAsyncUpdate();
}

bool PraatPluginProcessor::consumeAudioOutputNotification() noexcept
{
    return audioOutputWasReceived_.exchange (false);
}

void PraatPluginProcessor::handleAsyncUpdate()
{
    // Message thread: editor polls via Timer, so this is a no-op hook point
    // for future expansion.
}

juce::AudioProcessorEditor* PraatPluginProcessor::createEditor()
{
    return new PraatPluginEditor (*this);
}

//==============================================================================
// State persistence

void PraatPluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree state ("PraatPluginState");

    state.setProperty ("activeScriptPath",
                        scriptManager_.activeScript().getFullPathName(), nullptr);

    state.setProperty ("praatExecutableOverride",
                        praatLocator_.userOverriddenExecutablePath().getFullPathName(), nullptr);

    // Persist the last loaded audio file so we can restore the waveform display.
    state.setProperty ("lastLoadedAudioFile",
                        loadedAudioFile_.getFullPathName(), nullptr);

    // Persist per-script parameter values so sliders survive session reloads.
    // Each script gets a child node identified by its file path.
    for (const auto& [scriptPath, paramValues] : scriptParameterValues_)
    {
        if (paramValues.size() == 0)
            continue;

        juce::ValueTree scriptNode ("ScriptParams");
        scriptNode.setProperty ("scriptPath", scriptPath, nullptr);

        for (int i = 0; i < paramValues.size(); ++i)
            scriptNode.setProperty (paramValues.getAllKeys()[i],
                                    paramValues.getAllValues()[i], nullptr);

        state.appendChild (scriptNode, nullptr);
    }

    juce::MemoryOutputStream stream (destData, false);
    state.writeToStream (stream);
}

void PraatPluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto state = juce::ValueTree::readFromData (data, static_cast<size_t> (sizeInBytes));
    if (! state.isValid()) return;

    const auto scriptPath = state.getProperty ("activeScriptPath").toString();
    if (scriptPath.isNotEmpty())
        scriptManager_.setActiveScript (juce::File (scriptPath));

    const auto praatOverride = state.getProperty ("praatExecutableOverride").toString();
    if (praatOverride.isNotEmpty())
        praatLocator_.overrideExecutablePathWithUserChoice (juce::File (praatOverride));

    const auto lastAudioPath = state.getProperty ("lastLoadedAudioFile").toString();
    if (lastAudioPath.isNotEmpty())
    {
        const juce::File lastAudioFile (lastAudioPath);
        if (lastAudioFile.existsAsFile())
            loadAudioFromFile (lastAudioFile);
    }

    // Restore per-script parameter values from child nodes.
    for (int i = 0; i < state.getNumChildren(); ++i)
    {
        const auto child = state.getChild (i);
        if (! child.hasType ("ScriptParams"))
            continue;

        const auto childScriptPath = child.getProperty ("scriptPath").toString();
        if (childScriptPath.isEmpty())
            continue;

        juce::StringPairArray paramValues;
        for (int p = 0; p < child.getNumProperties(); ++p)
        {
            const auto propName = child.getPropertyName (p).toString();
            if (propName == "scriptPath")
                continue;
            paramValues.set (propName, child.getProperty (child.getPropertyName (p)).toString());
        }

        if (paramValues.size() > 0)
            scriptParameterValues_[childScriptPath] = std::move (paramValues);
    }
}

//==============================================================================
// Required by JUCE to instantiate the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PraatPluginProcessor();
}
