#include "ButtonClickDetector.h"

namespace gm {

const char * buttonEventName(ButtonEvent event) {
	switch (event) {
	case ButtonEvent::SingleClick: return "single click";
	case ButtonEvent::DoubleClick: return "double click";
	case ButtonEvent::TripleClick: return "triple click";
	case ButtonEvent::LongPress: return "long press";
	case ButtonEvent::VeryLongPress: return "very long press";
	default: return "none";
	}
}

void ButtonClickDetector::reset() {
	wasPressed_ = false;
	clicks_ = 0;
	pressStartMs_ = 0;
	lastReleaseMs_ = 0;
}

ButtonEvent ButtonClickDetector::update(bool pressed, uint64_t nowMs) {
	const bool justPressed = pressed && !wasPressed_;
	const bool justReleased = !pressed && wasPressed_;
	wasPressed_ = pressed;

	if (justPressed) {
		pressStartMs_ = nowMs;
		return ButtonEvent::None;
	}

	if (justReleased) {
		const uint64_t held = nowMs - pressStartMs_;
		if (held >= kVeryLongPressMs) {
			clicks_ = 0;
			return ButtonEvent::VeryLongPress;
		}
		if (held >= kLongPressMs) {
			clicks_ = 0;
			return ButtonEvent::LongPress;
		}
		clicks_ = clicks_ < 3 ? clicks_ + 1 : 3;
		lastReleaseMs_ = nowMs;
		return ButtonEvent::None;
	}

	// The click sequence ends when the button stays up longer than the timeout.
	if (!pressed && clicks_ > 0 && nowMs - lastReleaseMs_ > clickTimeoutMs_) {
		int clicks = clicks_;
		clicks_ = 0;
		if (clicks == 1) return ButtonEvent::SingleClick;
		if (clicks == 2) return ButtonEvent::DoubleClick;
		return ButtonEvent::TripleClick;
	}
	return ButtonEvent::None;
}

} // namespace gm
