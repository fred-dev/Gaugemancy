#pragma once

// -----------------------------------------------------------------------------
// Command line options. Run `Gaugemancy --help` for the user-facing list.
//
// Typical uses:
//   Gaugemancy                               laptop: GUI, simulated sensors
//   Gaugemancy --headless                    Pi: no window, hardware sensors
//   Gaugemancy --selftest                    check hardware, files, audio; exit
//   Gaugemancy --benchmark 20                render 20 s offline, report CPU
//   Gaugemancy --sensors replay:take1.csv    play back a recording
//   Gaugemancy --record take1.csv            record the sensors while playing
// -----------------------------------------------------------------------------

#include <string>
#include <vector>

namespace gm {

enum class SensorMode {
	Auto,        ///< hardware if it opens, otherwise simulation
	Hardware,
	Simulation,
	Replay
};

enum class Command {
	Run,
	SelfTest,
	Benchmark,
	ListAudio,
	Help
};

struct Options {
	Command command = Command::Run;
	std::string dataRoot;           ///< empty = platform default
	std::string profile;            ///< empty = the profile named in gaugemancy.json
	bool headless = false;
	bool forceGui = false;
	SensorMode sensors = SensorMode::Auto;
	std::string replayFile;
	bool loopReplay = true;
	std::string recordFile;
	float benchmarkSeconds = 10.0f;
	std::string logLevel;           ///< empty = from the profile
	std::string logFile;
	bool noAudio = false;           ///< run without opening a sound device
	std::vector<std::string> errors;
};

/// Parse argv. Unknown options are reported in Options::errors.
Options parseCommandLine(const std::vector<std::string> & args);

/// The text printed by --help.
std::string helpText();

} // namespace gm
