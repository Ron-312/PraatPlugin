#pragma once
#include <juce_core/juce_core.h>
#include "praat/PraatFormParser.h"

// What a script needs beyond "one Sound selected", read from its source.
//
// Community scripts are written for interactive Praat.  The plugin runs them
// headless with one Sound, so some need extra inputs (a second Sound, a folder)
// and some cannot work at all (image input, external tools, Praat editors).
// The checks are deliberately narrow — each was matched against a real run of
// the AudioTools library (docs/script-compatibility.md).
struct ScriptCapabilities
{
    // Script reads a second selected Sound (cross-synthesis, morphing, convolution…).
    // Optional for some (XMod, LPC_Excitation_Lab), so it enables the Sound B
    // input rather than blocking a run.
    bool usesSecondSound { false };

    // Script may open a Demo window that waits for mouse input.  Often behind an
    // option, so this is a warning only.
    bool mayOpenWindow { false };

    // Script calls chooseFolder$ / chooseReadFile$ / chooseWriteFile$.  Headless
    // Praat cannot show the dialog and hangs, so path fields must be filled.
    // Without path fields the dialog is often behind an option: warning only.
    bool opensFileDialog { false };

    // Form fields that hold a file or folder path (shown with a Browse button).
    juce::StringArray pathFieldNames;

    // Non-empty → the plugin cannot run this script; the text says why.
    juce::String unsupportedReason;

    bool isSupported() const noexcept { return unsupportedReason.isEmpty(); }

    // Short tag for the script browser: "2 SOUNDS", "FOLDER", "DIALOG", "WINDOW",
    // "UNSUPPORTED", or empty.
    juce::String badge() const;

    static ScriptCapabilities analyse (const juce::File& scriptFile);
    static ScriptCapabilities analyseSource (const juce::String& source,
                                             const juce::Array<FormParam>& fields,
                                             const juce::String& fileName = {});

    // The value the plugin starts a field at: the script's default, except
    // "Play…" and "…editor" booleans, which start off.
    static juce::String headlessDefault (const FormParam& field);

    // Returns an error message if the script cannot run with these parameter
    // values (e.g. a file dialog would open), or empty if it can.
    juce::String checkBeforeRun (const juce::StringPairArray& paramValues) const;
};
