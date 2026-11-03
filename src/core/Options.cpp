#include "Options.h"

#include <cstdlib>

namespace gm {

std::string helpText() {
	return R"(Gaugemancy - granular instrument

usage: Gaugemancy [options]

  --data <dir>            data folder (default: /media/Data on the Pi if it
                          exists, otherwise the app's bin/data folder)
  --profile <name>        performer profile to load (default: from gaugemancy.json)
  --headless              no window (the instrument on the Pi)
  --gui                   force the programming window
  --sensors <mode>        hardware | sim | replay:<file.csv> | auto (default)
  --no-loop               play a replay once instead of looping
  --record <file.csv>     record raw sensor frames while running
  --no-audio              do not open a sound device
  --log <level>           verbose | notice | warning | error | silent
  --log-file <file>       also write the log to a file

  --selftest              check settings, samples, GPIO, ADC, accelerometer
                          and audio devices, print a report and exit
  --benchmark [seconds]   render every preset offline with simulated squeezes
                          and report the CPU load, then exit
  --list-audio            list audio output devices and exit
  --help                  this text
)";
}

Options parseCommandLine(const std::vector<std::string> & args) {
	Options o;
	auto needValue = [&](size_t & i, const std::string & flag) -> std::string {
		if (i + 1 >= args.size()) {
			o.errors.push_back(flag + " needs a value");
			return "";
		}
		return args[++i];
	};

	for (size_t i = 1; i < args.size(); ++i) {
		const std::string & a = args[i];
		if (a == "--data") {
			o.dataRoot = needValue(i, a);
		} else if (a == "--profile") {
			o.profile = needValue(i, a);
		} else if (a == "--headless") {
			o.headless = true;
		} else if (a == "--gui") {
			o.forceGui = true;
		} else if (a == "--sensors") {
			std::string mode = needValue(i, a);
			if (mode == "hardware" || mode == "hw") {
				o.sensors = SensorMode::Hardware;
			} else if (mode == "sim" || mode == "simulation") {
				o.sensors = SensorMode::Simulation;
			} else if (mode == "auto") {
				o.sensors = SensorMode::Auto;
			} else if (mode.rfind("replay:", 0) == 0 && mode.size() > 7) {
				o.sensors = SensorMode::Replay;
				o.replayFile = mode.substr(7);
			} else {
				o.errors.push_back("unknown sensor mode '" + mode + "'");
			}
		} else if (a == "--no-loop") {
			o.loopReplay = false;
		} else if (a == "--record") {
			o.recordFile = needValue(i, a);
		} else if (a == "--no-audio") {
			o.noAudio = true;
		} else if (a == "--log") {
			o.logLevel = needValue(i, a);
		} else if (a == "--log-file") {
			o.logFile = needValue(i, a);
		} else if (a == "--selftest") {
			o.command = Command::SelfTest;
		} else if (a == "--benchmark") {
			o.command = Command::Benchmark;
			if (i + 1 < args.size() && !args[i + 1].empty() && args[i + 1][0] != '-') {
				o.benchmarkSeconds = static_cast<float>(std::atof(args[++i].c_str()));
				if (o.benchmarkSeconds <= 0.0f) o.benchmarkSeconds = 10.0f;
			}
		} else if (a == "--list-audio") {
			o.command = Command::ListAudio;
		} else if (a == "--help" || a == "-h") {
			o.command = Command::Help;
		} else if (a.rfind("-NS", 0) == 0) {
			// macOS adds -NSDocumentRevisionsDebugMode when run from Xcode.
			if (i + 1 < args.size()) ++i;
		} else {
			o.errors.push_back("unknown option '" + a + "'");
		}
	}
	return o;
}

} // namespace gm
