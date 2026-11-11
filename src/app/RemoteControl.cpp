#include "RemoteControl.h"

namespace gm {

std::string RemoteControl::setup(const OscSettings & settings) {
	settings_ = settings;
	std::string report;
	receiver_.reset();
	sender_.reset();
	if (settings.control) {
		receiver_ = std::make_unique<ofxOscReceiver>();
		if (receiver_->setup(settings.controlPort)) {
			report += "OSC control on port " + std::to_string(settings.controlPort);
		} else {
			report += "OSC control port " + std::to_string(settings.controlPort) + " unavailable";
			receiver_.reset();
		}
	}
	if (settings.telemetry) {
		sender_ = std::make_unique<ofxOscSender>();
		sender_->setup(settings.remoteHost, settings.remotePort);
		report += (report.empty() ? "" : ", ") + std::string("telemetry to ")
			+ settings.remoteHost + ":" + std::to_string(settings.remotePort);
	}
	return report;
}

std::vector<RemoteCommand> RemoteControl::poll() {
	std::vector<RemoteCommand> commands;
	if (!receiver_) return commands;
	ofxOscMessage message;
	while (receiver_->getNextMessage(message)) {
		RemoteCommand command;
		command.address = message.getAddress();
		command.replyHost = message.getRemoteHost();
		for (size_t i = 0; i < message.getNumArgs(); ++i) {
			switch (message.getArgType(i)) {
			case OFXOSC_TYPE_INT32:
			case OFXOSC_TYPE_INT64:
			case OFXOSC_TYPE_FLOAT:
			case OFXOSC_TYPE_DOUBLE:
				command.numbers.push_back(message.getArgAsFloat(i));
				break;
			case OFXOSC_TYPE_STRING:
			case OFXOSC_TYPE_SYMBOL:
				command.texts.push_back(message.getArgAsString(i));
				break;
			default:
				break;
			}
		}
		commands.push_back(std::move(command));
	}
	return commands;
}

ofxOscMessage RemoteControl::sensorsMessage(const Telemetry & t) const {
	ofxOscMessage m;
	m.setAddress("/gm/sensors");
	for (int i = 0; i < kNumSensors; ++i) {
		m.addFloatArg(t.inputs.sensor(i));
	}
	m.addFloatArg(t.inputs.get(ModSource::Total));
	return m;
}

ofxOscMessage RemoteControl::stateMessage(const Telemetry & t) const {
	ofxOscMessage m;
	m.setAddress("/gm/state");
	m.addStringArg(t.mode);
	m.addIntArg(t.preset);
	m.addStringArg(t.sensorSource);
	m.addInt64Arg(static_cast<int64_t>(t.skippedBuffers));
	return m;
}

bool RemoteControl::telemetryDue(uint64_t nowMs) {
	if (!sender_) return false;
	uint64_t interval = 1000 / static_cast<uint64_t>(std::max(settings_.telemetryRateHz, 1));
	if (nowMs - lastTelemetryMs_ < interval) return false;
	lastTelemetryMs_ = nowMs;
	return true;
}

void RemoteControl::sendTelemetry(const Telemetry & telemetry) {
	if (!sender_) return;
	sender_->sendMessage(sensorsMessage(telemetry), false);
	sender_->sendMessage(stateMessage(telemetry), false);
}

void RemoteControl::replyStatus(const std::string & host, const Telemetry & telemetry) {
	if (host.empty()) return;
	// Replies go to the sender's host on the telemetry port.
	ofxOscSender reply;
	reply.setup(host, settings_.remotePort);
	reply.sendMessage(stateMessage(telemetry), false);
	reply.sendMessage(sensorsMessage(telemetry), false);
}

} // namespace gm
