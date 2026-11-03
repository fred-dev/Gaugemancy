#pragma once

// -----------------------------------------------------------------------------
// NarrationVoice: plays the narration file, and turns it into a granular
// "glitch" when a performer squeezes a ball during the narration.
//
//   player (sampler L/R, fade envelope) ─┐
//                                        ├─> EffectChain -> out(ch)
//   glitch GrainCloud on the same file ──┘
//
// pause() really pauses: it remembers the position, and resume() restarts the
// sampler from there. (The old player only closed a gate while playback ran on
// silently, so the glitch drifted away from where the narration stopped.)
// -----------------------------------------------------------------------------

#include "EffectChain.h"

#include "../core/Settings.h"

#include "ofxPDSP.h"

#include <memory>
#include <string>

namespace gm {

class NarrationVoice {
public:
	NarrationVoice();
	~NarrationVoice();
	NarrationVoice(const NarrationVoice &) = delete;
	NarrationVoice & operator=(const NarrationVoice &) = delete;

	/// Decode the file (slow, call WITHOUT the patch lock).
	static std::unique_ptr<pdsp::SampleBuffer> loadFile(const std::string & path, std::string & error);

	/// Install a decoded file and rebuild for `settings` (call WITH the lock).
	/// Returns the previous buffer so it can be freed after unlocking.
	std::unique_ptr<pdsp::SampleBuffer> setFile(std::unique_ptr<pdsp::SampleBuffer> sample);
	void rebuild(const NarrationSettings & settings);
	bool needsRebuild(const NarrationSettings & settings) const;

	bool isLoaded() const { return sample_ != nullptr; }

	// ---- transport -----------------------------------------------------------
	void play();            ///< from the start, or resume if paused
	void pause();
	void stop();
	bool isPlaying() const { return playing_; }
	bool isPaused() const { return paused_; }

	/// 0..1 through the file (the paused position while paused).
	float position() const;

	/// True once playback has reached the end.
	bool finished() const;

	// ---- glitch --------------------------------------------------------------
	void setGlitchActive(bool active);
	void setGlitchPosition(float position) { glitchPosition_.set(position); }

	/// Per-frame parameter update (volume, grain settings, effects).
	void update(const NarrationSettings & settings, const ControlInputs & inputs);

	pdsp::PatchNode & out(int channel) { return chain_.out(channel); }

	// ---- for the GUI ---------------------------------------------------------
	pdsp::SampleBuffer * sample() { return sample_.get(); }
	int glitchVoices() const;
	float glitchGrainPosition(int voice) const;
	float glitchGrainEnvelope(int voice) const;

private:
	void connectSamplers();
	void startFrom(float position);

	std::unique_ptr<pdsp::SampleBuffer> sample_;
	std::unique_ptr<pdsp::GrainCloud> cloud_;

	// Player: sampler per channel, faded by an envelope so pauses do not click.
	std::array<pdsp::Sampler, 2> samplers_;
	pdsp::TriggerControl samplerTrigger_;
	pdsp::ValueControl startPosition_;
	pdsp::ADSR fade_;
	pdsp::TriggerControl fadeGate_;
	std::array<pdsp::Amp, 2> playerAmp_;       ///< fade envelope
	std::array<pdsp::Amp, 2> playerVolumeAmp_; ///< narration volume
	pdsp::ValueControl playerVolume_;

	// Glitch cloud
	std::array<pdsp::Amp, 2> glitchAmp_;
	pdsp::ValueControl glitchLevel_;
	pdsp::ValueControl glitchPosition_;
	pdsp::ValueControl length_, density_, distanceJitter_, pitchJitter_, pitch_, spread_, direction_;

	EffectChain chain_;

	bool playing_ = false;
	bool paused_ = false;
	bool glitchActive_ = false;
	float pausedPosition_ = 0.0f;
	WindowType builtWindow_ = WindowType::Count;
	float volume_ = 1.0f;
};

} // namespace gm
