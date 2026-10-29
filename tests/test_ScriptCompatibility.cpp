// Runs every .praat script in a library through real Praat, the same way the
// plugin does (PraatFormParser → ScriptInvocation → PraatRunner), and writes a
// TSV report of what works.  Hidden from the default run; it needs Praat and a
// script folder:
//
//   PRAAT_EXE=/Applications/Praat.app/Contents/MacOS/Praat \
//   PRAAT_SCRIPTS_DIR=~/Library/Application\ Support/PraatPlugin/community_scripts/Praat-plugin_AudioTools-main \
//   PRAAT_COMPAT_REPORT=/tmp/compat.tsv \
//   PRAAT_COMPAT_SHARD=0/8 \
//   PRAAT_COMPAT_CHANNELS=2 \       (optional, default 1)
//   PRAAT_COMPAT_TIMEOUT_S=120 \    (optional, default PraatRunner's 30 s)
//   PRAAT_COMPAT_ONLY=Stereo \      (optional, substring filter on the path)
//   ./PraatPluginTests "[compat]"
//
// Default parameter values are used — what a user gets on first Run — except
// boolean "Play…" fields, which are forced off so the run stays silent.

#include <catch2/catch_test_macros.hpp>
#include <juce_audio_formats/juce_audio_formats.h>
#include "praat/PraatFormParser.h"
#include "praat/PraatRunner.h"
#include "praat/ScriptInvocation.h"

namespace
{
    juce::String env (const char* name)
    {
        return juce::SystemStats::getEnvironmentVariable (name, {});
    }

    // 1.5 s of a vibrato'd, harmonic-rich tone — gives pitch, formant and
    // onset analyses something to find.  Mono, 44.1 kHz, 16-bit, like a DAW take.
    void writeTestWav (const juce::File& file, int channels)
    {
        constexpr double sr = 44100.0;
        const int n = (int) (sr * 1.5);
        juce::AudioBuffer<float> buf (channels, n);
        double phase = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const double t  = i / sr;
            const double f0 = 150.0 + 10.0 * std::sin (2.0 * juce::MathConstants<double>::pi * 5.0 * t);
            phase += 2.0 * juce::MathConstants<double>::pi * f0 / sr;
            double s = 0.0;
            for (int h = 1; h <= 8; ++h)
                s += std::sin (h * phase) / h;
            const double env = std::min (1.0, std::min (t, 1.5 - t) * 20.0);
            for (int c = 0; c < channels; ++c)   // channels differ slightly so stereo tools see width
                buf.setSample (c, i, (float) (0.3 * s * env * (1.0 - 0.3 * c)));
        }

        file.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> out (file.createOutputStream().release());
        std::unique_ptr<juce::AudioFormatWriter> w (
            wav.createWriterFor (out.get(), sr, (unsigned int) channels, 16, {}, 0));
        REQUIRE (w != nullptr);
        out.release();
        w->writeFromAudioSampleBuffer (buf, 0, n);
    }

    // "ok" | "silent" | "unreadable"
    juce::String inspectWav (const juce::File& file)
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (file));
        if (r == nullptr || r->lengthInSamples <= 0)
            return "unreadable";
        juce::Range<float> ranges[8];
        const int ch = juce::jmin ((int) r->numChannels, 8);
        r->readMaxLevels (0, r->lengthInSamples, ranges, ch);
        float peak = 0.0f;
        for (int c = 0; c < ch; ++c)
            peak = juce::jmax (peak, std::abs (ranges[c].getStart()), std::abs (ranges[c].getEnd()));
        return peak > 1.0e-5f ? "ok" : "silent";
    }

    juce::String firstErrorLine (const juce::String& out)
    {
        for (auto line : juce::StringArray::fromLines (out))
            if (line.trim().startsWith ("Error:"))
                return line.trim().replaceCharacters ("\t", " ").substring (0, 200);
        for (auto line : juce::StringArray::fromLines (out))
        {
            line = line.trim();
            if (line.isNotEmpty() && ! line.startsWith ("Script \"") && ! line.startsWith ("Menu command"))
                return line.replaceCharacters ("\t", " ").substring (0, 200);
        }
        return out.trim().replaceCharacters ("\t\n", "  ").substring (0, 200);
    }
}

TEST_CASE ("Every library script runs under the plugin's calling model", "[.][compat]")
{
    const juce::File praat (env ("PRAAT_EXE"));
    const juce::File root  (env ("PRAAT_SCRIPTS_DIR"));
    if (! praat.existsAsFile() || ! root.isDirectory())
        SKIP ("set PRAAT_EXE and PRAAT_SCRIPTS_DIR");

    int shard = 0, shards = 1;
    if (env ("PRAAT_COMPAT_SHARD").contains ("/"))
    {
        shard  = env ("PRAAT_COMPAT_SHARD").upToFirstOccurrenceOf ("/", false, false).getIntValue();
        shards = juce::jmax (1, env ("PRAAT_COMPAT_SHARD").fromFirstOccurrenceOf ("/", false, false).getIntValue());
    }

    juce::Array<juce::File> scripts;
    for (const auto& e : juce::RangedDirectoryIterator (root, true, "*.praat"))
        if (! e.getFile().getFullPathName().contains ("/py/")   // ScriptManager skips py/
            && e.getFile().getFullPathName().contains (env ("PRAAT_COMPAT_ONLY")))
            scripts.add (e.getFile());
    scripts.sort();

    const juce::File report (env ("PRAAT_COMPAT_REPORT").isNotEmpty()
                                 ? env ("PRAAT_COMPAT_REPORT")
                                 : juce::File::getSpecialLocation (juce::File::tempDirectory)
                                       .getChildFile ("praat_compat.tsv").getFullPathName());
    juce::String tsv;

    for (int i = shard; i < scripts.size(); i += shards)
    {
        const auto& script = scripts.getReference (i);
        const auto work = juce::File::createTempFile ("").withFileExtension ("");
        work.createDirectory();
        const auto inWav  = work.getChildFile ("input.wav");
        const auto outWav = work.getChildFile ("output.wav");
        writeTestWav (inWav, juce::jmax (1, env ("PRAAT_COMPAT_CHANNELS").getIntValue()));

        const auto fields   = PraatFormParser::parseAllFields (script);
        juce::StringPairArray userValues;
        for (const auto& f : fields)
            if (f.type == "boolean" && f.name.startsWithIgnoreCase ("play"))
                userValues.set (f.name, "0");

        const bool wrapped  = ScriptInvocation::needsSoundObjectWrapper (fields);
        juce::File toRun    = script;
        juce::StringArray args;
        if (wrapped)
        {
            toRun = work.getChildFile ("wrapper.praat");
            toRun.replaceWithText (ScriptInvocation::buildWrapperScript (
                inWav, outWav, script, PraatFormParser::parseExtraParams (script), userValues));
        }
        else
        {
            args = ScriptInvocation::buildScriptArgs (script, inWav, outWav, userValues);
        }

        const auto t0 = juce::Time::getMillisecondCounter();
        PraatRunner runner (praat, PraatRunner::executableNeedsFullTrust (praat));
        if (env ("PRAAT_COMPAT_TIMEOUT_S").getIntValue() > 0)
            runner.setTimeoutMilliseconds (env ("PRAAT_COMPAT_TIMEOUT_S").getIntValue() * 1000);
        const auto outcome = runner.launchPraatWithScript (toRun, args);
        const auto ms = juce::Time::getMillisecondCounter() - t0;

        juce::String status, detail;
        if (outcome.exitedSuccessfully)
        {
            if (outWav.existsAsFile())
            {
                const auto w = inspectWav (outWav);
                status = w == "ok" ? "PASS_AUDIO" : "BAD_AUDIO";
                detail = w;
            }
            else if (outcome.standardOutput.trim().isNotEmpty())
            {
                status = "PASS_TEXT";
                detail = firstErrorLine (outcome.standardOutput);
            }
            else
            {
                status = "NO_OUTPUT";
            }
        }
        else
        {
            status = outcome.standardError.contains ("timed out") ? "TIMEOUT" : "FAIL";
            detail = firstErrorLine (outcome.standardError);
        }

        tsv << script.getParentDirectory().getFileName() << "\t" << script.getFileName() << "\t"
            << (wrapped ? "wrapper" : "file-args") << "\t" << fields.size() << "\t"
            << status << "\t" << (int) ms << "\t" << detail << "\n";

        work.deleteRecursively();
    }

    report.replaceWithText (tsv);
    SUCCEED ("report: " + report.getFullPathName().toStdString());
}
