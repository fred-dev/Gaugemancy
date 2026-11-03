#include "ControlPanel.h"

#define OFXGPIO_NO_LEGACY
#include "ofxGPIO.h"

#include <chrono>
#include <thread>

namespace gm {

struct ControlPanel::Pins {
	ofxgpio::GpioPin button;
	ofxgpio::GpioPin blueLed;
	ofxgpio::GpioPin redLed;
	ofxgpio::GpioPin relay;
};

ControlPanel::ControlPanel() : pins_(std::make_unique<Pins>()) {}

ControlPanel::~ControlPanel() {
	allOff();
}

namespace {
// Request an output pin if configured (pin < 0 means "not fitted").
void openOutput(ofxgpio::GpioPin & pin, int number, const char * what, std::string & report, bool & ok) {
	if (number < 0) return;
	if (!pin.openOutput(number)) {
		report += std::string(what) + ": " + pin.lastError() + "\n";
		ok = false;
	}
}
} // namespace

bool ControlPanel::open(const HardwareSettings & settings, std::string & report) {
	settings_ = settings;
	ofxgpio::GpioPin::setConsumerLabel("gaugemancy");
	bool ok = true;

	ofxgpio::GpioPin::InputOptions buttonOptions;
	buttonOptions.activeLow = settings.buttonActiveLow;
	// An active-low button is wired to ground and needs a pull-up (and vice versa).
	buttonOptions.bias = settings.buttonActiveLow ? ofxgpio::Bias::PullUp : ofxgpio::Bias::PullDown;
	buttonOptions.debounceMicros = 5000;
	if (!pins_->button.openInput(settings.buttonPin, buttonOptions)) {
		report += "button: " + pins_->button.lastError() + "\n";
		ok = false;
	}
	openOutput(pins_->blueLed, settings.blueLedPin, "blue LED", report, ok);
	openOutput(pins_->redLed, settings.redLedPin, "red LED", report, ok);
	openOutput(pins_->relay, settings.relayPin, "relay", report, ok);
	return ok;
}

void ControlPanel::close() {
	allOff();
	pins_ = std::make_unique<Pins>();
}

bool ControlPanel::isOpen() const {
	return pins_->button.isOpen() || pins_->blueLed.isOpen();
}

bool ControlPanel::buttonPressed() const {
	return pins_->button.isOpen() && pins_->button.read();
}

void ControlPanel::pulseRelay(uint64_t nowMs) {
	if (!pins_->relay.isOpen()) return;
	pins_->relay.write(true);
	relayActive_ = true;
	relayOffAtMs_ = nowMs + static_cast<uint64_t>(settings_.relayPulseMs);
}

void ControlPanel::pulseRelayBlocking(int pulseMs) {
	if (!pins_->relay.isOpen()) return;
	pins_->relay.write(true);
	std::this_thread::sleep_for(std::chrono::milliseconds(pulseMs));
	pins_->relay.write(false);
	relayActive_ = false;
}

void ControlPanel::update(uint64_t nowMs) {
	if (relayActive_ && nowMs >= relayOffAtMs_) {
		pins_->relay.write(false);
		relayActive_ = false;
	}
	LedPattern now = pattern_.at(nowMs);
	if (pins_->blueLed.isOpen() && blueState_ != int(now.blue)) {
		pins_->blueLed.write(now.blue);
		blueState_ = int(now.blue);
	}
	if (pins_->redLed.isOpen() && redState_ != int(now.red)) {
		pins_->redLed.write(now.red);
		redState_ = int(now.red);
	}
}

void ControlPanel::allOff() {
	if (!pins_) return;
	if (pins_->blueLed.isOpen()) pins_->blueLed.write(false);
	if (pins_->redLed.isOpen()) pins_->redLed.write(false);
	if (pins_->relay.isOpen()) pins_->relay.write(false);
	relayActive_ = false;
	blueState_ = redState_ = 0;
}

} // namespace gm
