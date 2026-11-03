#pragma once

// -----------------------------------------------------------------------------
// MotionProcessor: turns accelerometer readings into musical controls.
//
// An accelerometer at rest measures gravity (1 g pointing "up" out of the
// device). From that vector we get:
//
//   tiltX  tilt left/right    0.5 = level, 0 / 1 = tilted by `rangeDegrees`
//   tiltY  tilt forward/back  same
//   tilt   tilt in any direction, 0 = level .. 1 = `rangeDegrees` or more
//   shake  how much the device is being moved (gravity removed), 0..1
//
// "Level" means one of two references (MotionSettings::zeroAtStartup):
//   * startup  - the orientation measured when the instrument starts (or is
//                recalibrated) counts as level, so it works however the
//                accelerometer board was mounted, and on any table.
//   * absolute - a fixed device axis (`upAxis`, e.g. "+z") pointing straight
//                up counts as level.
// -----------------------------------------------------------------------------

#include "ControlInputs.h"
#include "Settings.h"

#include <string>

namespace gm {

struct Vec3 {
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;

	float dot(const Vec3 & o) const { return x * o.x + y * o.y + z * o.z; }
	Vec3 cross(const Vec3 & o) const { return { y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x }; }
	Vec3 operator-(const Vec3 & o) const { return { x - o.x, y - o.y, z - o.z }; }
	Vec3 operator+(const Vec3 & o) const { return { x + o.x, y + o.y, z + o.z }; }
	Vec3 operator*(float s) const { return { x * s, y * s, z * s }; }
	float length() const;
	Vec3 normalized() const;
};

/// Device axis for an id like "+z" or "-x" (unknown ids give +z).
Vec3 axisFromId(const std::string & id);

struct MotionOutput {
	float tiltX = 0.5f;
	float tiltY = 0.5f;
	float tilt = 0.0f;
	float shake = 0.0f;
};

class MotionProcessor {
public:
	void configure(const MotionSettings & settings);

	/// Re-measure "level" (startup reference mode) over the next frames.
	void beginCalibration();
	bool isCalibrating() const;

	/// Process one reading; dt is the time since the previous one. Readings
	/// marked invalid (no accelerometer) give a level, still output.
	MotionOutput process(float ax, float ay, float az, bool valid, float dtSeconds);

	/// The reference "up" currently in use (unit vector, device coordinates).
	const Vec3 & referenceUp() const { return up_; }

	// ---- pure helpers (exposed for tests) -----------------------------------

	/// Signed tilt angles (degrees) of `gravity` around the reference basis.
	struct Angles {
		float x = 0.0f;     ///< left/right
		float y = 0.0f;     ///< forward/back
		float total = 0.0f; ///< from vertical, any direction (>= 0)
	};
	static Angles anglesFor(const Vec3 & gravity, const Vec3 & up);

	/// Map a signed angle to 0..1 around 0.5 with a dead zone.
	static float signedToUnit(float degrees, float deadzone, float range);
	/// Map an unsigned angle to 0..1 with a dead zone.
	static float magnitudeToUnit(float degrees, float deadzone, float range);

private:
	MotionSettings settings_;
	Vec3 up_ { 0.0f, 0.0f, 1.0f };
	Vec3 gravity_ { 0.0f, 0.0f, 1.0f };   ///< slow low-pass of the reading
	bool gravityPrimed_ = false;
	Vec3 calibrationSum_;
	int calibrationCount_ = 0;
	float shakeEnvelope_ = 0.0f;
};

} // namespace gm
