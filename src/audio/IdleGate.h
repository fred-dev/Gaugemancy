#pragma once

// -----------------------------------------------------------------------------
// IdleGate: a stereo switch placed after an effect chain so the chain costs no
// CPU while it has nothing to play.
//
//   chain out(ch) -> in(ch) [amp] -> out(ch)
//
// pdsp only processes what an output asks for, and an Amp whose gain is a
// steady 0 does not ask for its input. So closing the gate stops the whole
// chain behind it. The gate only closes once the chain's tail has died away
// (see core/IdleDetector.h), so the effects hold nothing but silence when
// they stop, and nothing stale is heard when they start again.
//
// update() runs on the control thread; it only reads an atomic meter and sets
// an atomic control, so it needs no patch lock.
// -----------------------------------------------------------------------------

#include "../core/IdleDetector.h"

#include "ofxPDSP.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace gm {

class IdleGate {
public:
	IdleGate() {
		open_.set(0.0f);
		for (int ch = 0; ch < 2; ++ch) {
			open_ >> amp_[ch].in_mod();
			amp_[ch] >> out_[ch];
		}
	}
	IdleGate(const IdleGate &) = delete;
	IdleGate & operator=(const IdleGate &) = delete;

	pdsp::Amp & in(int channel) { return amp_[channel & 1]; }
	pdsp::PatchNode & out(int channel) { return out_[channel & 1]; }

	/// Once per control frame. Returns true while the chain is running.
	bool update(bool inputActive, float dtSeconds) {
		float level = std::max(std::fabs(amp_[0].meter_output()), std::fabs(amp_[1].meter_output()));
		bool running = detector_.update(inputActive, level, dtSeconds);
		open_.set(running ? 1.0f : 0.0f);
		return running;
	}

	bool running() const { return detector_.running(); }

private:
	std::array<pdsp::Amp, 2> amp_;
	std::array<pdsp::PatchNode, 2> out_;
	pdsp::ValueControl open_; // not smoothed: it only changes during silence
	IdleDetector detector_;
};

} // namespace gm
