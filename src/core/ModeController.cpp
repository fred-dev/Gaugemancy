#include "ModeController.h"

#include <algorithm>

namespace gm {

const char * modeName(Mode mode) {
	switch (mode) {
	case Mode::WaitForNarration: return "wait for narration";
	case Mode::PlayNarration: return "play narration";
	case Mode::NarrationGlitch: return "narration glitch";
	case Mode::Granular: return "granular";
	}
	return "unknown";
}

float ModeController::computeGlitchPosition(float pausedPosition, float amount, float strand) {
	// A light squeeze reaches back by `strand`; a full squeeze sits on the
	// paused position itself.
	float a = std::clamp(amount, 0.0f, 1.0f);
	return std::clamp(pausedPosition - strand * (1.0f - a), 0.0f, 1.0f);
}

void ModeController::enter(Mode mode) {
	mode_ = mode;
	modeStartMs_ = nowMs_;
	actions_.modeChanged(mode);
}

int ModeController::firstSensorAbove(const ControlInputs & inputs, float threshold) const {
	for (int i = 0; i < kNumSensors; ++i) {
		if (inputs.sensor(i) > threshold) {
			return i;
		}
	}
	return -1;
}

void ModeController::start(const NarrationSettings & settings, bool narrationLoaded, uint64_t nowMs) {
	settings_ = settings;
	nowMs_ = nowMs;
	enter(settings.enabled && narrationLoaded ? Mode::WaitForNarration : Mode::Granular);
}

void ModeController::skipNarration() {
	if (mode_ == Mode::Granular) {
		return;
	}
	actions_.setNarrationGlitch(false);
	actions_.stopNarration();
	enter(Mode::Granular);
}

void ModeController::setManualGlitch(bool active, float amount) {
	manualGlitch_ = active;
	manualAmount_ = amount;
}

void ModeController::update(const ControlInputs & inputs, const NarrationProgress & progress, uint64_t nowMs) {
	nowMs_ = nowMs;
	const float threshold = settings_.glitchThreshold;

	switch (mode_) {
	case Mode::WaitForNarration: {
		bool triggered = settings_.startWithSensor
			? firstSensorAbove(inputs, threshold) >= 0
			: nowMs - modeStartMs_ >= static_cast<uint64_t>(settings_.autoStartDelayMs);
		if (triggered || manualGlitch_) {
			actions_.playNarration();
			glitchArmed_ = false; // the starting squeeze must be released first
			enter(Mode::PlayNarration);
		}
		break;
	}

	case Mode::PlayNarration: {
		if (progress.finished) {
			actions_.stopNarration();
			enter(Mode::Granular);
			break;
		}
		int sensor = manualGlitch_ ? 0 : firstSensorAbove(inputs, threshold);
		if (sensor < 0) {
			glitchArmed_ = true;
		}
		if (sensor >= 0 && (glitchArmed_ || manualGlitch_)) {
			glitchSensor_ = sensor;
			pausedPosition_ = progress.position;
			actions_.pauseNarration();
			actions_.setNarrationGlitch(true);
			enter(Mode::NarrationGlitch);
		}
		break;
	}

	case Mode::NarrationGlitch: {
		float amount = manualGlitch_ ? manualAmount_ : inputs.sensor(std::max(glitchSensor_, 0));
		glitchPosition_ = computeGlitchPosition(pausedPosition_, amount, settings_.glitchStrand);
		bool released = manualGlitch_ ? false : amount < threshold;
		if (released || (!manualGlitch_ && glitchSensor_ < 0)) {
			glitchSensor_ = -1;
			actions_.setNarrationGlitch(false);
			actions_.playNarration();
			enter(Mode::PlayNarration);
		}
		break;
	}

	case Mode::Granular:
		break;
	}
}

} // namespace gm
