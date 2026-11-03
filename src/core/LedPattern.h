#pragma once

// -----------------------------------------------------------------------------
// LedPattern: what the two status LEDs show, so performers and technicians can
// read the instrument state without a screen.
//
//   narration waiting/playing  red
//   preset 1                   blue
//   preset 2                   blue + red
//   preset 3                   blue, blinking
//   preset 4                   blue + red, blinking
//   (presets 5+ repeat the cycle)
//   error (no audio / no ADC)  red, fast blink
// -----------------------------------------------------------------------------

#include <cstdint>

namespace gm {

struct LedPattern {
	bool blue = false;
	bool red = false;
	int blinkMs = 0; ///< 0 = steady, otherwise on/off period

	/// Pattern for a 0-based preset index.
	static LedPattern forPreset(int presetIndex) {
		int i = presetIndex < 0 ? 0 : presetIndex % 4;
		LedPattern p;
		p.blue = true;
		p.red = (i == 1 || i == 3);
		p.blinkMs = i >= 2 ? 500 : 0;
		return p;
	}

	static LedPattern narration() { return { false, true, 0 }; }
	static LedPattern error() { return { false, true, 120 }; }
	static LedPattern off() { return {}; }

	/// The pattern resolved for the current time (applies blinking).
	LedPattern at(uint64_t nowMs) const {
		if (blinkMs <= 0 || (nowMs / static_cast<uint64_t>(blinkMs)) % 2 == 0) {
			return { blue, red, 0 };
		}
		return {};
	}
};

} // namespace gm
