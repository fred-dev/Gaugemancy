#pragma once

// -----------------------------------------------------------------------------
// SensorSimulator: fake sensor input for programming the instrument on a
// laptop (or testing the Pi without touching it).
//
// Two ways to drive it:
//   * manual  - set each sensor level directly (GUI sliders, OSC, mouse)
//   * ramp    - the selected sensors rise to `extent` and fall back to 0 at
//               `speed` (fraction of full scale per second), like a squeeze.
//               One ramp is one squeeze; set `loop` to repeat.
// -----------------------------------------------------------------------------

#include "ControlInputs.h"

#include <array>

namespace gm {

class SensorSimulator {
public:
	struct RampSettings {
		float speed = 0.5f;   ///< full scale per second
		float extent = 1.0f;  ///< peak level 0..1
		bool loop = false;
		std::array<bool, kNumSensors> sensors { true, false, false, false, false, false };
	};

	/// Set a sensor level directly (stops a running ramp on that sensor).
	void setManual(int sensor, float level);
	float manual(int sensor) const { return manual_[sensor]; }

	void setRamp(const RampSettings & settings) { ramp_ = settings; }
	const RampSettings & ramp() const { return ramp_; }

	void startRamp();
	void stopRamp();
	bool rampRunning() const { return running_; }

	/// Advance by dt seconds and return normalised levels (0..1) per sensor.
	std::array<float, kNumSensors> update(float dtSeconds);

	/// Current ramp level (0..1).
	float rampLevel() const { return level_; }

private:
	std::array<float, kNumSensors> manual_ {};
	RampSettings ramp_;
	bool running_ = false;
	bool rising_ = true;
	float level_ = 0.0f;
};

} // namespace gm
