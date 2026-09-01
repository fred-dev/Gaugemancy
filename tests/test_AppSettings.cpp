#include "doctest.h"
#include "AppSettings.h"
#include <fstream>

static nlohmann::json loadJsonFile(const std::string & path)
{
    std::ifstream f(path);
    REQUIRE_MESSAGE(f.good(), "couldn't open " << path);
    nlohmann::json j;
    f >> j;
    return j;
}

TEST_CASE("an empty JSON object produces exactly the struct's own defaults")
{
    // guards against loadFrom()'s .value(key, default) literals drifting
    // out of sync with the struct's in-class default member initializers
    AppSettings defaults;
    AppSettings loaded = AppSettings::loadFrom(nlohmann::json::object());

    CHECK(loaded.logLevel == defaults.logLevel);
    CHECK(loaded.hasNarration == defaults.hasNarration);
    CHECK(loaded.narrationVolume == defaults.narrationVolume);
    CHECK(loaded.maxSensorValue == defaults.maxSensorValue);
    CHECK(loaded.engineBufferSize == defaults.engineBufferSize);
    CHECK(loaded.useHitGesture == defaults.useHitGesture);
    CHECK(loaded.requiredHits == defaults.requiredHits);
    CHECK(loaded.useAccumulatedPressure == defaults.useAccumulatedPressure);
    CHECK(loaded.timeAdvanceInterval == defaults.timeAdvanceInterval);
    CHECK(loaded.localIp == defaults.localIp);
    CHECK(loaded.remoteOSCIp == defaults.remoteOSCIp);
    CHECK(loaded.buttonPressTimeOut == defaults.buttonPressTimeOut);
    CHECK(loaded.shutdownPress == defaults.shutdownPress);
    CHECK(loaded.doShutdown == defaults.doShutdown);
}

TEST_CASE("loadUnitId reads USERNAME, falling back to NO_UNIT_ID")
{
    CHECK(loadUnitId(nlohmann::json{{"USERNAME", "Fred"}}) == "Fred");
    CHECK(loadUnitId(nlohmann::json::object()) == "NO_UNIT_ID");
}

// The five checks below load the real, checked-in bin/data/*/*_appSettings.json
// files and assert against the values they're known to contain -- this is
// the "diff against the current run for each of the five user profiles"
// check the extraction plan called for. A couple of these values were
// confirmed directly against a real console log from running the app as
// Fred (MAX_SENSOR_VALUE=530, BUFFER_SIZE=1024, AUDIO_DEVICE_ID=5, etc.)
// earlier in this refactor.

TEST_CASE("Fred's real appSettings.json loads correctly")
{
    AppSettings s = AppSettings::loadFrom(loadJsonFile("bin/data/Fred/Fred_appSettings.json"));
    CHECK(s.maxSensorValue == 530);
    CHECK(s.normalisedA2DValuesMin == doctest::Approx(0.035));
    CHECK(s.useAccumulatedPressure == false);
    CHECK(s.engineBufferSize == 1024);
    CHECK(s.numberOfBuffers == 2);
    CHECK(s.audioDeviceId == 4);
    CHECK(s.logLevel == 2);
    CHECK(s.narrationUsesSensor == true);
    CHECK(s.hitThreshHold == doctest::Approx(0.25));
    CHECK(s.requiredHits == 6);
    CHECK(s.localIp == "192.168.1.5");
}

TEST_CASE("Emi's real appSettings.json loads correctly")
{
    AppSettings s = AppSettings::loadFrom(loadJsonFile("bin/data/Emi/Emi_appSettings.json"));
    CHECK(s.maxSensorValue == 1024);
    CHECK(s.normalisedA2DValuesMin == doctest::Approx(0.00018));
    CHECK(s.engineBufferSize == 512);
    CHECK(s.audioDeviceId == 2);
    CHECK(s.logLevel == 1);
    CHECK(s.shutdownPress == true);
    CHECK(s.doShutdown == true);
}

TEST_CASE("Clement's real appSettings.json loads correctly, including the "
          "per-preset TIME_ADVANCE_INTERVAL overrides")
{
    AppSettings s = AppSettings::loadFrom(loadJsonFile("bin/data/Clement/Clement_appSettings.json"));
    CHECK(s.maxSensorValue == 500);
    CHECK(s.numberOfBuffers == 2);
    CHECK(s.narrationVolume == doctest::Approx(0.9));
    // LOG_LEVEL 0 here reflects an XML->JSON migration quirk: the original
    // XML had LOG_LEVEL as the string "0.5", which ofxXmlSettings' int
    // parser truncated to 0 -- preserved as 0 in the JSON, not "fixed" to 1.
    CHECK(s.logLevel == 0);
    CHECK(s.timeAdvanceInterval[0] == doctest::Approx(0.00018));
    CHECK(s.timeAdvanceInterval[1] == doctest::Approx(0.00019));
    CHECK(s.timeAdvanceInterval[2] == doctest::Approx(0.00008));
    CHECK(s.timeAdvanceInterval[3] == doctest::Approx(0.00005));
}

TEST_CASE("Miguel's real appSettings.json loads correctly")
{
    AppSettings s = AppSettings::loadFrom(loadJsonFile("bin/data/Miguel/Miguel_appSettings.json"));
    CHECK(s.maxSensorValue == 500);
    CHECK(s.useAccumulatedPressure == true);
    CHECK(s.audioDeviceId == 1);
    CHECK(s.logLevel == 1);
}

TEST_CASE("Chandra's real appSettings.json loads correctly")
{
    AppSettings s = AppSettings::loadFrom(loadJsonFile("bin/data/Chandra/Chandra_appSettings.json"));
    CHECK(s.maxSensorValue == 500);
    CHECK(s.useAccumulatedPressure == true);
    CHECK(s.audioDeviceId == 1);
    CHECK(s.logLevel == 1);
}
