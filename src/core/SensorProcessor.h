#pragma once

// -----------------------------------------------------------------------------
// SensorProcessor: turns raw ADC readings into ControlInputs.
//
// For each of the six pressure sensors, every control frame:
//   1. calibration  - the first N frames are averaged to find each ball's
//                     resting pressure (its "zero"); balls differ a lot.
//   2. normalise    - raw is mapped from [zero .. maxRaw] to [0 .. 1].
//   3. smoothing    - optional one-pole low-pass against ADC noise.
//   4. threshold    - values below activeThreshold become exactly 0, so noise
//                     cannot make sounds.
//   5. curve        - the response curve (linear, sqrt, quartic, exponential).
//
// Then the combined sources are derived:
//   total  - sum of active sensors / accumulationDenominator (clamped 0..1)
//   accel  - tilt in g mapped from [-1 g .. +1 g] to [0 .. 1]
// -----------------------------------------------------------------------------

#include "ControlInputs.h"
#include "Settings.h"

#include <array>

namespace gm {

using RawSensorFrame = std::array<int, kNumSensors>;

struct AccelSample {
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	bool valid = false;
};

class SensorProcessor {
public:
	void configure(const SensorSettings & settings) { settings_ = settings; }
	const SensorSettings & settings() const { return settings_; }

	/// Start (or restart) calibration; the next calibrationFrames raw frames are
	/// averaged to find each sensor's zero point.
	void beginCalibration();

	/// True while calibration frames are still being collected.
	bool isCalibrating() const { return calibrationCount_ < settings_.calibrationFrames; }

	/// Set zero points directly (used by tests and when calibration is skipped).
	void setZeroPoints(const RawSensorFrame & zeros);
	const RawSensorFrame & zeroPoints() const { return zeros_; }

	/// Process one frame. While calibrating, outputs zeros.
	ControlInputs process(const RawSensorFrame & raw, const AccelSample & accel);

	/// Process values that are already normalised (simulation, replay, GUI):
	/// skips calibration and normalisation but applies threshold and curve.
	ControlInputs processNormalised(const std::array<float, kNumSensors> & normalised, const AccelSample & accel);

	/// Pure helpers, exposed for unit tests.
	static float normalise(int raw, int zero, int maxRaw);
	static float shape(float normalised, float threshold, Curve curve);
	static float totalPressure(const ControlInputs & inputs, float denominator);
	static float tiltToUnit(float g);

private:
	ControlInputs finish(const std::array<float, kNumSensors> & shaped, const AccelSample & accel);

	SensorSettings settings_;
	RawSensorFrame zeros_ {};
	std::array<long, kNumSensors> calibrationSums_ {};
	int calibrationCount_ = 0;
	std::array<float, kNumSensors> smoothed_ {};
};

} // namespace gm
