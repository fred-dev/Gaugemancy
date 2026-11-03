#pragma once

// -----------------------------------------------------------------------------
// SensorSource: where sensor readings come from. The rest of the app does not
// care which one is active.
//
//   HardwareSensorSource  MCP3008 ADC (6 balls) + LIS3DH accelerometer (Pi)
//   ReplaySensorSource    a CSV recording (see core/SensorRecording.h)
//   SimulatedSensorSource SensorSimulator (GUI sliders, ramps, OSC)
//
// A frame is either raw ADC values (they then go through calibration) or
// already-normalised values (simulation skips calibration).
// -----------------------------------------------------------------------------

#include "../core/SensorProcessor.h"
#include "../core/SensorRecording.h"
#include "../core/SensorSimulator.h"
#include "../core/Settings.h"

#include <cstdint>
#include <memory>
#include <string>

namespace gm {

struct SensorFrame {
	bool isRaw = true;                          ///< true: use `raw`, false: use `normalised`
	RawSensorFrame raw {};
	std::array<float, kNumSensors> normalised {};
	AccelSample accel;
};

class SensorSource {
public:
	virtual ~SensorSource() = default;

	/// Prepare the source. Returns false with a reason if it cannot work.
	virtual bool open(std::string & error) = 0;

	/// Read the current frame. Returns false if nothing could be read.
	virtual bool read(uint64_t nowMs, float dtSeconds, SensorFrame & frame) = 0;

	/// Short name for logs and the GUI ("hardware", "replay", "simulation").
	virtual std::string name() const = 0;

	/// Non-fatal problem found by open() (e.g. accelerometer missing), or "".
	virtual std::string warning() const { return ""; }
};

/// The Raspberry Pi's ADC and accelerometer (stubs elsewhere: open() fails).
std::unique_ptr<SensorSource> makeHardwareSensorSource(const HardwareSettings & settings);

/// Plays back a CSV recording.
std::unique_ptr<SensorSource> makeReplaySensorSource(const std::string & path, bool loop);

/// Simulation driven by `simulator` (owned by the caller, must outlive it).
std::unique_ptr<SensorSource> makeSimulatedSensorSource(SensorSimulator & simulator);

} // namespace gm
