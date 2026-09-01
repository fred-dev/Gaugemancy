#pragma once

#include <array>
#include <string>
#include <nlohmann/json.hpp>

// Pure logic -- no openFrameworks dependency beyond the vendored
// nlohmann::json header (the same library ofJson.h wraps as `ofJson`) -- so
// this is unit tested directly (see tests/test_AppSettings.cpp) against
// literal JSON, no filesystem or ofApp needed.
//
// Faithful extraction of the field-reading half of what used to be the
// 244-line ofApp::setupParamsFromSettings(): every key/default pair here is
// copied verbatim from that function, including the odd ones (e.g.
// ACCUMULATION_DENOMINATOR's JSON default is the integer 4, not 4.0f --
// preserved exactly, since nlohmann deduces the read type from the default
// argument, not from the field it gets assigned into).
//
// What's deliberately NOT here, and stays in ofApp::setupParamsFromSettings()
// as orchestration: ofSetLogLevel(), deriving numberOfSlots/
// grainOperationModeTranslate/simulationOperationModeTranslate from
// accumulatedPressure, constructing HitGestureDetector/ButtonClickClassifier,
// the OSC sender setup + debug sends, and the dhclient/ifconfig shell
// commands. None of that is "read a field with a default," so none of it
// belongs in a pure loader.
//
// The three BUTTON_PRESS_MAX_WAIT/SH_P/D_SH fields were only ever read
// under `#ifdef HAS_ADC` in the original -- they're read unconditionally
// here (reading three extra, harmlessly-defaulted fields from a laptop's
// appSettings.json costs nothing) and it's still only ofApp's HAS_ADC-only
// code that does anything with them.
struct AppSettings {
    static constexpr int kNumberOfPresets = 4;

    int logLevel = 1;

    bool hasNarration = false;
    float narrationVolume = 1.0f;
    float narrationGlitchThreshold = 0.025f;
    float narrationGlitchStrand = 0.0025f;
    bool narrationUsesSensor = false;

    int maxSensorValue = 520;
    float normalisedA2DValuesMin = 0.0025f;
    int curveSelector = 0;
    float accumulationDenominator = 4;

    int engineBufferSize = 512;
    int numberOfBuffers = 3;
    int audioDeviceId = 1;

    bool useHitGesture = false;
    int maxTroughDuration = 250;
    int maxPeakDuration = 80;
    int requiredHits = 8;
    float hitThreshHold = 0.085f;
    float troughThreshold = 0.025f;

    bool useAccumulatedPressure = false;
    std::array<float, kNumberOfPresets> timeAdvanceInterval = {0.0002f, 0.0002f, 0.0002f, 0.0002f};

    bool setLocalIp = false;
    std::string localIp = "192.168.1.15";
    bool setLocalToDHCP = false;
    int localOSCPport = 1234;
    int remoteOSCPort = 1235;
    std::string remoteOSCIp = "192.168.178.236";
    bool oscDebug = false;
    bool oscLive = false;

    int buttonPressTimeOut = 220;
    bool shutdownPress = false;
    bool doShutdown = false;

    static AppSettings loadFrom(const nlohmann::json & json);
};

// Reads username.json's USERNAME field (falls back to "NO_UNIT_ID", matching
// the original ofApp::setupParamsFromSettings()).
std::string loadUnitId(const nlohmann::json & usernameJson);
