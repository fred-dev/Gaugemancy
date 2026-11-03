#pragma once

// -----------------------------------------------------------------------------
// ButtonClickDetector: gets several actions out of the instrument's one button.
//
// Feed it the button state and the current time every frame; it reports:
//   Single / Double / Triple click  - presses separated by < clickTimeoutMs,
//                                     reported once the sequence has ended
//   LongPress                       - held >= 3 s, reported on release
//   VeryLongPress                   - held >= 6 s, reported on release
// -----------------------------------------------------------------------------

#include <cstdint>

namespace gm {

enum class ButtonEvent {
	None,
	SingleClick,
	DoubleClick,
	TripleClick,
	LongPress,
	VeryLongPress
};

const char * buttonEventName(ButtonEvent event);

class ButtonClickDetector {
public:
	static constexpr uint64_t kLongPressMs = 3000;
	static constexpr uint64_t kVeryLongPressMs = 6000;

	void setClickTimeout(uint64_t ms) { clickTimeoutMs_ = ms; }

	/// Call once per frame. Returns the event completed this frame, if any.
	ButtonEvent update(bool pressed, uint64_t nowMs);

	void reset();

private:
	uint64_t clickTimeoutMs_ = 240;
	bool wasPressed_ = false;
	int clicks_ = 0;
	uint64_t pressStartMs_ = 0;
	uint64_t lastReleaseMs_ = 0;
};

} // namespace gm
