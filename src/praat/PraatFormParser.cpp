#include "praat/PraatFormParser.h"

// Removes one pair of surrounding double quotes: "\"0.25\"" → "0.25".
static juce::String unquote (juce::String s)
{
    s = s.trim();
    if (s.length() >= 2 && s.startsWithChar ('"') && s.endsWithChar ('"'))
        s = s.substring (1, s.length() - 1).replace ("\"\"", "\"");
    return s;
}

// Splits the colon-syntax argument list `"Label", "default"` at the first
// comma outside quotes.  Returns { label, default }.
static std::pair<juce::String, juce::String> splitColonArgs (const juce::String& args)
{
    bool inQuotes = false;
    for (int i = 0; i < args.length(); ++i)
    {
        if (args[i] == '"') inQuotes = ! inQuotes;
        else if (args[i] == ',' && ! inQuotes)
            return { unquote (args.substring (0, i)), unquote (args.substring (i + 1)) };
    }
    return { unquote (args), {} };
}

// Praat turns a colon-syntax label into a variable name the same way the
// classic syntax spells it: spaces → underscores, "(…)" hint dropped.
static juce::String labelToFieldName (const juce::String& label)
{
    return label.upToFirstOccurrenceOf ("(", false, false).trim().replaceCharacter (' ', '_');
}

// All Praat form field type keywords (case-insensitive prefix match).
static const char* const kFieldTypes[] = {
    "sentence", "word", "text",
    "real", "positive", "natural", "integer",
    "boolean",
    "choice", "optionmenu",
    "infile", "outfile", "folder",
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

        if (! inForm && (line.startsWithIgnoreCase ("form ") || line.startsWithIgnoreCase ("form:")
                         || line.trimEnd().equalsIgnoreCase ("form")))
        {
            inForm = true;
            continue;
        }
        if (line.trimEnd().equalsIgnoreCase ("endform"))
            break;
        if (! inForm)
            continue;

        // button / option lines belong to the most recent choice/optionmenu param.
        // Classic: `option Soft`   Colon syntax: `option: "Soft"`
        if (line.startsWithIgnoreCase ("button") || line.startsWithIgnoreCase ("option"))
        {
            const auto keywordEnd = line.indexOfAnyOf (" \t:");
            const auto keyword    = line.substring (0, keywordEnd < 0 ? line.length() : keywordEnd);
            if (keyword.equalsIgnoreCase ("button") || keyword.equalsIgnoreCase ("option"))
            {
                if (lastChoiceIndex >= 0 && lastChoiceIndex < params.size())
                {
                    auto optText = line.substring (keyword.length()).trimStart();
                    if (optText.startsWithChar (':'))
                        optText = unquote (optText.substring (1));
                    params.getReference (lastChoiceIndex).options.add (optText);
                }
                continue;
            }
        }

        // Look for a recognised field-type keyword at the start of the line.
        for (int t = 0; kFieldTypes[t] != nullptr; ++t)
        {
            const juce::String keyword = kFieldTypes[t];
            if (! line.startsWithIgnoreCase (keyword))
                continue;

            // Classic `real Gain 0.5` or colon syntax `real: "Gain", "0.5"`.
            const auto rest = line.substring (keyword.length());
            const bool colonSyntax = rest.trimStart().startsWithChar (':');
            if (! colonSyntax && ! (rest.startsWithChar (' ') || rest.startsWithChar ('\t')))
                continue;

            const juce::String typeStr = keyword.toLowerCase();

            juce::String fieldName, defaultVal;
            if (colonSyntax)
            {
                const auto [label, def] = splitColonArgs (rest.trimStart().substring (1));
                fieldName  = labelToFieldName (label);
                defaultVal = def;
            }
            else
            {
                // Parse:  <type> <fieldName> [<default>...]
                const auto afterKeyword = rest.trimStart();
                const int  nameEnd      = afterKeyword.indexOfAnyOf (" \t");
                fieldName  = (nameEnd >= 0) ? afterKeyword.substring (0, nameEnd) : afterKeyword;
                defaultVal = (nameEnd >= 0) ? afterKeyword.substring (nameEnd + 1).trimStart()
                                            : juce::String{};
            }

            // Numeric defaults may carry a hint: `integer Dots 5 (= top line)`.
            // Praat reads only the number, so pass only the number.
            if (typeStr == "real" || typeStr == "positive" || typeStr == "natural"
                || typeStr == "integer" || typeStr == "boolean")
                defaultVal = defaultVal.upToFirstOccurrenceOf (" ", false, false)
                                       .upToFirstOccurrenceOf ("\t", false, false);

            FormParam p;
            p.name         = fieldName;
            p.type         = typeStr;
            p.defaultValue = defaultVal;

            // Detect input/output file path fields.
            // A sentence/word field is a file path only if its name contains
            // "file", "input", "output", "wav", "path", "source", or "dest"
            // (case-insensitive) AND its default is empty or looks like a path.
            // The default check keeps `word Output_name montage` and
            // `sentence Sofa_file kemar` as ordinary text parameters.
            if (typeStr == "sentence" || typeStr == "word")
            {
                const auto lower = fieldName.toLowerCase();
                const bool nameLooksLikePath =
                    lower.contains ("file")   || lower.contains ("input")  ||
                    lower.contains ("output") || lower.contains ("wav")    ||
                    lower.contains ("path")   || lower.contains ("source") ||
                    lower.contains ("dest");
                const bool defaultLooksLikePath =
                    defaultVal.isEmpty() || defaultVal.containsAnyOf ("/\\")
                    || defaultVal.endsWithIgnoreCase (".wav");
                const bool looksLikeFilePath = nameLooksLikePath && defaultLooksLikePath;

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
