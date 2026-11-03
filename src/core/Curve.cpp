#include "Curve.h"

#include <algorithm>
#include <cmath>

namespace gm {

float applyCurve(Curve curve, float value) {
	float x = std::clamp(value, 0.0f, 1.0f);
	switch (curve) {
	case Curve::SquareRoot:
		return std::sqrt(x);
	case Curve::Quartic:
		return x * x * x * x;
	case Curve::Exponential:
		// 2^(10(x-1)) is 1/1024 at x=0, so it is pinned to exactly 0 there.
		return x <= 0.0f ? 0.0f : std::pow(2.0f, 10.0f * (x - 1.0f));
	case Curve::Linear:
	case Curve::Count:
		break;
	}
	return x;
}

std::string curveId(Curve curve) {
	switch (curve) {
	case Curve::SquareRoot: return "sqrt";
	case Curve::Quartic: return "quartic";
	case Curve::Exponential: return "exponential";
	default: return "linear";
	}
}

std::string curveLabel(Curve curve) {
	switch (curve) {
	case Curve::SquareRoot: return "Square root (early)";
	case Curve::Quartic: return "Quartic (late)";
	case Curve::Exponential: return "Exponential (very late)";
	default: return "Linear";
	}
}

Curve curveFromId(const std::string & id) {
	for (int i = 0; i < static_cast<int>(Curve::Count); ++i) {
		Curve curve = static_cast<Curve>(i);
		if (curveId(curve) == id) {
			return curve;
		}
	}
	return Curve::Linear;
}

} // namespace gm
