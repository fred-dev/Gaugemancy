#include "ModulatedParam.h"

#include <algorithm>

namespace gm {

ModulatedParam ModulatedParam::fromSpec(const ParamSpec & spec) {
	ModulatedParam p;
	p.value = spec.defaultValue;
	p.min = spec.lowest;
	p.max = spec.highest;
	p.source = ModSource::None;
	return p;
}

float ModulatedParam::resolve(const ControlInputs & inputs) const {
	if (source == ModSource::None) {
		return value;
	}
	float amount = std::clamp(inputs.get(source), 0.0f, 1.0f);
	return min + (max - min) * amount;
}

void ModulatedParam::clampTo(const ParamSpec & spec) {
	value = std::clamp(value, spec.lowest, spec.highest);
	min = std::clamp(min, spec.lowest, spec.highest);
	max = std::clamp(max, spec.lowest, spec.highest);
}

bool ModulatedParam::operator==(const ModulatedParam & other) const {
	return value == other.value && min == other.min && max == other.max && source == other.source;
}

} // namespace gm
