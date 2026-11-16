#include "AudioEngine.h"

#include "../core/AudioRates.h"

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

// After a fade reaches its end, wait this long before removing the old graph:
// its fade gain is smoothed and must have settled on exactly 0.
constexpr float kFadeSettleSeconds = 0.1f;

std::string lower(std::string text) {
	std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return text;
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
	current_ = std::make_unique<PresetGraph>();
	masterGain_.enableSmoothing(50.0f);
	masterGain_.set(1.0f);
	for (int ch = 0; ch < 2; ++ch) {
		masterGain_ >> masterAmp_[ch].in_mod();
		current_->out(ch) >> masterAmp_[ch];
		masterAmp_[ch] >> engine_.audio_out(ch);
	}
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

namespace {

/// The device `settings` selects: by name first (device numbers change when
/// USB devices are plugged in a different order, names do not), then by id.
/// Returns nullptr for "use the system default"; `report` gets any mismatch.
const ofSoundDevice * chooseDevice(const std::vector<ofSoundDevice> & devices, const AudioSettings & settings,
	std::string & report) {
	if (!settings.deviceName.empty()) {
		for (const ofSoundDevice & d : devices) {
			if (lower(d.name).find(lower(settings.deviceName)) != std::string::npos) return &d;
		}
		report += "audio device '" + settings.deviceName + "' not found; ";
	}
	if (settings.deviceId >= 0) {
		for (const ofSoundDevice & d : devices) {
			if (d.deviceID == settings.deviceId) return &d;
		}
		report += "audio device id " + std::to_string(settings.deviceId) + " not found; ";
	}
	return nullptr;
}

/// The device that will really be opened: the chosen one or the default output.
const ofSoundDevice * effectiveDevice(const std::vector<ofSoundDevice> & devices, const AudioSettings & settings,
	std::string & report) {
	if (const ofSoundDevice * chosen = chooseDevice(devices, settings, report)) return chosen;
	for (const ofSoundDevice & d : devices) {
		if (d.isDefaultOutput) return &d;
	}
	return devices.empty() ? nullptr : &devices.front();
}

std::vector<int> ratesOf(const ofSoundDevice * device) {
	std::vector<int> rates;
	if (device != nullptr) {
		for (unsigned int r : device->sampleRates) rates.push_back(static_cast<int>(r));
	}
	std::sort(rates.begin(), rates.end());
	return rates;
}

} // namespace

std::vector<int> AudioEngine::supportedSampleRates(const AudioSettings & settings) {
	std::string ignored;
	const std::vector<ofSoundDevice> devices = outputDevices(apiFromName(settings.api));
	return ratesOf(effectiveDevice(devices, settings, ignored));
}

bool AudioEngine::startDevice(const AudioSettings & settings, std::string & report) {
	const ofSoundDevice::Api api = apiFromName(settings.api);
	const std::vector<ofSoundDevice> devices = outputDevices(api);
	if (devices.empty()) {
		report += "no output devices found for api '" + settings.api + "'";
		return false;
	}
	const ofSoundDevice * chosen = chooseDevice(devices, settings, report);
	std::string ignored;
	const ofSoundDevice * device = chosen != nullptr ? chosen : effectiveDevice(devices, settings, ignored);

	// A rate the device cannot run at gives silence, so never ask for one.
	const int rate = pickSampleRate(ratesOf(device), settings.sampleRate);
	if (rate != settings.sampleRate) {
		report += "sample rate " + std::to_string(settings.sampleRate) + " not supported by this device, using "
			+ std::to_string(rate) + "; ";
	}

	engine_.setApi(api);
	engine_.setChannels(0, 2);
	if (chosen != nullptr) {
		engine_.setOutputDeviceID(chosen->deviceID);
	}
	sampleRate_ = rate;
	bufferSize_ = settings.bufferSize;
	engine_.setup(rate, settings.bufferSize, settings.numBuffers);
	running_ = true;
	report += "audio: " + (chosen != nullptr ? chosen->name : "system default"
			+ (device != nullptr ? " (" + device->name + ")" : std::string()))
		+ " @ " + std::to_string(rate) + " Hz, buffer " + std::to_string(settings.bufferSize) + " x "
		+ std::to_string(settings.numBuffers);
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

std::vector<std::string> AudioEngine::loadPreset(const PresetSettings & preset, const PathResolver & resolve, float fadeMs) {
	if (outgoing_) {
		fade_.finish();
		retireOutgoing();
	}
	// 1. Build the whole new graph. It is not connected to anything yet, so
	//    this needs no lock and the playing preset is not interrupted.
	auto fresh = std::make_unique<PresetGraph>();
	std::vector<std::string> problems = fresh->build(preset, resolve);

	// 2. Connect it (pointer operations only) and start the fade. The very
	//    first preset has nothing to fade from.
	bool fade = hasPreset_ && fadeMs > 0.0f;
	connectIncoming(std::move(fresh), fade);
	if (fade) {
		fade_.start(fadeMs / 1000.0f);
		fadeSettledSeconds_ = 0.0f;
	}
	hasPreset_ = true;
	return problems;
}

void AudioEngine::connectIncoming(std::unique_ptr<PresetGraph> fresh, bool fade) {
	std::unique_ptr<PresetGraph> replaced; // destroyed after the lock is released
	{
		auto lock = engine_.lockPatching();
		fresh->setFade(fade ? 0.0f : 1.0f);
		for (int ch = 0; ch < 2; ++ch) {
			fresh->out(ch) >> masterAmp_[ch];
		}
		if (scopesEnabled_) {
			fresh->connectScopes(engine_.blackhole());
			current_->disconnectScopes();
		}
		if (fade) {
			outgoing_ = std::move(current_);
		} else {
			for (int ch = 0; ch < 2; ++ch) {
				current_->out(ch).disconnectOut();
			}
			replaced = std::move(current_);
		}
		current_ = std::move(fresh);
	}
}

void AudioEngine::advanceFade(const ControlInputs & inputs, const std::array<SlotOverrides, kMaxSlots> & overrides,
	float dtSeconds) {
	if (!outgoing_) return;
	// The old preset keeps answering the sensors while it fades, so a held
	// squeeze crossfades from the old sound to the new one.
	outgoing_->updateWithBuiltSettings(inputs, overrides, dtSeconds);
	fade_.advance(dtSeconds);
	current_->setFade(fade_.incomingGain());
	outgoing_->setFade(fade_.outgoingGain());
	if (fade_.done()) {
		fadeSettledSeconds_ += dtSeconds;
		if (fadeSettledSeconds_ >= kFadeSettleSeconds) {
			retireOutgoing();
		}
	}
}

void AudioEngine::retireOutgoing() {
	if (!outgoing_) return;
	current_->setFade(1.0f);
	std::unique_ptr<PresetGraph> retired;
	{
		auto lock = engine_.lockPatching();
		for (int ch = 0; ch < 2; ++ch) {
			outgoing_->out(ch).disconnectOut();
		}
		retired = std::move(outgoing_);
	}
	// `retired` (samples, effects) is freed here, after the lock.
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
		current_->slot(slot).mute();
		old = current_->slot(slot).setSample(std::move(decoded), displayName);
	}
	// A slot that had no sample is connected by the caller's syncStructure(),
	// which knows whether the slot is enabled.
	return "";
}

void AudioEngine::syncStructure(const PresetSettings & preset, const NarrationSettings & narration) {
	bool presetChanged = current_->needsSync(preset);
	bool narrationChanged = narration_.isLoaded() && narration_.needsRebuild(narration);
	if (!presetChanged && !narrationChanged) {
		return;
	}
	auto lock = engine_.lockPatching();
	if (presetChanged) current_->syncStructure(preset);
	if (narrationChanged) narration_.rebuild(narration);
}

void AudioEngine::update(const PresetSettings & preset, const NarrationSettings & narration,
	const ControlInputs & inputs, const std::array<SlotOverrides, kMaxSlots> & overrides, float masterGain,
	float dtSeconds) {
	current_->update(preset, inputs, overrides, dtSeconds);
	advanceFade(inputs, overrides, dtSeconds);
	if (narrationConnected_) {
		narration_.update(narration, inputs);
	}
	// The preset's own gain is applied inside its PresetGraph, so that two
	// crossfading presets each keep their level.
	masterGain_.set(masterGain);
}

void AudioEngine::enableScopes() {
	if (scopesEnabled_) return;
	auto lock = engine_.lockPatching();
	current_->connectScopes(engine_.blackhole());
	scopesEnabled_ = true;
}

void AudioEngine::muteAll() {
	current_->mute();
	if (outgoing_) outgoing_->mute();
	narration_.stop();
}

} // namespace gm
