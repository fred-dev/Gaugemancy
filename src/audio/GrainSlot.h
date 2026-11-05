#pragma once

// -----------------------------------------------------------------------------
// GrainSlot: one voice of the instrument.
//
//   sample -> GrainCloud --------> cloudGain -----\
//   VAOscillator (4 waveforms) --> vaWaveGain[0-3] -\
//   (always     FMOperator ------> fmGain -----------+-> volume/gate amp -> EffectChain -> out(ch)
//    running)   LFO (4 waveforms)-> lfoWaveGain[0-3] -/
//               BitNoise ---------> noiseGain --------/
//
// Every source - and every waveform of the two multi-waveform oscillators -
// is always patched and always running; which one is actually heard is just
// which single gain is set to the resolved volume (every other one to 0) for
// SlotSettings::sourceType (and, for "va_osc"/"lfo_osc", the "waveform"
// param), each control frame. That makes switching source or waveform an
// ordinary atomic control change, not a structural rebuild - no patch lock
// needed, unlike voices/window/effects - and even lets a sensor sweep
// through waveforms live.
//
// Every grain and synth parameter has a pdsp::ValueControl patched to its
// module once; update() resolves the parameters from the settings and
// sensors each control frame and sets the controls (atomic, no repatching).
//
// Structural changes (sample, voice count, window, effect list) go through
// rebuild()/setSample(), which must be called while holding the engine's
// patch lock. Sample files are decoded beforehand with loadSampleFile() so the
// lock is only held for pointer swaps.
// -----------------------------------------------------------------------------

#include "EffectChain.h"

#include "../core/Settings.h"

#include "ofxPDSP.h"

#include <memory>
#include <string>

namespace gm {

/// pdsp window for a settings window type.
pdsp::Window_t toPdspWindow(WindowType type);

/// Per-frame values the app imposes on a slot (accumulated mode, mouse).
struct SlotOverrides {
	bool overridePosition = false;
	float position = 0.0f;
	bool overrideGate = false;
	bool gateOpen = false;
	bool overrideVolume = false; ///< auditioning: be heard even if volume is sensor-modulated
	float volume = 1.0f;
};

class GrainSlot {
public:
	GrainSlot();
	~GrainSlot();
	GrainSlot(const GrainSlot &) = delete;
	GrainSlot & operator=(const GrainSlot &) = delete;

	/// Decode an audio file (slow, call WITHOUT the patch lock).
	/// Returns nullptr and sets `error` when the file cannot be used.
	static std::unique_ptr<pdsp::SampleBuffer> loadSampleFile(const std::string & path, std::string & error);

	/// Swap in a decoded sample (call WITH the patch lock). Returns the old
	/// sample so it can be freed after the lock is released.
	std::unique_ptr<pdsp::SampleBuffer> setSample(std::unique_ptr<pdsp::SampleBuffer> sample, const std::string & name);

	/// Apply voices, window and effect chain (call WITH the patch lock).
	void rebuild(const SlotSettings & settings);

	/// True when rebuild() is needed for these settings.
	bool needsRebuild(const SlotSettings & settings) const;

	/// Resolve and send this frame's parameters.
	void update(const SlotSettings & settings, const ControlInputs & inputs, const SlotOverrides & overrides);

	/// Silence immediately (volume to 0).
	void mute();

	/// Stereo output of the slot (after its effect chain).
	pdsp::PatchNode & out(int channel) { return chain_.out(channel); }

	bool hasSample() const { return sample_ != nullptr; }
	const std::string & sampleName() const { return sampleName_; }

	// ---- for the GUI --------------------------------------------------------
	pdsp::SampleBuffer * sample() { return sample_.get(); }
	int voices() const;
	float grainPosition(int voice) const;  ///< 0..1 in the sample
	float grainEnvelope(int voice) const;  ///< 0..1
	float currentPosition() const { return lastPosition_; }
	float currentVolume() const { return lastVolume_; }
	bool gateOpen() const { return lastGate_; }
	float effectMeter(int effectIndex) const { return chain_.meter(effectIndex); }

	/// Pure helper: is the slot audible this frame? (exposed for tests)
	static bool gateFor(const SlotSettings & settings, const ControlInputs & inputs);

private:
	void createCloud(int voices);

	// Declared before the cloud so the cloud (which points at it) dies first.
	std::unique_ptr<pdsp::SampleBuffer> sample_;
	std::unique_ptr<pdsp::GrainCloud> cloud_;
	std::string sampleName_;
	std::array<pdsp::Amp, 2> amp_;
	EffectChain chain_;

	// One control per grain parameter except volume, which drives the amps.
	pdsp::ValueControl length_, density_, distanceJitter_, pitchJitter_;
	pdsp::ValueControl pitch_, spread_, position_, direction_, volume_;

	// The synth/noise sources, always running; see the class comment for how
	// only one "voice" gain is ever actually open. vaWaveGain_/lfoWaveGain_
	// hold one gain per waveform (so "waveform" can be swept by a sensor
	// with no repatching) - index order matches SourceCatalog's "waveform"
	// param comment (0=saw/1=pulse/2=tri/3=sine for VA, 0=tri/1=sine/2=saw/
	// 3=square for the LFO).
	pdsp::VAOscillator vaOsc_;
	pdsp::FMOperator fmOp_;
	pdsp::LFO lfoOsc_;
	pdsp::BitNoise noise_;
	std::array<pdsp::Amp, 2> cloudGain_, fmGain_, noiseGain_;
	std::array<std::array<pdsp::Amp, 2>, 4> vaWaveGain_, lfoWaveGain_;
	pdsp::ValueControl cloudGainCtrl_, fmGainCtrl_, noiseGainCtrl_;
	std::array<pdsp::ValueControl, 4> vaWaveGainCtrl_, lfoWaveGainCtrl_;
	pdsp::ValueControl vaPitch_, vaPulseWidth_;
	pdsp::ValueControl fmPitch_, fmRatio_, fmAmount_, fmFeedback_;
	pdsp::ValueControl lfoFreq_;
	pdsp::ValueControl noisePitch_, noiseDecimation_, noiseBits_;

	int builtVoices_ = 0;
	WindowType builtWindow_ = WindowType::Count;
	float lastPosition_ = 0.0f;
	float lastVolume_ = 0.0f;
	bool lastGate_ = false;
};

} // namespace gm
