#pragma once

// -----------------------------------------------------------------------------
// Sample rate choice. A sound device only works at the rates it supports; asking
// for any other rate gives silence (the stream never starts). These helpers
// keep the configured rate when the device supports it and otherwise fall back
// to the closest rate it does support.
// -----------------------------------------------------------------------------

#include <cstdlib>
#include <vector>

namespace gm {

/// True when `rate` is in `supported`. An empty list means "unknown device":
/// every rate is then accepted, because there is nothing to check against.
inline bool isSampleRateSupported(const std::vector<int> & supported, int rate) {
	if (supported.empty()) return true;
	for (int r : supported) {
		if (r == rate) return true;
	}
	return false;
}

/// `wanted` if the device supports it, else the supported rate closest to it
/// (the higher one on a tie). With an empty list, `wanted` is returned as is.
inline int pickSampleRate(const std::vector<int> & supported, int wanted) {
	if (isSampleRateSupported(supported, wanted)) return wanted;
	int best = supported.front();
	for (int r : supported) {
		int d = std::abs(r - wanted);
		int bestD = std::abs(best - wanted);
		if (d < bestD || (d == bestD && r > best)) best = r;
	}
	return best;
}

} // namespace gm
