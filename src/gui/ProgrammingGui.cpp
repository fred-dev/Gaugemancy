#include "ProgrammingGui.h"

#include <algorithm>

namespace gm {

namespace {
constexpr float kPanelWidth = 300.0f;
constexpr float kMargin = 10.0f;

std::vector<std::string> windowNames() {
	std::vector<std::string> names;
	for (int i = 0; i < static_cast<int>(WindowType::Count); ++i) names.push_back(windowTypeLabel(static_cast<WindowType>(i)));
	return names;
}

std::vector<std::string> curveNames() {
	std::vector<std::string> names;
	for (int i = 0; i < static_cast<int>(Curve::Count); ++i) names.push_back(curveLabel(static_cast<Curve>(i)));
	return names;
}

const std::vector<std::string> kAxisIds = { "+z", "-z", "+x", "-x", "+y", "-y" };

std::vector<std::string> effectNames() {
	std::vector<std::string> names;
	for (const EffectInfo & info : EffectCatalog::all()) {
		names.push_back(info.label + " (cpu " + std::to_string(info.cpuCost) + ")");
	}
	return names;
}
} // namespace

ProgrammingGui::ProgrammingGui(Instrument & instrument) : instrument_(instrument) {}

// ======================================================================= setup

void ProgrammingGui::setup() {
	ofSetWindowTitle("Gaugemancy");
	ofBackground(18);
	ofxGuiSetDefaultWidth(static_cast<int>(kPanelWidth));
	ramp_ = instrument_.simulator().ramp();
	buildTransport();
	buildSimulation();
	buildProfile();
	buildEditor();
	layout();
}

void ProgrammingGui::layout() {
	float w = static_cast<float>(ofGetWidth());
	float h = static_cast<float>(ofGetHeight());

	transportPanel_.setPosition(kMargin, kMargin);
	simPanel_.setPosition(kMargin, transportPanel_.getShape().getBottom() + kMargin);
	profilePanel_.setPosition(w - kPanelWidth - kMargin, kMargin);
	editorPanel_.setPosition(w - 2.0f * (kPanelWidth + kMargin), kMargin);

	// Centre area between the left column and the two right panels.
	float left = kPanelWidth + 2.0f * kMargin;
	float right = w - 2.0f * (kPanelWidth + kMargin) - kMargin;
	float width = std::max(right - left, 300.0f);
	float slotW = (width - 2.0f * kMargin) / 3.0f;
	float slotH = std::min(140.0f, h * 0.17f);
	for (int i = 0; i < kMaxSlots; ++i) {
		int row = i / 3;
		int col = i % 3;
		slotViews_[i].setRect(ofRectangle(left + col * (slotW + kMargin), 40.0f + row * (slotH + kMargin), slotW, slotH));
	}
	float y = 40.0f + 2.0f * (slotH + kMargin);
	narrationView_.setRect(ofRectangle(left, y, width, slotH * 0.7f));
	y += slotH * 0.7f + kMargin;
	// Bottom row: the 3D instrument on the left; sensor bars and messages right.
	float deviceSize = std::max(std::min({ h - y - kMargin, width * 0.45f, 420.0f }), 200.0f);
	deviceView_.setRect(ofRectangle(left, y, deviceSize, deviceSize));
	float rightX = left + deviceSize + kMargin;
	float rightW = std::max(width - deviceSize - kMargin, 200.0f);
	sensorView_.setRect(ofRectangle(rightX, y, rightW, 150.0f));
	statusRect_.set(rightX, y + 150.0f + kMargin, rightW, std::max(h - y - 150.0f - 2.0f * kMargin, 60.0f));
}

void ProgrammingGui::windowResized(int, int) {
	layout();
}

std::string ProgrammingGui::audioDir() const {
	return instrument_.store().audioDir(instrument_.profile().name);
}

// ================================================================== transport

void ProgrammingGui::buildTransport() {
	transportPanel_.clear();
	transportBuilder_.clear();
	transportPanel_.setup("Gaugemancy - " + instrument_.profile().name);

	ofxGuiGroup & g = *transportBuilder_.group("Presets");
	std::vector<std::string> presetNames;
	for (size_t i = 0; i < instrument_.presets().size(); ++i) {
		presetNames.push_back(std::to_string(i + 1) + ": " + instrument_.presets()[i].name);
	}
	presetChoice_ = instrument_.presetIndex();
	transportBuilder_.choice(g, "preset", presetChoice_, presetNames, [this](int i) {
		presetChoice_ = i;
		rebuildTransport_ = true; // applied in update()
	});
	transportBuilder_.button(g, "next preset", [this]() {
		presetChoice_ = (instrument_.presetIndex() + 1) % static_cast<int>(instrument_.presets().size());
		rebuildTransport_ = true;
	});
	transportBuilder_.button(g, "add preset (copy of this one)", [this]() {
		ProfileSettings & profile = instrument_.profile();
		instrument_.presets().push_back(instrument_.preset());
		profile.presetFiles.push_back("presets/preset_" + std::to_string(instrument_.presets().size()) + ".json");
		instrument_.note("added preset " + std::to_string(instrument_.presets().size()) + " (save to keep it)");
		presetChoice_ = static_cast<int>(instrument_.presets().size()) - 1;
		rebuildTransport_ = true;
	});
	transportPanel_.add(&g);

	ofxGuiGroup & files = *transportBuilder_.group("Files");
	transportBuilder_.button(files, "save everything (F12)", [this]() {
		std::string error;
		instrument_.saveToDisk(error);
	});
	transportBuilder_.button(files, "save this preset", [this]() {
		std::string error;
		instrument_.savePresetToDisk(error);
	});
	transportBuilder_.button(files, "reload from disk (discard edits)", [this]() {
		instrument_.reloadFromDisk();
		rebuildTransport_ = true;
	});
	transportPanel_.add(&files);

	ofxGuiGroup & play = *transportBuilder_.group("Performance");
	transportBuilder_.button(play, "skip narration", [this]() { instrument_.skipNarration(); });
	transportBuilder_.button(play, "recalibrate sensors", [this]() { instrument_.recalibrate(); });
	sourceChoice_ = instrument_.sensorSourceName() == "hardware" ? 1 : 0;
	transportBuilder_.choice(play, "sensor input", sourceChoice_, { "Simulation", "Hardware" }, [this](int i) {
		instrument_.useSensorMode(i == 1 ? SensorMode::Hardware : SensorMode::Simulation);
	});
	transportBuilder_.text(play, "record file", recordFile_);
	transportBuilder_.button(play, "start / stop recording", [this]() {
		if (instrument_.isRecording()) instrument_.stopRecording();
		else instrument_.startRecording(ofToDataPath(recordFile_, true));
	});
	transportPanel_.add(&play);
}

// ================================================================= simulation

void ProgrammingGui::buildSimulation() {
	simPanel_.clear();
	simBuilder_.clear();
	simPanel_.setup("Simulation");
	SensorSimulator & sim = instrument_.simulator();

	ofxGuiGroup & manual = *simBuilder_.group("Squeeze by hand");
	for (int i = 0; i < kNumSensors; ++i) {
		simLevels_[i] = sim.manual(i);
		simBuilder_.floatSlider(manual, "sensor " + std::to_string(i + 1), simLevels_[i], 0.0f, 1.0f, [this, i]() {
			instrument_.simulator().setManual(i, simLevels_[i]);
		});
	}
	simPanel_.add(&manual);

	ofxGuiGroup & ramp = *simBuilder_.group("Squeeze ramp (F9)");
	simBuilder_.floatSlider(ramp, "speed (full scale/s)", ramp_.speed, 0.05f, 5.0f);
	simBuilder_.floatSlider(ramp, "peak", ramp_.extent, 0.0f, 1.0f);
	simBuilder_.toggle(ramp, "loop", ramp_.loop);
	for (int i = 0; i < kNumSensors; ++i) {
		simBuilder_.toggle(ramp, "include sensor " + std::to_string(i + 1), ramp_.sensors[i]);
	}
	simBuilder_.button(ramp, "start / stop ramp", [this]() {
		SensorSimulator & s = instrument_.simulator();
		if (s.rampRunning()) {
			s.stopRamp();
		} else {
			s.setRamp(ramp_);
			s.startRamp();
		}
	});
	simPanel_.add(&ramp);
}

// ===================================================================== editor

void ProgrammingGui::select(Target target, int slot) {
	target_ = target;
	selectedSlot_ = std::clamp(slot, 0, kMaxSlots - 1);
	requestRebuild();
}

void ProgrammingGui::buildEditor() {
	editorPanel_.clear();
	editorBuilder_.clear();
	switch (target_) {
	case Target::Slot:
		editorPanel_.setup("Slot " + std::to_string(selectedSlot_ + 1));
		buildSlotEditor(instrument_.preset().slots[selectedSlot_], selectedSlot_);
		break;
	case Target::Master: {
		editorPanel_.setup("Master output");
		ofxGuiGroup & g = *editorBuilder_.group("Master");
		editorBuilder_.floatSlider(g, "preset gain", instrument_.preset().masterGain, 0.0f, 4.0f);
		editorBuilder_.label(g, "effects on the mix of all slots:");
		editorPanel_.add(&g);
		buildEffectChainEditor(*editorBuilder_.group("Master effects"), instrument_.preset().masterEffects);
		break;
	}
	case Target::Narration:
		editorPanel_.setup("Narration");
		buildNarrationEditor(instrument_.profile().narration);
		break;
	}
	ofxGuiGroup & targetGroup = *editorBuilder_.group("Edit...", true);
	for (int i = 0; i < kMaxSlots; ++i) {
		editorBuilder_.button(targetGroup, "slot " + std::to_string(i + 1) + " (F" + std::to_string(i + 1) + ")",
			[this, i]() { select(Target::Slot, i); });
	}
	editorBuilder_.button(targetGroup, "master (F7)", [this]() { select(Target::Master); });
	editorBuilder_.button(targetGroup, "narration (F8)", [this]() { select(Target::Narration); });
	editorPanel_.add(&targetGroup);
}

void ProgrammingGui::buildSlotEditor(SlotSettings & slot, int index) {
	ofxGuiGroup & g = *editorBuilder_.group("Sample");
	editorBuilder_.toggle(g, "enabled", slot.enabled, [this]() { structureChanged(); });
	editorBuilder_.pathField(g, "file", slot.file, audioDir(), [this, index]() { samplesToReload_.push_back(index); });
	editorBuilder_.choice(g, "grain window", static_cast<int>(slot.window), windowNames(), [this, &slot](int i) {
		slot.window = static_cast<WindowType>(i);
		structureChanged();
	});
	editorBuilder_.intSlider(g, "grain voices (cpu)", slot.voices, 1, 32, [this]() { structureChanged(); });
	std::vector<std::string> gates = { "Auto (while its sensors are pressed)", "Always" };
	for (int i = 1; i < kNumModSources; ++i) gates.push_back("While " + modSourceLabel(static_cast<ModSource>(i)));
	int gateIndex = slot.gate == GateMode::Auto ? 0 : slot.gate == GateMode::Always ? 1 : 1 + static_cast<int>(slot.gateSource);
	editorBuilder_.choice(g, "heard", gateIndex, gates, [&slot](int i) {
		if (i == 0) slot.gate = GateMode::Auto;
		else if (i == 1) slot.gate = GateMode::Always;
		else {
			slot.gate = GateMode::Source;
			slot.gateSource = static_cast<ModSource>(i - 1);
		}
	});
	editorPanel_.add(&g);

	ofxGuiGroup & grain = *editorBuilder_.group("Grain parameters");
	const auto & specs = grainParamSpecs();
	for (int i = 0; i < GrainSettings::count(); ++i) {
		editorBuilder_.modulated(grain, specs[i], slot.grain.at(i));
	}
	editorPanel_.add(&grain);

	buildEffectChainEditor(*editorBuilder_.group("Effects (in order)"), slot.effects);
}

void ProgrammingGui::buildNarrationEditor(NarrationSettings & n) {
	ofxGuiGroup & g = *editorBuilder_.group("Narration");
	editorBuilder_.toggle(g, "enabled", n.enabled, [this]() { reloadNarration_ = true; });
	editorBuilder_.pathField(g, "file", n.file, audioDir(), [this]() { reloadNarration_ = true; });
	editorBuilder_.floatSlider(g, "volume", n.volume, 0.0f, 4.0f);
	editorBuilder_.toggle(g, "start when a ball is squeezed", n.startWithSensor);
	editorBuilder_.intSlider(g, "else start after (ms)", n.autoStartDelayMs, 0, 60000);
	editorBuilder_.floatSlider(g, "glitch threshold", n.glitchThreshold, 0.0f, 1.0f);
	editorBuilder_.floatSlider(g, "glitch reach (of file)", n.glitchStrand, 0.0f, 0.2f);
	editorBuilder_.choice(g, "glitch window", static_cast<int>(n.window), windowNames(), [this, &n](int i) {
		n.window = static_cast<WindowType>(i);
		structureChanged();
	});
	editorBuilder_.label(g, "drag in the narration view to glitch");
	editorPanel_.add(&g);

	ofxGuiGroup & grain = *editorBuilder_.group("Glitch grain");
	const auto & specs = grainParamSpecs();
	for (int i = 0; i < GrainSettings::count(); ++i) {
		if (std::string(specs[i].id) == "position" || std::string(specs[i].id) == "volume") continue;
		editorBuilder_.modulated(grain, specs[i], n.grain.at(i), {}, false);
	}
	editorPanel_.add(&grain);
	buildEffectChainEditor(*editorBuilder_.group("Narration effects"), n.effects);
}

void ProgrammingGui::buildEffectChainEditor(ofxGuiGroup & parent, EffectChainSettings & chain) {
	for (size_t e = 0; e < chain.size(); ++e) {
		EffectSettings & effect = chain[e];
		const EffectInfo * info = effect.info();
		if (info == nullptr) continue;
		ofxGuiGroup & g = *editorBuilder_.group(std::to_string(e + 1) + ". " + info->label, true);
		editorBuilder_.label(g, info->description);
		editorBuilder_.toggle(g, "enabled", effect.enabled, [this]() { structureChanged(); });
		for (size_t p = 0; p < info->params.size() && p < effect.params.size(); ++p) {
			editorBuilder_.modulated(g, info->params[p], effect.params[p]);
		}
		editorBuilder_.button(g, "move up", [this, &chain, e]() {
			if (e > 0) std::swap(chain[e], chain[e - 1]);
			structureChanged();
			requestRebuild();
		});
		editorBuilder_.button(g, "move down", [this, &chain, e]() {
			if (e + 1 < chain.size()) std::swap(chain[e], chain[e + 1]);
			structureChanged();
			requestRebuild();
		});
		editorBuilder_.button(g, "remove", [this, &chain, e]() {
			chain.erase(chain.begin() + static_cast<long>(e));
			structureChanged();
			requestRebuild();
		});
		parent.add(&g);
	}
	editorBuilder_.choice(parent, "new effect", addEffectChoice_, effectNames(), [this](int i) { addEffectChoice_ = i; });
	editorBuilder_.button(parent, "add effect at the end", [this, &chain]() {
		chain.push_back(EffectSettings::make(EffectCatalog::all()[addEffectChoice_].id));
		structureChanged();
		requestRebuild();
	});
	editorPanel_.add(&parent);
}

// ==================================================================== profile

void ProgrammingGui::buildProfile() {
	profilePanel_.clear();
	profileBuilder_.clear();
	profilePanel_.setup("Profile settings");
	ProfileSettings & p = instrument_.profile();
	auto changed = [this]() { structureChanged(); };

	ofxGuiGroup & play = *profileBuilder_.group("Playing", false);
	profileBuilder_.choice(play, "mode", static_cast<int>(p.playMode), { "Multi (6 slots)", "Accumulated (slot 1)" },
		[&p](int i) { p.playMode = i == 1 ? PlayMode::Accumulated : PlayMode::Multi; });
	profileBuilder_.floatSlider(play, "accumulated playhead speed", instrument_.preset().playheadSpeed, 0.0f, 0.2f);
	profileBuilder_.intSlider(play, "control rate (Hz)", p.controlRateHz, 20, 500);
	profileBuilder_.intSlider(play, "min ms between preset changes", p.presetSwitchMinIntervalMs, 0, 20000);
	profilePanel_.add(&play);

	ofxGuiGroup & s = *profileBuilder_.group("Sensors", true);
	profileBuilder_.intSlider(s, "raw value at full squeeze", p.sensors.maxRaw, 1, 1023, changed);
	profileBuilder_.floatSlider(s, "noise threshold", p.sensors.activeThreshold, 0.0f, 0.5f, changed);
	profileBuilder_.choice(s, "response curve", static_cast<int>(p.sensors.curve), curveNames(), [&p, changed](int i) {
		p.sensors.curve = static_cast<Curve>(i);
		changed();
	});
	profileBuilder_.floatSlider(s, "smoothing (ms)", p.sensors.smoothingMs, 0.0f, 300.0f, changed);
	profileBuilder_.floatSlider(s, "balls for 100% total", p.sensors.accumulationDenominator, 0.5f, 6.0f, changed);
	profileBuilder_.intSlider(s, "calibration frames", p.sensors.calibrationFrames, 1, 200, changed);
	profilePanel_.add(&s);

	ofxGuiGroup & m = *profileBuilder_.group("Motion (accelerometer)", true);
	profileBuilder_.toggle(m, "enabled", p.motion.enabled, changed);
	profileBuilder_.toggle(m, "level = position at startup", p.motion.zeroAtStartup, changed);
	profileBuilder_.choice(m, "else 'up' axis", std::max(0, static_cast<int>(std::find(kAxisIds.begin(), kAxisIds.end(), p.motion.upAxis) - kAxisIds.begin()) % 6),
		{ "+z", "-z", "+x", "-x", "+y", "-y" }, [&p, changed](int i) {
			p.motion.upAxis = kAxisIds[i];
			changed();
		});
	profileBuilder_.floatSlider(m, "tilt range (deg)", p.motion.rangeDegrees, 5.0f, 180.0f, changed);
	profileBuilder_.floatSlider(m, "dead zone (deg)", p.motion.deadzoneDegrees, 0.0f, 20.0f, changed);
	profileBuilder_.floatSlider(m, "tilt smoothing (ms)", p.motion.smoothingMs, 0.0f, 1000.0f, changed);
	profileBuilder_.floatSlider(m, "shake sensitivity (g)", p.motion.shakeSensitivity, 0.05f, 4.0f, changed);
	profileBuilder_.floatSlider(m, "shake release (ms)", p.motion.shakeReleaseMs, 10.0f, 3000.0f, changed);
	profileBuilder_.button(m, "set level now (recalibrate)", [this]() { instrument_.recalibrate(); });
	profilePanel_.add(&m);

	ofxGuiGroup & g = *profileBuilder_.group("Hit gesture (change preset)", true);
	profileBuilder_.toggle(g, "enabled", p.gestures.hitToChangePreset, changed);
	profileBuilder_.floatSlider(g, "squeeze above", p.gestures.hitThreshold, 0.0f, 1.0f, changed);
	profileBuilder_.floatSlider(g, "release below", p.gestures.troughThreshold, 0.0f, 1.0f, changed);
	profileBuilder_.intSlider(g, "max squeeze ms", p.gestures.maxPeakMs, 10, 2000, changed);
	profileBuilder_.intSlider(g, "max pause ms", p.gestures.maxTroughMs, 10, 2000, changed);
	profileBuilder_.intSlider(g, "hits needed", p.gestures.requiredHits, 1, 20, changed);
	profileBuilder_.intSlider(g, "balls per hit", p.gestures.minSensorsPerHit, 1, kNumSensors, changed);
	profilePanel_.add(&g);

	ofxGuiGroup & a = *profileBuilder_.group("Audio device (restart to apply)", true);
	profileBuilder_.text(a, "api", p.audio.api);
	profileBuilder_.text(a, "device name contains", p.audio.deviceName);
	profileBuilder_.intSlider(a, "device id (fallback)", p.audio.deviceId, -1, 64);
	profileBuilder_.intSlider(a, "sample rate", p.audio.sampleRate, 22050, 96000);
	profileBuilder_.intSlider(a, "buffer size", p.audio.bufferSize, 64, 4096);
	profileBuilder_.intSlider(a, "buffers", p.audio.numBuffers, 1, 8);
	profileBuilder_.floatSlider(a, "master gain", p.audio.masterGain, 0.0f, 4.0f);
	profileBuilder_.button(a, "restart audio with these settings", [this]() { instrument_.restartAudio(); });
	for (const std::string & device : AudioEngine::describeOutputDevices(p.audio.api)) {
		profileBuilder_.label(a, device);
	}
	profilePanel_.add(&a);

	ofxGuiGroup & h = *profileBuilder_.group("Raspberry Pi hardware", true);
	profileBuilder_.text(h, "SPI device", p.hardware.spiDevice);
	profileBuilder_.intSlider(h, "button pin (BCM)", p.hardware.buttonPin, 0, 27);
	profileBuilder_.toggle(h, "button pulls to ground", p.hardware.buttonActiveLow);
	profileBuilder_.intSlider(h, "blue LED pin", p.hardware.blueLedPin, -1, 27);
	profileBuilder_.intSlider(h, "red LED pin", p.hardware.redLedPin, -1, 27);
	profileBuilder_.intSlider(h, "speaker relay pin", p.hardware.relayPin, -1, 27);
	profileBuilder_.intSlider(h, "multi-click gap (ms)", p.hardware.clickTimeoutMs, 50, 1000);
	profileBuilder_.toggle(h, "6 s press quits", p.hardware.longPressQuits);
	profileBuilder_.toggle(h, "power off on quit", p.hardware.shutdownOnQuit);
	profileBuilder_.toggle(h, "accelerometer", p.hardware.accelerometer);
	profilePanel_.add(&h);

	ofxGuiGroup & o = *profileBuilder_.group("Network (OSC)", true);
	profileBuilder_.toggle(o, "accept commands", p.osc.control);
	profileBuilder_.intSlider(o, "command port", p.osc.controlPort, 1024, 65535);
	profileBuilder_.toggle(o, "send telemetry", p.osc.telemetry);
	profileBuilder_.text(o, "telemetry host", p.osc.remoteHost);
	profileBuilder_.intSlider(o, "telemetry port", p.osc.remotePort, 1024, 65535);
	profilePanel_.add(&o);

	ofxGuiGroup & l = *profileBuilder_.group("Logging", true);
	profileBuilder_.text(l, "level", p.logLevel);
	profilePanel_.add(&l);
}

// ===================================================================== update

void ProgrammingGui::update() {
	uint64_t now = ofGetElapsedTimeMillis();
	float dt = lastUpdateMs_ == 0 ? 0.0f : static_cast<float>(now - lastUpdateMs_) / 1000.0f;
	lastUpdateMs_ = now;
	bool simulating = instrument_.sensorSourceName() == "simulation";
	deviceView_.update(dt, simulating, instrument_.simulator(), instrument_.lastAccel());
	if (rebuildTransport_) {
		rebuildTransport_ = false;
		if (presetChoice_ != instrument_.presetIndex()) {
			instrument_.selectPreset(presetChoice_);
		}
		buildTransport();
		buildProfile(); // the playhead speed belongs to the preset
		requestRebuild();
		layout();
	}
	for (int slot : samplesToReload_) {
		instrument_.reloadSlotSample(slot);
	}
	samplesToReload_.clear();
	if (reloadNarration_) {
		reloadNarration_ = false;
		instrument_.reloadNarration();
	}
	if (structureChanged_) {
		structureChanged_ = false;
		instrument_.settingsChanged();
	}
	if (rebuildEditor_) {
		rebuildEditor_ = false;
		buildEditor();
		layout();
	}
}

// ======================================================================= draw

void ProgrammingGui::draw() {
	ofBackground(18);
	ofSetColor(220);
	std::string header = instrument_.profile().name + " | preset " + std::to_string(instrument_.presetIndex() + 1) + ": " +
		instrument_.preset().name + " | " + modeName(instrument_.mode()) + " | sensors: " + instrument_.sensorSourceName() +
		(instrument_.isRecording() ? " | RECORDING" : "");
	size_t headerChars = static_cast<size_t>(std::max(slotViews_[2].rect().getRight() - slotViews_[0].rect().x, 8.0f) / 8.0f);
	if (header.size() > headerChars) header = header.substr(0, headerChars - 1) + "~";
	ofDrawBitmapString(header, slotViews_[0].rect().x, 24);

	drawSlots();
	drawNarration();
	const ProfileSettings & p = instrument_.profile();
	sensorView_.draw(instrument_.inputs(), p.sensors.curve, p.sensors.activeThreshold);
	deviceView_.draw(instrument_.inputs());
	drawStatus();

	transportPanel_.draw();
	simPanel_.draw();
	editorPanel_.draw();
	profilePanel_.draw();
}

void ProgrammingGui::drawSlots() {
	bool accumulated = instrument_.profile().playMode == PlayMode::Accumulated;
	for (int i = 0; i < kMaxSlots; ++i) {
		GrainSlot & slot = instrument_.audio().slot(i);
		const SlotSettings & settings = instrument_.preset().slots[i];
		GrainMeter meter {
			[&slot]() { return slot.voices(); },
			[&slot](int v) { return slot.grainPosition(v); },
			[&slot](int v) { return slot.grainEnvelope(v); },
		};
		std::string title = std::to_string(i + 1) + ": " + (settings.file.empty() ? "(empty)" : settings.file)
			+ (settings.enabled ? "" : "  [off]") + (accumulated && i > 0 ? "  [unused in accumulated mode]" : "");
		bool selected = target_ == Target::Slot && selectedSlot_ == i;
		slotViews_[i].draw(slot.sample(), title, meter, slot.currentPosition(), slot.currentVolume(), selected, slot.gateOpen());
	}
}

void ProgrammingGui::drawNarration() {
	NarrationVoice & n = instrument_.audio().narration();
	GrainMeter meter {
		[&n]() { return n.glitchVoices(); },
		[&n](int v) { return n.glitchGrainPosition(v); },
		[&n](int v) { return n.glitchGrainEnvelope(v); },
	};
	bool glitch = instrument_.mode() == Mode::NarrationGlitch;
	float head = glitch ? instrument_.glitchPosition() : n.position();
	std::string title = "narration: " + (instrument_.profile().narration.enabled ? instrument_.profile().narration.file : std::string("(disabled)"));
	narrationView_.draw(n.sample(), title, meter, head, 1.0f, target_ == Target::Narration, glitch);
}

void ProgrammingGui::drawStatus() {
	ofPushStyle();
	ofSetColor(40);
	ofDrawRectangle(statusRect_);
	ofSetColor(200);
	float y = statusRect_.y + 16;
	// Bitmap font: 8 px per character; long lines are cut to fit the box.
	size_t maxChars = static_cast<size_t>(std::max(statusRect_.width - 12.0f, 8.0f) / 8.0f);
	for (const std::string & line : instrument_.messages()) {
		if (y > statusRect_.getBottom()) break;
		ofDrawBitmapString(line.size() > maxChars ? line.substr(0, maxChars - 1) + "~" : line, statusRect_.x + 6, y);
		y += 14;
	}
	ofPopStyle();
}

// ====================================================================== input

void ProgrammingGui::keyPressed(int key) {
	if (key >= OF_KEY_F1 && key <= OF_KEY_F6) {
		select(Target::Slot, key - OF_KEY_F1);
	} else if (key == OF_KEY_F7) {
		select(Target::Master);
	} else if (key == OF_KEY_F8) {
		select(Target::Narration);
	} else if (key == OF_KEY_F9) {
		SensorSimulator & s = instrument_.simulator();
		s.setRamp(ramp_);
		s.startRamp();
	} else if (key == OF_KEY_F11) {
		ofToggleFullscreen();
	} else if (key == OF_KEY_F12) {
		std::string error;
		instrument_.saveToDisk(error);
	}
}

void ProgrammingGui::mouseMoved(int, int) {}

void ProgrammingGui::mousePressed(int x, int y, int) {
	if (deviceView_.mousePressed(x, y)) return;
	for (int i = 0; i < kMaxSlots; ++i) {
		if (slotViews_[i].inside(x, y)) {
			if (target_ != Target::Slot || selectedSlot_ != i) select(Target::Slot, i);
			auditionSlot_ = i;
			instrument_.setAudition(i, true, slotViews_[i].positionAt(x));
			return;
		}
	}
	if (narrationView_.inside(x, y)) {
		glitchByMouse_ = true;
		instrument_.setManualGlitch(true, narrationView_.positionAt(x));
	}
}

void ProgrammingGui::mouseDragged(int x, int y, int) {
	if (deviceView_.mouseDragged(x, y)) return;
	if (auditionSlot_ >= 0) {
		instrument_.setAudition(auditionSlot_, true, slotViews_[auditionSlot_].positionAt(x));
	}
	if (glitchByMouse_) {
		instrument_.setManualGlitch(true, narrationView_.positionAt(x));
	}
}

void ProgrammingGui::mouseReleased(int x, int y, int) {
	if (deviceView_.mouseReleased(x, y)) return;
	if (auditionSlot_ >= 0) {
		instrument_.setAudition(auditionSlot_, false, 0.0f);
		auditionSlot_ = -1;
	}
	if (glitchByMouse_) {
		glitchByMouse_ = false;
		instrument_.setManualGlitch(false, 0.0f);
	}
}

void ProgrammingGui::dragEvent(const ofDragInfo & info) {
	if (info.files.empty()) return;
	for (int i = 0; i < kMaxSlots; ++i) {
		if (!slotViews_[i].inside(info.position.x, info.position.y)) continue;
		std::string error;
		std::string name = GuiBuilder::adoptFile(info.files[0].string(), audioDir(), error);
		if (name.empty()) {
			instrument_.note(error, true);
			return;
		}
		instrument_.preset().slots[i].file = name;
		samplesToReload_.push_back(i);
		select(Target::Slot, i);
		return;
	}
}

} // namespace gm
