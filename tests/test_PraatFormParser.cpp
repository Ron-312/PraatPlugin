#include <catch2/catch_test_macros.hpp>
#include "praat/PraatFormParser.h"

namespace
{
    // Writes `content` to a fresh temp .praat file that is deleted on scope exit.
    struct TempScript
    {
        explicit TempScript (const juce::String& content)
            : file (juce::File::createTempFile (".praat"))
        {
            file.replaceWithText (content);
        }
        ~TempScript() { file.deleteFile(); }
        juce::File file;
    };
}

TEST_CASE ("PraatFormParser reads the plugin's file-arg calling convention", "[PraatFormParser]")
{
    TempScript script ("form Pitch Shift\n"
                       "    sentence inputFile\n"
                       "    sentence outputFile\n"
                       "    real semitones 7.0\n"
                       "    boolean preserve_formants 1\n"
                       "endform\n");

    const auto all = PraatFormParser::parseAllFields (script.file);
    REQUIRE (all.size() == 4);
    REQUIRE (all[0].isInputFilePath);
    REQUIRE (all[1].isOutputFilePath);
    REQUIRE (all[2].name == "semitones");
    REQUIRE (all[2].type == "real");
    REQUIRE (all[2].defaultValue == "7.0");

    const auto extra = PraatFormParser::parseExtraParams (script.file);
    REQUIRE (extra.size() == 2);
    REQUIRE (extra[1].type == "boolean");
}

TEST_CASE ("PraatFormParser collects optionmenu options and skips comments", "[PraatFormParser]")
{
    TempScript script ("form Shaper\n"
                       "    comment Select a Sound object first\n"
                       "    optionmenu Preset: 1\n"
                       "        option Default\n"
                       "        option Aggressive Drive\n"
                       "    positive Fold_threshold 0.6\n"
                       "endform\n");

    const auto fields = PraatFormParser::parseAllFields (script.file);
    REQUIRE (fields.size() == 2);
    REQUIRE (fields[0].type == "optionmenu");
    REQUIRE (fields[0].options.size() == 2);
    REQUIRE (fields[0].options[1] == "Aggressive Drive");
    REQUIRE (fields[1].name == "Fold_threshold");
}

TEST_CASE ("PraatFormParser treats a script without file fields as Sound-object model", "[PraatFormParser]")
{
    TempScript script ("form Gain\n"
                       "    sentence Pattern abc\n"
                       "    real Gain 0.5\n"
                       "endform\n");

    for (const auto& f : PraatFormParser::parseAllFields (script.file))
    {
        REQUIRE_FALSE (f.isInputFilePath);
        REQUIRE_FALSE (f.isOutputFilePath);
    }
}

TEST_CASE ("PraatFormParser returns nothing for missing or form-less scripts", "[PraatFormParser]")
{
    REQUIRE (PraatFormParser::parseAllFields (juce::File ("/nonexistent/x.praat")).isEmpty());

    TempScript script ("writeInfoLine: \"hi\"\n");
    REQUIRE (PraatFormParser::parseAllFields (script.file).isEmpty());
}

// ── Known gaps, found by running the community library (see docs/script-compatibility.md).
// These document current behaviour; flip them to REQUIRE when the parser is fixed.

TEST_CASE ("KNOWN GAP: colon-syntax form fields are not parsed", "[PraatFormParser][!mayfail]")
{
    TempScript script ("form Echo\n"
                       "    real: \"Delay (s)\", \"0.25\"\n"
                       "    boolean: \"Play result\", 1\n"
                       "endform\n");

    CHECK (PraatFormParser::parseAllFields (script.file).size() == 2);
}

TEST_CASE ("KNOWN GAP: non-path word fields named like files are taken as file paths", "[PraatFormParser][!mayfail]")
{
    TempScript script ("form Montage\n"
                       "    word Output_name peephole_montage\n"
                       "    real Gain 1\n"
                       "endform\n");

    CHECK_FALSE (PraatFormParser::parseAllFields (script.file)[0].isInputFilePath);
}
