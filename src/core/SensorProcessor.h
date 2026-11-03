#pragma once

// -----------------------------------------------------------------------------
// SensorProcessor: turns raw readings into ControlInputs.
//
// For each of the six pressure sensors, every control frame:
//   1. calibration  - the first N frames are averaged to find each ball's
//                     resting pressure (its "zero"); balls differ a lot.
//   2. normalise    - raw is mapped from [zero .. maxRaw] to [0 .. 1].
//   3. smoothing    - optional low-pass against ADC noise (SmoothingBank).
//   4. threshold    - values below activeThreshold become exactly 0, so noise
//                     cannot make sounds.
//   5. curve        - the response curve (linear, sqrt, quartic, exponential).
//
// Then the combined and motion sources are derived:
//   total            sum of active sensors / accumulationDenominator (0..1)
//   tiltX/tiltY/tilt from the accelerometer via MotionProcessor, smoothed
//   shake            movement of the instrument via MotionProcessor
// -----------------------------------------------------------------------------

#include "ControlInputs.h"
#include "MotionProcessor.h"
#include "Settings.h"
#include "Smoother.h"

#include <array>

namespace gm {

using RawSensorFrame = std::array<int, kNumSensors>;

/// One accelerometer reading in g (device coordinates).
struct AccelSample {
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	bool valid = false;
};

class SensorProcessor {
public:
	void configure(const SensorSettings & sensors, const MotionSettings & motion);
	const SensorSettings & settings() const { return settings_; }

	/// Start (or restart) calibration of the balls and of the motion
	/// reference: the next frames are averaged to find "at rest" and "level".
	void beginCalibration();

	/// True while the balls' resting values are still being measured.
	bool isCalibrating() const;

	/// True while the motion reference ("level") is still being measured.
	bool isCalibratingMotion() const { return motion_.isCalibrating(); }

	/// Set zero points directly (used by tests and when calibration is skipped).
	void setZeroPoints(const RawSensorFrame & zeros);
	const RawSensorFrame & zeroPoints() const { return zeros_; }

	/// Process one frame of raw ADC values. While calibrating, outputs rest.
	ControlInputs process(const RawSensorFrame & raw, const AccelSample & accel, float dtSeconds);

	/// Process values that are already normalised (simulation, GUI): skips
	/// ball calibration and normalisation but applies smoothing, threshold
	/// and curve.
	ControlInputs processNormalised(const std::array<float, kNumSensors> & normalised, const AccelSample & accel,
		float dtSeconds);

	/// The motion processor (e.g. to show its reference in the GUI).
	const MotionProcessor & motion() const { return motion_; }

	/// Pure helpers, exposed for unit tests.
	static float normalise(int raw, int zero, int maxRaw);
	static float shape(float normalised, float threshold, Curve curve);
	static float totalPressure(const ControlInputs & inputs, float denominator);

private:
	ControlInputs finish(const std::array<float, kNumSensors> & normalised, const AccelSample & accel, float dtSeconds);

	SensorSettings settings_;
	RawSensorFrame zeros_ {};
	std::array<long, kNumSensors> calibrationSums_ {};
	int calibrationCount_ = 0;
	SmoothingBank smoothing_;
	MotionProcessor motion_;
};

} // namespace gm
