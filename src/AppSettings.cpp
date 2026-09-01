#include "AppSettings.h"

AppSettings AppSettings::loadFrom(const nlohmann::json & json)
{
    AppSettings s;

    s.logLevel = json.value("LOG_LEVEL", 1);

    s.hasNarration = json.value("HAS_NARRATION", 0);
    s.narrationVolume = json.value("NARRATION_VOLUME", 1.0);
    s.narrationGlitchThreshold = json.value("NARRATION_GLITCH_THRESH", 0.025);
    s.narrationGlitchStrand = json.value("NARRATION_GLITCH_STRAND", 0.0025);
    s.narrationUsesSensor = json.value("NARR_PLAY_WITH_SENSOR", false);

    s.maxSensorValue = json.value("MAX_SENSOR_VALUE", 520);
    s.normalisedA2DValuesMin = json.value("ACTIVE_THRESHOLD", 0.0025);
    s.curveSelector = json.value("EASING_SELECTOR", 0);
    s.accumulationDenominator = json.value("ACCUMULATION_DENOMINATOR", 4);

    s.engineBufferSize = json.value("BUFFER_SIZE", 512);
    s.numberOfBuffers = json.value("NUMBER_OF_BUFFERS", 3);
    s.audioDeviceId = json.value("AUDIO_DEVICE_ID", 1);

    s.useHitGesture = json.value("HIT_TO_CHANGE_PRESETS", false);
    s.maxTroughDuration = json.value("MAX_TROUGH_DURATION", 250);
    s.maxPeakDuration = json.value("MAX_PEAK_DURATION", 80);
    s.requiredHits = json.value("REQUIRED_HITS", 8);
    s.hitThreshHold = json.value("HIT_THRESHOLD", 0.085);
    s.troughThreshold = json.value("TROUGH_THRESHOLD", 0.025);

    s.useAccumulatedPressure = json.value("ACCUMULATED_PRESSURE", false);
    for (int i = 0; i < kNumberOfPresets; i++) {
        s.timeAdvanceInterval[i] = json.value("TIME_ADVANCE_INTERVAL_" + std::to_string(i + 1), 0.0002);
    }

    s.setLocalIp = json.value("SET_LOCAL_IP", false);
    s.localIp = json.value("LOCAL_IP", "192.168.1.15");
    s.setLocalToDHCP = json.value("SET_DHCP", false);
    s.localOSCPport = json.value("LOCAL_OSC_PORT", 1234);
    s.remoteOSCPort = json.value("REMOTE_OSC_PORT", 1235);
    s.remoteOSCIp = json.value("REMOTE_OSC_IP", "192.168.178.236");
    s.oscDebug = json.value("OSC_DEBUG", false);
    s.oscLive = json.value("OSC_LIVE", false);

    s.buttonPressTimeOut = json.value("BUTTON_PRESS_MAX_WAIT", 220);
    s.shutdownPress = json.value("SH_P", false);
    s.doShutdown = json.value("D_SH", false);

    return s;
}

std::string loadUnitId(const nlohmann::json & usernameJson)
{
    return usernameJson.value("USERNAME", "NO_UNIT_ID");
}
