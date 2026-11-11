#pragma once

// -----------------------------------------------------------------------------
// IdleDetector: decides when an effect chain may stop running.
//
// A chain must run while sound goes into it, and afterwards until its tail
// (reverb, delay echoes) has died away. The tail has died when the output
// stays below a tiny level for `holdSeconds`. The hold must be longer than
// the longest delay time, because the gap between two echoes is silent too.
//
// Call update() once per control frame. No openFrameworks dependency, so it
// is unit tested.
// -----------------------------------------------------------------------------

#include <algorithm>
#include <cmath>

namespace gm {

class IdleDetector {
public:
	/// Output below this (about -80 dB) counts as silence.
	static constexpr float kSilenceLevel = 0.0001f;
	/// Longest time step counted at once, so a stalled control loop (a
	/// preset loading) cannot use up the hold time in one frame.
	static constexpr float kMaxStepSeconds = 0.1f;

	void setHoldSeconds(float seconds) { holdSeconds_ = std::max(seconds, 0.0f); }

	/// `inputActive`: sound is going into the chain this frame.
	/// `outputLevel`: a recent sample of the chain's output.
	/// Returns true while the chain must keep running.
	bool update(bool inputActive, float outputLevel, float dtSeconds) {
		if (inputActive) {
			running_ = true;
			silentSeconds_ = 0.0f;
		} else if (running_) {
			bool silent = std::fabs(outputLevel) < kSilenceLevel;
			silentSeconds_ = silent ? silentSeconds_ + std::clamp(dtSeconds, 0.0f, kMaxStepSeconds) : 0.0f;
			if (silentSeconds_ >= holdSeconds_) {
				running_ = false;
			}
		}
		return running_;
	}

	bool running() const { return running_; }

private:
	float holdSeconds_ = 2.5f; // the delay effect goes up to 2 s
	float silentSeconds_ = 0.0f;
	bool running_ = false;
};

} // namespace gm
