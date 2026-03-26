#pragma once

#include <juce_core/juce_core.h>

// The structured output of a single Praat analysis job.
//
// Populated by ResultParser after PraatRunner completes.
// Immutable after construction — passed by value through the system.
//
// See docs/adr/ADR-006 for the KEY: value output convention.
struct AnalysisResult
{
    // True when Praat ran and exited successfully (exit code 0).
    // False when Praat was not found, timed out, or returned a non-zero exit code.
    bool parsedSuccessfully { false };

    // Structured pairs extracted from lines matching "KEY: value" in Praat's stdout.
    // Example: { "mean_pitch", "220.5 Hz" }, { "voiced_frames", "82%" }
    juce::StringPairArray keyValuePairs;

    // Everything Praat printed that did not match "KEY: value".
    // Shown verbatim in the plugin's log panel.
    juce::String rawConsoleOutput;

    // Full stderr captured from Praat, if any. Shown in the error panel.
    juce::String praatErrorOutput;

    // Human-readable description of why parsing failed, if parsedSuccessfully == false.
    juce::String failureReason;

    // When the analysis completed (set by JobDispatcher on the worker thread).
    juce::Time completionTime;

    // A convenience for the UI: true if there are structured key-value pairs to display.
    bool hasStructuredResults() const noexcept
    {
        return parsedSuccessfully && keyValuePairs.size() > 0;
    }
};
