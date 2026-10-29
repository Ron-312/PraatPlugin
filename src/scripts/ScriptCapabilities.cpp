#include "scripts/ScriptCapabilities.h"
#include <regex>

namespace
{
    // Script source without comment lines (descriptions mention many features).
    std::string codeOnly (const juce::String& source)
    {
        juce::String code;
        for (const auto& line : juce::StringArray::fromLines (source))
            if (! line.trimStart().startsWithChar ('#'))
                code << line << "\n";
        return code.toStdString();
    }

    bool contains (const std::string& code, const char* pattern, bool ignoreCase = false)
    {
        auto flags = std::regex::ECMAScript | std::regex::optimize;
        if (ignoreCase)
            flags |= std::regex::icase;
        return std::regex_search (code, std::regex (pattern, flags));
    }

    bool isPathField (const FormParam& f)
    {
        if (f.type == "folder" || f.type == "infile" || f.type == "outfile")
            return true;
        if (f.type != "sentence" && f.type != "word" && f.type != "text")
            return false;
        const auto n = f.name.toLowerCase();
        return n.contains ("folder") || n.contains ("director") || n.contains ("path")
            || n.endsWith ("_dir") || n.endsWith ("file");
    }
}

ScriptCapabilities ScriptCapabilities::analyse (const juce::File& scriptFile)
{
    return analyseSource (scriptFile.loadFileAsString(),
                          PraatFormParser::parseAllFields (scriptFile),
                          scriptFile.getFileName());
}

ScriptCapabilities ScriptCapabilities::analyseSource (const juce::String& source,
                                                      const juce::Array<FormParam>& fields,
                                                      const juce::String& fileName)
{
    ScriptCapabilities caps;
    const auto code = codeOnly (source);

    caps.usesSecondSound =
           contains (code, R"(selected\s*\(\s*"Sound"\s*,\s*2\s*\))")
        || contains (code, R"(numberOfSelected\s*\(\s*"Sound"\s*\)\s*(<>|!=|<|=)\s*2)")
        || contains (code, R"(select[^"\n]{0,40}\b(two|2|at least two|2 to \d)\b[^"\n]{0,30}sound)", true);

    caps.mayOpenWindow   = contains (code, R"(demoWaitForInput)");
    caps.opensFileDialog = contains (code, R"(choose(ReadFile|WriteFile|Folder|Directory)\$)");

    for (const auto& f : fields)
        if (! f.isInputFilePath && ! f.isOutputFilePath && isPathField (f))
            caps.pathFieldNames.add (f.name);

    // ── Cannot run inside the plugin ──────────────────────────────────────
    const bool readsSound = contains (code, R"(selected\s*\(\s*"Sound")")
                         || contains (code, R"(numberOfSelected\s*\(\s*"Sound")");

    if (fileName.equalsIgnoreCase ("setup.praat"))
        caps.unsupportedReason = "Library installer, not an audio script.";
    else if (contains (code, R"(ffmpeg)", true))
        caps.unsupportedReason = "Needs FFmpeg installed and called from Praat.";
    else if (contains (code, R"(\.py\b)") && contains (code, R"(runSystem)"))
        caps.unsupportedReason = "Needs a Python helper script.";
    else if (contains (code, R"(supports Windows only)", true) && ! juce::SystemStats::getOperatingSystemName().containsIgnoreCase ("windows"))
        caps.unsupportedReason = "Windows only.";
    else if (contains (code, R"(selected\s*\(\s*"Photo")") && ! readsSound)
        caps.unsupportedReason = "Needs an image (Photo object) as input.";

    return caps;
}

juce::String ScriptCapabilities::badge() const
{
    if (! isSupported())   return "UNSUPPORTED";
    if (usesSecondSound)   return "2 SOUNDS";
    if (opensFileDialog)   return pathFieldNames.isEmpty() ? "DIALOG" : "FOLDER";
    if (mayOpenWindow)     return "WINDOW";
    return {};
}

juce::String ScriptCapabilities::checkBeforeRun (const juce::StringPairArray& paramValues) const
{
    if (! isSupported())
        return "This script can't run in the plugin: " + unsupportedReason;

    // A blank path field makes these scripts fall back to a file dialog, which
    // headless Praat can't show — it would hang until the timeout.
    if (opensFileDialog)
        for (const auto& name : pathFieldNames)
            if (paramValues.getValue (name, {}).trim().isEmpty())
                return "Choose a path for \"" + name.replaceCharacter ('_', ' ') + "\" first "
                       "(the script would otherwise open a file dialog).";

    return {};
}

juce::String ScriptCapabilities::headlessDefault (const FormParam& field)
{
    // Playback and editor windows make no sense from inside a DAW: Praat would
    // play through the speakers, and editors can't open headless.
    if (field.type == "boolean")
    {
        const auto n = field.name.toLowerCase();
        if (n.startsWith ("play") || n.contains ("editor"))
            return "0";
    }
    return field.defaultValue;
}
