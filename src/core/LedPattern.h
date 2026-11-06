#pragma once

// -----------------------------------------------------------------------------
// LedPattern: what the two status LEDs show, so performers and technicians can
// read the instrument state without a screen.
//
//   narration waiting/playing   red, steady
//   error (no audio / no ADC)   red, fast blink
//   a preset                    one of three steady colours - blue, purple
//                                (both), or red - picked by the preset's own
//                                number and held with no flashing and no
//                                further change until the preset changes.
//                                (A separate one-off blink-through-all-three
//                                happens once at boot, in ControlPanel's
//                                playStartupSequenceBlocking() - not here;
//                                this struct only ever describes the steady
//                                state a preset settles into.)
// -----------------------------------------------------------------------------

#include <cstdint>

namespace gm {

struct LedPattern {
	bool blue = false;
	bool red = false;
	int blinkMs = 0; ///< 0 = steady, otherwise on/off period (narration/error only)

	/// A steady, non-flashing colour for a 0-based preset index: blue, then
	/// purple (both), then red, then repeats. Two LEDs can only tell three
	/// colours apart on their own - a 4th preset reuses the 1st's colour
	/// rather than inventing a blink or a fade to fake a 4th one.
	static LedPattern forPreset(int presetIndex) {
		int i = presetIndex < 0 ? 0 : presetIndex % 3;
		LedPattern p;
		p.blue = (i == 0 || i == 1);
		p.red = (i == 1 || i == 2);
		return p;
	}

	static LedPattern narration() { return { false, true, 0 }; }
	static LedPattern error() { return { false, true, 120 }; }
	static LedPattern off() { return {}; }

	/// The pattern resolved for the current time: steady, or blinking if
	/// blinkMs is set (narration/error only - a preset's blinkMs is always 0).
	LedPattern at(uint64_t nowMs) const {
		if (blinkMs <= 0 || (nowMs / static_cast<uint64_t>(blinkMs)) % 2 == 0) {
			return { blue, red, 0 };
		}
		return {};
	}
};

} // namespace gm
