#pragma once

#include <cstdint>

// Pure logic, no openFrameworks/ofxPDSP/GPIO dependency -- unit tested
// directly (see tests/test_ButtonClickClassifier.cpp).
//
// Faithful, structure-preserving extraction of the single-button
// single/double/triple-click + long-press state machine that used to live in
// ofApp::buttonStateMachine(). This is a behavior-preserving port, not a
// cleanup -- see the class comment on HitGestureDetector.h for why that
// distinction matters here.
//
// The caller owns everything hardware/action-related: relay pulses, OSC
// debug sends, mode switching, and whether SixSecondHold actually triggers a
// shutdown (that was gated on a `shutdownPress` setting in the original,
// which stays a caller-side decision, not something this class knows about).
//
// One documented removal, not a preservation: the original's double-click
// handler had a nested `#ifndef HAS_ADC` branch that could never compile in
// (the enclosing function only existed under `#ifdef HAS_ADC` at all), so it
// never executed on any build -- it's simply not present here.
//
// Preserved quirk: nothing is emitted at all until `startupGuardMs`
// (default 5000ms) has elapsed -- the original silently ignored all button
// input for the first 5 seconds after boot, and click-count state is never
// even touched during that window.
class ButtonClickClassifier {
public:
    enum class Event {
        None,
        SingleClick,
        DoubleClick,
        TripleClick,
        ThreeSecondHold,
        SixSecondHold,
    };

    struct Config {
        uint64_t clickWindowMs = 220;  // was buttonPressTimeOut / BUTTON_PRESS_MAX_WAIT
        uint64_t startupGuardMs = 5000;
    };

    ButtonClickClassifier() = default;
    explicit ButtonClickClassifier(Config config) : config(config) {}

    // Call once per frame with the current (already-debounced/inverted)
    // button state -- true while pressed -- and the current elapsed-time
    // clock (matching ofGetElapsedTimeMillis()). Returns at most one event
    // per call.
    Event update(bool buttonPressed, uint64_t elapsedMillis);

private:
    Config config;

    int clicks = 0;
    uint64_t click1Time = 0, click2Time = 0, click3Time = 0;
    uint64_t clickOneReleaseTime = 0;
    bool waitingForClick = true;
};
