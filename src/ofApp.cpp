#include "ofApp.h"

#include "gui/ProgrammingGui.h"

#include <atomic>
#include <cstdlib>

namespace {
// Set by SIGINT/SIGTERM (Ctrl-C over SSH, systemctl stop) in main.cpp.
std::atomic<bool> * stopRequestedFlag = nullptr;
} // namespace

void setStopFlag(std::atomic<bool> * flag) {
	stopRequestedFlag = flag;
}

ofApp::ofApp(gm::Options options, std::string dataRoot, bool withGui)
	: options_(std::move(options)), dataRoot_(std::move(dataRoot)), withGui_(withGui) {}

ofApp::~ofApp() = default;

void ofApp::setup() {
	if (!options_.logFile.empty()) {
		ofLogToFile(options_.logFile, true);
	}
	instrument_.setup(options_, dataRoot_);

	// Sensors are read once per frame, so the frame rate is the control rate.
	// openFrameworks' own limiter is switched off (it busy-waits, see
	// FramePacer.h) and the loop sleeps in update() instead.
	ofSetFrameRate(0);
	ofSetVerticalSync(false);
	pacer_.setRate(instrument_.profile().controlRateHz);

	if (withGui_) {
		gui_ = std::make_unique<gm::ProgrammingGui>(instrument_);
		gui_->setup();
	}
}

void ofApp::update() {
	pacer_.wait();
	if (stopRequestedFlag != nullptr && stopRequestedFlag->load()) {
		ofExit(0);
		return;
	}
	instrument_.update(ofGetElapsedTimeMillis());
	if (gui_) {
		gui_->update();
	}
	if (instrument_.quitRequested()) {
		ofExit(0);
	}
}

void ofApp::draw() {
	if (gui_) {
		gui_->draw();
	}
}

void ofApp::exit() {
	bool powerOff = instrument_.shutdownSystemRequested();
	instrument_.shutdown();
	if (powerOff) {
		ofLogNotice("gaugemancy") << "powering off";
		// Needs passwordless sudo for systemctl (the default on Raspberry Pi OS).
		if (std::system("sudo -n systemctl poweroff") != 0) {
			ofLogError("gaugemancy") << "poweroff failed (needs passwordless sudo)";
		}
	}
}

void ofApp::keyPressed(int key) {
	if (gui_) gui_->keyPressed(key);
}

void ofApp::mouseMoved(int x, int y) {
	if (gui_) gui_->mouseMoved(x, y);
}

void ofApp::mouseDragged(int x, int y, int button) {
	if (gui_) gui_->mouseDragged(x, y, button);
}

void ofApp::mousePressed(int x, int y, int button) {
	if (gui_) gui_->mousePressed(x, y, button);
}

void ofApp::mouseReleased(int x, int y, int button) {
	if (gui_) gui_->mouseReleased(x, y, button);
}

void ofApp::windowResized(int w, int h) {
	if (gui_) gui_->windowResized(w, h);
}

void ofApp::dragEvent(ofDragInfo info) {
	if (gui_) gui_->dragEvent(info);
}
