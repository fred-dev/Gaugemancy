#include "SettingsJson.h"

#include <algorithm>

namespace gm {

// =============================================================================
// Reader: small helper that reads optional, typed, range-checked values from a
// JSON object and records a warning (with the JSON path) for anything odd.
// =============================================================================

namespace {

class Reader {
public:
	Reader(const Json & json, std::string path, Warnings & warnings)
		: json_(json), path_(std::move(path)), warnings_(warnings) {
		if (!json_.is_null() && !json_.is_object()) {
			warn("", "expected an object, using defaults");
		}
	}

	bool has(const char * key) const {
		return json_.is_object() && json_.contains(key);
	}

	const Json & raw(const char * key) const {
		static const Json null;
		return has(key) ? json_.at(key) : null;
	}

	std::string pathOf(const char * key) const {
		return path_.empty() ? key : path_ + "." + key;
	}

	void read(const char * key, bool & out) const {
		if (!has(key)) return;
		const Json & v = json_.at(key);
		if (v.is_boolean()) out = v.get<bool>();
		else if (v.is_number()) out = v.get<double>() != 0.0; // legacy 0/1
		else warn(key, "expected true/false");
	}

	void read(const char * key, int & out, int lo, int hi) const {
		if (!has(key)) return;
		const Json & v = json_.at(key);
		if (!v.is_number()) {
			warn(key, "expected a number");
			return;
		}
		int value = static_cast<int>(v.get<double>());
		out = clampWithWarning(key, value, lo, hi);
	}

	void read(const char * key, float & out, float lo, float hi) const {
		if (!has(key)) return;
		const Json & v = json_.at(key);
		if (!v.is_number()) {
			warn(key, "expected a number");
			return;
		}
		out = clampWithWarning(key, v.get<float>(), lo, hi);
	}

	void read(const char * key, std::string & out) const {
		if (!has(key)) return;
		const Json & v = json_.at(key);
		if (v.is_string()) out = v.get<std::string>();
		else warn(key, "expected text");
	}

	void warn(const char * key, const std::string & message) const {
		std::string where = (key == nullptr || *key == '\0') ? path_ : pathOf(key);
		warnings_.push_back((where.empty() ? "(root)" : where) + ": " + message);
	}

private:
	template <typename T>
	T clampWithWarning(const char * key, T value, T lo, T hi) const {
		T clamped = std::clamp(value, lo, hi);
		if (clamped != value) {
			warn(key, "out of range, clamped to " + std::to_string(clamped));
		}
		return clamped;
	}

	const Json & json_;
	std::string path_;
	Warnings & warnings_;
};

std::string indexPath(const std::string & path, const char * key, size_t index) {
	return (path.empty() ? std::string(key) : path + "." + key) + "[" + std::to_string(index) + "]";
}

} // namespace

// =============================================================================
// Writing
// =============================================================================

Json toJson(const ModulatedParam & param) {
	Json j;
	j["value"] = param.value;
	j["min"] = param.min;
	j["max"] = param.max;
	j["source"] = modSourceId(param.source);
	return j;
}

Json toJson(const GrainSettings & grain) {
	Json j = Json::object();
	const auto & specs = grainParamSpecs();
	for (int i = 0; i < GrainSettings::count(); ++i) {
		j[specs[i].id] = toJson(grain.at(i));
	}
	return j;
}

Json toJson(const EffectSettings & effect) {
	Json j;
	j["type"] = effect.type;
	j["enabled"] = effect.enabled;
	Json params = Json::object();
	if (const EffectInfo * info = effect.info()) {
		for (size_t i = 0; i < info->params.size() && i < effect.params.size(); ++i) {
			params[info->params[i].id] = toJson(effect.params[i]);
		}
	}
	j["params"] = params;
	return j;
}

Json toJson(const EffectChainSettings & chain) {
	Json j = Json::array();
	for (const EffectSettings & effect : chain) {
		j.push_back(toJson(effect));
	}
	return j;
}

Json toJson(const SlotSettings & slot) {
	Json j;
	j["enabled"] = slot.enabled;
	j["file"] = slot.file;
	j["window"] = windowTypeId(slot.window);
	j["gate"] = slot.gate == GateMode::Source ? modSourceId(slot.gateSource) : gateModeId(slot.gate);
	j["voices"] = slot.voices;
	j["grain"] = toJson(slot.grain);
	j["effects"] = toJson(slot.effects);
	return j;
}

Json toJson(const PresetSettings & preset) {
	Json j;
	j["version"] = kSettingsVersion;
	j["name"] = preset.name;
	j["playheadSpeed"] = preset.playheadSpeed;
	Json slots = Json::array();
	for (const SlotSettings & slot : preset.slots) {
		slots.push_back(toJson(slot));
	}
	j["slots"] = slots;
	j["master"] = { { "gain", preset.masterGain }, { "effects", toJson(preset.masterEffects) } };
	return j;
}

Json toJson(const NarrationSettings & n) {
	Json j;
	j["enabled"] = n.enabled;
	j["file"] = n.file;
	j["volume"] = n.volume;
	j["startWithSensor"] = n.startWithSensor;
	j["autoStartDelayMs"] = n.autoStartDelayMs;
	j["glitchThreshold"] = n.glitchThreshold;
	j["glitchStrand"] = n.glitchStrand;
	j["window"] = windowTypeId(n.window);
	j["grain"] = toJson(n.grain);
	j["effects"] = toJson(n.effects);
	return j;
}

Json toJson(const ProfileSettings & p) {
	Json j;
	j["version"] = kSettingsVersion;
	j["name"] = p.name;
	j["logLevel"] = p.logLevel;
	j["playMode"] = playModeId(p.playMode);
	j["controlRateHz"] = p.controlRateHz;
	j["presetSwitchMinIntervalMs"] = p.presetSwitchMinIntervalMs;
	j["presets"] = p.presetFiles;

	j["sensors"] = {
		{ "maxRaw", p.sensors.maxRaw },
		{ "activeThreshold", p.sensors.activeThreshold },
		{ "curve", curveId(p.sensors.curve) },
		{ "calibrationFrames", p.sensors.calibrationFrames },
		{ "accumulationDenominator", p.sensors.accumulationDenominator },
		{ "smoothingMs", p.sensors.smoothingMs },
	};
	j["motion"] = {
		{ "enabled", p.motion.enabled },
		{ "zeroAtStartup", p.motion.zeroAtStartup },
		{ "upAxis", p.motion.upAxis },
		{ "rangeDegrees", p.motion.rangeDegrees },
		{ "deadzoneDegrees", p.motion.deadzoneDegrees },
		{ "smoothingMs", p.motion.smoothingMs },
		{ "shakeSensitivity", p.motion.shakeSensitivity },
		{ "shakeReleaseMs", p.motion.shakeReleaseMs },
		{ "calibrationFrames", p.motion.calibrationFrames },
	};
	j["audio"] = {
		{ "sampleRate", p.audio.sampleRate },
		{ "bufferSize", p.audio.bufferSize },
		{ "numBuffers", p.audio.numBuffers },
		{ "api", p.audio.api },
		{ "deviceName", p.audio.deviceName },
		{ "deviceId", p.audio.deviceId },
		{ "masterGain", p.audio.masterGain },
	};
	j["gestures"] = {
		{ "hitToChangePreset", p.gestures.hitToChangePreset },
		{ "hitThreshold", p.gestures.hitThreshold },
		{ "troughThreshold", p.gestures.troughThreshold },
		{ "maxPeakMs", p.gestures.maxPeakMs },
		{ "maxTroughMs", p.gestures.maxTroughMs },
		{ "requiredHits", p.gestures.requiredHits },
		{ "minSensorsPerHit", p.gestures.minSensorsPerHit },
	};
	j["hardware"] = {
		{ "spiDevice", p.hardware.spiDevice },
		{ "adcChannels", p.hardware.adcChannels },
		{ "buttonPin", p.hardware.buttonPin },
		{ "buttonActiveLow", p.hardware.buttonActiveLow },
		{ "blueLedPin", p.hardware.blueLedPin },
		{ "redLedPin", p.hardware.redLedPin },
		{ "relayPin", p.hardware.relayPin },
		{ "relayPulseMs", p.hardware.relayPulseMs },
		{ "clickTimeoutMs", p.hardware.clickTimeoutMs },
		{ "longPressQuits", p.hardware.longPressQuits },
		{ "shutdownOnQuit", p.hardware.shutdownOnQuit },
		{ "accelerometer", p.hardware.accelerometer },
		{ "i2cBus", p.hardware.i2cBus },
		{ "accelAddress", p.hardware.accelAddress },
	};
	j["osc"] = {
		{ "telemetry", p.osc.telemetry },
		{ "remoteHost", p.osc.remoteHost },
		{ "remotePort", p.osc.remotePort },
		{ "control", p.osc.control },
		{ "controlPort", p.osc.controlPort },
		{ "telemetryRateHz", p.osc.telemetryRateHz },
	};
	j["narration"] = toJson(p.narration);
	return j;
}

Json toJson(const RootSettings & root) {
	Json j;
	j["version"] = kSettingsVersion;
	j["activeProfile"] = root.activeProfile;
	return j;
}

// =============================================================================
// Reading
// =============================================================================

ModulatedParam modulatedParamFromJson(const Json & json, const ParamSpec & spec, const std::string & path, Warnings & warnings) {
	ModulatedParam param = ModulatedParam::fromSpec(spec);
	if (json.is_number()) {
		// Shorthand: a plain number is a fixed value.
		param.value = json.get<float>();
	} else {
		Reader r(json, path, warnings);
		r.read("value", param.value, -1e9f, 1e9f);
		r.read("min", param.min, -1e9f, 1e9f);
		r.read("max", param.max, -1e9f, 1e9f);
		std::string source;
		r.read("source", source);
		if (!source.empty()) {
			param.source = modSourceFromId(source);
			if (param.source == ModSource::None && source != "none") {
				r.warn("source", "unknown source '" + source + "', disconnected");
			}
		}
	}
	ModulatedParam unclamped = param;
	param.clampTo(spec);
	if (param != unclamped) {
		warnings.push_back(path + ": value outside " + std::to_string(spec.lowest) + ".." + std::to_string(spec.highest) + ", clamped");
	}
	return param;
}

GrainSettings grainFromJson(const Json & json, const std::string & path, Warnings & warnings) {
	GrainSettings grain;
	Reader r(json, path, warnings);
	const auto & specs = grainParamSpecs();
	for (int i = 0; i < GrainSettings::count(); ++i) {
		if (r.has(specs[i].id)) {
			grain.at(i) = modulatedParamFromJson(r.raw(specs[i].id), specs[i], r.pathOf(specs[i].id), warnings);
		}
	}
	return grain;
}

EffectSettings effectFromJson(const Json & json, const std::string & path, Warnings & warnings) {
	Reader r(json, path, warnings);
	std::string type;
	r.read("type", type);
	EffectSettings effect = EffectSettings::make(type);
	const EffectInfo * info = effect.info();
	if (info == nullptr) {
		r.warn("type", "unknown effect '" + type + "', ignored");
		return effect;
	}
	r.read("enabled", effect.enabled);
	Reader params(r.raw("params"), r.pathOf("params"), warnings);
	for (size_t i = 0; i < info->params.size(); ++i) {
		const ParamSpec & spec = info->params[i];
		if (params.has(spec.id)) {
			effect.params[i] = modulatedParamFromJson(params.raw(spec.id), spec, params.pathOf(spec.id), warnings);
		}
	}
	return effect;
}

EffectChainSettings effectChainFromJson(const Json & json, const std::string & path, Warnings & warnings) {
	EffectChainSettings chain;
	if (json.is_null()) {
		return chain;
	}
	if (!json.is_array()) {
		warnings.push_back(path + ": expected a list of effects");
		return chain;
	}
	for (size_t i = 0; i < json.size(); ++i) {
		std::string itemPath = path + "[" + std::to_string(i) + "]";
		EffectSettings effect = effectFromJson(json[i], itemPath, warnings);
		if (effect.info() != nullptr) {
			chain.push_back(std::move(effect));
		}
	}
	return chain;
}

SlotSettings slotFromJson(const Json & json, const std::string & path, Warnings & warnings) {
	SlotSettings slot;
	Reader r(json, path, warnings);
	r.read("enabled", slot.enabled);
	r.read("file", slot.file);
	std::string window;
	r.read("window", window);
	if (!window.empty()) slot.window = windowTypeFromId(window);
	std::string gate;
	r.read("gate", gate);
	if (gate == "always") {
		slot.gate = GateMode::Always;
	} else if (gate.empty() || gate == "auto") {
		slot.gate = GateMode::Auto;
	} else {
		slot.gate = GateMode::Source;
		slot.gateSource = modSourceFromId(gate);
		if (slot.gateSource == ModSource::None) {
			r.warn("gate", "unknown gate '" + gate + "', using auto");
			slot.gate = GateMode::Auto;
		}
	}
	r.read("voices", slot.voices, 1, 32);
	if (r.has("grain")) slot.grain = grainFromJson(r.raw("grain"), r.pathOf("grain"), warnings);
	slot.effects = effectChainFromJson(r.raw("effects"), r.pathOf("effects"), warnings);
	return slot;
}

PresetSettings presetFromJson(const Json & json, Warnings & warnings) {
	PresetSettings preset;
	Reader r(json, "", warnings);
	r.read("name", preset.name);
	r.read("playheadSpeed", preset.playheadSpeed, 0.0f, 10.0f);
	const Json & slots = r.raw("slots");
	if (slots.is_array()) {
		if (slots.size() > static_cast<size_t>(kMaxSlots)) {
			warnings.push_back("slots: more than " + std::to_string(kMaxSlots) + " slots, extra ignored");
		}
		for (size_t i = 0; i < slots.size() && i < static_cast<size_t>(kMaxSlots); ++i) {
			preset.slots[i] = slotFromJson(slots[i], indexPath("", "slots", i), warnings);
		}
	} else if (!slots.is_null()) {
		warnings.push_back("slots: expected a list");
	}
	Reader master(r.raw("master"), "master", warnings);
	master.read("gain", preset.masterGain, 0.0f, 4.0f);
	preset.masterEffects = effectChainFromJson(master.raw("effects"), "master.effects", warnings);
	return preset;
}

NarrationSettings narrationFromJson(const Json & json, const std::string & path, Warnings & warnings) {
	NarrationSettings n;
	Reader r(json, path, warnings);
	r.read("enabled", n.enabled);
	r.read("file", n.file);
	r.read("volume", n.volume, 0.0f, 4.0f);
	r.read("startWithSensor", n.startWithSensor);
	r.read("autoStartDelayMs", n.autoStartDelayMs, 0, 600000);
	r.read("glitchThreshold", n.glitchThreshold, 0.0f, 1.0f);
	r.read("glitchStrand", n.glitchStrand, 0.0f, 1.0f);
	std::string window;
	r.read("window", window);
	if (!window.empty()) n.window = windowTypeFromId(window);
	if (r.has("grain")) n.grain = grainFromJson(r.raw("grain"), r.pathOf("grain"), warnings);
	n.effects = effectChainFromJson(r.raw("effects"), r.pathOf("effects"), warnings);
	return n;
}

ProfileSettings profileFromJson(const Json & json, Warnings & warnings) {
	ProfileSettings p;
	Reader r(json, "", warnings);
	r.read("name", p.name);
	r.read("logLevel", p.logLevel);
	std::string playMode;
	r.read("playMode", playMode);
	if (!playMode.empty()) p.playMode = playModeFromId(playMode);
	r.read("controlRateHz", p.controlRateHz, 10, 1000);
	r.read("presetSwitchMinIntervalMs", p.presetSwitchMinIntervalMs, 0, 60000);

	const Json & presets = r.raw("presets");
	if (presets.is_array()) {
		p.presetFiles.clear();
		for (const Json & file : presets) {
			if (file.is_string()) p.presetFiles.push_back(file.get<std::string>());
		}
		if (p.presetFiles.empty()) {
			warnings.push_back("presets: list is empty, using presets/preset_1.json");
			p.presetFiles.push_back("presets/preset_1.json");
		}
	}

	Reader s(r.raw("sensors"), "sensors", warnings);
	s.read("maxRaw", p.sensors.maxRaw, 1, 1023);
	s.read("activeThreshold", p.sensors.activeThreshold, 0.0f, 1.0f);
	std::string curve;
	s.read("curve", curve);
	if (!curve.empty()) p.sensors.curve = curveFromId(curve);
	s.read("calibrationFrames", p.sensors.calibrationFrames, 1, 1000);
	s.read("accumulationDenominator", p.sensors.accumulationDenominator, 0.1f, 6.0f);
	s.read("smoothingMs", p.sensors.smoothingMs, 0.0f, 2000.0f);

	Reader m(r.raw("motion"), "motion", warnings);
	m.read("enabled", p.motion.enabled);
	m.read("zeroAtStartup", p.motion.zeroAtStartup);
	m.read("upAxis", p.motion.upAxis);
	m.read("rangeDegrees", p.motion.rangeDegrees, 5.0f, 180.0f);
	m.read("deadzoneDegrees", p.motion.deadzoneDegrees, 0.0f, 45.0f);
	m.read("smoothingMs", p.motion.smoothingMs, 0.0f, 2000.0f);
	m.read("shakeSensitivity", p.motion.shakeSensitivity, 0.01f, 8.0f);
	m.read("shakeReleaseMs", p.motion.shakeReleaseMs, 1.0f, 5000.0f);
	m.read("calibrationFrames", p.motion.calibrationFrames, 1, 1000);

	Reader a(r.raw("audio"), "audio", warnings);
	a.read("sampleRate", p.audio.sampleRate, 8000, 192000);
	a.read("bufferSize", p.audio.bufferSize, 32, 8192);
	a.read("numBuffers", p.audio.numBuffers, 1, 16);
	a.read("api", p.audio.api);
	a.read("deviceName", p.audio.deviceName);
	a.read("deviceId", p.audio.deviceId, -1, 1000);
	a.read("masterGain", p.audio.masterGain, 0.0f, 4.0f);

	Reader g(r.raw("gestures"), "gestures", warnings);
	g.read("hitToChangePreset", p.gestures.hitToChangePreset);
	g.read("hitThreshold", p.gestures.hitThreshold, 0.0f, 1.0f);
	g.read("troughThreshold", p.gestures.troughThreshold, 0.0f, 1.0f);
	g.read("maxPeakMs", p.gestures.maxPeakMs, 1, 10000);
	g.read("maxTroughMs", p.gestures.maxTroughMs, 1, 10000);
	g.read("requiredHits", p.gestures.requiredHits, 1, 100);
	g.read("minSensorsPerHit", p.gestures.minSensorsPerHit, 1, kNumSensors);

	Reader h(r.raw("hardware"), "hardware", warnings);
	h.read("spiDevice", p.hardware.spiDevice);
	const Json & channels = h.raw("adcChannels");
	if (channels.is_array() && channels.size() == static_cast<size_t>(kNumSensors)) {
		for (int i = 0; i < kNumSensors; ++i) {
			p.hardware.adcChannels[i] = std::clamp(channels[i].is_number() ? channels[i].get<int>() : i, 0, 7);
		}
	} else if (!channels.is_null()) {
		h.warn("adcChannels", "expected a list of 6 channel numbers");
	}
	h.read("buttonPin", p.hardware.buttonPin, 0, 53);
	h.read("buttonActiveLow", p.hardware.buttonActiveLow);
	h.read("blueLedPin", p.hardware.blueLedPin, -1, 53);
	h.read("redLedPin", p.hardware.redLedPin, -1, 53);
	h.read("relayPin", p.hardware.relayPin, -1, 53);
	h.read("relayPulseMs", p.hardware.relayPulseMs, 10, 5000);
	h.read("clickTimeoutMs", p.hardware.clickTimeoutMs, 50, 2000);
	h.read("longPressQuits", p.hardware.longPressQuits);
	h.read("shutdownOnQuit", p.hardware.shutdownOnQuit);
	h.read("accelerometer", p.hardware.accelerometer);
	h.read("i2cBus", p.hardware.i2cBus);
	h.read("accelAddress", p.hardware.accelAddress, 0, 127);

	Reader o(r.raw("osc"), "osc", warnings);
	o.read("telemetry", p.osc.telemetry);
	o.read("remoteHost", p.osc.remoteHost);
	o.read("remotePort", p.osc.remotePort, 1, 65535);
	o.read("control", p.osc.control);
	o.read("controlPort", p.osc.controlPort, 1, 65535);
	o.read("telemetryRateHz", p.osc.telemetryRateHz, 1, 200);

	if (r.has("narration")) {
		p.narration = narrationFromJson(r.raw("narration"), "narration", warnings);
	}
	return p;
}

RootSettings rootFromJson(const Json & json, Warnings & warnings) {
	RootSettings root;
	Reader r(json, "", warnings);
	r.read("activeProfile", root.activeProfile);
	return root;
}

} // namespace gm
