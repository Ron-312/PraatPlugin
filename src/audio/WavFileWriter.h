#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

// Writes an AudioBuffer to a WAV file on disk using JUCE's WavAudioFormat.
//
// Used by JobDispatcher to materialise the captured audio ring buffer
// into a concrete file that can be passed to PraatRunner.
//
// All methods are synchronous and BLOCKING.
// Call only from the worker thread (JobDispatcher).
class WavFileWriter
{
public:
    // Outcome of a write attempt.
    enum class WriteResult
    {
        Success,
        DestinationFileCouldNotBeCreated,
        NothingToWrite,
        WriteError
    };

    // Writes buffer to destinationFile at the given sample rate.
    // Creates any parent directories that don't yet exist.
    // Returns the outcome so the caller can surface errors to the user.
    static WriteResult writeAudioBufferToWavFile (const juce::AudioBuffer<float>& buffer,
                                                   double sampleRate,
                                                   const juce::File& destinationFile);

    // Returns a human-readable explanation for display in the status bar.
    static juce::String describeWriteResult (WriteResult result);
};
