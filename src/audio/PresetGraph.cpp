#include "PresetGraph.h"

namespace gm {

namespace {

constexpr float kFadeSmoothingMs = 20.0f; // hides the control-rate steps of a fade

/// A synth source always has something to play; granular still needs a
/// loaded sample.
bool isPlayable(const SlotSettings & settings, const GrainSlot & slot) {
	return settings.enabled && (settings.sourceType != "granular" || slot.hasSample());
}

} // namespace

PresetGraph::PresetGraph() {
	for (auto & slot : slots_) {
		slot = std::make_unique<GrainSlot>();
	}
	fade_.enableSmoothing(kFadeSmoothingMs);
	fade_.set(1.0f);
	for (int ch = 0; ch < 2; ++ch) {
		master_.out(ch) >> masterIdle_.in(ch);
		masterIdle_.out(ch) >> fadeAmp_[ch];
		fade_ >> fadeAmp_[ch].in_mod();
		fadeAmp_[ch] >> out_[ch];
	}
	master_.build({});
}

std::vector<std::string> PresetGraph::build(const PresetSettings & preset, const PathResolver & resolve) {
	std::vector<std::string> problems;
	for (int i = 0; i < kMaxSlots; ++i) {
		const SlotSettings & s = preset.slots[i];
		std::unique_ptr<pdsp::SampleBuffer> decoded;
		if (s.enabled && !s.file.empty()) {
			std::string error;
			decoded = GrainSlot::loadSampleFile(resolve(s.file), error);
			if (!decoded) {
				problems.push_back("slot " + std::to_string(i + 1) + ": " + error);
			}
		}
		slots_[i]->mute();
		slots_[i]->setSample(std::move(decoded), s.file);
		slots_[i]->rebuild(s);
	}
	master_.build(preset.masterEffects);
	connectSlots(preset);
	settings_ = preset;
	return problems;
}

void PresetGraph::connectSlots(const PresetSettings & preset) {
	for (int i = 0; i < kMaxSlots; ++i) {
		bool playable = isPlayable(preset.slots[i], *slots_[i]);
		if (playable == slotConnected_[i]) continue;
		for (int ch = 0; ch < 2; ++ch) {
			if (playable) {
				slots_[i]->out(ch) >> master_.in(ch);
			} else {
				slots_[i]->out(ch).disconnectOut();
			}
		}
		slotConnected_[i] = playable;
	}
}

bool PresetGraph::needsSync(const PresetSettings & preset) const {
	for (int i = 0; i < kMaxSlots; ++i) {
		bool playable = isPlayable(preset.slots[i], *slots_[i]);
		if (slots_[i]->needsRebuild(preset.slots[i]) || playable != slotConnected_[i]) {
			return true;
		}
	}
	return !master_.matches(preset.masterEffects);
}

void PresetGraph::syncStructure(const PresetSettings & preset) {
	for (int i = 0; i < kMaxSlots; ++i) {
		if (slots_[i]->needsRebuild(preset.slots[i])) slots_[i]->rebuild(preset.slots[i]);
	}
	if (!master_.matches(preset.masterEffects)) master_.build(preset.masterEffects);
	connectSlots(preset);
	settings_ = preset;
}

void PresetGraph::update(const PresetSettings & preset, const ControlInputs & inputs,
	const std::array<SlotOverrides, kMaxSlots> & overrides, float dtSeconds) {
	bool anySlotRunning = false;
	for (int i = 0; i < kMaxSlots; ++i) {
		if (slotConnected_[i]) {
			slots_[i]->update(preset.slots[i], inputs, overrides[i], dtSeconds);
			anySlotRunning |= slots_[i]->chainRunning();
		} else {
			slots_[i]->updateIdle(dtSeconds);
		}
	}
	masterIdle_.update(anySlotRunning, dtSeconds);
	master_.apply(preset.masterEffects, inputs);
}

void PresetGraph::mute() {
	for (auto & slot : slots_) {
		slot->mute();
	}
}

void PresetGraph::connectScopes(pdsp::Patchable & blackhole) {
	for (auto & slot : slots_) {
		// The scope's own output is deliberately silent; patching it into
		// the blackhole is just what makes pdsp actually process it.
		slot->scopeTap() >> blackhole;
	}
}

void PresetGraph::disconnectScopes() {
	for (auto & slot : slots_) {
		slot->scopeTap().disconnectOut();
	}
}

} // namespace gm
