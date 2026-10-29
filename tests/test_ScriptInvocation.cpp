#include <catch2/catch_test_macros.hpp>
#include "praat/ScriptInvocation.h"

namespace
{
    FormParam makeParam (juce::String name, juce::String type, juce::String def,
                         juce::StringArray options = {})
    {
        FormParam p;
        p.name = name; p.type = type; p.defaultValue = def; p.options = options;
        return p;
    }
}

TEST_CASE ("buildScriptArgs fills file fields and falls back to defaults", "[ScriptInvocation]")
{
    auto script = juce::File::createTempFile (".praat");
    script.replaceWithText ("form X\n"
                            "    optionmenu Mode: 1\n"
                            "        option Soft\n"
                            "        option Hard\n"
                            "    sentence inputFile\n"
                            "    sentence outputFile\n"
                            "    real Gain 0.5\n"
                            "endform\n");

    juce::StringPairArray user;
    user.set ("Mode:", "2");

    const auto args = ScriptInvocation::buildScriptArgs (
        script, juce::File ("/tmp/in.wav"), juce::File ("/tmp/out.wav"), user);
    script.deleteFile();

    REQUIRE (args.size() == 4);
    REQUIRE (args[0] == "Hard");          // 1-based index → option name
    REQUIRE (args[1] == "/tmp/in.wav");
    REQUIRE (args[2] == "/tmp/out.wav");
    REQUIRE (args[3] == "0.5");           // default
}

TEST_CASE ("buildWrapperScript loads, runs and saves in Praat syntax", "[ScriptInvocation]")
{
    juce::Array<FormParam> params;
    params.add (makeParam ("Preset:", "optionmenu", "1", { "Default", "Hard" }));
    params.add (makeParam ("Gain", "real", "0.5"));
    params.add (makeParam ("Label", "sentence", "a \"quoted\" name"));

    juce::StringPairArray user;
    user.set ("Gain", "0.9");

    const auto w = ScriptInvocation::buildWrapperScript (
        juce::File ("/tmp/in.wav"), juce::File ("/tmp/out.wav"),
        juce::File ("/tmp/s.praat"), params, user);

    REQUIRE (w.contains ("Read from file: \"/tmp/in.wav\""));
    REQUIRE (w.contains ("runScript: \"/tmp/s.praat\", \"Default\", 0.9, \"a \\\"quoted\\\" name\""));
    REQUIRE (w.contains ("Save as WAV file: \"/tmp/out.wav\""));
}

TEST_CASE ("needsSoundObjectWrapper follows the input-file flag", "[ScriptInvocation]")
{
    juce::Array<FormParam> fields;
    fields.add (makeParam ("Gain", "real", "1"));
    REQUIRE (ScriptInvocation::needsSoundObjectWrapper (fields));

    auto in = makeParam ("inputFile", "sentence", "");
    in.isInputFilePath = true;
    fields.add (in);
    REQUIRE_FALSE (ScriptInvocation::needsSoundObjectWrapper (fields));
}

TEST_CASE ("buildWrapperScript selects Sound A then Sound B", "[ScriptInvocation]")
{
    const auto w = ScriptInvocation::buildWrapperScript (
        juce::File ("/tmp/a.wav"), juce::File ("/tmp/out.wav"), juce::File ("/tmp/s.praat"),
        {}, {}, juce::File ("/tmp/b.wav"));

    REQUIRE (w.indexOf ("/tmp/a.wav") < w.indexOf ("/tmp/b.wav"));
    REQUIRE (w.contains ("selectObject: inputSound__, inputSoundB__"));
}
