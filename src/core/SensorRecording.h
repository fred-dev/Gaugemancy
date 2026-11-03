#pragma once

// -----------------------------------------------------------------------------
// SensorRecording: record raw sensor frames to CSV and play them back.
//
// This is how the instrument is tested without hands on it: record a
// performance on the Pi (--record take1.csv), copy the file anywhere, and
// replay it (--replay take1.csv) on the Pi over SSH or on a laptop. Replay goes
// through exactly the same calibration/normalisation path as live input.
//
// File format (one header line, then one line per control frame):
//   time_ms,s1,s2,s3,s4,s5,s6[,ax,ay,az]
//   0,212,198,240,205,220,190,0.01,-0.02,0.98
// s1..s6 are raw ADC values (0..1023); ax..az (optional) are in g.
// Lines starting with '#' are comments.
// -----------------------------------------------------------------------------

#include "SensorProcessor.h"

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace gm {

struct RecordedFrame {
	uint64_t timeMs = 0;
	RawSensorFrame raw {};
	AccelSample accel;
};

class SensorRecorder {
public:
	bool open(const std::string & path, std::string & error);
	void write(uint64_t timeMs, const RawSensorFrame & raw, const AccelSample & accel);
	void close();
	bool isOpen() const { return out_.is_open(); }

private:
	std::ofstream out_;
	uint64_t startMs_ = 0;
	bool started_ = false;
};

class SensorPlayback {
public:
	/// Load a whole recording. Returns false and sets `error` if unusable.
	bool load(const std::string & path, std::string & error);

	/// Parse one CSV line (exposed for tests). Returns false for headers,
	/// comments and malformed lines.
	static bool parseLine(const std::string & line, RecordedFrame & frame);

	/// Frame to use at `elapsedMs` since playback start (holds the last frame
	/// until the next timestamp). Loops when `loop` is true.
	const RecordedFrame & frameAt(uint64_t elapsedMs, bool loop) const;

	bool empty() const { return frames_.empty(); }
	size_t size() const { return frames_.size(); }
	uint64_t durationMs() const { return frames_.empty() ? 0 : frames_.back().timeMs; }
	bool finished(uint64_t elapsedMs) const { return elapsedMs > durationMs(); }

private:
	std::vector<RecordedFrame> frames_;
};

} // namespace gm
