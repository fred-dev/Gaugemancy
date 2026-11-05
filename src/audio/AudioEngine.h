#pragma once

// -----------------------------------------------------------------------------
// AudioEngine: owns the whole sound graph.
//
//   slot 1..6 ──┐
//               ├─> master EffectChain ─┐
//               ┘                       ├─> master gain -> audio out L/R
//   narration ──────────────────────────┘
//
// Only slots that are enabled AND have a sample are connected; pdsp only
// processes what is connected to the output, so idle slots cost no CPU.
//
// Thread safety: structural changes (load, rebuild, connect) take the pdsp
// patch lock; per-frame parameter updates use atomic controls only.
// -----------------------------------------------------------------------------

#include "GrainSlot.h"
#include "NarrationVoice.h"

#include "../core/Settings.h"

#include "ofxPDSP.h"

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace gm {

/// Turns a file name from the settings into a full path.
using PathResolver = std::function<std::string(const std::string &)>;

class AudioEngine {
public:
	AudioEngine();
	~AudioEngine();
	AudioEngine(const AudioEngine &) = delete;
	AudioEngine & operator=(const AudioEngine &) = delete;

	// ---- device --------------------------------------------------------------

	/// Open the sound device. Fills `report` with what was chosen and why.
	bool startDevice(const AudioSettings & settings, std::string & report);

	/// Prepare for renderOffline() without any sound device (benchmarks).
	void startOffline(int sampleRate, int bufferSize);

	/// Render `frames` stereo frames into `interleaved` (2 * frames floats).
	void renderOffline(float * interleaved, int frames);

	void stop();

	/// Human readable list of output devices for an API ("default", "alsa"...).
	static std::vector<std::string> describeOutputDevices(const std::string & api);

	/// Plain device names for an API, in the same order as describeOutputDevices()
	/// - for a GUI picker that sets AudioSettings::deviceName directly.
	static std::vector<std::string> outputDeviceNames(const std::string & api);

	// ---- content -------------------------------------------------------------

	/// Load every slot's sample and build the chains for a preset. Files are
	/// decoded before the patch lock is taken. Returns one line per problem.
	std::vector<std::string> loadPreset(const PresetSettings & preset, const PathResolver & resolve);

	/// Load the narration file (if enabled). Returns problems, if any.
	std::vector<std::string> loadNarration(const NarrationSettings & settings, const PathResolver & resolve);

	/// Rebuild anything whose structure no longer matches the settings (effect
	/// added in the GUI, voice count changed...). Cheap when nothing changed.
	void syncStructure(const PresetSettings & preset, const NarrationSettings & narration);

	/// Per control frame: resolve and send every parameter.
	void update(const PresetSettings & preset, const NarrationSettings & narration,
		const ControlInputs & inputs, const std::array<SlotOverrides, kMaxSlots> & overrides,
		float masterGain);

	/// Replace one slot's sample (e.g. dragged in from the GUI).
	std::string loadSlotSample(int slot, const std::string & path, const std::string & displayName);

	void muteAll();

	GrainSlot & slot(int index) { return *slots_[index]; }
	NarrationVoice & narration() { return narration_; }
	EffectChain & master() { return master_; }

	/// Final output level after every slot and narration are summed and the
	/// master chain/gain applied - channel 0 = left, 1 = right. Thread-safe.
	float masterMeter(int channel) const { return masterAmp_[channel & 1].meter_output(); }

	/// Buffers output as silence because the graph was being changed.
	uint64_t skippedCallbacks() const { return engine_.getSkippedCallbacks(); }

	bool isRunning() const { return running_; }
	int sampleRate() const { return sampleRate_; }
	int bufferSize() const { return bufferSize_; }

private:
	void connectOutputs(const PresetSettings & preset);

	pdsp::Engine engine_;
	std::array<std::unique_ptr<GrainSlot>, kMaxSlots> slots_;
	std::array<bool, kMaxSlots> slotConnected_ {};
	NarrationVoice narration_;
	bool narrationConnected_ = false;
	EffectChain master_;
	std::array<pdsp::Amp, 2> masterAmp_;
	pdsp::ValueControl masterGain_;
	bool running_ = false;
	int sampleRate_ = 44100;
	int bufferSize_ = 512;
};

} // namespace gm
