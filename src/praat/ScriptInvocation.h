#pragma once
#include "praat/PraatFormParser.h"

// Turns a script's form fields plus the user's parameter values into what
// Praat actually runs.  Two calling models (see JobDispatcher::runPraatScript):
//   • file-arg model     — script has sentence inputFile / outputFile fields;
//                          values are passed positionally to `Praat --run`.
//   • Sound-object model — community scripts that act on the selected Sound;
//                          a generated wrapper loads the WAV, calls runScript,
//                          and saves the selected Sound to outputWav.
namespace ScriptInvocation
{
    // True when no form field was tagged as the input file path.
    bool needsSoundObjectWrapper (const juce::Array<FormParam>& allFields);

    juce::StringArray buildScriptArgs (const juce::File&            scriptFile,
                                       const juce::File&            inputWav,
                                       const juce::File&            outputWav,
                                       const juce::StringPairArray& userParams);

    juce::String buildWrapperScript (const juce::File&             inputWav,
                                     const juce::File&             outputWav,
                                     const juce::File&             communityScript,
                                     const juce::Array<FormParam>& params,
                                     const juce::StringPairArray&  userValues);

    juce::String praatStringEscape (const juce::String& s);
}
