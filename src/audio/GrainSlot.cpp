#include "GrainSlot.h"

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
	for (int ch = 0; ch < 2; ++ch) {
		volume_ >> amp_[ch].in_mod();
		amp_[ch] >> chain_.in(ch);
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
		cloud_->ch(ch) >> amp_[ch];
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
	// Auto: open while any source used by a grain parameter is active, as the
	// original instrument did (a slot sounds while "its" balls are pressed).
	for (int i = 0; i < GrainSettings::count(); ++i) {
		const ModulatedParam & p = settings.grain.at(i);
		if (p.isModulated() && inputs.get(p.source) > 0.0f) {
			return true;
		}
	}
	return false;
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

	lastGate_ = overrides.overrideGate ? overrides.gateOpen : gateFor(settings, inputs);
	float volume = overrides.overrideVolume ? overrides.volume : g.volume.resolve(inputs);
	lastVolume_ = (settings.enabled && lastGate_ && sample_) ? volume : 0.0f;
	volume_.set(lastVolume_);

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
