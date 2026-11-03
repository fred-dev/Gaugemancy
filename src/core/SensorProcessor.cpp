#include "SensorProcessor.h"

#include <algorithm>

namespace gm {

void SensorProcessor::beginCalibration() {
	calibrationSums_.fill(0);
	calibrationCount_ = 0;
	smoothed_.fill(0.0f);
}

void SensorProcessor::setZeroPoints(const RawSensorFrame & zeros) {
	zeros_ = zeros;
	calibrationCount_ = settings_.calibrationFrames; // calibration complete
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

float SensorProcessor::tiltToUnit(float g) {
	return std::clamp((g + 1.0f) * 0.5f, 0.0f, 1.0f);
}

ControlInputs SensorProcessor::process(const RawSensorFrame & raw, const AccelSample & accel) {
	if (isCalibrating()) {
		for (int i = 0; i < kNumSensors; ++i) {
			calibrationSums_[i] += raw[i];
		}
		++calibrationCount_;
		if (!isCalibrating()) {
			for (int i = 0; i < kNumSensors; ++i) {
				zeros_[i] = static_cast<int>(calibrationSums_[i] / calibrationCount_);
			}
		}
		return finish({}, accel);
	}

	std::array<float, kNumSensors> normalised {};
	for (int i = 0; i < kNumSensors; ++i) {
		normalised[i] = normalise(raw[i], zeros_[i], settings_.maxRaw);
	}
	return processNormalised(normalised, accel);
}

ControlInputs SensorProcessor::processNormalised(const std::array<float, kNumSensors> & normalised, const AccelSample & accel) {
	const float keep = settings_.smoothing;
	std::array<float, kNumSensors> shaped {};
	for (int i = 0; i < kNumSensors; ++i) {
		float value = std::clamp(normalised[i], 0.0f, 1.0f);
		smoothed_[i] = keep * smoothed_[i] + (1.0f - keep) * value;
		shaped[i] = shape(smoothed_[i], settings_.activeThreshold, settings_.curve);
	}
	return finish(shaped, accel);
}

ControlInputs SensorProcessor::finish(const std::array<float, kNumSensors> & shaped, const AccelSample & accel) {
	ControlInputs inputs;
	for (int i = 0; i < kNumSensors; ++i) {
		inputs.setSensor(i, shaped[i]);
	}
	inputs.set(ModSource::Total, totalPressure(inputs, settings_.accumulationDenominator));
	// With no accelerometer the tilt sources sit in the middle (level).
	inputs.set(ModSource::AccelX, accel.valid ? tiltToUnit(accel.x) : 0.5f);
	inputs.set(ModSource::AccelY, accel.valid ? tiltToUnit(accel.y) : 0.5f);
	inputs.set(ModSource::AccelZ, accel.valid ? tiltToUnit(accel.z) : 0.5f);
	return inputs;
}

} // namespace gm
