#pragma once

// -----------------------------------------------------------------------------
// ModeController: the instrument's top level state machine.
//
//              (narration enabled)
//   start ──► WaitForNarration ──sensor/timer──► PlayNarration ──finished──► Granular
//                                                  │      ▲
//                                    sensor pressed│      │sensor released
//                                                  ▼      │
//                                               NarrationGlitch
//
//   start ──► Granular                     (no narration)
//
// WaitForNarration  nothing plays; waits for a squeeze (or a timer).
// PlayNarration     the narration file plays.
// NarrationGlitch   narration is paused; squeezing a ball scrubs a granular
//                   cloud just behind the paused position.
// Granular          the normal instrument: grain slots driven by sensors.
//
// The controller only decides; it talks to the audio through ModeActions, so
// it can be tested without any audio code.
// -----------------------------------------------------------------------------

#include "ControlInputs.h"
#include "Settings.h"

#include <cstdint>
#include <string>

namespace gm {

enum class Mode {
	WaitForNarration,
	PlayNarration,
	NarrationGlitch,
	Granular
};

const char * modeName(Mode mode);

/// What the controller asks the rest of the app to do.
class ModeActions {
public:
	virtual ~ModeActions() = default;
	virtual void playNarration() = 0;         ///< start, or resume after a pause
	virtual void pauseNarration() = 0;
	virtual void stopNarration() = 0;         ///< narration is over for this run
	virtual void setNarrationGlitch(bool active) = 0;
	virtual void modeChanged(Mode mode) = 0;
};

/// Playback state reported back by the narration player.
struct NarrationProgress {
	float position = 0.0f; ///< 0..1
	bool finished = false;
};

class ModeController {
public:
	explicit ModeController(ModeActions & actions) : actions_(actions) {}

	/// Choose the first mode. Call once after audio is set up.
	void start(const NarrationSettings & settings, bool narrationLoaded, uint64_t nowMs);

	/// Advance the state machine one control frame.
	void update(const ControlInputs & inputs, const NarrationProgress & progress, uint64_t nowMs);

	/// Leave the narration for good and go to the granular instrument.
	void skipNarration();

	/// Desktop testing: start/stop the glitch without sensors.
	void setManualGlitch(bool active, float amount);

	Mode mode() const { return mode_; }

	/// Narration-grain playhead while in NarrationGlitch (0..1).
	float glitchPosition() const { return glitchPosition_; }

	/// Which sensor is scrubbing (-1 when none).
	int glitchSensor() const { return glitchSensor_; }

	/// Pure helper: playhead for a paused position and a squeeze amount.
	static float computeGlitchPosition(float pausedPosition, float amount, float strand);

private:
	void enter(Mode mode);
	int firstSensorAbove(const ControlInputs & inputs, float threshold) const;

	ModeActions & actions_;
	NarrationSettings settings_;
	Mode mode_ = Mode::Granular;
	uint64_t modeStartMs_ = 0;
	uint64_t nowMs_ = 0;
	int glitchSensor_ = -1;
	float glitchPosition_ = 0.0f;
	float pausedPosition_ = 0.0f;
	bool glitchArmed_ = false; ///< false until the squeeze that started the narration is released
	bool manualGlitch_ = false;
	float manualAmount_ = 0.0f;
};

} // namespace gm
