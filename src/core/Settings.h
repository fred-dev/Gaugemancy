#pragma once

// -----------------------------------------------------------------------------
// Settings model for Gaugemancy. Plain structs with sensible defaults; the
// JSON conversion lives in SettingsJson.h and file handling in SettingsStore.h.
//
// Files on disk (one folder per performer, the "profile"):
//
//   <dataRoot>/gaugemancy.json                 which profile is active
//   <dataRoot>/<Profile>/profile.json           ProfileSettings
//   <dataRoot>/<Profile>/presets/preset_1.json  PresetSettings (one per preset)
//   <dataRoot>/<Profile>/audio/...              sample and narration files
//
// On the Raspberry Pi <dataRoot> is normally the USB stick (/media/Data), on a
// laptop it is the app's bin/data folder. Both can be overridden from the
// command line (--data <path>).
// -----------------------------------------------------------------------------

#include "ControlInputs.h"
#include "Curve.h"
#include "EffectCatalog.h"
#include "ModulatedParam.h"

#include <string>
#include <vector>

namespace gm {

constexpr int kSettingsVersion = 1;

// ------------------------------------------------------------------- grain --

/// Grain window shapes (map 1:1 onto pdsp::Window_t).
enum class WindowType {
	Rectangular = 0,
	Triangular,
	Hann,
	Hamming,
	Blackman,
	BlackmanHarris,
	Sine,
	Welch,
	Gaussian,
	Tukey,
	Count
};

std::string windowTypeId(WindowType type);
std::string windowTypeLabel(WindowType type);
WindowType windowTypeFromId(const std::string & id);

/// The parameters of one grain cloud. All can be fixed or sensor driven.
struct GrainSettings {
	ModulatedParam length;          ///< grain length, ms
	ModulatedParam density;         ///< grains overlap, 0.1..1
	ModulatedParam distanceJitter;  ///< random time between grains, ms
	ModulatedParam pitchJitter;     ///< random pitch per grain, semitones
	ModulatedParam pitch;           ///< pitch, semitones
	ModulatedParam spread;          ///< random position around the playhead, 0..1
	ModulatedParam volume;          ///< 0..1
	ModulatedParam position;        ///< playhead in the file, 0..1 (multi mode)
	ModulatedParam direction;       ///< -1 backwards .. 1 forwards

	GrainSettings(); ///< every parameter at its spec default

	/// Number of parameters and access by index, in the order of grainParamSpecs().
	static int count();
	ModulatedParam & at(int index);
	const ModulatedParam & at(int index) const;
};

/// Range/default/label of each grain parameter, same order as GrainSettings::at().
const std::vector<ParamSpec> & grainParamSpecs();

// ----------------------------------------------------------------- effects --

struct EffectSettings {
	std::string type;                   ///< EffectCatalog id
	bool enabled = true;                ///< disabled effects are bypassed
	std::vector<ModulatedParam> params; ///< same order as EffectInfo::params

	/// A new effect of `type` with default parameters. Unknown types give an
	/// empty, disabled effect.
	static EffectSettings make(const std::string & type);

	const EffectInfo * info() const { return EffectCatalog::find(type); }
};

using EffectChainSettings = std::vector<EffectSettings>;

// ------------------------------------------------------------------- slots --

/// When a slot is heard.
enum class GateMode {
	Auto,    ///< while any source used by its grain parameters is above zero
	Always,  ///< always (useful for drones and testing)
	Source   ///< while `gateSource` is above zero
};

struct SlotSettings {
	bool enabled = true;
	std::string file;                       ///< sample file, relative to the profile audio folder
	WindowType window = WindowType::Hann;
	GateMode gate = GateMode::Auto;
	ModSource gateSource = ModSource::None;
	int voices = 8;                         ///< grain voices (CPU cost scales with this)
	GrainSettings grain;
	EffectChainSettings effects;
};

// ----------------------------------------------------------------- presets --

struct PresetSettings {
	std::string name = "Preset";
	/// Accumulated mode only: how fast the playhead advances while pressed,
	/// in fractions of the file per second.
	float playheadSpeed = 0.012f;
	std::vector<SlotSettings> slots;        ///< kMaxSlots entries after loading
	EffectChainSettings masterEffects;      ///< shared chain on the mixed output
	float masterGain = 1.0f;

	PresetSettings(); ///< kMaxSlots default slots
};

// --------------------------------------------------------------- narration --

struct NarrationSettings {
	bool enabled = false;           ///< false: this profile has no narration at all
	bool skip = false;              ///< true: narration exists but is permanently
	                                 ///< skipped for this profile (sticks across
	                                 ///< restarts, unlike a one-off runtime skip)
	std::string file;               ///< relative to the profile audio folder
	float volume = 1.0f;
	bool startWithSensor = true;    ///< else starts automatically after autoStartDelayMs
	int autoStartDelayMs = 5000;
	float glitchThreshold = 0.04f;  ///< sensor level that switches to the glitch granular
	float glitchStrand = 0.016f;    ///< how far back (fraction of file) a full squeeze scrubs
	WindowType window = WindowType::Hann;
	GrainSettings grain;            ///< fixed values only; sources are ignored
	EffectChainSettings effects;    ///< chain on the narration output
};

// ----------------------------------------------------------------- profile --

enum class PlayMode {
	Multi,       ///< six slots, each parameter mapped to sensors
	Accumulated  ///< one slot, total pressure advances the playhead
};

struct SensorSettings {
	int maxRaw = 530;               ///< raw ADC value treated as "fully squeezed"
	float activeThreshold = 0.035f; ///< normalised values below this count as 0
	Curve curve = Curve::Linear;
	int calibrationFrames = 16;     ///< frames averaged at start for the zero point
	float accumulationDenominator = 3.0f; ///< total pressure that counts as 100%
	float smoothingMs = 0.0f;       ///< low-pass time on each ball, 0 = none
};

/// Accelerometer interpretation (see MotionProcessor).
struct MotionSettings {
	bool enabled = true;
	bool zeroAtStartup = true;      ///< "level" = orientation at startup; else `upAxis`
	std::string upAxis = "+z";      ///< device axis pointing up when level (absolute mode)
	float rangeDegrees = 45.0f;     ///< tilt that reaches 0 or 1
	float deadzoneDegrees = 3.0f;   ///< small tilts are ignored
	float smoothingMs = 80.0f;      ///< low-pass time on the tilt values
	float shakeSensitivity = 0.5f;  ///< movement (g) that counts as a full shake
	float shakeReleaseMs = 300.0f;  ///< how fast "shake" falls back
	int calibrationFrames = 20;     ///< frames averaged for the startup reference
};

struct AudioSettings {
	int sampleRate = 44100;
	int bufferSize = 512;
	int numBuffers = 3;
	std::string api = "default";    ///< "default", "alsa", "pulse", "jack", "coreaudio", "wasapi", "asio", "ds"
	std::string deviceName;         ///< substring match on the output device name (preferred)
	int deviceId = -1;              ///< used when deviceName is empty or not found; -1 = system default
	float masterGain = 1.0f;
};

struct GestureSettings {
	bool hitToChangePreset = false;
	float hitThreshold = 0.25f;
	float troughThreshold = 0.08f;
	int maxPeakMs = 200;
	int maxTroughMs = 200;
	int requiredHits = 6;
	int minSensorsPerHit = 2;       ///< how many balls must be squeezed together
};

struct HardwareSettings {
	std::string spiDevice = "/dev/spidev0.0";
	std::vector<int> adcChannels = { 0, 1, 2, 3, 4, 5 }; ///< MCP3008 channel per sensor
	int buttonPin = 19;             ///< BCM numbering
	bool buttonActiveLow = true;    ///< pressed pulls the pin to ground
	int blueLedPin = 5;
	int redLedPin = 6;
	int relayPin = 13;              ///< speaker power relay (pulsed to toggle)
	int relayPulseMs = 400;
	int clickTimeoutMs = 240;       ///< max gap between clicks of a double/triple click
	bool longPressQuits = false;    ///< 6 s press quits the app
	bool shutdownOnQuit = false;    ///< power off the Pi when the app quits
	bool accelerometer = true;
	std::string i2cBus = "/dev/i2c-1";
	int accelAddress = 0x18;
};

struct OscSettings {
	bool telemetry = false;         ///< send sensor values and state to remoteHost
	std::string remoteHost = "127.0.0.1";
	int remotePort = 1235;
	bool control = true;            ///< accept remote commands on controlPort
	int controlPort = 1234;
	int telemetryRateHz = 20;
};

struct ProfileSettings {
	std::string name = "Default";
	std::string logLevel = "notice"; ///< verbose, notice, warning, error, silent
	PlayMode playMode = PlayMode::Multi;
	int controlRateHz = 100;          ///< sensor read / parameter update rate
	int presetSwitchMinIntervalMs = 4000;
	std::vector<std::string> presetFiles = { "presets/preset_1.json" };
	SensorSettings sensors;
	MotionSettings motion;
	AudioSettings audio;
	GestureSettings gestures;
	HardwareSettings hardware;
	OscSettings osc;
	NarrationSettings narration;
};

/// Contents of <dataRoot>/gaugemancy.json.
struct RootSettings {
	std::string activeProfile = "Default";
};

std::string playModeId(PlayMode mode);
PlayMode playModeFromId(const std::string & id);
std::string gateModeId(GateMode mode);

} // namespace gm
