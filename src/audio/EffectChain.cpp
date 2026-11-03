#include "EffectChain.h"

namespace gm {

namespace {
// What a settings list would build: one entry per effect, "" when disabled.
std::vector<std::string> signatureOf(const EffectChainSettings & settings) {
	std::vector<std::string> signature;
	signature.reserve(settings.size());
	for (const EffectSettings & effect : settings) {
		signature.push_back(effect.enabled ? effect.type : std::string());
	}
	return signature;
}
} // namespace

void EffectChain::disconnectEnds() {
	for (int ch = 0; ch < 2; ++ch) {
		in_[ch].disconnectOut();
		out_[ch].disconnectIn();
	}
}

void EffectChain::build(const EffectChainSettings & settings) {
	// Drop the old effects first; their destructors disconnect them.
	disconnectEnds();
	effects_.clear();

	for (size_t i = 0; i < settings.size(); ++i) {
		if (!settings[i].enabled) continue;
		std::unique_ptr<Effect> effect = createEffect(settings[i].type);
		if (effect) {
			effects_.push_back({ static_cast<int>(i), std::move(effect) });
		}
	}

	// Wire in -> e1 -> e2 -> ... -> out for both channels.
	for (int ch = 0; ch < 2; ++ch) {
		pdsp::PatchNode * previous = &in_[ch];
		for (Slot & slot : effects_) {
			*previous >> slot.effect->in(ch);
			previous = &slot.effect->out(ch);
		}
		*previous >> out_[ch];
	}
	signature_ = signatureOf(settings);
}

bool EffectChain::matches(const EffectChainSettings & settings) const {
	return signatureOf(settings) == signature_;
}

void EffectChain::apply(const EffectChainSettings & settings, const ControlInputs & inputs) {
	if (settings.size() != signature_.size()) {
		return; // structure changed: wait for build()
	}
	for (Slot & slot : effects_) {
		const EffectSettings & effectSettings = settings[slot.settingsIndex];
		if (effectSettings.type == slot.effect->info().id) {
			slot.effect->apply(effectSettings, inputs);
		}
	}
}

float EffectChain::meter(int settingsIndex) const {
	for (const Slot & slot : effects_) {
		if (slot.settingsIndex == settingsIndex) {
			return slot.effect->meter();
		}
	}
	return 0.0f;
}

} // namespace gm
