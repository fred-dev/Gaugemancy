#pragma once

// -----------------------------------------------------------------------------
// PresetGraph: everything in the sound graph that belongs to ONE preset.
//
//   slot 1..6 -> master EffectChain -> idle gate -> fade gain -> out(ch)
//
// The AudioEngine holds two of these while it crossfades from one preset to
// the next: the outgoing one fades to 0, the incoming one fades up.
//
// Threading rules:
//   * build() needs NO patch lock as long as out(ch) is not connected to
//     anything: pdsp only processes units reachable from the audio output,
//     and units created while audio runs prepare themselves.
//   * Once out(ch) is connected, structural calls (syncStructure, setSample
//     on a slot, connectScopes, disconnectScopes) need the patch lock.
//   * update() and setFade() only set atomic controls: no lock.
//   * Destroy a graph only after it has been disconnected under the lock.
// -----------------------------------------------------------------------------

#include "EffectChain.h"
#include "GrainSlot.h"
#include "IdleGate.h"

#include "../core/Settings.h"

#include "ofxPDSP.h"

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace gm {

/// Turns a file name from the settings into a full path.
using PathResolver = std::function<std::string(const std::string &)>;

class PresetGraph {
public:
	PresetGraph();
	PresetGraph(const PresetGraph &) = delete;
	PresetGraph & operator=(const PresetGraph &) = delete;

	/// Decode the samples, create the effects and wire everything for
	/// `preset`. Slow. Returns one line per problem.
	std::vector<std::string> build(const PresetSettings & preset, const PathResolver & resolve);

	/// True when the graph's structure no longer matches `preset`.
	bool needsSync(const PresetSettings & preset) const;

	/// Rebuild whatever no longer matches (effect added, voice count...).
	void syncStructure(const PresetSettings & preset);

	/// Per control frame: resolve and send every parameter.
	void update(const PresetSettings & preset, const ControlInputs & inputs,
		const std::array<SlotOverrides, kMaxSlots> & overrides, float dtSeconds);

	/// The same, with the settings this graph was last built or synced with.
	/// For the outgoing graph, whose preset is no longer the current one.
	void updateWithBuiltSettings(const ControlInputs & inputs, const std::array<SlotOverrides, kMaxSlots> & overrides,
		float dtSeconds) {
		update(settings_, inputs, overrides, dtSeconds);
	}

	/// Output level of the whole graph, 0..1 (lightly smoothed against clicks).
	void setFade(float gain) { fade_.set(gain); }

	void mute();

	pdsp::PatchNode & out(int channel) { return out_[channel & 1]; }
	GrainSlot & slot(int index) { return *slots_[index]; }
	EffectChain & master() { return master_; }

	/// Feed / stop feeding the slots' live scopes (GUI only).
	void connectScopes(pdsp::Patchable & blackhole);
	void disconnectScopes();

private:
	void connectSlots(const PresetSettings & preset);

	std::array<std::unique_ptr<GrainSlot>, kMaxSlots> slots_;
	std::array<bool, kMaxSlots> slotConnected_ {};
	EffectChain master_;
	IdleGate masterIdle_;
	std::array<pdsp::Amp, 2> fadeAmp_;
	pdsp::ValueControl fade_;
	std::array<pdsp::PatchNode, 2> out_;
	PresetSettings settings_; ///< what the graph was last built or synced with
};

} // namespace gm
