#include "SensorSource.h"

#define OFXGPIO_NO_LEGACY
#include "ofxGPIO.h"

namespace gm {

namespace {

// ------------------------------------------------------------------ hardware

class HardwareSensorSource : public SensorSource {
public:
	explicit HardwareSensorSource(const HardwareSettings & settings) : settings_(settings) {}

	bool open(std::string & error) override {
		if (!adc_.open(settings_.spiDevice)) {
			error = "ADC: " + adc_.lastError();
			return false;
		}
		if (settings_.accelerometer && !accel_.begin(settings_.i2cBus, static_cast<uint8_t>(settings_.accelAddress))) {
			// Not fatal: the instrument works without tilt.
			accelWarning_ = "accelerometer not found on " + settings_.i2cBus;
		}
		return true;
	}

	bool read(uint64_t nowMs, float, SensorFrame & frame) override {
		frame.isRaw = true;
		bool ok = true;
		for (int i = 0; i < kNumSensors; ++i) {
			int value = adc_.readChannel(settings_.adcChannels[i]);
			if (value < 0) {
				ok = false;
				value = 0;
			}
			frame.raw[i] = value;
		}
		// The accelerometer samples at 100 Hz; reading it every frame would only
		// cost I2C traffic, so it is polled every 20 ms.
		if (accel_.isOpen() && nowMs - lastAccelMs_ >= 20) {
			ofxgpio::Lis3dh::Sample s;
			if (accel_.read(s)) {
				lastAccel_ = { s.x, s.y, s.z, true };
			}
			lastAccelMs_ = nowMs;
		}
		frame.accel = lastAccel_;
		return ok;
	}

	std::string name() const override { return "hardware"; }
	std::string warning() const override { return accelWarning_; }

private:
	HardwareSettings settings_;
	ofxgpio::Mcp3008 adc_;
	ofxgpio::Lis3dh accel_;
	AccelSample lastAccel_;
	uint64_t lastAccelMs_ = 0;
	std::string accelWarning_;
};

// -------------------------------------------------------------------- replay

class ReplaySensorSource : public SensorSource {
public:
	ReplaySensorSource(std::string path, bool loop) : path_(std::move(path)), loop_(loop) {}

	bool open(std::string & error) override {
		started_ = false;
		return playback_.load(path_, error);
	}

	bool read(uint64_t nowMs, float, SensorFrame & frame) override {
		if (!started_) {
			startMs_ = nowMs;
			started_ = true;
		}
		const RecordedFrame & recorded = playback_.frameAt(nowMs - startMs_, loop_);
		frame.isRaw = true;
		frame.raw = recorded.raw;
		frame.accel = recorded.accel;
		return true;
	}

	std::string name() const override { return "replay"; }

private:
	std::string path_;
	bool loop_;
	SensorPlayback playback_;
	bool started_ = false;
	uint64_t startMs_ = 0;
};

// ---------------------------------------------------------------- simulation

class SimulatedSensorSource : public SensorSource {
public:
	explicit SimulatedSensorSource(SensorSimulator & simulator) : simulator_(simulator) {}

	bool open(std::string &) override { return true; }

	bool read(uint64_t, float dtSeconds, SensorFrame & frame) override {
		frame.isRaw = false;
		frame.normalised = simulator_.update(dtSeconds);
		frame.accel = simulator_.accel();
		return true;
	}

	std::string name() const override { return "simulation"; }

private:
	SensorSimulator & simulator_;
};

} // namespace

std::unique_ptr<SensorSource> makeHardwareSensorSource(const HardwareSettings & settings) {
	return std::make_unique<HardwareSensorSource>(settings);
}

std::unique_ptr<SensorSource> makeReplaySensorSource(const std::string & path, bool loop) {
	return std::make_unique<ReplaySensorSource>(path, loop);
}

std::unique_ptr<SensorSource> makeSimulatedSensorSource(SensorSimulator & simulator) {
	return std::make_unique<SimulatedSensorSource>(simulator);
}

} // namespace gm
