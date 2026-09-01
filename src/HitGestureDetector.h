#pragma once

#include <array>
#include <cstdint>

// Pure logic, no openFrameworks/ofxPDSP dependency -- unit tested directly
// (see tests/test_HitGestureDetector.cpp).
//
// This is a faithful, structure-preserving extraction of the gestural-trigger
// state machine that used to live in ofApp::checkForHits(): performers squeeze
// several pressure sensors together, rapidly, N times in a row, to trigger an
// action (originally: switch presets). It's a literal behavior-preserving
// port, not a cleanup -- the original state machine has real quirks (below)
// that are preserved here exactly, pinned down by tests, so a later pass can
// clean it up with confidence instead of guessing at intent.
//
// Known preserved quirks (do not "fix" without discussing first -- these may
// or may not be intentional):
//   1. The peak/trough sensor scan can hit its "found enough" branch mid-loop
//      (e.g. right after sensor index 1), before later-indexed sensors are
//      scanned on that same call.
//   2. A hit-gesture completing while still inside the trough-detection
//      *window* (elapsed < maxTroughDurationMs) resets fully back to idle.
//      A hit-gesture completing *after* the window has expired (elapsed >
//      maxTroughDurationMs) does NOT clear hadHitPeak/hadHitTrough and
//      immediately re-arms into peak-checking instead of going idle. These
//      are two different code paths for what looks like the same logical
//      event ("enough troughs found"), with different reset behavior.
//   3. Aborting because the trough window expired without enough troughs
//      resets completedFullHitsCount() to 0 (partial progress is lost).
//      Aborting because the *peak* window expired without enough peaks does
//      NOT reset it -- partial progress from an earlier completed cycle
//      survives a peak-timeout, but not a trough-timeout.
class HitGestureDetector {
public:
    static constexpr int kNumSensors = 6;

    struct Config {
        float hitThreshold = 0.085f;
        float troughThreshold = 0.025f;
        uint64_t maxPeakDurationMs = 80;
        uint64_t maxTroughDurationMs = 250;
        int requiredHits = 8;
    };

    HitGestureDetector() = default;
    explicit HitGestureDetector(Config config) : config(config) {}

    // Call once per frame with the current normalized sensor readings and the
    // current elapsed-time clock (matching ofGetElapsedTimeMillis()). Returns
    // true on exactly the frame a full hit gesture completes.
    bool update(const std::array<float, kNumSensors> & normalizedValues, uint64_t elapsedMillis);

    bool isCheckingPeaks() const { return isCheckingHitPeaks; }
    bool isCheckingTroughs() const { return isCheckingHitTroughs; }
    int completedFullHitsCount() const { return completedFullHits; }

private:
    Config config;

    bool isCheckingHitPeaks = false;
    bool isCheckingHitTroughs = false;
    int hitPeakChecker = 0;
    int hitTroughChecker = 0;
    std::array<bool, kNumSensors> hadHitPeak{};
    std::array<bool, kNumSensors> hadHitTrough{};
    uint64_t timeSinceLastHitPeak = 0;
    uint64_t timeSinceLastHitTrough = 0;
    int completedFullHits = 0;
};
