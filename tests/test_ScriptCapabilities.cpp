#include <catch2/catch_test_macros.hpp>
#include "scripts/ScriptCapabilities.h"

namespace
{
    ScriptCapabilities analyse (const juce::String& code, juce::Array<FormParam> fields = {},
                                const juce::String& fileName = "x.praat")
    {
        return ScriptCapabilities::analyseSource (code, fields, fileName);
    }

    FormParam field (juce::String name, juce::String type, juce::String def = {})
    {
        FormParam p;
        p.name = name; p.type = type; p.defaultValue = def;
        return p;
    }
}

TEST_CASE ("ScriptCapabilities spots scripts that read a second Sound", "[ScriptCapabilities]")
{
    REQUIRE (analyse ("b = selected (\"Sound\", 2)\n").usesSecondSound);
    REQUIRE (analyse ("if numberOfSelected(\"Sound\") <> 2\n").usesSecondSound);
    REQUIRE (analyse ("exitScript: \"Please select exactly TWO Sound objects.\"\n").usesSecondSound);
    REQUIRE (analyse ("exitScript: \"Please select at least two Sound objects.\"\n").usesSecondSound);
    REQUIRE_FALSE (analyse ("s = selected (\"Sound\")\nids# = selected# (\"Sound\")\n").usesSecondSound);
    REQUIRE_FALSE (analyse ("# select two sounds for cross mode\n").usesSecondSound);   // comment only
    REQUIRE (analyse ("b = selected (\"Sound\", 2)\n").badge() == "2 SOUNDS");
}

TEST_CASE ("ScriptCapabilities marks scripts the plugin cannot run", "[ScriptCapabilities]")
{
    REQUIRE_FALSE (analyse ("runSystem: \"ffmpeg -i x\"\n").isSupported());
    REQUIRE_FALSE (analyse ("runSystem: \"python3 matrix_chain.py\"\n").isSupported());
    REQUIRE_FALSE (analyse ("p = selected (\"Photo\")\n").isSupported());
    REQUIRE (analyse ("s = selected (\"Sound\")\np = selected (\"Photo\")\n").isSupported());
    REQUIRE_FALSE (analyse ("", {}, "setup.praat").isSupported());
    REQUIRE (analyse ("p = selected (\"Photo\")\n").badge() == "UNSUPPORTED");
}

TEST_CASE ("ScriptCapabilities needs a path before scripts that open file dialogs", "[ScriptCapabilities]")
{
    const juce::String code = "if folder$ = \"\"\n  folder$ = chooseFolder$: \"Pick\"\nendif\n";

    // With a folder field: runnable once the field is filled.
    const auto caps = analyse (code, { field ("Folder", "sentence"), field ("Gain", "real", "1") });
    REQUIRE (caps.isSupported());
    REQUIRE (caps.pathFieldNames == juce::StringArray { "Folder" });
    REQUIRE (caps.badge() == "FOLDER");

    juce::StringPairArray values;
    REQUIRE (caps.checkBeforeRun (values).contains ("Folder"));
    values.set ("Folder", "/tmp/clips");
    REQUIRE (caps.checkBeforeRun (values).isEmpty());

    // No field to fill: often behind an option, so warn without blocking.
    const auto noField = analyse ("x$ = chooseWriteFile$: \"Save\", \"a.xml\"\n");
    REQUIRE (noField.isSupported());
    REQUIRE (noField.badge() == "DIALOG");
}

TEST_CASE ("ScriptCapabilities warns about Demo windows without blocking", "[ScriptCapabilities]")
{
    const auto caps = analyse ("if draw\n  demoWaitForInput ( )\nendif\n");
    REQUIRE (caps.mayOpenWindow);
    REQUIRE (caps.isSupported());
    REQUIRE (caps.badge() == "WINDOW");
}

TEST_CASE ("headlessDefault turns playback and editors off", "[ScriptCapabilities]")
{
    REQUIRE (ScriptCapabilities::headlessDefault (field ("Play_result", "boolean", "1")) == "0");
    REQUIRE (ScriptCapabilities::headlessDefault (field ("Open_textgrid_editor", "boolean", "1")) == "0");
    REQUIRE (ScriptCapabilities::headlessDefault (field ("Draw_visualization", "boolean", "1")) == "1");
    REQUIRE (ScriptCapabilities::headlessDefault (field ("Player_count", "integer", "3")) == "3");
}
