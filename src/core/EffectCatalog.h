#pragma once

// -----------------------------------------------------------------------------
// EffectCatalog: the list of effect types and their parameters.
//
// This is the single place that defines what effects exist. The JSON loader,
// the GUI and the audio factory all read from it, so adding an effect means:
//   1. add an EffectInfo entry in EffectCatalog.cpp
//   2. add a class in src/audio/effects and one line in EffectFactory.cpp
// -----------------------------------------------------------------------------

#include "ModulatedParam.h"

#include <string>
#include <vector>

namespace gm {

struct EffectInfo {
	std::string id;          ///< JSON type, e.g. "filter"
	std::string label;       ///< GUI name, e.g. "Low-pass filter"
	std::string description; ///< one line help shown in the GUI
	std::vector<ParamSpec> params;
	int cpuCost = 1;         ///< rough relative cost, shown in the GUI (1 cheap .. 5 heavy)
};

class EffectCatalog {
public:
	/// Every effect type in the order shown in the GUI.
	static const std::vector<EffectInfo> & all();

	/// Look up by id. Returns nullptr for unknown types.
	static const EffectInfo * find(const std::string & id);

	/// Index of a parameter inside an effect, or -1.
	static int paramIndex(const EffectInfo & info, const std::string & paramId);
};

} // namespace gm
