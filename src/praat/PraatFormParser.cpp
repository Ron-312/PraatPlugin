#include "praat/PraatFormParser.h"

juce::Array<FormParam> PraatFormParser::parseExtraParams (const juce::File& scriptFile)
{
    juce::Array<FormParam> params;

    if (! scriptFile.existsAsFile())
        return params;

    const juce::String content = scriptFile.loadFileAsString();

    bool inForm     = false;
    int  fieldIndex = 0;
    int  lastChoiceIndex = -1;   // index into params[] of the most recent choice/optionmenu

    static const char* const kFieldTypes[] = {
        "sentence", "word", "text",
        "real", "positive", "natural", "integer",
        "boolean",
        "choice", "optionmenu",
        nullptr
    };

    for (auto rawLine : juce::StringArray::fromLines (content))
    {
        const auto line = rawLine.trimStart();

        if (line.startsWithIgnoreCase ("form ") || line.equalsIgnoreCase ("form"))
        {
            inForm = true;
            continue;
        }
        if (line.equalsIgnoreCase ("endform"))
            break;
        if (! inForm)
            continue;

        // button / option lines belong to the most recent choice/optionmenu param.
        if (line.startsWithIgnoreCase ("button ") || line.startsWithIgnoreCase ("option "))
        {
            if (lastChoiceIndex >= 0 && lastChoiceIndex < params.size())
            {
                const auto optText = line.fromFirstOccurrenceOf (" ", false, false).trimStart();
                params.getReference (lastChoiceIndex).options.add (optText);
            }
            continue;
        }

        // Look for a recognised field-type keyword at the start of the line.
        for (int t = 0; kFieldTypes[t] != nullptr; ++t)
        {
            const juce::String prefix = juce::String (kFieldTypes[t]) + " ";
            if (line.startsWithIgnoreCase (prefix))
            {
                ++fieldIndex;

                if (fieldIndex <= 2)   // skip inputFile and outputFile
                    break;

                // Parse:  <type> <fieldName> [<default>...]
                const auto afterKeyword = line.substring ((int) prefix.length()).trimStart();
                const int  nameEnd      = afterKeyword.indexOfChar (' ');
                const auto fieldName    = (nameEnd >= 0)
                                              ? afterKeyword.substring (0, nameEnd)
                                              : afterKeyword;
                const auto defaultVal   = (nameEnd >= 0)
                                              ? afterKeyword.substring (nameEnd + 1).trimStart()
                                              : juce::String{};

                FormParam p;
                p.name         = fieldName;
                p.type         = juce::String (kFieldTypes[t]).toLowerCase();
                p.defaultValue = defaultVal;

                params.add (p);

                if (p.type == "choice" || p.type == "optionmenu")
                    lastChoiceIndex = params.size() - 1;
                else
                    lastChoiceIndex = -1;

                break;
            }
        }
    }

    return params;
}
