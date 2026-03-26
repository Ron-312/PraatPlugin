#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

// Accumulates audio samples arriving from the DAW's audio thread into a
// lock-free ring buffer. When the user triggers analysis, the ring buffer
// can be flushed to a WAV file on the worker thread.
//
// THREAD SAFETY
//   appendSamplesFromProcessBlock() — AUDIO-THREAD SAFE (lock-free, no alloc)
//   All other methods                — MESSAGE-THREAD SAFE
//
// See docs/adr/ADR-003 for the audio thread isolation contract.
class AudioCapture
{
public:
    // The most audio we retain for a single analysis pass.
    // Older samples are silently overwritten once the window is full.
    static constexpr double maximumCaptureDurationInSeconds { 30.0 };

    AudioCapture();

    // Called from prepareToPlay — allocates the ring buffer to hold
    // maximumCaptureDurationInSeconds of audio at the given sample rate.
    // Must be called before the first processBlock.
    void prepareForCapture (double sampleRate, int numberOfChannels);

    // AUDIO-THREAD SAFE. Appends all samples in block to the ring buffer.
    // If the ring buffer is full, the oldest samples are overwritten.
    void appendSamplesFromProcessBlock (const juce::AudioBuffer<float>& block);

    // Returns true if at least one block of audio has been captured
    // since the last call to clearCaptureBuffer().
    bool hasAudioReadyForAnalysis() const noexcept;

    // How many samples are currently stored in the ring buffer.
    int numberOfSamplesAvailable() const noexcept;

    // AUDIO-THREAD SAFE. Peak absolute amplitude (0..1) of the most recently
    // written block.  Used by the editor's VU meter during live recording.
    float latestPeakLevel() const noexcept;

    // Copies the currently captured audio into dest WITHOUT advancing the ring
    // buffer read pointer.  The next writeCapturedAudioToWavFile() call will
    // still see the full recording.
    // MESSAGE-THREAD ONLY — only the message thread moves the read pointer.
    void copyCurrentContentsTo (juce::AudioBuffer<float>& dest) const;

    // Outcome of writing the captured audio to disk.
    enum class WriteResult { Success, NoAudioCaptured, FileWriteFailed };

    // Writes the contents of the ring buffer to a WAV file at destinationFile.
    // BLOCKING — call only from the worker thread (JobDispatcher), never from
    // the audio thread or message thread.
    WriteResult writeCapturedAudioToWavFile (const juce::File& destinationFile) const;

    // Discards all captured audio. Safe to call on the message thread.
    void clearCaptureBuffer();

private:
    juce::AbstractFifo             fifoController_;
    juce::AudioBuffer<float>       ringBuffer_;
    double                         currentSampleRate_       { 44100.0 };
    int                            currentNumberOfChannels_ { 1 };
    std::atomic<int>               totalSamplesWritten_     { 0 };
    std::atomic<float>             latestPeakLevel_         { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioCapture)
};
