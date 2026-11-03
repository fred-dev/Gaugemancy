#include "HitGestureDetector.h"

namespace gm {

void HitGestureDetector::reset() {
	state_ = State::Idle;
	stateStartMs_ = 0;
	hits_ = 0;
}

int HitGestureDetector::countAbove(const ControlInputs & inputs, float threshold) const {
	int count = 0;
	for (int i = 0; i < kNumSensors; ++i) {
		if (inputs.sensor(i) > threshold) ++count;
	}
	return count;
}

int HitGestureDetector::countBelow(const ControlInputs & inputs, float threshold) const {
	int count = 0;
	for (int i = 0; i < kNumSensors; ++i) {
		if (inputs.sensor(i) < threshold) ++count;
	}
	return count;
}

bool HitGestureDetector::update(const ControlInputs & inputs, uint64_t nowMs) {
	const int needed = settings_.minSensorsPerHit;
	const bool squeezed = countAbove(inputs, settings_.hitThreshold) >= needed;
	// "Released" means every ball is low again, not just some of them.
	const bool released = countBelow(inputs, settings_.troughThreshold) >= kNumSensors;
	const uint64_t elapsed = nowMs - stateStartMs_;

	switch (state_) {
	case State::Idle:
		if (squeezed) {
			state_ = State::InPeak;
			stateStartMs_ = nowMs;
		}
		break;

	case State::InPeak:
		if (released) {
			++hits_;
			if (hits_ >= settings_.requiredHits) {
				reset();
				return true;
			}
			state_ = State::InTrough;
			stateStartMs_ = nowMs;
		} else if (elapsed > static_cast<uint64_t>(settings_.maxPeakMs)) {
			reset(); // held too long: that was a squeeze, not a hit
		}
		break;

	case State::InTrough:
		if (squeezed) {
			state_ = State::InPeak;
			stateStartMs_ = nowMs;
		} else if (elapsed > static_cast<uint64_t>(settings_.maxTroughMs)) {
			reset(); // paused too long between hits
		}
		break;
	}
	return false;
}

} // namespace gm
