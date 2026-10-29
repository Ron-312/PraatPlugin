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

// ── Found by running the community library (see docs/script-compatibility.md).

TEST_CASE ("PraatFormParser reads colon-syntax form fields", "[PraatFormParser]")
{
    TempScript script ("form Echo\n"
                       "    comment: \"Select a Sound\"\n"
                       "    real: \"Delay (s)\", \"0.25\"\n"
                       "    optionMenu: \"Mode\", 2\n"
                       "        option: \"Soft, warm\"\n"
                       "        option: \"Hard\"\n"
                       "    boolean: \"Play result\", 1\n"
                       "    sentence: \"Pitch range\", \"75 600\"\n"
                       "    folder: \"Output folder\", \".\"\n"
                       "endform\n");

    const auto f = PraatFormParser::parseAllFields (script.file);
    REQUIRE (f.size() == 5);
    REQUIRE (f[0].name == "Delay");
    REQUIRE (f[0].defaultValue == "0.25");
    REQUIRE (f[1].type == "optionmenu");
    REQUIRE (f[1].options == juce::StringArray { "Soft, warm", "Hard" });
    REQUIRE (f[2].name == "Play_result");
    REQUIRE (f[3].defaultValue == "75 600");
    REQUIRE (f[4].type == "folder");
    for (const auto& p : f)
        REQUIRE_FALSE (p.isInputFilePath);
}

TEST_CASE ("PraatFormParser does not take named text fields for file paths", "[PraatFormParser]")
{
    TempScript script ("form Montage\n"
                       "    word Output_name peephole_montage\n"
                       "    sentence Sofa_file kemar\n"
                       "    real Gain 1\n"
                       "endform\n");

    for (const auto& p : PraatFormParser::parseAllFields (script.file))
        REQUIRE_FALSE (p.isInputFilePath);
}

TEST_CASE ("PraatFormParser keeps the bundled path-default convention", "[PraatFormParser]")
{
    TempScript script ("form X\n"
                       "    sentence inputFile /tmp/input.wav\n"
                       "    sentence outputFile /tmp/output.wav\n"
                       "endform\n");

    const auto f = PraatFormParser::parseAllFields (script.file);
    REQUIRE (f[0].isInputFilePath);
    REQUIRE (f[1].isOutputFilePath);
}

TEST_CASE ("PraatFormParser handles form: headers, padded endform and hinted defaults", "[PraatFormParser]")
{
    TempScript script ("form: \"IR Analysis\"\n"
                       "    integer Dots_line_1 5 (= top line, left-panned)\n"
                       "endform   \r\n"
                       "form Second\n"
                       "    real Ignored 1\n"
                       "endform\n");

    const auto f = PraatFormParser::parseAllFields (script.file);
    REQUIRE (f.size() == 1);
    REQUIRE (f[0].defaultValue == "5");
}
