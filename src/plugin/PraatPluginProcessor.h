#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_events/juce_events.h>
#include "audio/AudioCapture.h"
#include "praat/PraatInstallationLocator.h"
#include "scripts/ScriptManager.h"
#include "jobs/JobQueue.h"
#include "jobs/JobDispatcher.h"
#include "results/AnalysisResult.h"
#include <mutex>

// The central coordinator of PraatPlugin.
//
// Owns all subsystems and enforces the threading contract:
//   - Audio thread   : lock-free AudioCapture (when recording) + AudioTransportSource (playback)
//   - Message thread : file loading, state reads/writes, UI updates, job dispatch
//   - Worker thread  : WavFileWriter, Praat process, ResultParser
//
// Two ways to load audio:
//   1. loadAudioFromFile()     — pick a file from disk (Load Audio button)
//   2. startLiveCapture()      — captures from processBlock while the DAW plays
//      stopLiveCapture()       — finalises capture, writes temp WAV, loads it
//
// Inherits AsyncUpdater so JobDispatcher can safely deliver results back
// to the message thread after a job finishes.
//
// See docs/ARCHITECTURE.md and docs/adr/ADR-008.
class PraatPluginProcessor : public juce::AudioProcessor,
                               public juce::AsyncUpdater
{
public:
    PraatPluginProcessor();
    ~PraatPluginProcessor() override;

    //──────────────────────────────────────────────────────────────────────────
    // AudioProcessor interface (required overrides)
    //──────────────────────────────────────────────────────────────────────────

    void prepareToPlay (double sampleRate, int maximumExpectedBlockSize) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "PraatPlugin"; }

    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }

    double getTailLengthSeconds() const override { return 0.0; }

    int  getNumPrograms()                             override { return 1; }
    int  getCurrentProgram()                          override { return 0; }
    void setCurrentProgram (int)                      override {}
    const juce::String getProgramName (int)           override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //──────────────────────────────────────────────────────────────────────────
    // Audio loading — two routes to get audio into the waveform display
    // All methods below are MESSAGE-THREAD ONLY unless noted.
    //──────────────────────────────────────────────────────────────────────────

    // Loads the audio file into memory.
    // Replaces any previously loaded file.
    // Returns true if the file was read successfully.
    bool loadAudioFromFile (const juce::File& audioFile);

    //──────────────────────────────────────────────────────────────────────────
    // Live capture from DAW track input
    //──────────────────────────────────────────────────────────────────────────

    // Begins recording audio from processBlock input.
    // The DAW must be playing and audio must be routed to the plugin's input.
    void startLiveCapture();

    // Stops recording, writes the ring buffer to a temp WAV, and loads it as
    // the new working audio buffer (updates the waveform display).
    // The finalisation happens on a background thread, so this returns quickly.
    void stopLiveCapture();

    // True while capture is active. AUDIO-THREAD SAFE (reads std::atomic).
    bool isCapturing() const noexcept;

    // Peak amplitude (0..1) of the most recently captured block.
    // Used by the editor's real-time VU meter. Safe to call from any thread.
    float captureInputPeakLevel() const noexcept;

    // Fills dest with a non-consuming snapshot of the ring buffer so the editor
    // can show a growing live waveform.  Does NOT consume the captured audio.
    // MESSAGE-THREAD ONLY.
    void copyLiveCaptureSnapshotTo (juce::AudioBuffer<float>& dest) const;

    // DAW sample rate set in prepareToPlay.  Used to map live-capture samples
    // to time when displaying the growing waveform.
    double captureSampleRate() const noexcept;

    // True if an audio file has been loaded successfully.
    bool hasAudioLoaded() const noexcept;

    // Reference to the in-memory audio data (message-thread only).
    // Do not hold a reference across a loadAudioFromFile() call.
    const juce::AudioBuffer<float>& loadedAudioBuffer() const noexcept;

    // Sample rate of the currently loaded file (0.0 if nothing loaded).
    double loadedAudioSampleRate() const noexcept;

    // Path of the currently loaded file (empty if nothing loaded).
    juce::File loadedAudioFile() const noexcept;

    //──────────────────────────────────────────────────────────────────────────
    // Processed audio  (set by a Praat script that wrote audio output)
    //──────────────────────────────────────────────────────────────────────────

    // Loads a script's audio output into the processed slot WITHOUT replacing
    // the original.  Called by receiveCompletedJob on the message thread.
    bool loadProcessedAudioFromFile (const juce::File& audioFile);

    // True if a script has produced audio output since the last loadAudioFromFile().
    bool hasProcessedAudio() const noexcept;

    // In-memory buffer of the processed audio (message-thread only).
    const juce::AudioBuffer<float>& processedAudioBuffer() const noexcept;

    // Sample rate of the processed audio (0.0 if none available).
    double processedAudioSampleRate() const noexcept;

    //──────────────────────────────────────────────────────────────────────────
    // Region selection
    //──────────────────────────────────────────────────────────────────────────

    // Sets the region (start, end) in seconds that Analyze will operate on.
    // An empty range (start == end) implies the full file.
    void setAnalysisRegionInSeconds (juce::Range<double> regionInSeconds);

    // Returns the current analysis region.
    juce::Range<double> selectedRegionInSeconds() const noexcept;

    //──────────────────────────────────────────────────────────────────────────
    // Playback  (audio thread safe via AudioTransportSource)
    //──────────────────────────────────────────────────────────────────────────

    // Plays the original audio from the currently selected region (or the full
    // file if nothing is selected).  Does nothing if no audio is loaded.
    void startPlaybackOfOriginalRegion();

    // Plays the processed output from the beginning (full clip).
    // Does nothing if no processed audio is available yet.
    void startPlaybackOfProcessedOutput();

    // Stops playback immediately.
    void stopPlayback();

    // True if the transport source is currently playing.
    // Safe to call from any thread.
    bool isPlayingBack() const noexcept;

    // Current playback head position in seconds.  0.0 when stopped.
    double currentPlaybackPosition() const noexcept;

    // True when the transport is playing the ORIGINAL (A) source.
    // False when playing the PROCESSED (B) source.
    bool isPlayingOriginal() const noexcept;

    //──────────────────────────────────────────────────────────────────────────
    // Analysis
    //──────────────────────────────────────────────────────────────────────────

    // Extracts the selected region, creates an AnalysisJob, and enqueues it.
    // Does nothing if Praat is not installed, no audio is loaded, or no script
    // is selected.
    void beginAnalysisOfSelectedRegion();

    //──────────────────────────────────────────────────────────────────────────
    // State queries — safe to call on the message thread
    //──────────────────────────────────────────────────────────────────────────

    bool isPraatAvailable()     const noexcept;
    bool isAnalysisInProgress() const noexcept;

    // Returns true (and resets the flag) if a script produced audio output since
    // the last call. Used by the editor to know when to refresh the waveform.
    bool consumeAudioOutputNotification() noexcept;

    // Returns the most recently completed AnalysisResult.
    // Empty/invalid if no analysis has completed yet.
    AnalysisResult mostRecentAnalysisResult() const;

    // Human-readable status for the status bar.
    juce::String currentStatusMessage() const;

    // Access to subsystems needed by the editor.
    ScriptManager&            scriptManager() noexcept { return scriptManager_; }
    PraatInstallationLocator& praatLocator()  noexcept { return praatLocator_; }

private:
    // Plays back the transport source or clears the output buffer.
    // AUDIO-THREAD SAFE: only touches AudioTransportSource (which handles its own safety).
    void renderNextAudioBlock (juce::AudioBuffer<float>& buffer);

    // Writes bundled .praat scripts to ~/Application Support/PraatPlugin/scripts/
    // if they do not already exist, then auto-loads the scripts directory.
    // Called once from the constructor on the message thread.
    void installBundledScriptsIfNeeded();

    // Called on the message thread by AsyncUpdater when a job completes.
    void handleAsyncUpdate() override;

    // Called (by value) from JobDispatcher's worker thread via message thread post.
    void receiveCompletedJob (AnalysisJob completedJob);

    //──────────────────────────────────────────────────────────────────────────
    // Live capture (AudioCapture handles audio-thread safety)
    //──────────────────────────────────────────────────────────────────────────
    AudioCapture              audioCapture_;
    std::atomic<bool>         isCapturing_ { false };
    double                    captureSampleRate_ { 44100.0 };  // set in prepareToPlay

    //──────────────────────────────────────────────────────────────────────────
    // Audio file state  (message-thread only)
    //──────────────────────────────────────────────────────────────────────────
    juce::AudioBuffer<float>  loadedAudioBuffer_;
    double                    loadedAudioSampleRate_ { 0.0 };
    juce::File                loadedAudioFile_;
    juce::Range<double>       selectedRegionSeconds_;    // (0,0) = full file
    std::atomic<bool>         audioIsLoaded_ { false };

    //──────────────────────────────────────────────────────────────────────────
    // Processed audio state  (message-thread only)
    //──────────────────────────────────────────────────────────────────────────
    juce::AudioBuffer<float>  processedAudioBuffer_;
    double                    processedAudioSampleRate_ { 0.0 };
    juce::File                processedAudioFile_;        // kept on disk while in use
    std::atomic<bool>         hasProcessedAudio_  { false };

    //──────────────────────────────────────────────────────────────────────────
    // Playback (AudioTransportSource owns thread safety internally)
    //──────────────────────────────────────────────────────────────────────────
    juce::AudioFormatManager                          audioFormatManager_;
    std::unique_ptr<juce::AudioFormatReaderSource>    audioReaderSource_;
    std::unique_ptr<juce::AudioFormatReaderSource>    processedReaderSource_;
    juce::AudioTransportSource                        transportSource_;

    // The end position of the currently playing region.
    // Written on the message thread before start(); read on the audio thread.
    std::atomic<double>                               playbackRegionEndSeconds_ { 0.0 };
    // Which source (A=true / B=false) the transport is currently playing.
    std::atomic<bool>                                 isPlayingOriginal_ { true };

    //──────────────────────────────────────────────────────────────────────────
    // Subsystems
    //──────────────────────────────────────────────────────────────────────────
    PraatInstallationLocator  praatLocator_;
    ScriptManager             scriptManager_;
    JobQueue                  jobQueue_;
    JobDispatcher             jobDispatcher_;

    //──────────────────────────────────────────────────────────────────────────
    // Analysis state (message-thread only, except where noted)
    //──────────────────────────────────────────────────────────────────────────
    mutable std::mutex        latestResultMutex_;
    AnalysisResult            latestResult_;
    std::atomic<bool>         analysisIsInProgress_ { false };

    // Set by receiveCompletedJob() when a script wrote audio output.
    // Consumed (and reset) by consumeAudioOutputNotification().
    std::atomic<bool>         audioOutputWasReceived_ { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PraatPluginProcessor)
};
