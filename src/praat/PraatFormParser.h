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

    // Set by parseAllFields() for sentence/word fields:
    // The 1st sentence/word field in the form is treated as the inputFile path.
    // The 2nd sentence/word field is treated as the outputFile path.
    // These are substituted with actual WAV file paths at run time and are
    // excluded from the parameter UI.
    bool isInputFilePath  { false };
    bool isOutputFilePath { false };
};

// Reads a Praat script file and returns form parameter metadata.
//
// Call parseExtraParams() to get the user-visible parameters (those shown in
// the parameter panel).  Call parseAllFields() when building the CLI argument
// list for Praat — it returns every field in form order, including the
// inputFile / outputFile sentence fields needed for path substitution.
class PraatFormParser
{
public:
    // Returns ALL form fields in the order they appear in the form block.
    // The first sentence/word field has isInputFilePath=true; the second has
    // isOutputFilePath=true.  All other fields have both flags false.
    static juce::Array<FormParam> parseAllFields (const juce::File& scriptFile);

    // Returns only the user-editable fields (those shown in the UI).
    // Filters out fields with isInputFilePath or isOutputFilePath.
    // Previously these were identified by skipping "the first 2 fields by
    // count"; now they are identified by sentence/word type and position,
    // so optionMenu fields that precede the sentence fields are correctly
    // included rather than silently dropped.
    static juce::Array<FormParam> parseExtraParams (const juce::File& scriptFile);
};
