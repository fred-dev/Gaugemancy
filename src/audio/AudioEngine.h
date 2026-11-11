#pragma once

// -----------------------------------------------------------------------------
// AudioEngine: owns the whole sound graph.
//
//   playing PresetGraph  (slots + master chain, fading out) ─┐
//   incoming PresetGraph (slots + master chain, fading in) ──┼─> master gain -> audio out L/R
//   narration ───────────────────────────────────────────────┘
//
// Most of the time there is one PresetGraph. A preset change builds a second
// one WITHOUT the patch lock (it is not connected yet, so audio keeps
// playing), connects it silently under a very short lock, then crossfades
// with equal power on the control thread. The old graph is disconnected and
// destroyed when its fade has reached zero. See PresetGraph.h.
//
// Thread safety: structural changes to a connected graph take the pdsp patch
// lock; per-frame parameter updates use atomic controls only.
// -----------------------------------------------------------------------------

#include "GrainSlot.h"
#include "NarrationVoice.h"
#include "PresetGraph.h"

#include "../core/Crossfade.h"
#include "../core/Settings.h"

#include "ofxPDSP.h"

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace gm {

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

	/// Build a new graph for `preset` (slow, but audio keeps playing) and
	/// crossfade to it over `fadeMs` (0 = switch at once). If a fade is still
	/// running it is cut short first, so callers that care should wait for
	/// isFading() to be false. Returns one line per problem.
	std::vector<std::string> loadPreset(const PresetSettings & preset, const PathResolver & resolve, float fadeMs = 0.0f);

	/// True while the previous preset is still fading out.
	bool isFading() const { return outgoing_ != nullptr; }

	/// Load the narration file (if enabled). Returns problems, if any.
	std::vector<std::string> loadNarration(const NarrationSettings & settings, const PathResolver & resolve);

	/// Rebuild anything whose structure no longer matches the settings (effect
	/// added in the GUI, voice count changed...). Cheap when nothing changed.
	void syncStructure(const PresetSettings & preset, const NarrationSettings & narration);

	/// Per control frame: resolve and send every parameter. `dtSeconds` is the
	/// time since the last control frame.
	void update(const PresetSettings & preset, const NarrationSettings & narration,
		const ControlInputs & inputs, const std::array<SlotOverrides, kMaxSlots> & overrides,
		float masterGain, float dtSeconds);

	/// Start feeding the slots' live scopes (GUI only: it costs a little CPU
	/// per slot, so the headless instrument never calls this).
	void enableScopes();

	/// Replace one slot's sample (e.g. dragged in from the GUI).
	std::string loadSlotSample(int slot, const std::string & path, const std::string & displayName);

	void muteAll();

	/// Slots and master chain of the current (incoming, during a fade) preset.
	/// Do not keep these references across a preset change.
	GrainSlot & slot(int index) { return current_->slot(index); }
	NarrationVoice & narration() { return narration_; }
	EffectChain & master() { return current_->master(); }

	/// Final output level after every slot and narration are summed and the
	/// master chain/gain applied - channel 0 = left, 1 = right. Thread-safe.
	float masterMeter(int channel) const { return masterAmp_[channel & 1].meter_output(); }

	/// Buffers output as silence because the graph was being changed.
	uint64_t skippedCallbacks() const { return engine_.getSkippedCallbacks(); }

	bool isRunning() const { return running_; }
	int sampleRate() const { return sampleRate_; }
	int bufferSize() const { return bufferSize_; }

private:
	/// Put `fresh` on the output (silent when fading) and make it current.
	void connectIncoming(std::unique_ptr<PresetGraph> fresh, bool fade);
	void advanceFade(const ControlInputs & inputs, const std::array<SlotOverrides, kMaxSlots> & overrides,
		float dtSeconds);
	/// Disconnect the outgoing graph under the lock, destroy it after.
	void retireOutgoing();

	pdsp::Engine engine_;
	std::unique_ptr<PresetGraph> current_;  ///< always set
	std::unique_ptr<PresetGraph> outgoing_; ///< only during a fade
	Crossfade fade_;
	float fadeSettledSeconds_ = 0.0f; ///< time since the fade reached its end
	bool hasPreset_ = false;          ///< false until the first loadPreset()
	NarrationVoice narration_;
	bool narrationConnected_ = false;
	bool scopesEnabled_ = false;
	std::array<pdsp::Amp, 2> masterAmp_;
	pdsp::ValueControl masterGain_;
	bool running_ = false;
	int sampleRate_ = 44100;
	int bufferSize_ = 512;
};

} // namespace gm
