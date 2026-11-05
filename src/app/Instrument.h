#pragma once

// -----------------------------------------------------------------------------
// Instrument: the running instrument, shared by the headless Raspberry Pi build
// and the desktop programming GUI.
//
// Every control frame (update()):
//   1. read sensors (hardware / simulation / replay), optionally record them
//   2. turn them into ControlInputs (calibration, threshold, curve)
//   3. handle the button, the hit gesture and remote (OSC) commands
//   4. advance the mode state machine (narration / glitch / granular)
//   5. send every sound parameter to the audio engine
//   6. update LEDs and telemetry
//
// It owns the loaded settings; the GUI edits them in place through profile()
// and preset() and then calls settingsChanged() so structural edits (effects
// added, sample changed, ...) reach the audio graph.
// -----------------------------------------------------------------------------

#include "RemoteControl.h"

#include "../audio/AudioEngine.h"
#include "../core/ButtonClickDetector.h"
#include "../core/HitGestureDetector.h"
#include "../core/ModeController.h"
#include "../core/Options.h"
#include "../core/Playhead.h"
#include "../core/SensorProcessor.h"
#include "../core/SensorRecording.h"
#include "../core/SensorSimulator.h"
#include "../core/SettingsStore.h"
#include "../hardware/ControlPanel.h"
#include "../hardware/SensorSource.h"

#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <vector>

namespace gm {

class Instrument : private ModeActions {
public:
	Instrument();
	~Instrument() override;

	/// Load settings, open hardware and audio. Returns false only if the
	/// instrument cannot run at all; problems are logged and kept in messages().
	bool setup(const Options & options, const std::string & dataRoot);

	/// One control frame. `nowMs` is the app time in milliseconds.
	void update(uint64_t nowMs);

	/// Stop audio, switch LEDs off, finish recordings.
	void shutdown();

	// ---- actions (button, gesture, GUI, OSC) ----------------------------------
	void nextPreset(bool respectMinInterval);
	void selectPreset(int index);
	void skipNarration();
	void recalibrate();
	bool reloadFromDisk();
	/// Load a different profile by name (its folder under the data root) and
	/// switch to it - same machinery as reloadFromDisk(), for a different name.
	bool switchProfile(const std::string & name);
	bool saveToDisk(std::string & error);
	bool savePresetToDisk(std::string & error);
	bool useSensorMode(SensorMode mode, const std::string & replayFile = "");
	bool startRecording(const std::string & path);
	void stopRecording();

	/// The GUI changed something structural (sample file, effects, voices...).
	void settingsChanged();

	/// Reload one slot's sample after its file name changed in the GUI.
	void reloadSlotSample(int slot);

	/// Reload the narration file (after it was enabled or changed).
	void reloadNarration();

	/// Close and reopen the sound device with the current audio settings.
	bool restartAudio();

	/// GUI audition: hold a slot open at a position (mouse in a waveform).
	void setAudition(int slot, bool active, float position);

	/// GUI: hold the narration glitch with the mouse.
	void setManualGlitch(bool active, float amount) { modes_.setManualGlitch(active, amount); }

	bool quitRequested() const { return quitRequested_; }
	bool shutdownSystemRequested() const { return shutdownSystem_; }

	// ---- state for the GUI -----------------------------------------------------
	ProfileSettings & profile() { return loaded_.profile; }
	PresetSettings & preset() { return loaded_.presets[presetIndex_]; }
	std::vector<PresetSettings> & presets() { return loaded_.presets; }
	int presetIndex() const { return presetIndex_; }
	const ControlInputs & inputs() const { return inputs_; }
	Mode mode() const { return modes_.mode(); }
	float glitchPosition() const { return modes_.glitchPosition(); }
	float playheadPosition() const { return playhead_.position(); }
	AudioEngine & audio() { return audio_; }
	SensorSimulator & simulator() { return simulator_; }
	SettingsStore & store() { return store_; }
	std::string sensorSourceName() const;
	/// Latest accelerometer reading from the active sensor source.
	const AccelSample & lastAccel() const { return lastAccel_; }
	bool isRecording() const { return recorder_.isOpen(); }

	/// Recent log lines (warnings and important events) for on-screen display.
	const std::deque<std::string> & messages() const { return messages_; }

	/// Log a line and keep it for messages().
	void note(const std::string & message, bool warning = false);

private:
	// ModeActions: what the mode state machine asks for.
	void playNarration() override;
	void pauseNarration() override;
	void stopNarration() override;
	void setNarrationGlitch(bool active) override;
	void modeChanged(Mode mode) override;

	/// True on a Raspberry Pi build, where missing GPIO/ADC is worth a warning.
	static bool hardwareExpected();
	/// settings.pi, applied in place of api/deviceName/deviceId, when enabled
	/// and hardwareExpected() - so one profile file works unmodified on both
	/// a laptop and the Pi's USB sound card.
	static AudioSettings resolveAudioSettings(const AudioSettings & settings);
	void applyLogLevel(const std::string & level);
	void openSensors(SensorMode mode, const std::string & replayFile);
	void loadCurrentPreset();
	void readSensors(uint64_t nowMs, float dt);
	void handleButton(uint64_t nowMs);
	void handleGesture(uint64_t nowMs);
	void handleRemote();
	void sendToAudio();
	void updateLeds();
	Telemetry telemetry() const;
	PathResolver audioResolver() const;

	Options options_;
	SettingsStore store_;
	LoadedProfile loaded_;
	int presetIndex_ = 0;

	AudioEngine audio_;
	bool audioOpen_ = false;

	std::unique_ptr<SensorSource> sensors_;
	SensorSimulator simulator_;
	SensorProcessor processor_;
	ControlInputs inputs_;
	AccelSample lastAccel_;
	SensorRecorder recorder_;

	ModeController modes_;
	HitGestureDetector gesture_;
	ButtonClickDetector button_;
	ControlPanel panel_;
	RemoteControl remote_;
	Playhead playhead_;

	std::array<SlotOverrides, kMaxSlots> audition_ {};
	uint64_t nowMs_ = 0;
	uint64_t lastUpdateMs_ = 0;
	uint64_t startMs_ = 0;
	uint64_t lastPresetSwitchMs_ = 0;
	bool quitRequested_ = false;
	bool shutdownSystem_ = false;
	bool hadSensorError_ = false;
	std::deque<std::string> messages_;
};

} // namespace gm
