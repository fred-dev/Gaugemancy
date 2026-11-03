#pragma once

// -----------------------------------------------------------------------------
// Playhead: the position used in accumulated mode.
//
// While the total pressure is above zero the playhead moves forward through
// the sample at `speed` (fractions of the file per second); it holds still
// when the balls are released and never runs past `end`, so a performance
// walks once through the file. Changing preset rewinds it.
// -----------------------------------------------------------------------------

#include <algorithm>

namespace gm {

class Playhead {
public:
	static constexpr float kDefaultEnd = 0.985f; // stop just before the file end

	void rewind() { position_ = 0.0f; }

	/// Advance while `moving`. Returns the new position (0..end).
	float advance(float dtSeconds, bool moving, float speed, float end = kDefaultEnd) {
		if (moving) {
			position_ = std::min(position_ + speed * std::max(dtSeconds, 0.0f), end);
		}
		return position_;
	}

	float position() const { return position_; }
	void setPosition(float position) { position_ = std::clamp(position, 0.0f, 1.0f); }

private:
	float position_ = 0.0f;
};

} // namespace gm
