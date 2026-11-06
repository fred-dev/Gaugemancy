#pragma once

// -----------------------------------------------------------------------------
// ControlPanel: the instrument's physical controls on the Raspberry Pi:
// one push button, a blue and a red status LED, and the relay that switches
// the speaker (a pulse toggles it).
//
// Nothing here blocks except pulseRelayBlocking() and
// playStartupSequenceBlocking() (startup/shutdown only): relay pulses from
// the main loop and LED blinking are timed in update(). On a laptop open()
// fails cleanly and every method does nothing.
// -----------------------------------------------------------------------------

#include "../core/LedPattern.h"
#include "../core/Settings.h"

#include <cstdint>
#include <memory>
#include <string>

namespace gm {

class ControlPanel {
public:
	ControlPanel();
	~ControlPanel();

	/// Request the GPIO lines. Returns false (with reasons) if any is missing;
	/// the parts that did open still work.
	bool open(const HardwareSettings & settings, std::string & report);
	void close();

	bool isOpen() const;

	/// Logical button state (true = pressed).
	bool buttonPressed() const;

	/// Pattern shown on the LEDs from now on.
	void setLeds(const LedPattern & pattern) { pattern_ = pattern; }

	/// A one-off blink through blue -> purple -> red -> off at startup, so
	/// a technician can see both LEDs actually work before the instrument
	/// settles into its steady per-preset colour (set separately via
	/// setLeds(), right after this returns). Never repeats.
	void playStartupSequenceBlocking();

	/// Start a relay pulse (toggles the speaker power).
	void pulseRelay(uint64_t nowMs);

	/// Toggle the speaker power and block until the pulse finishes. Only for
	/// the one-shot startup/shutdown calls (turn the speaker on when the
	/// instrument starts, off when it stops), never from the main loop.
	void pulseRelayBlocking(int pulseMs);

	/// Drive LED blinking and finish relay pulses. Call every frame.
	void update(uint64_t nowMs);

	/// LEDs off, relay released.
	void allOff();

private:
	struct Pins;
	std::unique_ptr<Pins> pins_;
	HardwareSettings settings_;
	LedPattern pattern_;
	uint64_t relayOffAtMs_ = 0;
	bool relayActive_ = false;
	int blueState_ = -1; ///< last value written (-1 = unknown), to skip redundant writes
	int redState_ = -1;
};

} // namespace gm
