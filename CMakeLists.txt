cmake_minimum_required(VERSION 3.22...4.3)

project(PraatPlugin
    VERSION 1.0.0
    DESCRIPTION "DAW plugin that runs Praat speech-analysis scripts"
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Generates compile_commands.json — used by clangd and most IDE C++ integrations
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ─── JUCE 8.0.12 via FetchContent (see docs/adr/ADR-001) ─────────────────────
include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.12
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# ─── Plugin target ────────────────────────────────────────────────────────────
juce_add_plugin(PraatPlugin
    COMPANY_NAME                "PraatPlugin"
    PLUGIN_MANUFACTURER_CODE    Prat
    PLUGIN_CODE                 Prt1

    FORMATS                     VST3 AU
    PRODUCT_NAME                "PraatPlugin"

    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE

    VST3_CATEGORIES             "Analyzer"
    AU_MAIN_TYPE                kAudioUnitType_Effect

    BUNDLE_ID                   com.praatplugin.plugin
)

# ─── Bundled Praat scripts (embedded as BinaryData, extracted to app-support on first run) ──
juce_add_binary_data(PraatPlugin_BinaryData SOURCES
    scripts/examples/pitch_analysis.praat
    scripts/examples/formant_analysis.praat
    scripts/examples/fuzz_distortion.praat
    scripts/examples/robot_voice.praat
    scripts/examples/pitch_shift.praat
)

# ─── Source files ─────────────────────────────────────────────────────────────
target_sources(PraatPlugin PRIVATE
    src/plugin/PraatPluginProcessor.cpp
    src/plugin/PraatPluginEditor.cpp
    src/ui/WaveformDisplay.cpp
    src/audio/AudioCapture.cpp
    src/audio/WavFileWriter.cpp
    src/praat/PraatRunner.cpp
    src/praat/PraatInstallationLocator.cpp
    src/praat/PraatScriptInvoker.cpp
    src/jobs/AnalysisJob.cpp
    src/jobs/JobQueue.cpp
    src/jobs/JobDispatcher.cpp
    src/scripts/ScriptManager.cpp
    src/results/ResultParser.cpp
)

target_include_directories(PraatPlugin PRIVATE src)

# ─── JUCE modules ─────────────────────────────────────────────────────────────
target_link_libraries(PraatPlugin
    PRIVATE
        PraatPlugin_BinaryData          # bundled .praat scripts (see juce_add_binary_data above)
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats        # WavAudioFormat for writing temp WAV files
        juce::juce_audio_plugin_client  # VST3 and AU plugin format wrappers
        juce::juce_audio_processors
        juce::juce_audio_utils          # AudioFormatManager, AudioTransportSource (ADR-008)
        juce::juce_core                 # File, ChildProcess, Thread, Uuid
        juce::juce_events               # MessageManager, AsyncUpdater, Timer
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ─── Compile definitions ──────────────────────────────────────────────────────
target_compile_definitions(PraatPlugin
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

# ─── Ad-hoc re-sign after build (fixes "sealed resource invalid" on macOS) ────
# JUCE's initial ad-hoc signature can be incomplete. This post-build step
# strips it and applies a clean one so Ableton's scanner accepts the bundle.
# See docs/adr/ADR-007 for context.
add_custom_command(TARGET PraatPlugin_VST3 POST_BUILD
    COMMAND codesign --force --deep --sign -
            "$<TARGET_BUNDLE_DIR:PraatPlugin_VST3>"
    COMMENT "Re-applying clean ad-hoc signature to VST3 bundle"
)

# ─── Optional test suite ──────────────────────────────────────────────────────
option(BUILD_TESTS "Build unit tests (requires Catch2)" OFF)
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
