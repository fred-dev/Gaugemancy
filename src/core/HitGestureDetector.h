#pragma once

// -----------------------------------------------------------------------------
// HitGestureDetector: recognises the "pump" gesture performers use to change
// preset: squeeze several balls hard and let go, quickly, N times in a row.
//
// One hit = a PEAK (at least minSensorsPerHit balls above hitThreshold)
//           followed within maxPeakMs by a TROUGH (the same number of balls
//           back below troughThreshold).
// The next peak must start within maxTroughMs of the trough, otherwise the
// count starts again. After requiredHits hits, update() returns true once.
// -----------------------------------------------------------------------------

#include "ControlInputs.h"
#include "Settings.h"

#include <cstdint>

namespace gm {

class HitGestureDetector {
public:
	void configure(const GestureSettings & settings) { settings_ = settings; }

	/// Call every frame. Returns true on the frame the gesture completes.
	bool update(const ControlInputs & inputs, uint64_t nowMs);

	void reset();

	int hitsCounted() const { return hits_; }

private:
	enum class State { Idle, InPeak, InTrough };

	int countAbove(const ControlInputs & inputs, float threshold) const;
	int countBelow(const ControlInputs & inputs, float threshold) const;

	GestureSettings settings_;
	State state_ = State::Idle;
	uint64_t stateStartMs_ = 0;
	int hits_ = 0;
};

} // namespace gm
