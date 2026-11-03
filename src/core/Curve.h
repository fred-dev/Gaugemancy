#pragma once

// -----------------------------------------------------------------------------
// Response curves applied to normalised sensor values (0..1 in, 0..1 out).
// They change how a squeeze "feels": Linear is direct, SquareRoot responds
// early (light touches matter), Quartic and Exponential respond late (only
// hard squeezes matter).
// -----------------------------------------------------------------------------

#include <string>

namespace gm {

enum class Curve {
	Linear = 0,
	SquareRoot,
	Quartic,
	Exponential,
	Count
};

/// Apply a curve. Input is clamped to 0..1 first; output is always 0..1.
float applyCurve(Curve curve, float value);

std::string curveId(Curve curve);    ///< "linear", "sqrt", "quartic", "exponential"
std::string curveLabel(Curve curve); ///< label for the GUI
Curve curveFromId(const std::string & id); ///< unknown ids give Linear

} // namespace gm
