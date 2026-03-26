cmake_minimum_required(VERSION 3.22)

# ─── Catch2 via FetchContent ──────────────────────────────────────────────────
include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.5.2
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(Catch2)

# ─── Test runner executable ───────────────────────────────────────────────────
add_executable(PraatPluginTests
    test_ResultParser.cpp
    test_JobQueue.cpp
    test_PraatInstallationLocator.cpp
)

target_include_directories(PraatPluginTests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(PraatPluginTests
    PRIVATE
        Catch2::Catch2WithMain
        juce::juce_core
        juce::juce_audio_basics
        juce::juce_audio_formats
)

# ─── Source files under test ──────────────────────────────────────────────────
# We pull in individual source files rather than linking the whole plugin
# so tests don't require a full plugin host environment.
target_sources(PraatPluginTests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/results/ResultParser.cpp
    ${CMAKE_SOURCE_DIR}/src/jobs/AnalysisJob.cpp
    ${CMAKE_SOURCE_DIR}/src/praat/PraatInstallationLocator.cpp
    ${CMAKE_SOURCE_DIR}/src/scripts/ScriptManager.cpp
)

# ─── Register with CTest ──────────────────────────────────────────────────────
include(CTest)
include(Catch)
catch_discover_tests(PraatPluginTests)
