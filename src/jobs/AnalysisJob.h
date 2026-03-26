#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include "results/AnalysisResult.h"

// The lifecycle state of a single analysis job.
enum class JobState
{
    Pending,               // Created, waiting in JobQueue
    Running,               // JobDispatcher is actively processing it
    CompletedSuccessfully, // Praat ran and result was parsed
    FailedWithError        // Praat could not run, timed out, or returned an error
};

// Returns a human-readable label for the given job state.
// Used in the status bar.
juce::String describeJobState (JobState state);

// A single unit of work: capture a WAV file, run a Praat script on it,
// and produce an AnalysisResult.
//
// Created on the message thread by PraatPluginProcessor.
// Owned and mutated exclusively by JobDispatcher while running.
// Returned (by value) to the message thread via AsyncUpdater on completion.
struct AnalysisJob
{
    // Unique identifier — used for temp directory naming.
    // See docs/adr/ADR-004.
    juce::Uuid jobIdentifier;

    // The audio samples to analyse — a copy of the selected region extracted
    // from PraatPluginProcessor::loadedAudioBuffer_ on the message thread.
    // See docs/adr/ADR-008 for the file-based workflow design.
    juce::AudioBuffer<float> audioToAnalyze;
    double                   audioSampleRate { 44100.0 };

    // Temporary WAV file path — set by JobDispatcher::writeAudioToTempFile()
    // after writing audioToAnalyze to disk.  Not set by the caller.
    juce::File capturedAudioWavFile;

    // Path passed to the Praat script as the second argument.
    // Scripts that produce audio should write a WAV file to this path.
    // If the file exists after the script runs, JobDispatcher moves it to a
    // stable location and the processor loads it back as the new audio buffer.
    // Analysis-only scripts ignore this path (but must have it in their form block).
    juce::File outputAudioWavFile;

    // The .praat script to run against capturedAudioWavFile.
    juce::File praatScriptFile;

    // Path to the Praat executable, resolved by PraatInstallationLocator.
    juce::File praatExecutableFile;

    // Tracks progress through the job lifecycle.
    JobState currentState { JobState::Pending };

    // Populated when currentState == CompletedSuccessfully.
    AnalysisResult result;

    // Populated when currentState == FailedWithError.
    juce::String errorDescription;
};
