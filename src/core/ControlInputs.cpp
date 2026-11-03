#include "ControlInputs.h"

namespace gm {

namespace {
struct SourceName {
	const char * id;
	const char * label;
};

// Order must match the ModSource enum.
const SourceName kSourceNames[kNumModSources] = {
	{ "none", "None" },
	{ "sensor1", "Sensor 1" },
	{ "sensor2", "Sensor 2" },
	{ "sensor3", "Sensor 3" },
	{ "sensor4", "Sensor 4" },
	{ "sensor5", "Sensor 5" },
	{ "sensor6", "Sensor 6" },
	{ "total", "Total pressure" },
	{ "accelX", "Tilt X" },
	{ "accelY", "Tilt Y" },
	{ "accelZ", "Tilt Z" },
};
} // namespace

std::string modSourceId(ModSource source) {
	int index = static_cast<int>(source);
	return (index >= 0 && index < kNumModSources) ? kSourceNames[index].id : "none";
}

std::string modSourceLabel(ModSource source) {
	int index = static_cast<int>(source);
	return (index >= 0 && index < kNumModSources) ? kSourceNames[index].label : "None";
}

ModSource modSourceFromId(const std::string & id) {
	for (int i = 0; i < kNumModSources; ++i) {
		if (id == kSourceNames[i].id) {
			return static_cast<ModSource>(i);
		}
	}
	return ModSource::None;
}

ModSource modSourceFromLegacyIndex(int index) {
	// The XML settings used 0 for "not connected" and 1..6 for the sensors.
	if (index >= 1 && index <= kNumSensors) {
		return sensorSource(index - 1);
	}
	return ModSource::None;
}

ModSource sensorSource(int sensorIndex) {
	if (sensorIndex < 0 || sensorIndex >= kNumSensors) {
		return ModSource::None;
	}
	return static_cast<ModSource>(static_cast<int>(ModSource::Sensor1) + sensorIndex);
}

bool ControlInputs::anySensorActive() const {
	for (int i = 0; i < kNumSensors; ++i) {
		if (sensor(i) > 0.0f) {
			return true;
		}
	}
	return false;
}

} // namespace gm
