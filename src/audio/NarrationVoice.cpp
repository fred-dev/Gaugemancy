#include "NarrationVoice.h"
#include "GrainSlot.h"

#include <algorithm>

namespace gm {

namespace {
constexpr float kFadeMs = 15.0f;        // click-free start/pause
constexpr float kEndPosition = 0.998f;  // treat the last few ms as "finished"
} // namespace

NarrationVoice::NarrationVoice() {
	// Player: trigger -> samplers -> amps (faded by the envelope) -> chain.
	fade_.set(kFadeMs, 0.0f, 1.0f, kFadeMs);
	fadeGate_ >> fade_;
	playerVolume_.enableSmoothing(50.0f);
	for (int ch = 0; ch < 2; ++ch) {
		samplerTrigger_ >> samplers_[ch].in_trig();
		startPosition_ >> samplers_[ch].in_start();
		samplers_[ch] >> playerAmp_[ch];
		fade_ >> playerAmp_[ch].in_mod();
		playerVolume_ >> playerVolumeAmp_[ch].in_mod();
		playerAmp_[ch] >> playerVolumeAmp_[ch] >> chain_.in(ch);
	}

	// Glitch: cloud is created in rebuild(); its amps feed the same chain.
	glitchLevel_.enableSmoothing(50.0f);
	glitchLevel_.set(0.0f);
	glitchPosition_.enableSmoothing(30.0f);
	for (int ch = 0; ch < 2; ++ch) {
		glitchLevel_ >> glitchAmp_[ch].in_mod();
		glitchAmp_[ch] >> chain_.in(ch);
	}
}

NarrationVoice::~NarrationVoice() = default;

std::unique_ptr<pdsp::SampleBuffer> NarrationVoice::loadFile(const std::string & path, std::string & error) {
	return GrainSlot::loadSampleFile(path, error);
}

std::unique_ptr<pdsp::SampleBuffer> NarrationVoice::setFile(std::unique_ptr<pdsp::SampleBuffer> sample) {
	std::unique_ptr<pdsp::SampleBuffer> old = std::move(sample_);
	sample_ = std::move(sample);
	playing_ = false;
	paused_ = false;
	pausedPosition_ = 0.0f;
	connectSamplers();
	if (cloud_ && sample_) {
		cloud_->setSample(sample_.get());
	}
	return old;
}

void NarrationVoice::connectSamplers() {
	if (!sample_) {
		return;
	}
	// A mono file plays on both sides; a stereo file keeps its channels.
	bool stereo = sample_->channels >= 2;
	samplers_[0].setSample(sample_.get(), 0, 0);
	samplers_[1].setSample(sample_.get(), 0, stereo ? 1 : 0);
}

bool NarrationVoice::needsRebuild(const NarrationSettings & settings) const {
	return !cloud_ || settings.window != builtWindow_ || !chain_.matches(settings.effects);
}

void NarrationVoice::rebuild(const NarrationSettings & settings) {
	if (!cloud_) {
		cloud_ = std::make_unique<pdsp::GrainCloud>(8);
		length_ >> cloud_->in_length();
		density_ >> cloud_->in_density();
		distanceJitter_ >> cloud_->in_distance_jitter();
		pitchJitter_ >> cloud_->in_pitch_jitter();
		pitch_ >> cloud_->in_pitch();
		spread_ >> cloud_->in_position_jitter();
		direction_ >> cloud_->in_direction();
		glitchPosition_ >> cloud_->in_position();
		for (int ch = 0; ch < 2; ++ch) {
			cloud_->ch(ch) >> glitchAmp_[ch];
		}
		if (sample_) {
			cloud_->setSample(sample_.get());
		}
		builtWindow_ = WindowType::Count;
	}
	if (settings.window != builtWindow_) {
		cloud_->setWindowType(toPdspWindow(settings.window));
		builtWindow_ = settings.window;
	}
	if (!chain_.matches(settings.effects)) {
		chain_.build(settings.effects);
	}
}

void NarrationVoice::startFrom(float position) {
	startPosition_.set(std::clamp(position, 0.0f, 1.0f));
	samplerTrigger_.trigger(1.0f);
	fadeGate_.trigger(1.0f);
	playerVolume_.set(volume_);
	playing_ = true;
	paused_ = false;
}

void NarrationVoice::play() {
	if (!sample_) {
		return;
	}
	if (paused_) {
		startFrom(pausedPosition_);
	} else if (!playing_) {
		startFrom(0.0f);
	}
}

void NarrationVoice::pause() {
	if (playing_ && !paused_) {
		pausedPosition_ = position();
		fadeGate_.off();
		paused_ = true;
	}
}

void NarrationVoice::stop() {
	fadeGate_.off();
	playing_ = false;
	paused_ = false;
	pausedPosition_ = 0.0f;
	setGlitchActive(false);
}

float NarrationVoice::position() const {
	if (paused_) {
		return pausedPosition_;
	}
	return samplers_[0].meter_position();
}

bool NarrationVoice::finished() const {
	return playing_ && !paused_ && position() >= kEndPosition;
}

void NarrationVoice::setGlitchActive(bool active) {
	glitchActive_ = active;
	glitchLevel_.set(active ? volume_ : 0.0f);
}

void NarrationVoice::update(const NarrationSettings & settings, const ControlInputs & inputs) {
	volume_ = settings.volume;
	if (playing_ && !paused_) {
		playerVolume_.set(volume_);
	}
	if (glitchActive_) {
		glitchLevel_.set(volume_);
	}
	// The narration grain uses fixed values (its position comes from the glitch).
	const GrainSettings & g = settings.grain;
	length_.set(g.length.value);
	density_.set(g.density.value);
	distanceJitter_.set(g.distanceJitter.value);
	pitchJitter_.set(g.pitchJitter.value);
	pitch_.set(g.pitch.value);
	spread_.set(g.spread.value);
	direction_.set(g.direction.value);
	chain_.apply(settings.effects, inputs);
}

int NarrationVoice::glitchVoices() const {
	return cloud_ ? cloud_->getVoicesNum() : 0;
}

float NarrationVoice::glitchGrainPosition(int voice) const {
	return cloud_ ? cloud_->meter_position(voice) : 0.0f;
}

float NarrationVoice::glitchGrainEnvelope(int voice) const {
	return cloud_ ? cloud_->meter_env(voice) : 0.0f;
}

} // namespace gm
