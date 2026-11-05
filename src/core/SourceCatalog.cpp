#include "SourceCatalog.h"

namespace gm {

const std::vector<SourceInfo> & SourceCatalog::all() {
	// Pitch is a pdsp "pitch" value (MIDI-note-like, A3=440Hz is the pdsp
	// default tuning) - 57 is that A3, the range is roughly C1..C6.
	static const std::vector<SourceInfo> sources = {
		{ "granular", "Granular (sample)", "Plays grains from the loaded file. Uses the grain parameters below.", {} },
		{ "va_osc", "Analog oscillator", "Antialiased subtractive oscillator, no sample needed.",
			{
				{ "waveform", "Waveform", 0.0f, 0.0f, 3.0f, { "Saw", "Pulse", "Triangle", "Sine" } },
				{ "volume", "Volume", 0.8f, 0.0f, 1.0f },
				{ "pitch", "Pitch", 57.0f, 24.0f, 96.0f },
				{ "pulseWidth", "Pulse width (pulse waveform only)", 0.5f, 0.05f, 0.95f },
			} },
		{ "fm_operator", "FM operator", "Sine-wavetable FM operator with self-feedback - no sample needed.",
			{
				{ "volume", "Volume", 0.8f, 0.0f, 1.0f },
				{ "pitch", "Pitch", 57.0f, 24.0f, 96.0f },
				{ "ratio", "Ratio", 1.0f, 0.1f, 16.0f },
				{ "fmAmount", "FM amount", 0.0f, 0.0f, 4.0f },
				{ "feedback", "Feedback", 0.0f, 0.0f, 1.0f },
			} },
		{ "lfo_osc", "Simple oscillator", "Cheap, non-antialiased oscillator run at audio rate - rawer/more aliased "
			"character than the analog oscillator, no sample needed.",
			{
				{ "waveform", "Waveform", 0.0f, 0.0f, 3.0f, { "Triangle", "Sine", "Saw", "Square" } },
				{ "volume", "Volume", 0.8f, 0.0f, 1.0f },
				{ "freq", "Frequency (Hz)", 220.0f, 20.0f, 4000.0f },
			} },
		{ "noise", "Noise", "Stereo digital noise with a built-in decimator and bit-crusher, no sample needed. "
			"Good for texture/percussion rather than pitched sound.",
			{
				{ "volume", "Volume", 0.8f, 0.0f, 1.0f },
				{ "pitch", "Pitch (mostly unpitched below ~0)", -100.0f, -100.0f, 96.0f },
				{ "decimation", "Decimation pitch (lower = dirtier)", 151.0f, -24.0f, 151.0f },
				{ "bits", "Bit depth", 12.0f, 1.0f, 16.0f },
			} },
	};
	return sources;
}

const SourceInfo * SourceCatalog::find(const std::string & id) {
	for (const SourceInfo & info : all()) {
		if (info.id == id) {
			return &info;
		}
	}
	return nullptr;
}

std::vector<ModulatedParam> SourceCatalog::makeParams(const std::string & id) {
	std::vector<ModulatedParam> params;
	const SourceInfo * info = find(id);
	if (info == nullptr) return params;
	for (const ParamSpec & spec : info->params) {
		params.push_back(ModulatedParam::fromSpec(spec));
	}
	return params;
}

} // namespace gm
