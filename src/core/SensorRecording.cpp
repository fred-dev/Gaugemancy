#include "SensorRecording.h"

#include <algorithm>
#include <sstream>

namespace gm {

// ---------------------------------------------------------------- recorder --

bool SensorRecorder::open(const std::string & path, std::string & error) {
	out_.open(path, std::ios::trunc);
	if (!out_) {
		error = "cannot write " + path;
		return false;
	}
	out_ << "time_ms,s1,s2,s3,s4,s5,s6,ax,ay,az\n";
	started_ = false;
	return true;
}

void SensorRecorder::write(uint64_t timeMs, const RawSensorFrame & raw, const AccelSample & accel) {
	if (!out_.is_open()) {
		return;
	}
	if (!started_) {
		startMs_ = timeMs;
		started_ = true;
	}
	out_ << (timeMs - startMs_);
	for (int value : raw) {
		out_ << ',' << value;
	}
	out_ << ',' << accel.x << ',' << accel.y << ',' << accel.z << '\n';
}

void SensorRecorder::close() {
	if (out_.is_open()) {
		out_.close();
	}
}

// ---------------------------------------------------------------- playback --

bool SensorPlayback::parseLine(const std::string & line, RecordedFrame & frame) {
	if (line.empty() || line[0] == '#' || line[0] == 't') {
		return false; // blank, comment or header
	}
	std::vector<double> values;
	std::stringstream stream(line);
	std::string cell;
	while (std::getline(stream, cell, ',')) {
		try {
			values.push_back(std::stod(cell));
		} catch (...) {
			return false;
		}
	}
	if (values.size() < 1 + static_cast<size_t>(kNumSensors)) {
		return false;
	}
	frame.timeMs = static_cast<uint64_t>(std::max(values[0], 0.0));
	for (int i = 0; i < kNumSensors; ++i) {
		frame.raw[i] = std::clamp(static_cast<int>(values[1 + i]), 0, 1023);
	}
	if (values.size() >= 10) {
		frame.accel.x = static_cast<float>(values[7]);
		frame.accel.y = static_cast<float>(values[8]);
		frame.accel.z = static_cast<float>(values[9]);
		frame.accel.valid = true;
	}
	return true;
}

bool SensorPlayback::load(const std::string & path, std::string & error) {
	std::ifstream in(path);
	if (!in) {
		error = "cannot open " + path;
		return false;
	}
	frames_.clear();
	std::string line;
	while (std::getline(in, line)) {
		if (!line.empty() && line.back() == '\r') {
			line.pop_back(); // files edited on Windows
		}
		RecordedFrame frame;
		if (parseLine(line, frame)) {
			frames_.push_back(frame);
		}
	}
	std::stable_sort(frames_.begin(), frames_.end(), [](const RecordedFrame & a, const RecordedFrame & b) {
		return a.timeMs < b.timeMs;
	});
	if (frames_.empty()) {
		error = path + " contains no sensor frames";
		return false;
	}
	return true;
}

const RecordedFrame & SensorPlayback::frameAt(uint64_t elapsedMs, bool loop) const {
	static const RecordedFrame silent;
	if (frames_.empty()) {
		return silent;
	}
	uint64_t t = elapsedMs;
	if (loop && durationMs() > 0) {
		t %= (durationMs() + 1);
	}
	// Last frame whose timestamp is <= t.
	auto it = std::upper_bound(frames_.begin(), frames_.end(), t, [](uint64_t time, const RecordedFrame & f) {
		return time < f.timeMs;
	});
	return it == frames_.begin() ? frames_.front() : *(it - 1);
}

} // namespace gm
