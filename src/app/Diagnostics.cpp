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
#include <cmath>
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

namespace {

/// Which slot gates the benchmark holds open.
enum class GateCase { AllOpen, AllClosed, OneOpen };

struct BlockStats {
	double averageCpu = 0.0; ///< percent of one core
	double worstCpu = 0.0;   ///< worst block after the first
	double firstCpu = 0.0;   ///< the first block pays one-off costs
};

std::array<SlotOverrides, kMaxSlots> overridesFor(GateCase gates) {
	std::array<SlotOverrides, kMaxSlots> overrides {};
	for (int i = 0; i < kMaxSlots; ++i) {
		overrides[i].overrideGate = true;
		overrides[i].gateOpen = gates == GateCase::AllOpen || (gates == GateCase::OneOpen && i == 0);
	}
	return overrides;
}

/// Render `blocks` buffers while every ball is squeezed up and down.
BlockStats measure(AudioEngine & audio, const ProfileSettings & profile, const PresetSettings & preset, int blocks,
	GateCase gates) {
	const int bufferSize = profile.audio.bufferSize;
	const double blockSeconds = static_cast<double>(bufferSize) / profile.audio.sampleRate;
	std::vector<float> out(static_cast<size_t>(bufferSize) * 2);

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
	const std::array<SlotOverrides, kMaxSlots> overrides = overridesFor(gates);

	double total = 0.0;
	double worst = 0.0;
	double first = 0.0;
	for (int b = 0; b < blocks; ++b) {
		ControlInputs inputs = processor.processNormalised(sim.update(static_cast<float>(blockSeconds)), sim.accel(),
			static_cast<float>(blockSeconds));
		auto t0 = std::chrono::steady_clock::now();
		audio.update(preset, profile.narration, inputs, overrides, 1.0f, static_cast<float>(blockSeconds));
		audio.renderOffline(out.data(), bufferSize);
		double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
		total += seconds;
		if (b == 0) first = seconds;
		else worst = std::max(worst, seconds);
	}
	BlockStats stats;
	stats.averageCpu = 100.0 * total / (blocks * blockSeconds);
	stats.worstCpu = 100.0 * worst / blockSeconds;
	stats.firstCpu = 100.0 * first / blockSeconds;
	return stats;
}

// ------------------------------------------------------------ preset change

/// What a stretch of rendered audio looked like.
struct RenderStats {
	double averageCpu = 0.0;      ///< audio rendering only
	double worstCpu = 0.0;        ///< worst audio block
	double worstControlMs = 0.0;  ///< longest control frame (the old graph is freed in one)
	float quietestWindowRms = 1.0e9f; ///< quietest ~100 ms window
	float averageRms = 0.0f;
	float largestStep = 0.0f;         ///< biggest sample-to-sample jump (a click is a big one)
};

/// Renders with every ball held at a steady squeeze and all gates open, so
/// any silence or jump in the output comes from the engine, not the input.
class SteadyRenderer {
public:
	SteadyRenderer(AudioEngine & audio, const ProfileSettings & profile)
		: audio_(audio), profile_(profile), out_(static_cast<size_t>(profile.audio.bufferSize) * 2) {
		for (int i = 0; i < kNumSensors; ++i) inputs_.setSensor(i, 0.8f);
		inputs_.set(ModSource::Total, 0.8f);
		inputs_.set(ModSource::TiltX, 0.5f);
		inputs_.set(ModSource::TiltY, 0.5f);
		overrides_ = overridesFor(GateCase::AllOpen);
	}

	RenderStats render(const PresetSettings & preset, double seconds) {
		const int bufferSize = profile_.audio.bufferSize;
		const double blockSeconds = static_cast<double>(bufferSize) / profile_.audio.sampleRate;
		const int blocks = std::max(1, static_cast<int>(seconds / blockSeconds));
		const int blocksPerWindow = std::max(1, static_cast<int>(0.1 / blockSeconds));
		RenderStats stats;
		double total = 0.0;
		double worst = 0.0;
		double squares = 0.0;
		double windowSquares = 0.0;
		for (int b = 0; b < blocks; ++b) {
			// Timed apart: update() runs on the control thread, rendering on
			// the audio thread, and only the second one can cause a dropout.
			auto t0 = std::chrono::steady_clock::now();
			audio_.update(preset, profile_.narration, inputs_, overrides_, 1.0f, static_cast<float>(blockSeconds));
			auto t1 = std::chrono::steady_clock::now();
			audio_.renderOffline(out_.data(), bufferSize);
			double spent = std::chrono::duration<double>(std::chrono::steady_clock::now() - t1).count();
			stats.worstControlMs = std::max(stats.worstControlMs, 1000.0 * std::chrono::duration<double>(t1 - t0).count());
			total += spent;
			worst = std::max(worst, spent);
			for (int n = 0; n < bufferSize; ++n) {
				float left = out_[static_cast<size_t>(n) * 2];
				stats.largestStep = std::max(stats.largestStep, std::fabs(left - lastSample_));
				lastSample_ = left;
				windowSquares += static_cast<double>(left) * left;
			}
			if ((b + 1) % blocksPerWindow == 0) {
				float rms = static_cast<float>(std::sqrt(windowSquares / (static_cast<double>(blocksPerWindow) * bufferSize)));
				stats.quietestWindowRms = std::min(stats.quietestWindowRms, rms);
				squares += windowSquares;
				windowSquares = 0.0;
			}
		}
		int fullWindows = blocks / blocksPerWindow;
		if (fullWindows > 0) {
			stats.averageRms = static_cast<float>(std::sqrt(squares / (static_cast<double>(fullWindows) * blocksPerWindow * bufferSize)));
		}
		stats.averageCpu = 100.0 * total / (blocks * blockSeconds);
		stats.worstCpu = 100.0 * worst / blockSeconds;
		return stats;
	}

private:
	AudioEngine & audio_;
	const ProfileSettings & profile_;
	std::vector<float> out_;
	ControlInputs inputs_;
	std::array<SlotOverrides, kMaxSlots> overrides_ {};
	float lastSample_ = 0.0f;
};

/// Crossfade from one preset to another and report cost, gaps and clicks.
void benchmarkPresetChange(AudioEngine & audio, const ProfileSettings & profile, const LoadedProfile & loaded,
	size_t from, size_t to, const PathResolver & resolve) {
	const float fadeMs = static_cast<float>(profile.presetFadeMs);
	SteadyRenderer renderer(audio, profile);
	audio.loadPreset(loaded.presets[from], resolve);
	RenderStats before = renderer.render(loaded.presets[from], 1.0);

	auto t0 = std::chrono::steady_clock::now();
	audio.loadPreset(loaded.presets[to], resolve, fadeMs);
	double buildMs = 1000.0 * std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
	RenderStats during = renderer.render(loaded.presets[to], fadeMs / 1000.0 + 0.2);
	bool stillFading = audio.isFading();
	RenderStats after = renderer.render(loaded.presets[to], 1.0);

	// A gap: some 100 ms of the fade far quieter than both presets on their
	// own. A click: a jump clearly bigger than either preset makes by itself.
	float quietPreset = std::min(before.quietestWindowRms, after.quietestWindowRms);
	bool gap = during.quietestWindowRms < 0.25f * quietPreset;
	bool click = during.largestStep > 1.5f * std::max(before.largestStep, after.largestStep) + 0.02f;
	std::printf("preset %zu -> %zu: build %.0f ms (control loop waits, sound continues), fade %.0f ms\n", from + 1, to + 1,
		buildMs, fadeMs);
	std::printf("  audio during the fade: average %.1f%%  worst block %.1f%%   (before %.1f%%, after %.1f%%)\n",
		during.averageCpu, during.worstCpu, before.averageCpu, after.averageCpu);
	std::printf("  longest control frame during the fade: %.1f ms (freeing the old preset)\n", during.worstControlMs);
	std::printf("  quietest 100 ms: before %.4f  fade %.4f  after %.4f -> %s\n", before.quietestWindowRms,
		during.quietestWindowRms, after.quietestWindowRms, gap ? "SILENT GAP" : "no gap");
	std::printf("  largest step:    before %.4f  fade %.4f  after %.4f -> %s\n", before.largestStep, during.largestStep,
		after.largestStep, click ? "CLICK" : "no click");
	if (stillFading) std::printf("  WARNING: the old preset was still connected 0.2 s after the fade\n");
}

} // namespace

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
	int worstExit = 0;

	for (size_t p = 0; p < loaded.presets.size(); ++p) {
		const PresetSettings & preset = loaded.presets[p];
		for (const std::string & problem : audio.loadPreset(preset, resolve)) {
			std::printf("  warning: %s\n", problem.c_str());
		}

		// Main case: every slot open. Then the idle costs: no slot open, and
		// one slot open (what a performer squeezing one ball pays).
		BlockStats open = measure(audio, profile, preset, blocks, GateCase::AllOpen);
		BlockStats closed = measure(audio, profile, preset, blocks, GateCase::AllClosed);
		BlockStats one = measure(audio, profile, preset, blocks, GateCase::OneOpen);

		int activeSlots = 0;
		int effects = 0;
		for (const SlotSettings & s : preset.slots) {
			if (s.enabled && (s.sourceType != "granular" || !s.file.empty())) {
				++activeSlots;
				effects += static_cast<int>(std::count_if(s.effects.begin(), s.effects.end(), [](const EffectSettings & e) { return e.enabled; }));
			}
		}
		const char * verdict = open.averageCpu < 50.0 && open.worstCpu < 80.0 ? "ok" : (open.averageCpu < 80.0 ? "HIGH" : "TOO HIGH");
		std::printf("preset %zu '%s': %d slot(s), %d slot effect(s), %zu master effect(s)\n", p + 1, preset.name.c_str(),
			activeSlots, effects, preset.masterEffects.size());
		std::printf("  average %.1f%%  worst block %.1f%%  first block after loading %.1f%%  -> %s\n", open.averageCpu,
			open.worstCpu, open.firstCpu, verdict);
		std::printf("  all gates closed: average %.2f%%  worst %.1f%%   one slot open: average %.2f%%  worst %.1f%%\n",
			closed.averageCpu, closed.worstCpu, one.averageCpu, one.worstCpu);
		if (open.averageCpu >= 80.0) worstExit = 1;
	}

	if (loaded.presets.size() > 1) {
		std::printf("\nPreset changes (every ball held at a steady squeeze, both presets play during the fade)\n");
		for (size_t p = 0; p < loaded.presets.size(); ++p) {
			benchmarkPresetChange(audio, profile, loaded, p, (p + 1) % loaded.presets.size(), resolve);
		}
	}
	return worstExit;
}

} // namespace gm
