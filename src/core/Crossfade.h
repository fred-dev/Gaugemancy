#pragma once

// -----------------------------------------------------------------------------
// Crossfade: the progress and the two gains of an equal-power crossfade.
//
//   outgoing gain = cos(progress * 90 degrees)     1 -> 0
//   incoming gain = sin(progress * 90 degrees)     0 -> 1
//
// The two gains squared always add up to 1, so two unrelated sounds keep the
// same loudness all the way through. Call advance() once per control frame.
// No openFrameworks dependency, so it is unit tested.
// -----------------------------------------------------------------------------

#include <algorithm>
#include <cmath>

namespace gm {

class Crossfade {
public:
	/// Longest time step counted at once, so one stalled control frame cannot
	/// jump the fade.
	static constexpr float kMaxStepSeconds = 0.1f;

	/// Begin a fade of `seconds` (0 or less: finished at once).
	void start(float seconds) {
		seconds_ = seconds;
		progress_ = seconds > 0.0f ? 0.0f : 1.0f;
	}

	void advance(float dtSeconds) {
		if (progress_ >= 1.0f) return;
		progress_ = std::min(1.0f, progress_ + std::clamp(dtSeconds, 0.0f, kMaxStepSeconds) / seconds_);
	}

	void finish() { progress_ = 1.0f; }

	bool done() const { return progress_ >= 1.0f; }
	float progress() const { return progress_; }

	float incomingGain() const { return done() ? 1.0f : std::sin(progress_ * kQuarterTurn); }
	float outgoingGain() const { return done() ? 0.0f : std::cos(progress_ * kQuarterTurn); }

private:
	static constexpr float kQuarterTurn = 1.5707963f;
	float seconds_ = 0.0f;
	float progress_ = 1.0f;
};

} // namespace gm
