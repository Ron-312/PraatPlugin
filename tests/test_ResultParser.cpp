#include <catch2/catch_test_macros.hpp>
#include "results/ResultParser.h"

TEST_CASE ("ResultParser extracts KEY: value pairs from Praat stdout", "[ResultParser]")
{
    ResultParser parser;

    SECTION ("parses a single well-formed KEY: value line")
    {
        const auto result = parser.parseOutputFromSuccessfulRun ("mean_pitch: 220.5 Hz");

        REQUIRE (result.parsedSuccessfully);
        REQUIRE (result.keyValuePairs.size() == 1);
        REQUIRE (result.keyValuePairs.getValue ("mean_pitch", {}) == "220.5 Hz");
    }

    SECTION ("parses multiple KEY: value lines")
    {
        const juce::String praatOutput =
            "mean_pitch: 220.5 Hz\n"
            "min_pitch: 87.3 Hz\n"
            "max_pitch: 441.0 Hz\n"
            "voiced_frames: 82%";

        const auto result = parser.parseOutputFromSuccessfulRun (praatOutput);

        REQUIRE (result.parsedSuccessfully);
        REQUIRE (result.keyValuePairs.size() == 4);
        REQUIRE (result.keyValuePairs.getValue ("min_pitch",      {}) == "87.3 Hz");
        REQUIRE (result.keyValuePairs.getValue ("voiced_frames",  {}) == "82%");
    }

    SECTION ("stores non-KEY:value lines as raw console output")
    {
        const juce::String praatOutput =
            "mean_pitch: 220.5 Hz\n"
            "This is a debug line\n"
            "duration: 1.5 s";

        const auto result = parser.parseOutputFromSuccessfulRun (praatOutput);

        REQUIRE (result.parsedSuccessfully);
        REQUIRE (result.keyValuePairs.size() == 2);
        REQUIRE (result.rawConsoleOutput.contains ("This is a debug line"));
    }

    SECTION ("empty stdout is a valid successful result")
    {
        const auto result = parser.parseOutputFromSuccessfulRun ({});

        REQUIRE (result.parsedSuccessfully);
        REQUIRE (result.keyValuePairs.size() == 0);
        REQUIRE (result.rawConsoleOutput.isEmpty());
    }

    SECTION ("values can contain colons without breaking parsing")
    {
        const auto result = parser.parseOutputFromSuccessfulRun ("url: http://example.com");

        REQUIRE (result.parsedSuccessfully);
        REQUIRE (result.keyValuePairs.getValue ("url", {}) == "http://example.com");
    }

    SECTION ("duplicate keys: last value wins")
    {
        const juce::String praatOutput =
            "pitch: 200 Hz\n"
            "pitch: 220 Hz";

        const auto result = parser.parseOutputFromSuccessfulRun (praatOutput);

        REQUIRE (result.keyValuePairs.getValue ("pitch", {}) == "220 Hz");
    }
}

TEST_CASE ("ResultParser builds failure results correctly", "[ResultParser]")
{
    ResultParser parser;

    SECTION ("failure result is not marked as parsed successfully")
    {
        const auto result = parser.buildFailureResult ("Praat timed out", "stderr text");

        REQUIRE_FALSE (result.parsedSuccessfully);
        REQUIRE (result.failureReason == "Praat timed out");
        REQUIRE (result.praatErrorOutput == "stderr text");
    }

    SECTION ("hasStructuredResults returns false for failure results")
    {
        const auto result = parser.buildFailureResult ("Error occurred");

        REQUIRE_FALSE (result.hasStructuredResults());
    }
}
