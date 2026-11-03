#include "MotionProcessor.h"

#include <algorithm>
#include <cmath>

namespace gm {

namespace {
constexpr float kRadToDeg = 57.29578f;
constexpr float kGravityTimeMs = 250.0f; // separates slow tilt from fast movement

float lowPassAlpha(float dtSeconds, float timeMs) {
	if (timeMs <= 0.0f) return 1.0f;
	return 1.0f - std::exp(-std::max(dtSeconds, 0.0f) * 1000.0f / timeMs);
}
} // namespace

// ------------------------------------------------------------------- Vec3

float Vec3::length() const {
	return std::sqrt(x * x + y * y + z * z);
}

Vec3 Vec3::normalized() const {
	float len = length();
	if (len < 1e-6f) return { 0.0f, 0.0f, 1.0f };
	return { x / len, y / len, z / len };
}

Vec3 axisFromId(const std::string & id) {
	float sign = (!id.empty() && id[0] == '-') ? -1.0f : 1.0f;
	char axis = id.empty() ? 'z' : id.back();
	if (axis == 'x') return { sign, 0.0f, 0.0f };
	if (axis == 'y') return { 0.0f, sign, 0.0f };
	return { 0.0f, 0.0f, sign };
}

// ----------------------------------------------------------------- helpers

MotionProcessor::Angles MotionProcessor::anglesFor(const Vec3 & gravity, const Vec3 & up) {
	Vec3 u = up.normalized();
	Vec3 g = gravity.normalized();
	// Build a basis around "up": side = the device X axis projected onto the
	// plane perpendicular to up (device Y if X is nearly parallel to up).
	Vec3 reference { 1.0f, 0.0f, 0.0f };
	if (std::fabs(u.dot(reference)) > 0.9f) reference = { 0.0f, 1.0f, 0.0f };
	Vec3 side = (reference - u * u.dot(reference)).normalized();
	Vec3 front = u.cross(side);

	Angles a;
	a.x = std::atan2(g.dot(side), g.dot(u)) * kRadToDeg;
	a.y = std::atan2(g.dot(front), g.dot(u)) * kRadToDeg;
	a.total = std::acos(std::clamp(g.dot(u), -1.0f, 1.0f)) * kRadToDeg;
	return a;
}

float MotionProcessor::signedToUnit(float degrees, float deadzone, float range) {
	float magnitude = std::max(std::fabs(degrees) - deadzone, 0.0f);
	float span = std::max(range - deadzone, 1e-3f);
	float amount = std::clamp(magnitude / span, 0.0f, 1.0f);
	return 0.5f + (degrees < 0.0f ? -0.5f : 0.5f) * amount;
}

float MotionProcessor::magnitudeToUnit(float degrees, float deadzone, float range) {
	float span = std::max(range - deadzone, 1e-3f);
	return std::clamp((degrees - deadzone) / span, 0.0f, 1.0f);
}

// --------------------------------------------------------------- processor

void MotionProcessor::configure(const MotionSettings & settings) {
	settings_ = settings;
	if (!settings.zeroAtStartup) {
		up_ = axisFromId(settings.upAxis);
		calibrationCount_ = settings.calibrationFrames; // nothing to measure
	}
}

void MotionProcessor::beginCalibration() {
	calibrationSum_ = {};
	calibrationCount_ = 0;
	gravityPrimed_ = false;
	shakeEnvelope_ = 0.0f;
	if (!settings_.zeroAtStartup) {
		up_ = axisFromId(settings_.upAxis);
		calibrationCount_ = settings_.calibrationFrames;
	}
}

bool MotionProcessor::isCalibrating() const {
	return settings_.zeroAtStartup && calibrationCount_ < settings_.calibrationFrames;
}

MotionOutput MotionProcessor::process(float ax, float ay, float az, bool valid, float dtSeconds) {
	MotionOutput out;
	Vec3 reading { ax, ay, az };

	// Calibration: average the readings to find "level". Frames without a
	// reading still count, so a missing accelerometer never blocks startup.
	if (isCalibrating()) {
		if (valid) {
			calibrationSum_ = calibrationSum_ + reading;
			gravity_ = reading;
			gravityPrimed_ = true;
		}
		++calibrationCount_;
		if (!isCalibrating() && calibrationSum_.length() > 0.1f) {
			up_ = calibrationSum_.normalized();
		}
		return out;
	}
	if (!valid || !settings_.enabled) {
		return out; // level, no shake
	}

	// Slow low-pass = gravity (orientation); the remainder = movement.
	if (!gravityPrimed_) {
		gravity_ = reading;
		gravityPrimed_ = true;
	}
	float alpha = lowPassAlpha(dtSeconds, kGravityTimeMs);
	gravity_ = gravity_ + (reading - gravity_) * alpha;

	Angles angles = anglesFor(gravity_, up_);
	out.tiltX = signedToUnit(angles.x, settings_.deadzoneDegrees, settings_.rangeDegrees);
	out.tiltY = signedToUnit(angles.y, settings_.deadzoneDegrees, settings_.rangeDegrees);
	out.tilt = magnitudeToUnit(angles.total, settings_.deadzoneDegrees, settings_.rangeDegrees);

	// Shake: instant attack, exponential release.
	float movement = (reading - gravity_).length();
	if (movement > shakeEnvelope_) {
		shakeEnvelope_ = movement;
	} else {
		shakeEnvelope_ += (movement - shakeEnvelope_) * lowPassAlpha(dtSeconds, settings_.shakeReleaseMs);
	}
	out.shake = std::clamp(shakeEnvelope_ / std::max(settings_.shakeSensitivity, 0.01f), 0.0f, 1.0f);
	return out;
}

} // namespace gm
