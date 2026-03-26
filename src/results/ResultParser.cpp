#include "results/ResultParser.h"

AnalysisResult ResultParser::parseOutputFromSuccessfulRun (const juce::String& praatStdout) const
{
    AnalysisResult result;
    result.parsedSuccessfully = true;
    result.completionTime     = juce::Time::getCurrentTime();

    juce::StringArray lines;
    lines.addTokens (praatStdout, "\n", {});

    for (const auto& line : lines)
    {
        const auto trimmedLine = line.trim();

        if (trimmedLine.isEmpty())
            continue;

        if (lineFollowsKeyValueConvention (trimmedLine))
        {
            auto [key, value] = extractKeyValuePairFromLine (trimmedLine);
            result.keyValuePairs.set (key, value);
        }
        else
        {
            if (result.rawConsoleOutput.isNotEmpty())
                result.rawConsoleOutput += "\n";
            result.rawConsoleOutput += trimmedLine;
        }
    }

    return result;
}

AnalysisResult ResultParser::buildFailureResult (const juce::String& errorDescription,
                                                   const juce::String& praatStderr) const
{
    AnalysisResult result;
    result.parsedSuccessfully = false;
    result.failureReason      = errorDescription;
    result.praatErrorOutput   = praatStderr;
    result.completionTime     = juce::Time::getCurrentTime();
    return result;
}

bool ResultParser::lineFollowsKeyValueConvention (const juce::String& line) const
{
    // A KEY: value line must contain ": " with at least one character before the colon.
    const int separatorPosition = line.indexOf (": ");
    return separatorPosition > 0;
}

std::pair<juce::String, juce::String> ResultParser::extractKeyValuePairFromLine (const juce::String& line) const
{
    const int separatorPosition = line.indexOf (": ");
    const juce::String key   = line.substring (0, separatorPosition).trim();
    const juce::String value = line.substring (separatorPosition + 2).trim();
    return { key, value };
}
