#pragma once

// -----------------------------------------------------------------------------
// SourceCatalog: the list of sound sources a slot can be, and their
// parameters - the same shape as EffectCatalog, so a slot's source gets the
// same catalog-driven, sensor-modulatable editing every effect already has.
//
// "granular" is the default and has no params of its own here: it keeps
// using SlotSettings::grain (GrainSettings), unchanged. The synth types'
// params live in SlotSettings::synthParams, in the order given here.
// -----------------------------------------------------------------------------

#include "ModulatedParam.h"

#include <string>
#include <vector>

namespace gm {

struct SourceInfo {
	std::string id;          ///< JSON type, e.g. "va_osc"
	std::string label;       ///< GUI name, e.g. "Analog oscillator"
	std::string description; ///< one line help shown in the GUI
	std::vector<ParamSpec> params;
};

class SourceCatalog {
public:
	/// Every source type in the order shown in the GUI; "granular" is first.
	static const std::vector<SourceInfo> & all();

	/// Look up by id. Returns nullptr for unknown types.
	static const SourceInfo * find(const std::string & id);

	/// Default-valued params for `id`'s param list (empty for "granular" or
	/// an unknown id) - what a slot's synthParams should reset to when its
	/// sourceType changes.
	static std::vector<ModulatedParam> makeParams(const std::string & id);
};

} // namespace gm
