#pragma once

// -----------------------------------------------------------------------------
// ofApp: the openFrameworks entry point. Deliberately thin:
//   * the instrument itself lives in gm::Instrument (src/app)
//   * the programming window lives in gm::ProgrammingGui (src/gui), created
//     only when there is a window (not on the headless Raspberry Pi build)
// -----------------------------------------------------------------------------

#include "ofMain.h"

#include "app/FramePacer.h"
#include "app/Instrument.h"
#include "core/Options.h"

#include <memory>

namespace gm {
class ProgrammingGui;
}

class ofApp : public ofBaseApp {
public:
	ofApp(gm::Options options, std::string dataRoot, bool withGui);
	~ofApp() override;

	void setup() override;
	void update() override;
	void draw() override;
	void exit() override;

	void keyPressed(int key) override;
	void mouseMoved(int x, int y) override;
	void mouseDragged(int x, int y, int button) override;
	void mousePressed(int x, int y, int button) override;
	void mouseReleased(int x, int y, int button) override;
	void windowResized(int w, int h) override;
	void dragEvent(ofDragInfo info) override;

private:
	gm::Options options_;
	std::string dataRoot_;
	bool withGui_;
	gm::Instrument instrument_;
	gm::FramePacer pacer_;
	std::unique_ptr<gm::ProgrammingGui> gui_;
};
