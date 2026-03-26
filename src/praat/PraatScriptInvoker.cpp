#include "praat/PraatScriptInvoker.h"

PraatScriptInvoker::PraatScriptInvoker (juce::File scriptFile)
    : scriptFile_ (std::move (scriptFile))
{
}

void PraatScriptInvoker::addParameter (const juce::String& name, const juce::String& value)
{
    parameters_.set (name, value);
}

void PraatScriptInvoker::clearParameters()
{
    parameters_.clear();
}

juce::StringArray PraatScriptInvoker::buildScriptArguments() const
{
    // Return values in insertion order — Praat `form` fields are positional.
    // StringPairArray does not support access by integer index directly.
    // getAllValues() returns a StringArray in insertion order.
    juce::StringArray arguments;
    for (const auto& value : parameters_.getAllValues())
        arguments.add (value);
    return arguments;
}
