#include "Effect.h"

namespace gm {

Effect::Effect(const EffectInfo & info) : info_(info) {
	for (const ParamSpec & spec : info.params) {
		auto ctrl = std::make_unique<pdsp::ValueControl>();
		ctrl->set(spec.defaultValue);
		controls_.push_back(std::move(ctrl));
		lastValues_.push_back(spec.defaultValue);
	}
}

void Effect::apply(const EffectSettings & settings, const ControlInputs & inputs) {
	const size_t count = std::min(settings.params.size(), controls_.size());
	for (size_t i = 0; i < count; ++i) {
		float value = settings.params[i].resolve(inputs);
		if (value != lastValues_[i]) {
			lastValues_[i] = value;
			setParameter(static_cast<int>(i), value);
		}
	}
}

// ------------------------------------------------------------------ DryWetMix

void DryWetMix::connectDry(int channel, pdsp::PatchNode & in, pdsp::PatchNode & out) {
	int ch = channel & 1;
	if (!controlsPatched_) {
		// 10 ms smoothing avoids clicks when the mix is moved by a sensor.
		dryLevel_.enableSmoothing(10.0f);
		wetLevel_.enableSmoothing(10.0f);
		controlsPatched_ = true;
	}
	dryLevel_ >> dry_[ch].in_mod();
	wetLevel_ >> wet_[ch].in_mod();
	in >> dry_[ch] >> out;
}

void DryWetMix::setMix(float mix) {
	// Equal-gain crossfade: keeps the dry level when the mix is at 0.
	dryLevel_.set(1.0f - mix);
	wetLevel_.set(mix);
}

} // namespace gm
