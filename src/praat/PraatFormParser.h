#pragma once
#include <juce_core/juce_core.h>

// Metadata for a single field in a Praat script's form block.
struct FormParam
{
    juce::String      name;         // e.g. "stretchFactor"
    juce::String      type;         // "real" | "positive" | "natural" | "integer" |
                                    // "boolean" | "choice" | "optionmenu" |
                                    // "sentence" | "word" | "text"
    juce::String      defaultValue; // raw string from the form line
    juce::StringArray options;      // populated for choice / optionmenu; empty otherwise
};

// Reads a Praat script file and returns the extra form parameters —
// i.e. every field AFTER the first two (inputFile, outputFile).
//
// Call this when a script is selected to populate the parameter UI.
// Returns an empty array if the script has no form block or only
// the standard two fields.
class PraatFormParser
{
public:
    static juce::Array<FormParam> parseExtraParams (const juce::File& scriptFile);
};
