#include "GrainSlot.h"

#include <algorithm>
#include <filesystem>

namespace gm {

pdsp::Window_t toPdspWindow(WindowType type) {
	switch (type) {
	case WindowType::Rectangular: return pdsp::Rectangular;
	case WindowType::Triangular: return pdsp::Triangular;
	case WindowType::Hamming: return pdsp::Hamming;
	case WindowType::Blackman: return pdsp::Blackman;
	case WindowType::BlackmanHarris: return pdsp::BlackmanHarris;
	case WindowType::Sine: return pdsp::SineWindow;
	case WindowType::Welch: return pdsp::Welch;
	case WindowType::Gaussian: return pdsp::Gaussian;
	case WindowType::Tukey: return pdsp::Tukey;
	default: return pdsp::Hann;
	}
}

GrainSlot::GrainSlot() {
	// Smooth the controls a sensor moves most, so squeezes do not click.
	volume_.enableSmoothing(20.0f);
	position_.enableSmoothing(30.0f);
	// Gain switches between sources/waveforms are also smoothed: changing
	// SlotSettings::sourceType (or "waveform") is an ordinary atomic control
	// change (see the class comment), and without this it would click like
	// any other instant gain jump.
	cloudGainCtrl_.enableSmoothing(15.0f);
	fmGainCtrl_.enableSmoothing(15.0f);
	noiseGainCtrl_.enableSmoothing(15.0f);
	cloudGainCtrl_.set(1.0f);
	fmGainCtrl_.set(0.0f);
	noiseGainCtrl_.set(0.0f);
	for (auto & c : vaWaveGainCtrl_) c.enableSmoothing(15.0f);
	for (auto & c : lfoWaveGainCtrl_) c.enableSmoothing(15.0f);

	vaPitch_.set(57.0f);
	fmPitch_.set(57.0f);
	lfoFreq_.set(220.0f);
	noisePitch_.set(-100.0f);
	vaPitch_ >> vaOsc_.in_pitch();
	vaPulseWidth_ >> vaOsc_.in_pw();
	fmPitch_ >> fmOp_.in_pitch();
	fmRatio_ >> fmOp_.in_ratio();
	fmAmount_ >> fmOp_.in_fm();
	fmFeedback_ >> fmOp_.in_fb();
	lfoFreq_ >> lfoOsc_.in_freq();
	noisePitch_ >> noise_.in_pitch();
	noiseDecimation_ >> noise_.in_decimation();
	noiseBits_ >> noise_.in_bits();

	for (int ch = 0; ch < 2; ++ch) {
		cloudGainCtrl_ >> cloudGain_[ch].in_mod();
		fmGainCtrl_ >> fmGain_[ch].in_mod();
		noiseGainCtrl_ >> noiseGain_[ch].in_mod();
		fmOp_.out_signal() >> fmGain_[ch];
		cloudGain_[ch] >> amp_[ch];
		fmGain_[ch] >> amp_[ch];
		noiseGain_[ch] >> amp_[ch];

		volume_ >> amp_[ch].in_mod();
		amp_[ch] >> chain_.in(ch);
	}
	// BitNoise is natively stereo (two independent noise channels) - feed
	// each straight to its own channel instead of duplicating one signal.
	noise_.ch(0) >> noiseGain_[0];
	noise_.ch(1) >> noiseGain_[1];

	// VAOscillator/LFO compute all 4 of their waveforms unconditionally,
	// same as the FM/noise sources above; "waveform" in update() just picks
	// which one's gain is non-zero. Order matches SourceCatalog's
	// "waveform" param comment (0=saw 1=pulse 2=tri 3=sine for VA,
	// 0=tri 1=sine 2=saw 3=square for the LFO).
	//
	// out_X() selects that output on the oscillator itself (shared, mutable
	// state - see VAOscillator.h's own doc comments) and must be patched
	// immediately, not stored for later: calling out_pulse() after out_saw()
	// changes what out_saw()'s *already-returned* reference would connect.
	for (int ch = 0; ch < 2; ++ch) {
		vaWaveGainCtrl_[0] >> vaWaveGain_[0][ch].in_mod();
		vaOsc_.out_saw() >> vaWaveGain_[0][ch];
		vaWaveGain_[0][ch] >> amp_[ch];

		vaWaveGainCtrl_[1] >> vaWaveGain_[1][ch].in_mod();
		vaOsc_.out_pulse() >> vaWaveGain_[1][ch];
		vaWaveGain_[1][ch] >> amp_[ch];

		vaWaveGainCtrl_[2] >> vaWaveGain_[2][ch].in_mod();
		vaOsc_.out_triangle() >> vaWaveGain_[2][ch];
		vaWaveGain_[2][ch] >> amp_[ch];

		vaWaveGainCtrl_[3] >> vaWaveGain_[3][ch].in_mod();
		vaOsc_.out_sine() >> vaWaveGain_[3][ch];
		vaWaveGain_[3][ch] >> amp_[ch];

		lfoWaveGainCtrl_[0] >> lfoWaveGain_[0][ch].in_mod();
		lfoOsc_.out_triangle() >> lfoWaveGain_[0][ch];
		lfoWaveGain_[0][ch] >> amp_[ch];

		lfoWaveGainCtrl_[1] >> lfoWaveGain_[1][ch].in_mod();
		lfoOsc_.out_sine() >> lfoWaveGain_[1][ch];
		lfoWaveGain_[1][ch] >> amp_[ch];

		lfoWaveGainCtrl_[2] >> lfoWaveGain_[2][ch].in_mod();
		lfoOsc_.out_saw() >> lfoWaveGain_[2][ch];
		lfoWaveGain_[2][ch] >> amp_[ch];

		lfoWaveGainCtrl_[3] >> lfoWaveGain_[3][ch].in_mod();
		lfoOsc_.out_square() >> lfoWaveGain_[3][ch];
		lfoWaveGain_[3][ch] >> amp_[ch];
	}
	volume_.set(0.0f);
}

GrainSlot::~GrainSlot() = default;

std::unique_ptr<pdsp::SampleBuffer> GrainSlot::loadSampleFile(const std::string & path, std::string & error) {
	std::error_code ec;
	if (path.empty()) {
		error = "no file set";
		return nullptr;
	}
	if (!std::filesystem::exists(path, ec)) {
		error = "file not found: " + path;
		return nullptr;
	}
	auto buffer = std::make_unique<pdsp::SampleBuffer>();
	buffer->load(path);
	if (!buffer->loaded()) {
		error = "cannot decode " + path + " (use wav, flac, mp3 or ogg)";
		return nullptr;
	}
	return buffer;
}

std::unique_ptr<pdsp::SampleBuffer> GrainSlot::setSample(std::unique_ptr<pdsp::SampleBuffer> sample, const std::string & name) {
	std::unique_ptr<pdsp::SampleBuffer> old = std::move(sample_);
	sample_ = std::move(sample);
	sampleName_ = sample_ ? name : std::string();
	if (cloud_ && sample_) {
		cloud_->setSample(sample_.get());
	}
	return old;
}

void GrainSlot::createCloud(int voices) {
	// The old cloud's destructor disconnects it from the amps and controls.
	cloud_ = std::make_unique<pdsp::GrainCloud>(voices);
	length_ >> cloud_->in_length();
	density_ >> cloud_->in_density();
	distanceJitter_ >> cloud_->in_distance_jitter();
	pitchJitter_ >> cloud_->in_pitch_jitter();
	pitch_ >> cloud_->in_pitch();
	spread_ >> cloud_->in_position_jitter();
	position_ >> cloud_->in_position();
	direction_ >> cloud_->in_direction();
	for (int ch = 0; ch < 2; ++ch) {
		cloud_->ch(ch) >> cloudGain_[ch];
	}
	if (sample_) {
		cloud_->setSample(sample_.get());
	}
	builtVoices_ = voices;
	builtWindow_ = WindowType::Count; // force the window to be set again
}

bool GrainSlot::needsRebuild(const SlotSettings & settings) const {
	return !cloud_ || settings.voices != builtVoices_ || settings.window != builtWindow_ || !chain_.matches(settings.effects);
}

void GrainSlot::rebuild(const SlotSettings & settings) {
	if (!cloud_ || settings.voices != builtVoices_) {
		createCloud(settings.voices);
	}
	if (settings.window != builtWindow_) {
		cloud_->setWindowType(toPdspWindow(settings.window));
		builtWindow_ = settings.window;
	}
	if (!chain_.matches(settings.effects)) {
		chain_.build(settings.effects);
	}
}

bool GrainSlot::gateFor(const SlotSettings & settings, const ControlInputs & inputs) {
	switch (settings.gate) {
	case GateMode::Always:
		return true;
	case GateMode::Source:
		return inputs.get(settings.gateSource) > 0.0f;
	case GateMode::Auto:
		break;
	}
	// Auto: open while any source used by a grain (or, for a synth source,
	// synth) parameter is active, as the original instrument did (a slot
	// sounds while "its" balls are pressed) - but only once something is
	// actually mapped to gate on. A slot with nothing modulated yet (a
	// freshly-picked synth source, for instance) has nothing to "auto" on,
	// so it just plays - that's much more useful than silence by default.
	bool anyModulated = false;
	if (settings.sourceType == "granular") {
		for (int i = 0; i < GrainSettings::count(); ++i) {
			const ModulatedParam & p = settings.grain.at(i);
			if (p.isModulated()) {
				anyModulated = true;
				if (inputs.get(p.source) > 0.0f) return true;
			}
		}
	} else {
		for (const ModulatedParam & p : settings.synthParams) {
			if (p.isModulated()) {
				anyModulated = true;
				if (inputs.get(p.source) > 0.0f) return true;
			}
		}
	}
	return !anyModulated;
}

void GrainSlot::update(const SlotSettings & settings, const ControlInputs & inputs, const SlotOverrides & overrides) {
	const GrainSettings & g = settings.grain;
	length_.set(g.length.resolve(inputs));
	density_.set(g.density.resolve(inputs));
	distanceJitter_.set(g.distanceJitter.resolve(inputs));
	pitchJitter_.set(g.pitchJitter.resolve(inputs));
	pitch_.set(g.pitch.resolve(inputs));
	spread_.set(g.spread.resolve(inputs));
	direction_.set(g.direction.resolve(inputs));

	lastPosition_ = overrides.overridePosition ? overrides.position : g.position.resolve(inputs);
	position_.set(lastPosition_);

	// Each source type resolves its own volume from its own param list -
	// grain's for granular, the matching synth param otherwise - so "Volume"
	// is always right there with that source's other parameters, not a
	// separate grain-only control a synth source would have to borrow.
	const SourceInfo * sourceInfo = SourceCatalog::find(settings.sourceType);
	auto resolveSynth = [&](const std::string & id, float fallback) {
		if (sourceInfo == nullptr) return fallback;
		for (size_t i = 0; i < sourceInfo->params.size(); ++i) {
			if (sourceInfo->params[i].id == id && i < settings.synthParams.size()) {
				return settings.synthParams[i].resolve(inputs);
			}
		}
		return fallback;
	};

	lastGate_ = overrides.overrideGate ? overrides.gateOpen : gateFor(settings, inputs);
	float volume = overrides.overrideVolume ? overrides.volume
		: settings.sourceType == "granular" ? g.volume.resolve(inputs) : resolveSynth("volume", 0.8f);
	// A synth source always "has sound" (no file needed); granular still
	// needs a loaded sample.
	bool audible = settings.enabled && lastGate_ && (settings.sourceType != "granular" || sample_);
	lastVolume_ = audible ? volume : 0.0f;
	volume_.set(lastVolume_);

	cloudGainCtrl_.set(settings.sourceType == "granular" ? 1.0f : 0.0f);
	fmGainCtrl_.set(settings.sourceType == "fm_operator" ? 1.0f : 0.0f);
	noiseGainCtrl_.set(settings.sourceType == "noise" ? 1.0f : 0.0f);

	// "waveform" is just another resolved param (see SourceCatalog), so a
	// sensor can sweep through waveforms exactly like any other parameter -
	// round it to pick which single gain of the 4 is open.
	int vaWave = settings.sourceType == "va_osc" ? std::clamp(static_cast<int>(resolveSynth("waveform", 0.0f)), 0, 3) : -1;
	int lfoWave = settings.sourceType == "lfo_osc" ? std::clamp(static_cast<int>(resolveSynth("waveform", 0.0f)), 0, 3) : -1;
	for (int w = 0; w < 4; ++w) {
		vaWaveGainCtrl_[w].set(w == vaWave ? 1.0f : 0.0f);
		lfoWaveGainCtrl_[w].set(w == lfoWave ? 1.0f : 0.0f);
	}

	if (settings.sourceType == "va_osc") {
		vaPitch_.set(resolveSynth("pitch", 57.0f));
		vaPulseWidth_.set(resolveSynth("pulseWidth", 0.5f));
	} else if (settings.sourceType == "fm_operator") {
		fmPitch_.set(resolveSynth("pitch", 57.0f));
		fmRatio_.set(resolveSynth("ratio", 1.0f));
		fmAmount_.set(resolveSynth("fmAmount", 0.0f));
		fmFeedback_.set(resolveSynth("feedback", 0.0f));
	} else if (settings.sourceType == "lfo_osc") {
		lfoFreq_.set(resolveSynth("freq", 220.0f));
	} else if (settings.sourceType == "noise") {
		noisePitch_.set(resolveSynth("pitch", -100.0f));
		noiseDecimation_.set(resolveSynth("decimation", 151.0f));
		noiseBits_.set(resolveSynth("bits", 12.0f));
	}

	chain_.apply(settings.effects, inputs);
}

void GrainSlot::mute() {
	volume_.set(0.0f);
	lastVolume_ = 0.0f;
	lastGate_ = false;
}

int GrainSlot::voices() const {
	return cloud_ ? cloud_->getVoicesNum() : 0;
}

float GrainSlot::grainPosition(int voice) const {
	return cloud_ ? cloud_->meter_position(voice) : 0.0f;
}

float GrainSlot::grainEnvelope(int voice) const {
	return cloud_ ? cloud_->meter_env(voice) : 0.0f;
}

} // namespace gm
