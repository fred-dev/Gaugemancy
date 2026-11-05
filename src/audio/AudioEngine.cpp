#include "AudioEngine.h"

#include "ofSoundStream.h"

#include <algorithm>
#include <cctype>

namespace gm {

namespace {

ofSoundDevice::Api apiFromName(const std::string & name) {
	if (name == "alsa") return ofSoundDevice::ALSA;
	if (name == "pulse") return ofSoundDevice::PULSE;
	if (name == "jack") return ofSoundDevice::JACK;
	if (name == "coreaudio") return ofSoundDevice::OSX_CORE;
	if (name == "wasapi") return ofSoundDevice::MS_WASAPI;
	if (name == "asio") return ofSoundDevice::MS_ASIO;
	if (name == "ds") return ofSoundDevice::MS_DS;
	return ofSoundDevice::DEFAULT;
}

std::string lower(std::string text) {
	std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return text;
}

/// A synth source always has something to play; granular still needs a
/// loaded sample.
bool isPlayable(const SlotSettings & settings, const GrainSlot & slot) {
	return settings.enabled && (settings.sourceType != "granular" || slot.hasSample());
}

std::vector<ofSoundDevice> outputDevices(ofSoundDevice::Api api) {
	ofSoundStream stream;
	std::vector<ofSoundDevice> outputs;
	for (const ofSoundDevice & device : stream.getDeviceList(api)) {
		if (device.outputChannels >= 2) {
			outputs.push_back(device);
		}
	}
	return outputs;
}

} // namespace

AudioEngine::AudioEngine() {
	for (auto & slot : slots_) {
		slot = std::make_unique<GrainSlot>();
		// The scope's own output is deliberately silent; patching it into
		// the blackhole is just what makes pdsp actually process it.
		slot->scopeTap() >> engine_.blackhole();
	}
	masterGain_.enableSmoothing(50.0f);
	masterGain_.set(1.0f);
	for (int ch = 0; ch < 2; ++ch) {
		masterGain_ >> masterAmp_[ch].in_mod();
		master_.out(ch) >> masterAmp_[ch];
		masterAmp_[ch] >> engine_.audio_out(ch);
	}
	master_.build({});
}

AudioEngine::~AudioEngine() {
	stop();
}

// --------------------------------------------------------------------- device

std::vector<std::string> AudioEngine::describeOutputDevices(const std::string & api) {
	std::vector<std::string> lines;
	for (const ofSoundDevice & d : outputDevices(apiFromName(api))) {
		lines.push_back("[" + std::to_string(d.deviceID) + "] " + d.name + " (" + std::to_string(d.outputChannels) + " out)"
			+ (d.isDefaultOutput ? " default" : ""));
	}
	return lines;
}

std::vector<std::string> AudioEngine::outputDeviceNames(const std::string & api) {
	std::vector<std::string> names;
	for (const ofSoundDevice & d : outputDevices(apiFromName(api))) {
		names.push_back(d.name);
	}
	return names;
}

bool AudioEngine::startDevice(const AudioSettings & settings, std::string & report) {
	const ofSoundDevice::Api api = apiFromName(settings.api);
	const std::vector<ofSoundDevice> devices = outputDevices(api);

	// Prefer a name match: device numbers change when USB devices are plugged
	// in a different order, names do not.
	int chosen = -1;
	std::string chosenName = "system default";
	if (!settings.deviceName.empty()) {
		for (const ofSoundDevice & d : devices) {
			if (lower(d.name).find(lower(settings.deviceName)) != std::string::npos) {
				chosen = d.deviceID;
				chosenName = d.name;
				break;
			}
		}
		if (chosen < 0) {
			report += "audio device '" + settings.deviceName + "' not found; ";
		}
	}
	if (chosen < 0 && settings.deviceId >= 0) {
		for (const ofSoundDevice & d : devices) {
			if (d.deviceID == settings.deviceId) {
				chosen = d.deviceID;
				chosenName = d.name;
			}
		}
		if (chosen < 0) {
			report += "audio device id " + std::to_string(settings.deviceId) + " not found; ";
		}
	}
	if (devices.empty()) {
		report += "no output devices found for api '" + settings.api + "'";
		return false;
	}

	engine_.setApi(api);
	engine_.setChannels(0, 2);
	if (chosen >= 0) {
		engine_.setOutputDeviceID(chosen);
	}
	sampleRate_ = settings.sampleRate;
	bufferSize_ = settings.bufferSize;
	engine_.setup(settings.sampleRate, settings.bufferSize, settings.numBuffers);
	running_ = true;
	report += "audio: " + chosenName + " @ " + std::to_string(settings.sampleRate) + " Hz, buffer "
		+ std::to_string(settings.bufferSize) + " x " + std::to_string(settings.numBuffers);
	return true;
}

void AudioEngine::startOffline(int sampleRate, int bufferSize) {
	sampleRate_ = sampleRate;
	bufferSize_ = bufferSize;
	engine_.setChannels(0, 2);
	pdsp::prepareAllToPlay(bufferSize, static_cast<double>(sampleRate));
}

void AudioEngine::renderOffline(float * interleaved, int frames) {
	engine_.renderOffline(interleaved, 2, frames);
}

void AudioEngine::stop() {
	if (running_) {
		muteAll();
		engine_.stop();
		running_ = false;
	}
}

// -------------------------------------------------------------------- content

std::vector<std::string> AudioEngine::loadPreset(const PresetSettings & preset, const PathResolver & resolve) {
	std::vector<std::string> problems;

	// 1. Decode every file first, without blocking audio.
	std::array<std::unique_ptr<pdsp::SampleBuffer>, kMaxSlots> decoded;
	for (int i = 0; i < kMaxSlots; ++i) {
		const SlotSettings & s = preset.slots[i];
		if (!s.enabled || s.file.empty()) continue;
		std::string error;
		decoded[i] = GrainSlot::loadSampleFile(resolve(s.file), error);
		if (!decoded[i]) {
			problems.push_back("slot " + std::to_string(i + 1) + ": " + error);
		}
	}

	// 2. Swap them in and rebuild under the lock (pointer swaps only).
	std::array<std::unique_ptr<pdsp::SampleBuffer>, kMaxSlots> old;
	{
		auto lock = engine_.lockPatching();
		for (int i = 0; i < kMaxSlots; ++i) {
			slots_[i]->mute();
			old[i] = slots_[i]->setSample(std::move(decoded[i]), preset.slots[i].file);
			slots_[i]->rebuild(preset.slots[i]);
		}
		master_.build(preset.masterEffects);
		connectOutputs(preset);
	}
	// 3. `old` goes out of scope here, after the lock: buffers freed safely.
	return problems;
}

std::vector<std::string> AudioEngine::loadNarration(const NarrationSettings & settings, const PathResolver & resolve) {
	std::vector<std::string> problems;
	std::unique_ptr<pdsp::SampleBuffer> decoded;
	if (settings.enabled) {
		std::string error;
		decoded = NarrationVoice::loadFile(resolve(settings.file), error);
		if (!decoded) {
			problems.push_back("narration: " + error);
		}
	}
	std::unique_ptr<pdsp::SampleBuffer> old;
	{
		auto lock = engine_.lockPatching();
		old = narration_.setFile(std::move(decoded));
		narration_.rebuild(settings);
		bool connect = narration_.isLoaded();
		for (int ch = 0; ch < 2; ++ch) {
			narration_.out(ch).disconnectOut();
			if (connect) narration_.out(ch) >> masterAmp_[ch];
		}
		narrationConnected_ = connect;
	}
	return problems;
}

std::string AudioEngine::loadSlotSample(int slot, const std::string & path, const std::string & displayName) {
	if (slot < 0 || slot >= kMaxSlots) return "no such slot";
	std::string error;
	auto decoded = GrainSlot::loadSampleFile(path, error);
	if (!decoded) return error;
	std::unique_ptr<pdsp::SampleBuffer> old;
	{
		auto lock = engine_.lockPatching();
		slots_[slot]->mute();
		old = slots_[slot]->setSample(std::move(decoded), displayName);
		// The slot may have been disconnected for lack of a sample.
		if (!slotConnected_[slot]) {
			for (int ch = 0; ch < 2; ++ch) slots_[slot]->out(ch) >> master_.in(ch);
			slotConnected_[slot] = true;
		}
	}
	return "";
}

void AudioEngine::connectOutputs(const PresetSettings & preset) {
	for (int i = 0; i < kMaxSlots; ++i) {
		bool playable = isPlayable(preset.slots[i], *slots_[i]);
		if (playable == slotConnected_[i]) continue;
		for (int ch = 0; ch < 2; ++ch) {
			if (playable) {
				slots_[i]->out(ch) >> master_.in(ch);
			} else {
				slots_[i]->out(ch).disconnectOut();
			}
		}
		slotConnected_[i] = playable;
	}
}

void AudioEngine::syncStructure(const PresetSettings & preset, const NarrationSettings & narration) {
	bool slotChanges = false;
	for (int i = 0; i < kMaxSlots; ++i) {
		bool playable = isPlayable(preset.slots[i], *slots_[i]);
		slotChanges |= slots_[i]->needsRebuild(preset.slots[i]) || playable != slotConnected_[i];
	}
	bool masterChanged = !master_.matches(preset.masterEffects);
	bool narrationChanged = narration_.isLoaded() && narration_.needsRebuild(narration);
	if (!slotChanges && !masterChanged && !narrationChanged) {
		return;
	}
	auto lock = engine_.lockPatching();
	for (int i = 0; i < kMaxSlots; ++i) {
		if (slots_[i]->needsRebuild(preset.slots[i])) slots_[i]->rebuild(preset.slots[i]);
	}
	if (masterChanged) master_.build(preset.masterEffects);
	if (narrationChanged) narration_.rebuild(narration);
	connectOutputs(preset);
}

void AudioEngine::update(const PresetSettings & preset, const NarrationSettings & narration,
	const ControlInputs & inputs, const std::array<SlotOverrides, kMaxSlots> & overrides, float masterGain) {
	for (int i = 0; i < kMaxSlots; ++i) {
		if (slotConnected_[i]) {
			slots_[i]->update(preset.slots[i], inputs, overrides[i]);
		}
	}
	master_.apply(preset.masterEffects, inputs);
	if (narrationConnected_) {
		narration_.update(narration, inputs);
	}
	masterGain_.set(masterGain * preset.masterGain);
}

void AudioEngine::muteAll() {
	for (auto & slot : slots_) {
		slot->mute();
	}
	narration_.stop();
}

} // namespace gm
