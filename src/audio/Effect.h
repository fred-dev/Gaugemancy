#pragma once

// -----------------------------------------------------------------------------
// Effect: base class for every stereo effect in a chain.
//
// Each effect has two stereo connection points, in(0/1) and out(0/1), which
// are pdsp::PatchNodes. An EffectChain simply wires out(n) of one effect to
// in(n) of the next, so effects can be put in any order (this replaces the
// old code that hand-patched every possible combination of six effects).
//
// Parameters: an effect gets one pdsp::ValueControl per parameter listed in
// its EffectCatalog entry. apply() resolves each parameter for the current
// control frame (fixed value or sensor mapped) and stores it in the control.
// ValueControl::set() is atomic, so this is safe while audio is running.
// Subclasses only patch their DSP units to those controls.
// -----------------------------------------------------------------------------

#include "../core/ControlInputs.h"
#include "../core/EffectCatalog.h"
#include "../core/Settings.h"

#include "ofxPDSP.h"

#include <array>
#include <memory>
#include <vector>

namespace gm {

class Effect {
public:
	explicit Effect(const EffectInfo & info);
	virtual ~Effect() = default;

	Effect(const Effect &) = delete;
	Effect & operator=(const Effect &) = delete;

	const EffectInfo & info() const { return info_; }

	/// Stereo input / output connection points (channel 0 = left, 1 = right).
	pdsp::PatchNode & in(int channel) { return inputs_[channel & 1]; }
	pdsp::PatchNode & out(int channel) { return outputs_[channel & 1]; }

	/// Resolve this frame's parameter values and hand them to the DSP.
	void apply(const EffectSettings & settings, const ControlInputs & inputs);

	/// Optional level meter for the GUI (e.g. compressor gain reduction in dB).
	virtual float meter() const { return 0.0f; }

protected:
	/// The control carrying parameter `index` (same order as the catalogue).
	pdsp::ValueControl & control(int index) { return *controls_[index]; }

	/// Called when a parameter value changes. The default stores it in
	/// control(index); override to convert values (see DryWetMix).
	virtual void setParameter(int index, float value) { control(index).set(value); }

private:
	const EffectInfo & info_;
	std::array<pdsp::PatchNode, 2> inputs_;
	std::array<pdsp::PatchNode, 2> outputs_;
	std::vector<std::unique_ptr<pdsp::ValueControl>> controls_;
	std::vector<float> lastValues_;
};

/// Helper for effects with a wet/dry "mix" parameter: wires
///   in -> dry amp -> out   and   wet signal -> wet amp -> out
/// and turns one mix value into the two gains.
class DryWetMix {
public:
	/// Patch dry paths in(ch) -> out(ch) and prepare the wet amps.
	void connectDry(int channel, pdsp::PatchNode & in, pdsp::PatchNode & out);
	/// The wet signal for `channel` must be patched into this amp.
	pdsp::Amp & wetInput(int channel) { return wet_[channel & 1]; }
	void connectWet(int channel, pdsp::PatchNode & out) { wet_[channel & 1] >> out; }
	void setMix(float mix);

private:
	std::array<pdsp::Amp, 2> dry_;
	std::array<pdsp::Amp, 2> wet_;
	pdsp::ValueControl dryLevel_;
	pdsp::ValueControl wetLevel_;
	bool controlsPatched_ = false;
};

/// Create the effect for a catalogue id, or nullptr for unknown types.
std::unique_ptr<Effect> createEffect(const std::string & type);

} // namespace gm
