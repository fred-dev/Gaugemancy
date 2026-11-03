#pragma once

// -----------------------------------------------------------------------------
// ControlInputs: every value that can drive a sound parameter, normalised to
// 0..1, for one control frame.
//
// A "modulation source" is simply an index into this array. Sources are:
//   sensor1..sensor6  pressure on each ball (after calibration, threshold, curve)
//   total             combined pressure of all balls ("accumulated pressure")
//   tiltX, tiltY      tilt left/right and forward/back (0.5 = level, 0 / 1 =
//                     tilted by the configured range either way)
//   tilt              how far the instrument is tilted in any direction (0..1)
//   shake             how hard it is being shaken or moved (0..1)
// Motion sources come from the accelerometer (see MotionProcessor).
//
// This file has no openFrameworks dependency so it can be unit tested anywhere.
// -----------------------------------------------------------------------------

#include <array>
#include <string>
#include <vector>

namespace gm {

/// Number of pressure sensors (balls) on the instrument.
constexpr int kNumSensors = 6;

/// Number of grain slots (one sample + grain cloud + effect chain each).
constexpr int kMaxSlots = 6;

/// Everything a parameter can be connected to.
enum class ModSource {
	None = 0,
	Sensor1,
	Sensor2,
	Sensor3,
	Sensor4,
	Sensor5,
	Sensor6,
	Total,
	TiltX,
	TiltY,
	Tilt,
	Shake,
	Count
};

constexpr int kNumModSources = static_cast<int>(ModSource::Count);

/// Stable text id used in JSON ("none", "sensor1", ... "shake").
std::string modSourceId(ModSource source);

/// Label for the GUI ("None", "Sensor 1", ... "Shake").
std::string modSourceLabel(ModSource source);

/// Parse a JSON id. Unknown text returns ModSource::None. The early ids
/// "accelX", "accelY" and "accelZ" are read as tiltX, tiltY and tilt.
ModSource modSourceFromId(const std::string & id);
ModSource modSourceFromLegacyIndex(int index);

/// ModSource for sensor index 0..5.
ModSource sensorSource(int sensorIndex);

/// The values for one control frame.
struct ControlInputs {
	std::array<float, kNumModSources> values {};

	float get(ModSource source) const { return values[static_cast<int>(source)]; }
	void set(ModSource source, float value) { values[static_cast<int>(source)] = value; }

	float sensor(int index) const { return values[static_cast<int>(ModSource::Sensor1) + index]; }
	void setSensor(int index, float value) { values[static_cast<int>(ModSource::Sensor1) + index] = value; }

	/// True when any of the six pressure sensors is above zero.
	bool anySensorActive() const;
};

} // namespace gm
