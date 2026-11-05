// -----------------------------------------------------------------------------
// Gaugemancy entry point.
//
// The same program runs as:
//   * the instrument on the Raspberry Pi (--headless, no window)
//   * the programming tool on macOS/Windows/Linux (window + GUI)
//   * command line tools (--selftest, --benchmark, --list-audio, --help)
// -----------------------------------------------------------------------------

#include "ofMain.h"
#include "ofAppNoWindow.h"

#include "app/Diagnostics.h"
#include "core/Options.h"
#include "ofApp.h"

#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

void setStopFlag(std::atomic<bool> * flag); // ofApp.cpp

namespace {

std::atomic<bool> stopRequested { false };

void onSignal(int) {
	stopRequested = true; // handled in ofApp::update(), which exits cleanly
}

/// Where settings and audio live unless --data says otherwise.
std::string defaultDataRoot() {
	// The instrument reads a USB stick mounted at /media/Data (see docs/RaspberryPi.md).
	std::error_code ec;
	if (std::filesystem::exists("/media/Data/gaugemancy.json", ec)) {
		return "/media/Data";
	}
	return ofToDataPath("", true);
}

/// A window is used unless --headless, or there is no display (SSH, systemd).
bool wantWindow(const gm::Options & options) {
	if (options.forceGui) return true;
	if (options.headless) return false;
#if defined(TARGET_LINUX)
	const char * display = std::getenv("DISPLAY");
	const char * wayland = std::getenv("WAYLAND_DISPLAY");
	return (display != nullptr && *display != '\0') || (wayland != nullptr && *wayland != '\0');
#else
	return true;
#endif
}

} // namespace

int main(int argc, char ** argv) {
	std::vector<std::string> args(argv, argv + argc);
	gm::Options options = gm::parseCommandLine(args);
	for (const std::string & error : options.errors) {
		std::fprintf(stderr, "error: %s\n", error.c_str());
	}
	if (!options.errors.empty()) {
		std::fprintf(stderr, "run with --help for the list of options\n");
		return 2;
	}
	if (options.command == gm::Command::Help) {
		std::printf("%s", gm::helpText().c_str());
		return 0;
	}

	std::string dataRoot = options.dataRoot.empty() ? defaultDataRoot() : options.dataRoot;

	switch (options.command) {
	case gm::Command::SelfTest: return gm::runSelfTest(options, dataRoot);
	case gm::Command::Benchmark: return gm::runBenchmark(options, dataRoot);
	case gm::Command::ListAudio: return gm::runListAudio(options, dataRoot);
	default: break;
	}

	std::signal(SIGINT, onSignal);
	std::signal(SIGTERM, onSignal);
	setStopFlag(&stopRequested);

	// The window must exist before the app is constructed: the audio engine
	// registers openFrameworks events in its constructor.
	bool window = wantWindow(options);
	if (window) {
		ofGLWindowSettings settings;
		settings.setSize(1920, 1080);
		settings.windowMode = OF_WINDOW;
		settings.title = "Gaugemancy";
		auto win = ofCreateWindow(settings);
		ofRunApp(win, std::make_shared<ofApp>(options, dataRoot, window));
	} else {
		auto win = std::make_shared<ofAppNoWindow>();
		ofSetupOpenGL(win, 1024, 768, OF_WINDOW); // registers the (invisible) window
		ofRunApp(win, std::make_shared<ofApp>(options, dataRoot, window));
	}
	return ofRunMainLoop();
}
