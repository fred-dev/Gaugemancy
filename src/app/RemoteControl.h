#pragma once

// -----------------------------------------------------------------------------
// RemoteControl: OSC commands in, telemetry out. This is how the instrument is
// driven and watched over the network (e.g. from a laptop while SSH'd into the
// Pi), using tools/gm-ctl.py or any OSC app.
//
// Commands (UDP, port osc.controlPort, default 1234):
//   /gm/preset/next                 next preset
//   /gm/preset <int>                select preset (1-based)
//   /gm/narration/skip              end the narration, go to the granular
//   /gm/sensors/source <string>     "hardware", "sim" or "replay:<file>"
//   /gm/sim/sensor <int> <float>    simulated level of sensor 1..6
//   /gm/sim/ramp <float speed> [<int mask>]  start a squeeze ramp
//   /gm/sim/accel <f x> <f y> <f z> simulated accelerometer reading in g
//   /gm/calibrate                   re-measure resting values and "level"
//   /gm/reload                      reload the profile from disk
//   /gm/status                      reply with /gm/state (to the sender)
//   /gm/record <string file>        start recording sensors ("" stops)
//   /gm/quit                        quit the app
//
// Telemetry (to osc.remoteHost:remotePort when osc.telemetry is on, and to
// whoever sent /gm/status):
//   /gm/sensors f f f f f f f      six sensor levels + total pressure
//   /gm/state s i s i              mode, preset (1-based), sensor source,
//                                  skipped audio buffers
// -----------------------------------------------------------------------------

#include "../core/Settings.h"

#include "ofxOsc.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace gm {

struct RemoteCommand {
	std::string address;          ///< e.g. "/gm/preset"
	std::vector<float> numbers;   ///< numeric arguments in order
	std::vector<std::string> texts; ///< string arguments in order
	std::string replyHost;        ///< where the message came from
};

struct Telemetry {
	ControlInputs inputs;
	std::string mode;
	int preset = 1;
	std::string sensorSource;
	uint64_t skippedBuffers = 0;
};

class RemoteControl {
public:
	/// Start listening / sending as configured. Returns a description.
	std::string setup(const OscSettings & settings);

	/// Commands received since the last call.
	std::vector<RemoteCommand> poll();

	/// Send telemetry if enabled and due (rate limited).
	void sendTelemetry(const Telemetry & telemetry, uint64_t nowMs);

	/// Answer a /gm/status request.
	void replyStatus(const std::string & host, const Telemetry & telemetry);

private:
	ofxOscMessage sensorsMessage(const Telemetry & t) const;
	ofxOscMessage stateMessage(const Telemetry & t) const;

	OscSettings settings_;
	std::unique_ptr<ofxOscReceiver> receiver_;
	std::unique_ptr<ofxOscSender> sender_;
	uint64_t lastTelemetryMs_ = 0;
};

} // namespace gm
