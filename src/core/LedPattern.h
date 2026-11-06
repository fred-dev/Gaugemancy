#pragma once

// -----------------------------------------------------------------------------
// LedPattern: what the two status LEDs show, so performers and technicians can
// read the instrument state without a screen.
//
//   narration waiting/playing   red, steady
//   error (no audio / no ADC)   red, fast blink
//   a preset                    a unique, always-lit blue/red colour mix that
//                                slowly cycles blue -> purple (both) -> red and
//                                back. It never goes dark, so it reads as "this
//                                preset's colour slowly shifting," not a
//                                warning blink - see forPreset()'s own comment
//                                for how each preset's mix is chosen.
// -----------------------------------------------------------------------------

#include <cmath>
#include <cstdint>

namespace gm {

struct LedPattern {
	bool blue = false;
	bool red = false;
	int blinkMs = 0; ///< 0 = steady, otherwise on/off period (narration/error only)

	// A preset's colour-mix cycle: how long (ms) each of the three always-lit
	// states (blue / both=purple / red) shows before moving to the next, one
	// full rotation repeating forever. All zero = not used - narration/error
	// use blinkMs instead (see at()).
	int blueMs = 0;
	int bothMs = 0;
	int redMs = 0;

	/// A unique, non-flashing colour-mix cycle for a 0-based preset index.
	/// Spreads presets around a colour wheel by the golden angle - the usual
	/// trick for handing out N well separated categorical colours one at a
	/// time with no fixed N decided up front - then maps that single hue
	/// onto this two-LED hardware as a three-way blue/purple/red duty-cycle
	/// mix (three phasors 120 degrees apart always sum to zero, so the three
	/// mix weights always sum to exactly 3 - a free normalisation). Preset 0
	/// lands mostly on blue, the next preset falls far around the wheel so
	/// it reads as mostly red, and so on. A floor keeps every phase at least
	/// 15% of the cycle so the mix never collapses to what looks like one
	/// flat colour.
	static LedPattern forPreset(int presetIndex) {
		constexpr double kGoldenAngle = 2.39996322972865332; // radians, ~137.5 degrees
		constexpr double kThird = 2.0943951023931953;        // 120 degrees, in radians
		constexpr double kFloor = 0.15;
		constexpr int kTotalMs = 3000;

		double angle = (presetIndex < 0 ? 0 : presetIndex) * kGoldenAngle;
		double wBlue = 1.0 + std::cos(angle);
		double wBoth = 1.0 + std::cos(angle + kThird);
		auto withFloor = [](double w) { return kFloor + (1.0 - 3.0 * kFloor) * (w / 3.0); };

		LedPattern p;
		p.blueMs = static_cast<int>(std::lround(kTotalMs * withFloor(wBlue)));
		p.bothMs = static_cast<int>(std::lround(kTotalMs * withFloor(wBoth)));
		p.redMs = kTotalMs - p.blueMs - p.bothMs; // exact remainder, no rounding drift
		return p;
	}

	static LedPattern narration() { return { false, true, 0 }; }
	static LedPattern error() { return { false, true, 120 }; }
	static LedPattern off() { return {}; }

	/// The pattern resolved for the current time: walks the preset colour
	/// cycle if one is set, otherwise applies the narration/error on-off
	/// blink (or steady, if blinkMs is 0).
	LedPattern at(uint64_t nowMs) const {
		int total = blueMs + bothMs + redMs;
		if (total > 0) {
			int t = static_cast<int>(nowMs % static_cast<uint64_t>(total));
			if (t < blueMs) return { true, false, 0 };
			if (t < blueMs + bothMs) return { true, true, 0 };
			return { false, true, 0 };
		}
		if (blinkMs <= 0 || (nowMs / static_cast<uint64_t>(blinkMs)) % 2 == 0) {
			return { blue, red, 0 };
		}
		return {};
	}
};

} // namespace gm
