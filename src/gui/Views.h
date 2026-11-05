#pragma once

// -----------------------------------------------------------------------------
// Visual widgets of the programming window (drawing only, no settings logic).
//
//   WaveformView  a sample's waveform with the live grains, the playhead and
//                 the selection outline. Clicking/dragging in it auditions the
//                 slot at that position.
//   SensorView    six live sensor bars, total pressure and tilt, plus a
//                 preview of the response curve.
// -----------------------------------------------------------------------------

#include "../core/ControlInputs.h"
#include "../core/Curve.h"

#include "ofMain.h"
#include "ofxPDSP.h"

#include <functional>
#include <string>
#include <vector>

namespace gm {

struct GrainMeter {
	std::function<int()> voices;
	std::function<float(int)> position;
	std::function<float(int)> envelope;
};

class WaveformView {
public:
	void setRect(const ofRectangle & rect) { rect_ = rect; dirty_ = true; }
	const ofRectangle & rect() const { return rect_; }

	/// Draw the waveform of `sample` (cached until the buffer changes).
	void draw(pdsp::SampleBuffer * sample, const std::string & title, const GrainMeter & grains,
		float playhead, float volume, bool selected, bool active);

	/// Draw a live trace of `samples` instead of a static waveform - for a
	/// synth/generator source, which has no sample to plot.
	void drawLive(const std::vector<float> & samples, const std::string & title, float volume, bool selected, bool active);

	/// Position (0..1) of a screen x inside the view.
	float positionAt(float x) const;
	bool inside(float x, float y) const { return rect_.inside(x, y); }

private:
	ofRectangle rect_;
	pdsp::SampleBufferPlotter plotter_;
	const pdsp::SampleBuffer * plotted_ = nullptr;
	bool dirty_ = true;
};

class SensorView {
public:
	void setRect(const ofRectangle & rect) { rect_ = rect; }
	void draw(const ControlInputs & inputs, Curve curve, float threshold);

private:
	void drawCurve(const ofRectangle & area, Curve curve, float threshold);
	ofRectangle rect_;
};

} // namespace gm
