// -----------------------------------------------------------------------------
// The concrete effects. Each class only:
//   1. creates its pdsp units (one per channel for mono units),
//   2. wires in(ch) -> units -> out(ch),
//   3. patches control(i) (catalogue parameter i) to the matching unit input.
// Parameter order matches the entries in core/EffectCatalog.cpp.
// -----------------------------------------------------------------------------

#include "Effect.h"

namespace gm {

namespace {

// ---------------------------------------------------------------- bit crusher
// params: 0 bits
class BitCrushEffect : public Effect {
public:
	explicit BitCrushEffect(const EffectInfo & info) : Effect(info) {
		for (int ch = 0; ch < 2; ++ch) {
			in(ch) >> crusher_[ch] >> out(ch);
			control(0) >> crusher_[ch].in_bits();
		}
	}

private:
	std::array<pdsp::Bitcruncher, 2> crusher_;
};

// ------------------------------------------------------------------ decimator
// params: 0 rate (Hz)
class DecimatorEffect : public Effect {
public:
	explicit DecimatorEffect(const EffectInfo & info) : Effect(info) {
		for (int ch = 0; ch < 2; ++ch) {
			in(ch) >> decimator_[ch] >> out(ch);
			control(0) >> decimator_[ch].in_freq();
		}
	}

private:
	std::array<pdsp::Decimator, 2> decimator_;
};

// --------------------------------------------------------------------- filter
// params: 0 cutoff (Hz), 1 resonance
class FilterEffect : public Effect {
public:
	explicit FilterEffect(const EffectInfo & info) : Effect(info) {
		for (int ch = 0; ch < 2; ++ch) {
			in(ch) >> filter_[ch].in_signal();
			filter_[ch].out_lpf4() >> out(ch);
			control(0) >> filter_[ch].in_freq();
			control(1) >> filter_[ch].in_reso();
		}
	}

private:
	std::array<pdsp::MultiLadder4, 2> filter_;
};

// --------------------------------------------------------------------- chorus
// params: 0 speed (Hz), 1 depth (ms), 2 delay (ms)
class ChorusEffect : public Effect {
public:
	explicit ChorusEffect(const EffectInfo & info) : Effect(info) {
		for (int ch = 0; ch < 2; ++ch) {
			in(ch) >> chorus_.ch(ch) >> out(ch);
		}
		control(0) >> chorus_.in_speed();
		control(1) >> chorus_.in_depth();
		control(2) >> chorus_.in_delay();
	}

private:
	pdsp::DimensionChorus chorus_;
};

// ---------------------------------------------------------------------- delay
// params: 0 mix, 1 time (ms), 2 feedback, 3 damping
class DelayEffect : public Effect {
public:
	explicit DelayEffect(const EffectInfo & info)
		: Effect(info) {
		for (int ch = 0; ch < 2; ++ch) {
			delay_[ch].setMaxTime(kMaxTimeMs);
			mix_.connectDry(ch, in(ch), out(ch));
			in(ch) >> delay_[ch] >> mix_.wetInput(ch);
			mix_.connectWet(ch, out(ch));
			control(1) >> delay_[ch].in_time();
			control(2) >> delay_[ch].in_feedback();
			control(3) >> delay_[ch].in_damping();
		}
		mix_.setMix(info.params[0].defaultValue);
	}

protected:
	void setParameter(int index, float value) override {
		if (index == 0) {
			mix_.setMix(value);
		} else {
			Effect::setParameter(index, value);
		}
	}

private:
	static constexpr float kMaxTimeMs = 2100.0f; // catalogue max is 2000 ms
	std::array<pdsp::Delay, 2> delay_;
	DryWetMix mix_;
};

// --------------------------------------------------------------------- reverb
// params: 0 mix, 1 time (s), 2 density, 3 damping, 4 hiCut (Hz),
//         5 modFreq (Hz), 6 modAmount (ms)
class ReverbEffect : public Effect {
public:
	explicit ReverbEffect(const EffectInfo & info) : Effect(info) {
		// BasiVerb has a mono input: feed it the average of left and right.
		monoSum_.set(0.5f);
		in(0) >> monoSum_;
		in(1) >> monoSum_;
		monoSum_ >> reverb_.in_signal();
		for (int ch = 0; ch < 2; ++ch) {
			mix_.connectDry(ch, in(ch), out(ch));
			reverb_.ch(ch) >> mix_.wetInput(ch);
			mix_.connectWet(ch, out(ch));
		}
		control(1) >> reverb_.in_time();
		control(2) >> reverb_.in_density();
		control(3) >> reverb_.in_damping();
		control(4) >> reverb_.in_hi_cut();
		control(5) >> reverb_.in_mod_freq();
		control(6) >> reverb_.in_mod_amount();
		mix_.setMix(info.params[0].defaultValue);
	}

protected:
	void setParameter(int index, float value) override {
		if (index == 0) {
			mix_.setMix(value);
		} else {
			Effect::setParameter(index, value);
		}
	}

private:
	pdsp::Amp monoSum_;
	pdsp::BasiVerb reverb_;
	DryWetMix mix_;
};

// ----------------------------------------------------------------- compressor
// params: 0 gain, 1 threshold (dB), 2 ratio, 3 attack (ms), 4 release (ms),
//         5 knee (dB). Gain is applied before compression, as before.
class CompressorEffect : public Effect {
public:
	explicit CompressorEffect(const EffectInfo & info) : Effect(info) {
		for (int ch = 0; ch < 2; ++ch) {
			in(ch) >> gain_[ch] >> compressor_.ch(ch) >> out(ch);
			control(0) >> gain_[ch].in_mod();
		}
		control(1) >> compressor_.in_threshold();
		control(2) >> compressor_.in_ratio();
		control(3) >> compressor_.in_attack();
		control(4) >> compressor_.in_release();
		control(5) >> compressor_.in_knee();
	}

	float meter() const override { return compressor_.meter_GR(); }

private:
	std::array<pdsp::Amp, 2> gain_;
	pdsp::Compressor compressor_;
};

// ----------------------------------------------------------------------- gain
// params: 0 gain
class GainEffect : public Effect {
public:
	explicit GainEffect(const EffectInfo & info) : Effect(info) {
		control(0).enableSmoothing(10.0f);
		for (int ch = 0; ch < 2; ++ch) {
			in(ch) >> amp_[ch] >> out(ch);
			control(0) >> amp_[ch].in_mod();
		}
	}

private:
	std::array<pdsp::Amp, 2> amp_;
};

} // namespace

// -------------------------------------------------------------------- factory

std::unique_ptr<Effect> createEffect(const std::string & type) {
	const EffectInfo * info = EffectCatalog::find(type);
	if (info == nullptr) return nullptr;
	if (type == "bitcrush") return std::make_unique<BitCrushEffect>(*info);
	if (type == "decimator") return std::make_unique<DecimatorEffect>(*info);
	if (type == "filter") return std::make_unique<FilterEffect>(*info);
	if (type == "chorus") return std::make_unique<ChorusEffect>(*info);
	if (type == "delay") return std::make_unique<DelayEffect>(*info);
	if (type == "reverb") return std::make_unique<ReverbEffect>(*info);
	if (type == "compressor") return std::make_unique<CompressorEffect>(*info);
	if (type == "gain") return std::make_unique<GainEffect>(*info);
	return nullptr;
}

} // namespace gm
