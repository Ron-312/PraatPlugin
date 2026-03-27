#pragma once

#include <juce_core/juce_core.h>

// Parses the form...endform block of a Praat script file and returns a
// description of each user-controllable parameter found there.
//
// The plugin passes parameters to Praat as positional CLI arguments after
// the two mandatory paths (inputFile and outputFile).  The order returned
// here must match the order of fields in the script's form block exactly,
// since Praat reads them positionally.
//
// Supported field types:
//   real      — floating-point; maps to a Slider in the UI
//   positive  — floating-point ≥ 0; maps to a Slider (range starts at 0)
//   integer   — integer; maps to a Slider with integer step
//   boolean   — 0 or 1; maps to a ToggleButton in the UI
//
// Unsupported types (skipped, not exposed in UI):
//   sentence  — the inputFile and outputFile paths; handled by the plugin
//   text      — free-form string; not practically adjustable via slider
//   optionmenu / option — enums; skipped in v1

struct ScriptParameter
{
    enum class Type
    {
        Real,     // real field — full range slider, can be negative
        Positive, // positive field — slider clamped to > 0
        Integer,  // integer field
        Boolean   // boolean field (0/1)
    };

    juce::String name;          // field name as written in the form block
    Type         type;
    double       defaultValue;  // default value parsed from the form block
};

class ScriptParameterParser
{
public:
    // Reads scriptFile and returns the ordered list of user-controllable
    // parameters from its form block.
    //
    // Returns an empty array if:
    //   - the file cannot be read
    //   - no form...endform block is found
    //   - the form block contains only sentence/text fields
    static juce::Array<ScriptParameter> parse (const juce::File& scriptFile);

private:
    // Returns true if the trimmed line introduces a skipped field type.
    static bool lineIsSkippedFieldType (const juce::String& trimmedLine);

    // Attempts to parse a single form field line into a ScriptParameter.
    // Returns false if the line is not a recognised field declaration.
    static bool parseFieldLine (const juce::String& trimmedLine,
                                ScriptParameter&    outParameter);
};
