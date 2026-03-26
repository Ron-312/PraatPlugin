#pragma once

#include <juce_core/juce_core.h>

// Composes a PraatRunner invocation from a script file and a parameter map.
//
// Praat scripts receive arguments positionally via `form/endform` blocks.
// This class orders the parameters and builds the StringArray passed to PraatRunner.
//
// Example:
//   script: pitch_analysis.praat
//   parameters: { "inputFile": "/tmp/input.wav", "minPitch": "75", "maxPitch": "600" }
//
//   -> launchArguments: ["/tmp/input.wav", "75", "600"]
//      (order matches the form fields declared in the script)
//
// For v1, parameter order is determined by the order they are inserted
// into the StringPairArray. Future versions may parse the `form` block
// from the script to determine the correct argument order automatically.
class PraatScriptInvoker
{
public:
    explicit PraatScriptInvoker (juce::File scriptFile);

    // Adds a named parameter. Parameters are passed to Praat in insertion order.
    void addParameter (const juce::String& name, const juce::String& value);

    // Clears all parameters.
    void clearParameters();

    // Returns the ordered list of argument values to pass to PraatRunner.
    juce::StringArray buildScriptArguments() const;

    juce::File scriptFile() const noexcept { return scriptFile_; }

private:
    juce::File            scriptFile_;
    juce::StringPairArray parameters_;   // name → value, insertion-ordered
};
