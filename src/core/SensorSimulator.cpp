#include "SensorSimulator.h"

#include <algorithm>

namespace gm {

void SensorSimulator::setManual(int sensor, float level) {
	if (sensor >= 0 && sensor < kNumSensors) {
		manual_[sensor] = std::clamp(level, 0.0f, 1.0f);
	}
}

void SensorSimulator::startRamp() {
	running_ = true;
	rising_ = true;
	level_ = 0.0f;
}

void SensorSimulator::stopRamp() {
	running_ = false;
	level_ = 0.0f;
}

std::array<float, kNumSensors> SensorSimulator::update(float dtSeconds) {
	if (running_) {
		float step = ramp_.speed * std::max(dtSeconds, 0.0f);
		if (rising_) {
			level_ += step;
			if (level_ >= ramp_.extent) {
				level_ = ramp_.extent;
				rising_ = false;
			}
		} else {
			level_ -= step;
			if (level_ <= 0.0f) {
				level_ = 0.0f;
				rising_ = true;
				running_ = ramp_.loop;
			}
		}
	}

	std::array<float, kNumSensors> out = manual_;
	for (int i = 0; i < kNumSensors; ++i) {
		if (running_ && ramp_.sensors[i]) {
			out[i] = std::max(out[i], level_);
		}
	}
	return out;
}

} // namespace gm
