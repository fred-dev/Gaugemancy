#include "Diagnostics.h"

#include "../audio/AudioEngine.h"
#include "../core/SensorProcessor.h"
#include "../core/SensorSimulator.h"
#include "../core/SettingsStore.h"
#include "../hardware/ControlPanel.h"

#define OFXGPIO_NO_LEGACY
#include "ofxGPIO.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

namespace gm {

namespace {

// ------------------------------------------------------------------ reporting

struct Report {
	int failures = 0;
	int warnings = 0;

	void pass(const std::string & what) { std::printf("  PASS  %s\n", what.c_str()); }
	void warn(const std::string & what) {
		std::printf("  WARN  %s\n", what.c_str());
		++warnings;
	}
	void fail(const std::string & what) {
		std::printf("  FAIL  %s\n", what.c_str());
		++failures;
	}
	void section(const std::string & title) { std::printf("\n%s\n", title.c_str()); }

	int finish() {
		std::printf("\n%d failure(s), %d warning(s)\n", failures, warnings);
		return failures == 0 ? 0 : 1;
	}
};

LoadedProfile loadFor(const Options & options, const SettingsStore & store) {
	return options.profile.empty() ? store.loadActiveProfile() : store.loadProfile(options.profile);
}

void sleepMs(int ms) {
	std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

// ------------------------------------------------------------------ self test

void checkSettings(Report & r, const SettingsStore & store, const LoadedProfile & loaded) {
	r.section("Settings");
	std::printf("  data folder: %s\n", store.dataRoot().c_str());
	if (loaded.profileFileFound) {
		r.pass("profile '" + loaded.profile.name + "' loaded, " + std::to_string(loaded.presets.size()) + " preset(s)");
	} else {
		r.fail("profile '" + loaded.profile.name + "' not found");
	}
	for (const std::string & w : loaded.warnings) r.warn(w);
}

void checkSamples(Report & r, const SettingsStore & store, const LoadedProfile & loaded) {
	r.section("Audio files");
	const std::string & name = loaded.profile.name;
	for (size_t p = 0; p < loaded.presets.size(); ++p) {
		const PresetSettings & preset = loaded.presets[p];
		for (int s = 0; s < kMaxSlots; ++s) {
			const SlotSettings & slot = preset.slots[s];
			if (!slot.enabled || slot.file.empty()) continue;
			std::string error;
			auto buffer = GrainSlot::loadSampleFile(store.audioPath(name, slot.file), error);
			std::string label = "preset " + std::to_string(p + 1) + " slot " + std::to_string(s + 1) + ": " + slot.file;
			if (buffer) r.pass(label);
			else r.fail(label + " - " + error);
		}
	}
	const NarrationSettings & n = loaded.profile.narration;
	if (n.enabled) {
		std::string error;
		auto buffer = GrainSlot::loadSampleFile(store.audioPath(name, n.file), error);
		if (buffer) r.pass("narration: " + n.file);
		else r.fail("narration: " + n.file + " - " + error);
	}
}

void checkGpio(Report & r, const HardwareSettings & hw) {
	r.section("GPIO");
	if (!ofxgpio::hasHardwareSupport()) {
		r.warn("not a Linux build: GPIO, ADC and accelerometer checks skipped");
		return;
	}
	auto chips = ofxgpio::GpioChip::listChips();
	if (chips.empty()) {
		r.fail("no /dev/gpiochip* found");
		return;
	}
	std::string header = ofxgpio::GpioChip::findHeaderChip();
	for (const auto & chip : chips) {
		std::printf("        %s %s (%u lines)%s\n", chip.path.c_str(), chip.label.c_str(), chip.lines,
			chip.path == header ? "  <- header" : "");
	}
	ControlPanel panel;
	std::string report;
	if (panel.open(hw, report)) {
		r.pass("button, LEDs and relay pins requested");
	} else {
		r.fail("control panel: " + report);
	}
	std::printf("        button is %s\n", panel.buttonPressed() ? "PRESSED" : "released");
	std::printf("        blinking LEDs: blue, red, both...\n");
	const LedPattern steps[] = { { true, false, 0 }, { false, true, 0 }, { true, true, 0 }, LedPattern::off() };
	for (const LedPattern & step : steps) {
		panel.setLeds(step);
		panel.update(0);
		sleepMs(400);
	}
	panel.allOff();
}

void checkAdc(Report & r, const ProfileSettings & profile) {
	r.section("Pressure sensors (MCP3008)");
	if (!ofxgpio::hasHardwareSupport()) return;
	ofxgpio::Mcp3008 adc;
	if (!adc.open(profile.hardware.spiDevice)) {
		r.fail(profile.hardware.spiDevice + ": " + adc.lastError());
		return;
	}
	std::array<int, kNumSensors> lo, hi;
	lo.fill(1024);
	hi.fill(-1);
	for (int frame = 0; frame < 50; ++frame) {
		for (int i = 0; i < kNumSensors; ++i) {
			int v = adc.readChannel(profile.hardware.adcChannels[i]);
			lo[i] = std::min(lo[i], v);
			hi[i] = std::max(hi[i], v);
		}
		sleepMs(10);
	}
	for (int i = 0; i < kNumSensors; ++i) {
		std::string label = "sensor " + std::to_string(i + 1) + " (ch " + std::to_string(profile.hardware.adcChannels[i])
			+ "): " + std::to_string(lo[i]) + ".." + std::to_string(hi[i]);
		if (hi[i] < 0) r.fail(label + " read error");
		else if (hi[i] == 0) r.warn(label + " reads 0 - disconnected?");
		else if (lo[i] >= 1023) r.warn(label + " saturated");
		else if (lo[i] >= profile.sensors.maxRaw) r.warn(label + " resting value above sensors.maxRaw (" + std::to_string(profile.sensors.maxRaw) + ")");
		else if (hi[i] - lo[i] > 20) r.warn(label + " noisy at rest");
		else r.pass(label);
	}
}

void checkI2cBus(Report & r, const HardwareSettings & hw) {
	r.section("I2C bus scan (" + hw.i2cBus + ")");
	if (!ofxgpio::hasHardwareSupport()) return;
	std::vector<int> found;
	// 0x03..0x77: the address range i2cdetect scans; everything outside it
	// is reserved. Reading register 0 is a reasonable presence probe for
	// typical sensor chips, even when that register is meaningless to them.
	for (int addr = 0x03; addr <= 0x77; ++addr) {
		ofxgpio::I2cDevice dev;
		if (!dev.open(hw.i2cBus, static_cast<uint8_t>(addr))) continue;
		if (dev.readRegister(0x00) >= 0) found.push_back(addr);
	}
	if (found.empty()) {
		r.warn("nothing responded - check wiring/power before blaming the accelerometer");
		return;
	}
	std::string list;
	for (int addr : found) {
		char hex[8];
		std::snprintf(hex, sizeof(hex), "0x%02x", addr);
		if (!list.empty()) list += ", ";
		list += hex;
	}
	r.pass("responding: " + list + " (LIS3DH is usually 0x18 or 0x19)");
}

void checkAccelerometer(Report & r, const HardwareSettings & hw) {
	r.section("Accelerometer (LIS3DH)");
	if (!ofxgpio::hasHardwareSupport()) return;
	if (!hw.accelerometer) {
		r.pass("disabled in settings");
		return;
	}
	ofxgpio::Lis3dh accel;
	if (!accel.begin(hw.i2cBus, static_cast<uint8_t>(hw.accelAddress))) {
		r.warn("not found on " + hw.i2cBus + " (optional)");
		return;
	}
	sleepMs(30);
	ofxgpio::Lis3dh::Sample s;
	if (accel.read(s)) {
		char text[96];
		std::snprintf(text, sizeof(text), "x %+.2f g  y %+.2f g  z %+.2f g", s.x, s.y, s.z);
		r.pass(text);
	} else {
		r.fail("read failed: " + accel.lastError());
	}
}

void checkAudioDevices(Report & r, const AudioSettings & audio) {
	r.section("Audio output devices (api '" + audio.api + "')");
	auto devices = AudioEngine::describeOutputDevices(audio.api);
	if (devices.empty()) {
		r.fail("no output devices");
		return;
	}
	bool matched = audio.deviceName.empty();
	for (const std::string & d : devices) {
		std::printf("        %s\n", d.c_str());
		if (!audio.deviceName.empty() && d.find(audio.deviceName) != std::string::npos) matched = true;
	}
	if (matched) r.pass(audio.deviceName.empty() ? "using the system default device" : "configured device '" + audio.deviceName + "' present");
	else r.fail("configured device '" + audio.deviceName + "' not found");
}

} // namespace

int runSelfTest(const Options & options, const std::string & dataRoot) {
	Report r;
	std::printf("Gaugemancy self test\n");
	SettingsStore store(dataRoot);
	LoadedProfile loaded = loadFor(options, store);
	checkSettings(r, store, loaded);
	checkSamples(r, store, loaded);
	checkGpio(r, loaded.profile.hardware);
	checkAdc(r, loaded.profile);
	checkI2cBus(r, loaded.profile.hardware);
	checkAccelerometer(r, loaded.profile.hardware);
	checkAudioDevices(r, loaded.profile.audio);
	return r.finish();
}

int runListAudio(const Options & options, const std::string & dataRoot) {
	SettingsStore store(dataRoot);
	LoadedProfile loaded = loadFor(options, store);
	for (const char * api : { "default", "alsa", "pulse", "jack", "coreaudio", "wasapi", "ds", "asio" }) {
		auto devices = AudioEngine::describeOutputDevices(api);
		if (devices.empty()) continue;
		std::printf("%s:\n", api);
		for (const std::string & d : devices) std::printf("  %s\n", d.c_str());
	}
	std::printf("\nconfigured: api '%s', device '%s', id %d\n", loaded.profile.audio.api.c_str(),
		loaded.profile.audio.deviceName.c_str(), loaded.profile.audio.deviceId);
	return 0;
}

// =================================================================== benchmark

int runBenchmark(const Options & options, const std::string & dataRoot) {
	SettingsStore store(dataRoot);
	LoadedProfile loaded = loadFor(options, store);
	const ProfileSettings & profile = loaded.profile;
	const int sampleRate = profile.audio.sampleRate;
	const int bufferSize = profile.audio.bufferSize;
	const double blockSeconds = static_cast<double>(bufferSize) / sampleRate;
	const int blocks = std::max(1, static_cast<int>(options.benchmarkSeconds / blockSeconds));

	std::printf("Gaugemancy benchmark: profile '%s', %d Hz, buffer %d, %.1f s per preset\n",
		profile.name.c_str(), sampleRate, bufferSize, options.benchmarkSeconds);
	std::printf("CPU %% is render time / audio time for ONE core; the audio thread must stay well below 100%%.\n\n");

	AudioEngine audio;
	audio.startOffline(sampleRate, bufferSize);
	PathResolver resolve = [&](const std::string & f) { return store.audioPath(profile.name, f); };
	std::vector<float> out(static_cast<size_t>(bufferSize) * 2);
	int worstExit = 0;

	for (size_t p = 0; p < loaded.presets.size(); ++p) {
		const PresetSettings & preset = loaded.presets[p];
		for (const std::string & problem : audio.loadPreset(preset, resolve)) {
			std::printf("  warning: %s\n", problem.c_str());
		}

		// Squeeze every ball up and down continuously, all slots open.
		SensorSimulator sim;
		SensorSimulator::RampSettings ramp;
		ramp.speed = 0.8f;
		ramp.loop = true;
		ramp.sensors.fill(true);
		sim.setRamp(ramp);
		sim.startRamp();
		SensorProcessor processor;
		processor.configure(profile.sensors, profile.motion);
		processor.setZeroPoints({});
		std::array<SlotOverrides, kMaxSlots> overrides {};
		for (auto & o : overrides) {
			o.overrideGate = true;
			o.gateOpen = true;
		}

		double total = 0.0;
		double worst = 0.0;     // worst block after the first
		double firstBlock = 0.0; // the first block after loading pays one-off costs
		for (int b = 0; b < blocks; ++b) {
			ControlInputs inputs = processor.processNormalised(sim.update(static_cast<float>(blockSeconds)), sim.accel(),
				static_cast<float>(blockSeconds));
			auto t0 = std::chrono::steady_clock::now();
			audio.update(preset, profile.narration, inputs, overrides, 1.0f);
			audio.renderOffline(out.data(), bufferSize);
			double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
			total += seconds;
			if (b == 0) firstBlock = seconds;
			else worst = std::max(worst, seconds);
		}
		double cpu = 100.0 * total / (blocks * blockSeconds);
		double worstCpu = 100.0 * worst / blockSeconds;
		int activeSlots = 0;
		int effects = 0;
		for (const SlotSettings & s : preset.slots) {
			if (s.enabled && !s.file.empty()) {
				++activeSlots;
				effects += static_cast<int>(std::count_if(s.effects.begin(), s.effects.end(), [](const EffectSettings & e) { return e.enabled; }));
			}
		}
		const char * verdict = cpu < 50.0 && worstCpu < 80.0 ? "ok" : (cpu < 80.0 ? "HIGH" : "TOO HIGH");
		std::printf("preset %zu '%s': %d slot(s), %d slot effect(s), %zu master effect(s)\n", p + 1, preset.name.c_str(),
			activeSlots, effects, preset.masterEffects.size());
		std::printf("  average %.1f%%  worst block %.1f%%  first block after loading %.1f%%  -> %s\n", cpu, worstCpu,
			100.0 * firstBlock / blockSeconds, verdict);
		if (cpu >= 80.0) worstExit = 1;
	}
	return worstExit;
}

} // namespace gm
