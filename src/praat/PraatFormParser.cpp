#include "praat/PraatFormParser.h"

// All Praat form field type keywords (case-insensitive prefix match).
static const char* const kFieldTypes[] = {
    "sentence", "word", "text",
    "real", "positive", "natural", "integer",
    "boolean",
    "choice", "optionmenu",
    nullptr
};

juce::Array<FormParam> PraatFormParser::parseAllFields (const juce::File& scriptFile)
{
    juce::Array<FormParam> params;

    if (! scriptFile.existsAsFile())
        return params;

    const juce::String content = scriptFile.loadFileAsString();

    bool inForm          = false;
    int  sentenceCount   = 0;   // counts sentence + word fields only
    int  lastChoiceIndex = -1;  // index into params[] of the most recent choice/optionmenu

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
            if (! line.startsWithIgnoreCase (prefix))
                continue;

            const juce::String typeStr = juce::String (kFieldTypes[t]).toLowerCase();

            // Parse:  <type> <fieldName> [<default>...]
            const auto afterKeyword = line.substring ((int) prefix.length()).trimStart();
            const int  nameEnd      = afterKeyword.indexOfChar (' ');
            const auto fieldName    = (nameEnd >= 0) ? afterKeyword.substring (0, nameEnd)
                                                     : afterKeyword;
            const auto defaultVal   = (nameEnd >= 0)
                                          ? afterKeyword.substring (nameEnd + 1).trimStart()
                                          : juce::String{};

            FormParam p;
            p.name         = fieldName;
            p.type         = typeStr;
            p.defaultValue = defaultVal;

            // Detect input/output file path fields by name.
            // A sentence/word field is a file path only if its name contains
            // "file", "input", "output", "wav", "path", "source", or "dest"
            // (case-insensitive).  This avoids tagging unrelated sentence fields
            // (e.g. "sentence Pattern") as file paths in community scripts.
            if (typeStr == "sentence" || typeStr == "word")
            {
                const auto lower = fieldName.toLowerCase();
                const bool looksLikeFilePath =
                    lower.contains ("file")   || lower.contains ("input")  ||
                    lower.contains ("output") || lower.contains ("wav")    ||
                    lower.contains ("path")   || lower.contains ("source") ||
                    lower.contains ("dest");

                if (looksLikeFilePath)
                {
                    ++sentenceCount;
                    if (sentenceCount == 1)       p.isInputFilePath  = true;
                    else if (sentenceCount == 2)  p.isOutputFilePath = true;
                }
            }

            params.add (p);

            if (typeStr == "choice" || typeStr == "optionmenu")
                lastChoiceIndex = params.size() - 1;
            else
                lastChoiceIndex = -1;

            break;
        }
    }

    return params;
}

juce::Array<FormParam> PraatFormParser::parseExtraParams (const juce::File& scriptFile)
{
    juce::Array<FormParam> result;
    for (const auto& p : parseAllFields (scriptFile))
    {
        if (! p.isInputFilePath && ! p.isOutputFilePath)
            result.add (p);
    }
    return result;
}
