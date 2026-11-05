#pragma once

// -----------------------------------------------------------------------------
// EffectChain: an ordered list of effects between a fixed stereo input and a
// fixed stereo output.
//
//   in(ch) -> effect 1 -> effect 2 -> ... -> out(ch)        (empty: in -> out)
//
// Whoever owns the chain patches into in() and out of out() once. Rebuilding
// the chain (effects added, removed, reordered, enabled/disabled) only rewires
// what is between them. Rebuild while holding engine.lockPatching().
//
// Disabled effects are not created at all, so they cost no CPU.
// -----------------------------------------------------------------------------

#include "Effect.h"

#include <memory>
#include <string>
#include <vector>

namespace gm {

class EffectChain {
public:
	EffectChain() = default;
	EffectChain(const EffectChain &) = delete;
	EffectChain & operator=(const EffectChain &) = delete;

	pdsp::PatchNode & in(int channel) { return in_[channel & 1]; }
	pdsp::PatchNode & out(int channel) { return out_[channel & 1]; }

	/// Recreate the effects to match `settings`. Call under the patch lock.
	void build(const EffectChainSettings & settings);

	/// Push this frame's parameter values to every effect (thread safe).
	/// Safe to call with settings that no longer match the built chain: in
	/// that case nothing is applied until build() is called again.
	void apply(const EffectChainSettings & settings, const ControlInputs & inputs);

	/// True when `settings` would build the same effects as the current chain.
	bool matches(const EffectChainSettings & settings) const;

	/// Meter of the effect built from settings entry `settingsIndex` (0 if none).
	float meter(int settingsIndex) const;

	size_t activeEffects() const { return effects_.size(); }

private:
	struct Slot {
		int settingsIndex;               ///< position in the EffectChainSettings
		std::unique_ptr<Effect> effect;
	};

	void disconnectEnds();

	std::array<pdsp::PatchNode, 2> in_;
	std::array<pdsp::PatchNode, 2> out_;
	std::vector<Slot> effects_;
	std::vector<std::string> signature_; ///< type of each settings entry ("" = disabled)
	bool built_ = false; ///< false until build() has run at least once (in_/out_ not yet wired)
};

} // namespace gm
