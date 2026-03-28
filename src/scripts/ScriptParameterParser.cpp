#include "scripts/ScriptParameterParser.h"

juce::Array<ScriptParameter> ScriptParameterParser::parse (const juce::File& scriptFile)
{
    juce::Array<ScriptParameter> parameters;

    if (! scriptFile.existsAsFile())
        return parameters;

    const juce::String content = scriptFile.loadFileAsString();
    if (content.isEmpty())
        return parameters;

    // Scan for the form...endform block.
    bool insideFormBlock = false;
    bool pastFirstLine   = false;  // skip the "form Title" line itself

    for (const auto& rawLine : juce::StringArray::fromLines (content))
    {
        const juce::String line = rawLine.trim();

        if (! insideFormBlock)
        {
            // Detect the start of the form block.
            // The form line looks like: "form Title Text Here"
            if (line.startsWithIgnoreCase ("form ") || line.equalsIgnoreCase ("form"))
            {
                insideFormBlock = true;
                pastFirstLine   = false;
            }
            continue;
        }

        // Skip the "form ..." line itself (we just entered the block)
        if (! pastFirstLine)
        {
            pastFirstLine = true;
            // The "form" keyword was already handled above; this branch
            // handles the case where the scanner re-enters on the same line,
            // which cannot happen with the current logic — kept for clarity.
            continue;
        }

        // End of the form block
        if (line.startsWithIgnoreCase ("endform"))
            break;

        // Skip blank lines and comment lines
        if (line.isEmpty() || line.startsWith ("#"))
            continue;

        // Skip field types the plugin does not expose as UI controls
        if (lineIsSkippedFieldType (line))
            continue;

        // Attempt to parse the line as a controllable field
        ScriptParameter param;
        if (parseFieldLine (line, param))
            parameters.add (param);
    }

    return parameters;
}

bool ScriptParameterParser::lineIsSkippedFieldType (const juce::String& trimmedLine)
{
    // sentence and text fields hold file paths and free-form strings;
    // optionmenu / option are enum-style and not supported in v1.
    return trimmedLine.startsWithIgnoreCase ("sentence ")
        || trimmedLine.startsWithIgnoreCase ("sentence\t")
        || trimmedLine.equalsIgnoreCase     ("sentence")
        || trimmedLine.startsWithIgnoreCase ("text ")
        || trimmedLine.startsWithIgnoreCase ("text\t")
        || trimmedLine.equalsIgnoreCase     ("text")
        || trimmedLine.startsWithIgnoreCase ("optionmenu ")
        || trimmedLine.startsWithIgnoreCase ("option ");
}

bool ScriptParameterParser::parseFieldLine (const juce::String& trimmedLine,
                                            ScriptParameter&    outParameter)
{
    // A form field line has the structure:
    //   <type> <Name_with_underscores_or_spaces> <defaultValue>
    //
    // Examples:
    //   real Threshold 0.3
    //   integer Fold_iterations 1
    //   boolean Enable_DC_removal 1
    //   positive Wet_mix 0.5

    const juce::StringArray tokens = juce::StringArray::fromTokens (trimmedLine, " \t", "");

    if (tokens.size() < 3)
        return false;

    const juce::String typeToken = tokens[0].toLowerCase();

    ScriptParameter::Type type;
    if (typeToken == "real")
        type = ScriptParameter::Type::Real;
    else if (typeToken == "positive")
        type = ScriptParameter::Type::Positive;
    else if (typeToken == "integer")
        type = ScriptParameter::Type::Integer;
    else if (typeToken == "boolean")
        type = ScriptParameter::Type::Boolean;
    else
        return false;  // unrecognised type

    // The name is the second token; Praat uses underscores to represent
    // spaces in variable names, so we keep underscores as-is.
    const juce::String name = tokens[1];

    // The default value is the last token on the line.
    const juce::String defaultToken = tokens[tokens.size() - 1];
    const double defaultValue       = defaultToken.getDoubleValue();

    outParameter = { name, type, defaultValue };
    return true;
}
