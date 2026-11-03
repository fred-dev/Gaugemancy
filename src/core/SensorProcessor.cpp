#include "SensorProcessor.h"

#include <algorithm>

namespace gm {

void SensorProcessor::configure(const SensorSettings & sensors, const MotionSettings & motion) {
	settings_ = sensors;
	for (int i = 0; i < kNumSensors; ++i) {
		smoothing_.setTime(sensorSource(i), sensors.smoothingMs);
	}
	smoothing_.setTime(ModSource::TiltX, motion.smoothingMs);
	smoothing_.setTime(ModSource::TiltY, motion.smoothingMs);
	smoothing_.setTime(ModSource::Tilt, motion.smoothingMs);
	// Shake already has its own release envelope; total follows the balls.
	motion_.configure(motion);
}

void SensorProcessor::beginCalibration() {
	calibrationSums_.fill(0);
	calibrationCount_ = 0;
	smoothing_.clear();
	motion_.beginCalibration();
}

bool SensorProcessor::isCalibrating() const {
	return calibrationCount_ < settings_.calibrationFrames;
}

void SensorProcessor::setZeroPoints(const RawSensorFrame & zeros) {
	zeros_ = zeros;
	calibrationCount_ = settings_.calibrationFrames; // ball calibration complete
}

float SensorProcessor::normalise(int raw, int zero, int maxRaw) {
	if (maxRaw <= zero) {
		return 0.0f; // a ball resting above "full" pressure cannot be read
	}
	float value = static_cast<float>(raw - zero) / static_cast<float>(maxRaw - zero);
	return std::clamp(value, 0.0f, 1.0f);
}

float SensorProcessor::shape(float normalised, float threshold, Curve curve) {
	if (normalised < threshold || normalised <= 0.0f) {
		return 0.0f;
	}
	return applyCurve(curve, normalised);
}

float SensorProcessor::totalPressure(const ControlInputs & inputs, float denominator) {
	float sum = 0.0f;
	for (int i = 0; i < kNumSensors; ++i) {
		sum += inputs.sensor(i);
	}
	if (denominator <= 0.0f) {
		return 0.0f;
	}
	return std::clamp(sum / denominator, 0.0f, 1.0f);
}

ControlInputs SensorProcessor::process(const RawSensorFrame & raw, const AccelSample & accel, float dtSeconds) {
	if (calibrationCount_ < settings_.calibrationFrames) {
		for (int i = 0; i < kNumSensors; ++i) {
			calibrationSums_[i] += raw[i];
		}
		++calibrationCount_;
		if (calibrationCount_ == settings_.calibrationFrames) {
			for (int i = 0; i < kNumSensors; ++i) {
				zeros_[i] = static_cast<int>(calibrationSums_[i] / calibrationCount_);
			}
		}
		return finish({}, accel, dtSeconds);
	}

	std::array<float, kNumSensors> normalised {};
	for (int i = 0; i < kNumSensors; ++i) {
		normalised[i] = normalise(raw[i], zeros_[i], settings_.maxRaw);
	}
	return finish(normalised, accel, dtSeconds);
}

ControlInputs SensorProcessor::processNormalised(const std::array<float, kNumSensors> & normalised,
	const AccelSample & accel, float dtSeconds) {
	return finish(normalised, accel, dtSeconds);
}

ControlInputs SensorProcessor::finish(const std::array<float, kNumSensors> & normalised, const AccelSample & accel,
	float dtSeconds) {
	ControlInputs inputs;
	for (int i = 0; i < kNumSensors; ++i) {
		float value = smoothing_.process(sensorSource(i), std::clamp(normalised[i], 0.0f, 1.0f), dtSeconds);
		inputs.setSensor(i, shape(value, settings_.activeThreshold, settings_.curve));
	}
	inputs.set(ModSource::Total, totalPressure(inputs, settings_.accumulationDenominator));

	MotionOutput m = motion_.process(accel.x, accel.y, accel.z, accel.valid, dtSeconds);
	inputs.set(ModSource::TiltX, smoothing_.process(ModSource::TiltX, m.tiltX, dtSeconds));
	inputs.set(ModSource::TiltY, smoothing_.process(ModSource::TiltY, m.tiltY, dtSeconds));
	inputs.set(ModSource::Tilt, smoothing_.process(ModSource::Tilt, m.tilt, dtSeconds));
	inputs.set(ModSource::Shake, m.shake);
	return inputs;
}

} // namespace gm
