#include <catch2/catch_test_macros.hpp>
#include "praat/PraatInstallationLocator.h"

TEST_CASE ("PraatInstallationLocator respects user override path", "[PraatInstallationLocator]")
{
    PraatInstallationLocator locator;

    // Before any override, no override is active.
    REQUIRE_FALSE (locator.userHasOverriddenExecutablePath());

    // Setting an override stores it.
    const juce::File fakePath ("/usr/local/bin/praat");
    locator.overrideExecutablePathWithUserChoice (fakePath);

    REQUIRE (locator.userHasOverriddenExecutablePath());
    REQUIRE (locator.userOverriddenExecutablePath() == fakePath);
}

TEST_CASE ("PraatInstallationLocator describes its search result", "[PraatInstallationLocator]")
{
    PraatInstallationLocator locator;

    const auto description = locator.describeSearchResult();

    // Must return a non-empty human-readable string in all cases.
    REQUIRE (description.isNotEmpty());
}

TEST_CASE ("PraatInstallationLocator isPraatInstalled reflects filesystem reality",
           "[PraatInstallationLocator]")
{
    PraatInstallationLocator locator;

    // Override with a path we know doesn't exist — isPraatInstalled must return false.
    locator.overrideExecutablePathWithUserChoice (
        juce::File ("/nonexistent/path/Praat"));

    REQUIRE_FALSE (locator.isPraatInstalled());
}
