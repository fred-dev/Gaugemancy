#pragma once

// -----------------------------------------------------------------------------
// ModulatedParam: one sound parameter that is either fixed or driven by a
// modulation source.
//
//   source == None : the parameter is `value`
//   otherwise      : the parameter moves from `min` (source at 0) to `max`
//                    (source at 1). Setting min > max inverts the response.
//
// ParamSpec describes the legal range and default of a parameter; it is shared
// by the JSON loader (to clamp bad values), the GUI (slider ranges) and the
// audio code.
// -----------------------------------------------------------------------------

#include "ControlInputs.h"

#include <string>
#include <vector>

namespace gm {

struct ParamSpec {
	const char * id;      ///< JSON key, e.g. "cutoff"
	const char * label;   ///< GUI label, e.g. "Cutoff (Hz)"
	float defaultValue;
	float lowest;         ///< legal minimum
	float highest;        ///< legal maximum

	/// Named options for a discrete parameter (e.g. a waveform pick), lowest
	/// first - index i means value `lowest + i`. Empty (the default) means
	/// "continuous": the GUI draws a slider instead of a dropdown.
	std::vector<const char *> choices {};
};

struct ModulatedParam {
	float value = 0.0f;
	float min = 0.0f;
	float max = 1.0f;
	ModSource source = ModSource::None;

	/// A fixed parameter using the spec's default and full range for min/max.
	static ModulatedParam fromSpec(const ParamSpec & spec);

	/// The value for this control frame.
	float resolve(const ControlInputs & inputs) const;

	/// True when the parameter follows a source.
	bool isModulated() const { return source != ModSource::None; }

	/// Force value/min/max inside the spec's legal range.
	void clampTo(const ParamSpec & spec);

	bool operator==(const ModulatedParam & other) const;
	bool operator!=(const ModulatedParam & other) const { return !(*this == other); }
};

} // namespace gm
