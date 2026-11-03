#include "EffectCatalog.h"

namespace gm {

const std::vector<EffectInfo> & EffectCatalog::all() {
	// Ranges follow the original instrument's GUI so old settings convert
	// unchanged, except where the old range was unusable (noted below).
	static const std::vector<EffectInfo> effects = {
		{ "bitcrush", "Bit crusher", "Reduces bit depth for digital grit.",
			{
				{ "bits", "Bits", 16.0f, 1.0f, 16.0f },
			},
			1 },
		{ "decimator", "Decimator", "Reduces sample rate for aliasing and lo-fi tone.",
			{
				{ "rate", "Rate (Hz)", 44100.0f, 50.0f, 44100.0f },
			},
			1 },
		{ "filter", "Ladder filter", "4 pole low-pass with resonance.",
			{
				{ "cutoff", "Cutoff (Hz)", 20000.0f, 20.0f, 20000.0f },
				{ "resonance", "Resonance", 0.0f, 0.0f, 1.0f },
			},
			2 },
		{ "chorus", "Dimension chorus", "Stereo widening chorus.",
			{
				{ "speed", "Speed (Hz)", 0.5f, 0.0f, 100.0f },
				{ "depth", "Depth (ms)", 0.0f, 0.0f, 30.0f },
				{ "delay", "Delay (ms)", 0.0f, 0.0f, 200.0f },
			},
			3 },
		{ "delay", "Stereo delay", "Echo with feedback and damping, mixed with the dry signal.",
			{
				{ "mix", "Wet mix", 0.3f, 0.0f, 1.0f },
				{ "time", "Time (ms)", 250.0f, 1.0f, 2000.0f },
				// feedback is capped below 1 so the delay can never run away.
				{ "feedback", "Feedback", 0.3f, 0.0f, 0.95f },
				{ "damping", "Damping", 0.5f, 0.0f, 1.0f },
			},
			2 },
		{ "reverb", "Reverb", "Plate style reverb, mixed with the dry signal.",
			{
				{ "mix", "Wet mix", 0.3f, 0.0f, 1.0f },
				{ "time", "Time (s)", 3.0f, 0.1f, 20.0f },
				{ "density", "Density", 0.5f, 0.0f, 1.0f },
				{ "damping", "Damping", 0.5f, 0.0f, 1.0f },
				// the old GUI allowed 0 Hz, which silences the reverb.
				{ "hiCut", "High cut (Hz)", 5000.0f, 200.0f, 20000.0f },
				{ "modFreq", "Mod speed (Hz)", 0.2f, 0.0f, 20.0f },
				{ "modAmount", "Mod amount (ms)", 0.0f, 0.0f, 1.0f },
			},
			5 },
		{ "compressor", "Compressor", "Levels the output; gain is applied before compression.",
			{
				{ "gain", "Input gain", 1.0f, 0.0f, 10.0f },
				{ "threshold", "Threshold (dB)", 0.0f, -50.0f, 0.0f },
				{ "ratio", "Ratio", 4.0f, 1.0f, 40.0f },
				{ "attack", "Attack (ms)", 10.0f, 1.0f, 60.0f },
				{ "release", "Release (ms)", 50.0f, 10.0f, 250.0f },
				{ "knee", "Knee (dB)", 0.0f, 0.0f, 10.0f },
			},
			2 },
		{ "gain", "Gain", "Simple volume control.",
			{
				{ "gain", "Gain", 1.0f, 0.0f, 4.0f },
			},
			1 },
	};
	return effects;
}

const EffectInfo * EffectCatalog::find(const std::string & id) {
	for (const EffectInfo & info : all()) {
		if (info.id == id) {
			return &info;
		}
	}
	return nullptr;
}

int EffectCatalog::paramIndex(const EffectInfo & info, const std::string & paramId) {
	for (size_t i = 0; i < info.params.size(); ++i) {
		if (paramId == info.params[i].id) {
			return static_cast<int>(i);
		}
	}
	return -1;
}

} // namespace gm
