#pragma once

// -----------------------------------------------------------------------------
// Smoothing for control values (ball pressure, tilt, shake...).
//
// Smoother      one value, one-pole low-pass. The amount is a TIME constant in
//               milliseconds, so the feel does not change if the control rate
//               changes: after `timeMs` the output has moved ~63% of the way
//               to a new input. 0 ms = no smoothing.
//
// SmoothingBank one Smoother per modulation source, each with its own time,
//               applied to a whole ControlInputs frame in one call:
//
//   SmoothingBank bank;
//   bank.setTime(ModSource::Sensor1, 30.0f);   // light smoothing on a ball
//   bank.setTime(ModSource::TiltX, 120.0f);    // heavier on tilt
//   bank.process(inputs, dtSeconds);           // smooths in place
// -----------------------------------------------------------------------------

#include "ControlInputs.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace gm {

class Smoother {
public:
	void setTime(float milliseconds) { timeMs_ = milliseconds > 0.0f ? milliseconds : 0.0f; }
	float time() const { return timeMs_; }

	/// Feed one input; returns the smoothed value.
	float process(float input, float dtSeconds) {
		if (!primed_ || timeMs_ <= 0.0f) {
			value_ = input; // the first value is taken as-is, no slow fade-in
			primed_ = true;
			return value_;
		}
		float alpha = 1.0f - std::exp(-std::max(dtSeconds, 0.0f) * 1000.0f / timeMs_);
		value_ += alpha * (input - value_);
		return value_;
	}

	/// Jump to a value (e.g. after recalibrating).
	void reset(float value) {
		value_ = value;
		primed_ = true;
	}

	/// Forget the history; the next input is taken as-is.
	void clear() { primed_ = false; }

	float value() const { return value_; }

private:
	float timeMs_ = 0.0f;
	float value_ = 0.0f;
	bool primed_ = false;
};

class SmoothingBank {
public:
	void setTime(ModSource source, float milliseconds) { smoothers_[index(source)].setTime(milliseconds); }
	float time(ModSource source) const { return smoothers_[index(source)].time(); }

	/// Smooth one value of a given source.
	float process(ModSource source, float input, float dtSeconds) {
		return smoothers_[index(source)].process(input, dtSeconds);
	}

	/// Smooth every source of a frame in place.
	void process(ControlInputs & inputs, float dtSeconds) {
		for (int i = 0; i < kNumModSources; ++i) {
			inputs.values[i] = smoothers_[i].process(inputs.values[i], dtSeconds);
		}
	}

	/// Forget all history (next frame is taken as-is).
	void clear() {
		for (Smoother & s : smoothers_) s.clear();
	}

private:
	static int index(ModSource source) { return static_cast<int>(source); }
	std::array<Smoother, kNumModSources> smoothers_ {};
};

} // namespace gm
