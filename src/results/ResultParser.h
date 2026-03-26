#pragma once

#include <juce_core/juce_core.h>
#include "results/AnalysisResult.h"

// Parses the stdout captured from a Praat process into a structured AnalysisResult.
//
// Parsing convention (see docs/adr/ADR-006):
//   Lines matching "KEY: value" are extracted into keyValuePairs.
//   All other lines are preserved in rawConsoleOutput.
//
// The parser is intentionally lenient:
//   - Lines not matching the convention are never rejected
//   - An empty stdout is valid (parsedSuccessfully = true, no key-value pairs)
//   - Duplicate keys: the last value wins
//
// ResultParser is stateless — all methods are const or static.
class ResultParser
{
public:
    // Parses Praat's stdout from a successful run (exit code 0).
    AnalysisResult parseOutputFromSuccessfulRun (const juce::String& praatStdout) const;

    // Builds a failed AnalysisResult from a Praat error (non-zero exit, stderr, or timeout).
    AnalysisResult buildFailureResult (const juce::String& errorDescription,
                                        const juce::String& praatStderr = {}) const;

private:
    // Returns true if the line matches "KEY: value" (colon-space separator).
    bool lineFollowsKeyValueConvention (const juce::String& line) const;

    // Splits a "KEY: value" line into its two parts.
    std::pair<juce::String, juce::String> extractKeyValuePairFromLine (const juce::String& line) const;
};
