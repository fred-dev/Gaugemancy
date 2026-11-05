#include "Settings.h"

namespace gm {

// ------------------------------------------------------------- window type --

namespace {
struct WindowName {
	const char * id;
	const char * label;
};
const WindowName kWindowNames[static_cast<int>(WindowType::Count)] = {
	{ "rectangular", "Rectangular" },
	{ "triangular", "Triangular" },
	{ "hann", "Hann" },
	{ "hamming", "Hamming" },
	{ "blackman", "Blackman" },
	{ "blackmanHarris", "Blackman-Harris" },
	{ "sine", "Sine" },
	{ "welch", "Welch" },
	{ "gaussian", "Gaussian" },
	{ "tukey", "Tukey" },
};
} // namespace

std::string windowTypeId(WindowType type) {
	int i = static_cast<int>(type);
	return (i >= 0 && i < static_cast<int>(WindowType::Count)) ? kWindowNames[i].id : "hann";
}

std::string windowTypeLabel(WindowType type) {
	int i = static_cast<int>(type);
	return (i >= 0 && i < static_cast<int>(WindowType::Count)) ? kWindowNames[i].label : "Hann";
}

WindowType windowTypeFromId(const std::string & id) {
	for (int i = 0; i < static_cast<int>(WindowType::Count); ++i) {
		if (id == kWindowNames[i].id) {
			return static_cast<WindowType>(i);
		}
	}
	return WindowType::Hann;
}

// ------------------------------------------------------------------- grain --

const std::vector<ParamSpec> & grainParamSpecs() {
	// Order must match GrainSettings::at().
	static const std::vector<ParamSpec> specs = {
		{ "length", "Grain length (ms)", 500.0f, 10.0f, 3000.0f },
		{ "density", "Density", 0.9f, 0.1f, 1.0f },
		{ "distanceJitter", "Distance jitter (ms)", 20.0f, 0.0f, 1000.0f },
		{ "pitchJitter", "Pitch jitter (st)", 0.0f, -200.0f, 200.0f },
		{ "pitch", "Pitch (st)", 0.0f, -20.0f, 20.0f },
		{ "spread", "Position spread", 0.0f, 0.0f, 1.0f },
		{ "volume", "Volume", 0.5f, 0.0f, 1.0f },
		{ "position", "Play position", 0.5f, 0.0f, 1.0f },
		{ "direction", "Direction", 1.0f, -1.0f, 1.0f },
	};
	return specs;
}

GrainSettings::GrainSettings() {
	const auto & specs = grainParamSpecs();
	for (int i = 0; i < count(); ++i) {
		at(i) = ModulatedParam::fromSpec(specs[i]);
	}
}

int GrainSettings::count() {
	return 9;
}

ModulatedParam & GrainSettings::at(int index) {
	switch (index) {
	case 0: return length;
	case 1: return density;
	case 2: return distanceJitter;
	case 3: return pitchJitter;
	case 4: return pitch;
	case 5: return spread;
	case 6: return volume;
	case 7: return position;
	default: return direction;
	}
}

const ModulatedParam & GrainSettings::at(int index) const {
	return const_cast<GrainSettings *>(this)->at(index);
}

// ----------------------------------------------------------------- effects --

EffectSettings EffectSettings::make(const std::string & type) {
	EffectSettings effect;
	effect.type = type;
	const EffectInfo * info = EffectCatalog::find(type);
	if (info == nullptr) {
		effect.enabled = false;
		return effect;
	}
	for (const ParamSpec & spec : info->params) {
		effect.params.push_back(ModulatedParam::fromSpec(spec));
	}
	return effect;
}

// ----------------------------------------------------------------- presets --

PresetSettings::PresetSettings() : slots(kMaxSlots) {
	// Sensible default so a fresh slot is heard without any setup: slot N
	// plays while sensor N is pressed, matching the physical ball under
	// your hand - not "always on" (which would never be quiet) and not a
	// separate "enabled" switch (easy to flip by accident and forget about).
	for (int i = 0; i < kMaxSlots; ++i) {
		slots[i].gate = GateMode::Source;
		slots[i].gateSource = sensorSource(i);
	}
}

// ------------------------------------------------------------------- enums --

std::string playModeId(PlayMode mode) {
	return mode == PlayMode::Accumulated ? "accumulated" : "multi";
}

PlayMode playModeFromId(const std::string & id) {
	return id == "accumulated" ? PlayMode::Accumulated : PlayMode::Multi;
}

std::string gateModeId(GateMode mode) {
	switch (mode) {
	case GateMode::Always: return "always";
	case GateMode::Source: return "source";
	default: return "auto";
	}
}

} // namespace gm
