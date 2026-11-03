#pragma once

// -----------------------------------------------------------------------------
// FramePacer: keeps the control loop at a steady rate by SLEEPING.
//
// Why not ofSetFrameRate()? In current openFrameworks the frame limiter sleeps
// until 8 ms before the next frame and then polls every 5 microseconds. At the
// instrument's 100 Hz control rate that is about a quarter of a CPU core spent
// waiting, competing with the audio thread on a Raspberry Pi. A plain sleep is
// accurate to about a millisecond on Linux, which is plenty for reading
// pressure sensors, and costs nothing.
// -----------------------------------------------------------------------------

#include <chrono>
#include <thread>

namespace gm {

class FramePacer {
public:
	using Clock = std::chrono::steady_clock;

	void setRate(int framesPerSecond) {
		interval_ = std::chrono::microseconds(1000000 / (framesPerSecond > 0 ? framesPerSecond : 100));
		next_ = Clock::now() + interval_;
	}

	/// Sleep until the next frame is due. If a frame ran late, the schedule is
	/// reset rather than trying to "catch up" with a burst of frames.
	void wait() {
		Clock::time_point now = Clock::now();
		if (next_ > now) {
			std::this_thread::sleep_until(next_);
			next_ += interval_;
		} else {
			next_ = now + interval_;
		}
	}

private:
	std::chrono::microseconds interval_ { 10000 };
	Clock::time_point next_ = Clock::now();
};

} // namespace gm
