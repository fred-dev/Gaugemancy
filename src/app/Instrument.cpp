#include "Instrument.h"

#include "ofLog.h"
#include "ofUtils.h"

#include <algorithm>

namespace gm {

namespace {
constexpr const char * kLog = "gaugemancy";
constexpr size_t kMaxMessages = 12;
constexpr uint64_t kButtonIgnoreAfterStartMs = 2000; // ignore boot-time glitches
} // namespace

Instrument::Instrument() : modes_(*this) {}

Instrument::~Instrument() {
	shutdown();
}

// ======================================================================= setup

void Instrument::note(const std::string & message, bool warning) {
	if (warning) {
		ofLogWarning(kLog) << message;
	} else {
		ofLogNotice(kLog) << message;
	}
	messages_.push_back(message);
	while (messages_.size() > kMaxMessages) {
		messages_.pop_front();
	}
}

void Instrument::applyLogLevel(const std::string & level) {
	if (level == "verbose") ofSetLogLevel(OF_LOG_VERBOSE);
	else if (level == "warning") ofSetLogLevel(OF_LOG_WARNING);
	else if (level == "error") ofSetLogLevel(OF_LOG_ERROR);
	else if (level == "silent") ofSetLogLevel(OF_LOG_SILENT);
	else ofSetLogLevel(OF_LOG_NOTICE);
}

bool Instrument::setup(const Options & options, const std::string & dataRoot) {
	options_ = options;
	store_.setDataRoot(dataRoot);

	// 1. Settings.
	if (options.profile.empty()) {
		loaded_ = store_.loadActiveProfile();
	} else {
		loaded_ = store_.loadProfile(options.profile);
	}
	applyLogLevel(options.logLevel.empty() ? loaded_.profile.logLevel : options.logLevel);
	note("data folder " + dataRoot + ", profile '" + loaded_.profile.name + "', "
		+ std::to_string(loaded_.presets.size()) + " preset(s)");
	for (const std::string & warning : loaded_.warnings) {
		note(warning, true);
	}
	if (loaded_.presets.empty()) {
		loaded_.presets.emplace_back();
	}

	// 2. Hardware panel (button, LEDs, relay) - silently absent on a laptop.
	std::string panelReport;
	if (panel_.open(loaded_.profile.hardware, panelReport)) {
		note("control panel ready");
	} else if (hardwareExpected()) {
		note("control panel: " + panelReport, true);
	}
	button_.setClickTimeout(static_cast<uint64_t>(loaded_.profile.hardware.clickTimeoutMs));

	// 3. Sensors.
	processor_.configure(loaded_.profile.sensors, loaded_.profile.motion);
	gesture_.configure(loaded_.profile.gestures);
	openSensors(options.sensors, options.replayFile);
	if (!options.recordFile.empty()) {
		startRecording(options.recordFile);
	}

	// 4. Audio.
	if (!options.noAudio) {
		std::string report;
		audioOpen_ = audio_.startDevice(resolveAudioSettings(loaded_.profile.audio), report);
		note(report, !audioOpen_);
	}
	for (const std::string & problem : audio_.loadNarration(loaded_.profile.narration, audioResolver())) {
		note(problem, true);
	}
	loadCurrentPreset();

	// 5. Remote control.
	std::string oscReport = remote_.setup(loaded_.profile.osc);
	if (!oscReport.empty()) note(oscReport);

	// 6. Start in the right mode.
	startMs_ = lastUpdateMs_ = nowMs_ = ofGetElapsedTimeMillis();
	modes_.start(loaded_.profile.narration, audio_.narration().isLoaded(), nowMs_);

	// 7. Speaker on - last, once everything above is actually ready.
	panel_.pulseRelayBlocking(loaded_.profile.hardware.relayPulseMs);
	return true;
}

bool Instrument::hardwareExpected() {
	// Only complain about missing GPIO where GPIO is expected to exist.
#if defined(__linux__) && (defined(__arm__) || defined(__aarch64__))
	return true;
#else
	return false;
#endif
}

AudioSettings Instrument::resolveAudioSettings(const AudioSettings & settings) {
	if (!hardwareExpected() || !settings.pi.enabled) {
		return settings;
	}
	AudioSettings resolved = settings;
	resolved.api = settings.pi.api;
	resolved.deviceName = settings.pi.deviceName;
	resolved.deviceId = settings.pi.deviceId;
	return resolved;
}

void Instrument::openSensors(SensorMode mode, const std::string & replayFile) {
	std::string error;
	sensors_.reset();
	if (mode == SensorMode::Replay) {
		sensors_ = makeReplaySensorSource(replayFile, options_.loopReplay);
	} else if (mode == SensorMode::Hardware || mode == SensorMode::Auto) {
		sensors_ = makeHardwareSensorSource(loaded_.profile.hardware);
	} else {
		sensors_ = makeSimulatedSensorSource(simulator_);
	}

	if (!sensors_->open(error)) {
		bool quiet = (mode == SensorMode::Auto && !hardwareExpected());
		if (!quiet) note("sensors (" + sensors_->name() + "): " + error, true);
		if (mode == SensorMode::Auto || mode == SensorMode::Hardware) {
			sensors_ = makeSimulatedSensorSource(simulator_);
			sensors_->open(error);
		}
	}
	if (!sensors_->warning().empty()) {
		note(sensors_->warning(), true);
	}
	processor_.beginCalibration();
	note("sensor source: " + sensors_->name());
}

bool Instrument::useSensorMode(SensorMode mode, const std::string & replayFile) {
	openSensors(mode, replayFile);
	return sensors_ != nullptr;
}

std::string Instrument::sensorSourceName() const {
	return sensors_ ? sensors_->name() : "none";
}

PathResolver Instrument::audioResolver() const {
	std::string profileName = loaded_.profile.name;
	const SettingsStore & store = store_;
	return [&store, profileName](const std::string & file) { return store.audioPath(profileName, file); };
}

// ===================================================================== presets

void Instrument::loadCurrentPreset() {
	const PresetSettings & p = preset();
	note("preset " + std::to_string(presetIndex_ + 1) + " '" + p.name + "'");
	for (const std::string & problem : audio_.loadPreset(p, audioResolver())) {
		note(problem, true);
	}
	playhead_.rewind();
	updateLeds();
}

void Instrument::selectPreset(int index) {
	int count = static_cast<int>(loaded_.presets.size());
	if (count == 0) return;
	presetIndex_ = ((index % count) + count) % count;
	lastPresetSwitchMs_ = nowMs_;
	loadCurrentPreset();
}

void Instrument::nextPreset(bool respectMinInterval) {
	uint64_t minInterval = static_cast<uint64_t>(loaded_.profile.presetSwitchMinIntervalMs);
	if (respectMinInterval && nowMs_ - lastPresetSwitchMs_ < minInterval && lastPresetSwitchMs_ != 0) {
		note("preset change ignored (too soon after the last one)");
		return;
	}
	selectPreset(presetIndex_ + 1);
}

void Instrument::skipNarration() {
	// Sticks: once skipped, this profile starts straight in Granular mode
	// from now on (including after a restart), the way some performers'
	// devices always went straight to the grains and never played narration.
	// Saved immediately (not on the next explicit save) since the hardware
	// button that triggers this has no other way to persist anything.
	if (!loaded_.profile.narration.skip) {
		loaded_.profile.narration.skip = true;
		std::string error;
		if (!store_.saveProfile(loaded_.profile, error)) {
			note("could not save narration skip: " + error, true);
		}
	}
	modes_.skipNarration();
}

void Instrument::recalibrate() {
	processor_.beginCalibration();
	note("recalibrating - keep the instrument still and do not touch the balls");
}

bool Instrument::reloadFromDisk() {
	bool ok = switchProfile(loaded_.profile.name);
	if (ok) note("settings reloaded from disk");
	return ok;
}

bool Instrument::switchProfile(const std::string & name) {
	LoadedProfile fresh = store_.loadProfile(name);
	for (const std::string & warning : fresh.warnings) note(warning, true);
	if (fresh.presets.empty()) fresh.presets.emplace_back();
	loaded_ = std::move(fresh);
	presetIndex_ = std::min(presetIndex_, static_cast<int>(loaded_.presets.size()) - 1);
	processor_.configure(loaded_.profile.sensors, loaded_.profile.motion);
	gesture_.configure(loaded_.profile.gestures);
	for (const std::string & problem : audio_.loadNarration(loaded_.profile.narration, audioResolver())) {
		note(problem, true);
	}
	loadCurrentPreset();
	restartAudio();
	note("switched to profile '" + loaded_.profile.name + "'");
	return true;
}

bool Instrument::saveToDisk(std::string & error) {
	bool ok = store_.saveAll(loaded_.profile, loaded_.presets, error);
	note(ok ? "saved profile '" + loaded_.profile.name + "'" : "save failed: " + error, !ok);
	return ok;
}

bool Instrument::savePresetToDisk(std::string & error) {
	bool ok = store_.savePreset(loaded_.profile, presetIndex_, preset(), error);
	note(ok ? "saved preset " + std::to_string(presetIndex_ + 1) : "save failed: " + error, !ok);
	return ok;
}

void Instrument::settingsChanged() {
	processor_.configure(loaded_.profile.sensors, loaded_.profile.motion);
	gesture_.configure(loaded_.profile.gestures);
	audio_.syncStructure(preset(), loaded_.profile.narration);
}

void Instrument::reloadSlotSample(int slot) {
	if (slot < 0 || slot >= kMaxSlots) return;
	const std::string & file = preset().slots[slot].file;
	std::string error = audio_.loadSlotSample(slot, store_.audioPath(loaded_.profile.name, file), file);
	if (!error.empty()) note("slot " + std::to_string(slot + 1) + ": " + error, true);
}

void Instrument::reloadNarration() {
	for (const std::string & problem : audio_.loadNarration(loaded_.profile.narration, audioResolver())) {
		note(problem, true);
	}
	modes_.start(loaded_.profile.narration, audio_.narration().isLoaded(), nowMs_);
}

bool Instrument::restartAudio() {
	audio_.stop();
	std::string report;
	audioOpen_ = audio_.startDevice(resolveAudioSettings(loaded_.profile.audio), report);
	note(report, !audioOpen_);
	return audioOpen_;
}

void Instrument::setAudition(int slot, bool active, float position) {
	if (slot < 0 || slot >= kMaxSlots) return;
	audition_[slot].overridePosition = active;
	audition_[slot].position = position;
	audition_[slot].overrideGate = active;
	audition_[slot].gateOpen = active;
	// Be heard while auditioning even if volume normally follows a sensor
	// that is not currently pressed.
	audition_[slot].overrideVolume = active;
	audition_[slot].volume = 1.0f;
}

// =================================================================== recording

bool Instrument::startRecording(const std::string & path) {
	std::string error;
	if (!recorder_.open(path, error)) {
		note(error, true);
		return false;
	}
	note("recording sensors to " + path);
	return true;
}

void Instrument::stopRecording() {
	if (recorder_.isOpen()) {
		recorder_.close();
		note("recording stopped");
	}
}

// ====================================================================== update

void Instrument::update(uint64_t nowMs) {
	nowMs_ = nowMs;
	float dt = static_cast<float>(nowMs - lastUpdateMs_) / 1000.0f;
	lastUpdateMs_ = nowMs;

	readSensors(nowMs, dt);
	handleButton(nowMs);
	handleGesture(nowMs);
	handleRemote();

	NarrationProgress progress { audio_.narration().position(), audio_.narration().finished() };
	modes_.update(inputs_, progress, nowMs);
	if (modes_.mode() == Mode::NarrationGlitch) {
		audio_.narration().setGlitchPosition(modes_.glitchPosition());
	}

	// Accumulated mode: total pressure walks the playhead through the file.
	bool pressed = inputs_.get(ModSource::Total) > 0.0f;
	playhead_.advance(dt, pressed, preset().playheadSpeed);

	sendToAudio();
	panel_.update(nowMs);
	remote_.sendTelemetry(telemetry(), nowMs);
}

void Instrument::readSensors(uint64_t nowMs, float dt) {
	SensorFrame frame;
	if (!sensors_ || !sensors_->read(nowMs, dt, frame)) {
		if (!hadSensorError_) {
			note("sensor read failed", true);
			hadSensorError_ = true;
		}
		return;
	}
	hadSensorError_ = false;
	lastAccel_ = frame.accel;
	if (frame.isRaw) {
		if (recorder_.isOpen()) recorder_.write(nowMs, frame.raw, frame.accel);
		inputs_ = processor_.process(frame.raw, frame.accel, dt);
	} else {
		inputs_ = processor_.processNormalised(frame.normalised, frame.accel, dt);
	}
}

void Instrument::handleButton(uint64_t nowMs) {
	if (nowMs - startMs_ < kButtonIgnoreAfterStartMs) return;
	ButtonEvent event = button_.update(panel_.buttonPressed(), nowMs);
	if (event == ButtonEvent::None) return;
	note(std::string("button: ") + buttonEventName(event));

	switch (event) {
	case ButtonEvent::DoubleClick:
		if (modes_.mode() == Mode::Granular) {
			nextPreset(true);
		} else {
			skipNarration();
		}
		break;
	case ButtonEvent::TripleClick:
		panel_.pulseRelay(nowMs); // toggle the speaker power
		break;
	case ButtonEvent::LongPress:
		recalibrate();
		break;
	case ButtonEvent::VeryLongPress:
		if (loaded_.profile.hardware.longPressQuits) {
			quitRequested_ = true;
			shutdownSystem_ = loaded_.profile.hardware.shutdownOnQuit;
		}
		break;
	default:
		break;
	}
}

void Instrument::handleGesture(uint64_t nowMs) {
	if (!loaded_.profile.gestures.hitToChangePreset || modes_.mode() != Mode::Granular) return;
	if (gesture_.update(inputs_, nowMs)) {
		note("hit gesture: next preset");
		nextPreset(true);
	}
}

void Instrument::handleRemote() {
	for (const RemoteCommand & c : remote_.poll()) {
		const std::string & a = c.address;
		auto number = [&](size_t i, float fallback) { return i < c.numbers.size() ? c.numbers[i] : fallback; };
		auto text = [&](size_t i) { return i < c.texts.size() ? c.texts[i] : std::string(); };

		if (a == "/gm/preset/next") {
			nextPreset(false);
		} else if (a == "/gm/preset") {
			selectPreset(static_cast<int>(number(0, 1.0f)) - 1);
		} else if (a == "/gm/narration/skip") {
			skipNarration();
		} else if (a == "/gm/sensors/source") {
			std::string mode = text(0);
			if (mode == "hardware") useSensorMode(SensorMode::Hardware);
			else if (mode == "sim") useSensorMode(SensorMode::Simulation);
			else if (mode.rfind("replay:", 0) == 0) useSensorMode(SensorMode::Replay, mode.substr(7));
		} else if (a == "/gm/sim/sensor") {
			simulator_.setManual(static_cast<int>(number(0, 1.0f)) - 1, number(1, 0.0f));
		} else if (a == "/gm/sim/ramp") {
			SensorSimulator::RampSettings ramp = simulator_.ramp();
			ramp.speed = number(0, ramp.speed);
			int mask = static_cast<int>(number(1, 63.0f));
			for (int i = 0; i < kNumSensors; ++i) ramp.sensors[i] = (mask >> i) & 1;
			simulator_.setRamp(ramp);
			simulator_.startRamp();
		} else if (a == "/gm/sim/accel") {
			simulator_.setAccel(number(0, 0.0f), number(1, 0.0f), number(2, 1.0f));
		} else if (a == "/gm/calibrate") {
			recalibrate();
		} else if (a == "/gm/reload") {
			reloadFromDisk();
		} else if (a == "/gm/status") {
			remote_.replyStatus(c.replyHost, telemetry());
		} else if (a == "/gm/record") {
			if (text(0).empty()) stopRecording();
			else startRecording(text(0));
		} else if (a == "/gm/quit") {
			quitRequested_ = true;
		} else {
			note("unknown OSC command " + a, true);
		}
	}
}

void Instrument::sendToAudio() {
	std::array<SlotOverrides, kMaxSlots> overrides = audition_;
	if (loaded_.profile.playMode == PlayMode::Accumulated) {
		// Accumulated mode plays slot 1 only; the playhead replaces its position.
		bool pressed = inputs_.get(ModSource::Total) > 0.0f;
		if (!overrides[0].overridePosition) {
			overrides[0].overridePosition = true;
			overrides[0].position = playhead_.position();
			overrides[0].overrideGate = true;
			overrides[0].gateOpen = pressed;
		}
		for (int i = 1; i < kMaxSlots; ++i) {
			if (!overrides[i].overrideGate) {
				overrides[i].overrideGate = true;
				overrides[i].gateOpen = false;
			}
		}
	}
	// During the narration the grain slots stay silent.
	if (modes_.mode() != Mode::Granular) {
		for (auto & o : overrides) {
			if (!o.overridePosition) {
				o.overrideGate = true;
				o.gateOpen = false;
			}
		}
	}
	audio_.update(preset(), loaded_.profile.narration, inputs_, overrides, loaded_.profile.audio.masterGain);
}

void Instrument::updateLeds() {
	if (modes_.mode() == Mode::Granular) {
		panel_.setLeds(LedPattern::forPreset(presetIndex_));
	} else {
		panel_.setLeds(LedPattern::narration());
	}
}

Telemetry Instrument::telemetry() const {
	Telemetry t;
	t.inputs = inputs_;
	t.mode = modeName(modes_.mode());
	t.preset = presetIndex_ + 1;
	t.sensorSource = sensors_ ? sensors_->name() : "none";
	t.skippedBuffers = audio_.skippedCallbacks();
	return t;
}

// ================================================================ mode actions

void Instrument::playNarration() { audio_.narration().play(); }
void Instrument::pauseNarration() { audio_.narration().pause(); }
void Instrument::stopNarration() { audio_.narration().stop(); }
void Instrument::setNarrationGlitch(bool active) { audio_.narration().setGlitchActive(active); }

void Instrument::modeChanged(Mode mode) {
	note(std::string("mode: ") + modeName(mode));
	updateLeds();
}

// ==================================================================== shutdown

void Instrument::shutdown() {
	stopRecording();
	audio_.stop();
	panel_.pulseRelayBlocking(loaded_.profile.hardware.relayPulseMs); // turn the speaker off
	panel_.allOff();
}

} // namespace gm
