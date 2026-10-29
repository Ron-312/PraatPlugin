#include "praat/ScriptInvocation.h"

bool ScriptInvocation::needsSoundObjectWrapper (const juce::Array<FormParam>& allFields)
{
    for (const auto& f : allFields)
        if (f.isInputFilePath)
            return false;
    return true;
}

// ── Script argument builder ────────────────────────────────────────────────────
//
// Builds the complete positional CLI argument list for a Praat --run invocation.
//
// Praat maps CLI arguments to form fields positionally.  We parse ALL form
// fields in order (via PraatFormParser::parseAllFields) and emit one argument
// per field:
//   • isInputFilePath  → actual captured WAV path
//   • isOutputFilePath → actual output WAV path
//   • everything else  → user-supplied value (from scriptParameters) or field default
//
// This correctly handles scripts where optionMenu or other fields appear BEFORE
// the sentence inputFile / sentence outputFile fields.
juce::StringArray ScriptInvocation::buildScriptArgs (const juce::File&           scriptFile,
                                          const juce::File&           inputWav,
                                          const juce::File&           outputWav,
                                          const juce::StringPairArray& userParams)
{
    juce::StringArray args;

    for (const auto& field : PraatFormParser::parseAllFields (scriptFile))
    {
        if (field.isInputFilePath)
        {
            args.add (inputWav.getFullPathName());
        }
        else if (field.isOutputFilePath)
        {
            args.add (outputWav.getFullPathName());
        }
        else
        {
            const auto userVal = userParams.getValue (field.name, {});
            auto argVal = userVal.isNotEmpty() ? userVal : field.defaultValue;

            // Praat --run expects the option NAME string for optionMenu/choice
            // fields, not a numeric index.  The UI stores values as 1-based
            // indices ("1", "2", …), so convert here before passing to Praat.
            if (field.type == "optionmenu" || field.type == "choice")
            {
                const int zeroBasedIdx = argVal.getIntValue() - 1;
                if (juce::isPositiveAndBelow (zeroBasedIdx, field.options.size()))
                    argVal = field.options[zeroBasedIdx];
            }

            args.add (argVal);
        }
    }

    return args;
}

// ── Sound-object wrapper builder ───────────────────────────────────────────────
//
// Community scripts expect a Sound object to already be selected in Praat's
// object list (they start with `sound = selected("Sound")`).  They have no
// sentence inputFile / outputFile form fields — audio comes from the GUI selection.
//
// To bridge this with our headless file-based model, we generate a tiny wrapper
// script at runtime that:
//   1. Loads the captured WAV into Praat as a Sound object.
//   2. Calls the community script via runScript with the user's param values.
//   3. Saves whatever Sound is selected after the script finishes to outputFile.
//
// Detection: a script needs the wrapper if parseAllFields() finds no isInputFilePath
// field — i.e. no sentence/word field is the 1st of its type (so no file-arg model).
juce::String ScriptInvocation::praatStringEscape (const juce::String& s)
{
    return s.replace ("\\", "\\\\").replace ("\"", "\\\"");
}

juce::String ScriptInvocation::buildWrapperScript (const juce::File&            inputWav,
                                                    const juce::File&            outputWav,
                                         const juce::File&            communityScript,
                                         const juce::Array<FormParam>& params,
                                         const juce::StringPairArray& userValues)
{
    juce::String w;

    // Step 1 — load the plugin's captured audio as a Praat Sound object
    // and immediately select it so the community script finds it selected.
    w += "inputSound__ = Read from file: \"" + praatStringEscape (inputWav.getFullPathName()) + "\"\n";
    w += "selectObject: inputSound__\n\n";

    // Step 2 — call the community script with all its form params in order.
    // comment-type form fields are automatically skipped by Praat's runScript;
    // parseExtraParams already excludes them, so the count is correct.
    w += "runScript: \"" + praatStringEscape (communityScript.getFullPathName()) + "\"";

    for (const auto& param : params)
    {
        const auto userVal = userValues.getValue (param.name, {});
        auto val = userVal.isNotEmpty() ? userVal : param.defaultValue;

        // Convert 1-based optionMenu index to the option name string.
        if (param.type == "optionmenu" || param.type == "choice")
        {
            const int idx = val.getIntValue() - 1;
            if (juce::isPositiveAndBelow (idx, param.options.size()))
                val = param.options[idx];
            w += ", \"" + praatStringEscape (val) + "\"";
        }
        else if (param.type == "sentence" || param.type == "word" || param.type == "text")
        {
            w += ", \"" + praatStringEscape (val) + "\"";
        }
        else    // boolean, real, positive, natural, integer
        {
            w += ", " + (val.isNotEmpty() ? val : juce::String ("0"));
        }
    }

    w += "\n\n";

    // Step 3 — save the resulting Sound (if any) to the output path.
    w += "if numberOfSelected (\"Sound\") > 0\n";
    w += "    selectObject: selected (\"Sound\")\n";
    w += "    Save as WAV file: \"" + praatStringEscape (outputWav.getFullPathName()) + "\"\n";
    w += "endif\n";

    return w;
}
