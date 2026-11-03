#include "Views.h"

#include <algorithm>
#include <cstring>

namespace gm {

namespace {
const ofColor kFrame(0, 200, 200);
const ofColor kSelected(255, 200, 0);
const ofColor kWave(0, 100, 100);
const ofColor kText(230);
} // namespace

// ---------------------------------------------------------------- WaveformView

float WaveformView::positionAt(float x) const {
	if (rect_.width <= 0.0f) return 0.0f;
	return std::clamp((x - rect_.x) / rect_.width, 0.0f, 1.0f);
}

void WaveformView::draw(pdsp::SampleBuffer * sample, const std::string & title, const GrainMeter & grains,
	float playhead, float volume, bool selected, bool active) {
	ofPushStyle();
	ofNoFill();

	if (sample != nullptr && (dirty_ || sample != plotted_)) {
		plotter_.setWaveform(*sample, 0, kWave, static_cast<int>(rect_.width), static_cast<int>(rect_.height));
		plotted_ = sample;
		dirty_ = false;
	}
	if (sample != nullptr) {
		ofSetColor(255);
		plotter_.draw(static_cast<int>(rect_.x), static_cast<int>(rect_.y));
	} else {
		ofSetColor(120);
		ofDrawBitmapString("no sample", rect_.x + 8, rect_.getCenter().y);
	}

	// Grains: one bar per voice, height = envelope.
	if (active && grains.voices) {
		ofFill();
		ofSetColor(255, 255, 255, 180);
		float mid = rect_.getCenter().y;
		for (int v = 0; v < grains.voices(); ++v) {
			float env = grains.envelope(v);
			if (env <= 0.001f) continue;
			float x = rect_.x + rect_.width * grains.position(v);
			float h = env * rect_.height * 0.45f;
			ofDrawRectangle(x - 1.5f, mid - h, 3.0f, 2.0f * h);
		}
		ofNoFill();
	}

	// Playhead and volume.
	float px = rect_.x + rect_.width * std::clamp(playhead, 0.0f, 1.0f);
	ofSetColor(active ? ofColor(255, 80, 80) : ofColor(120, 60, 60));
	ofDrawLine(px, rect_.y, px, rect_.getBottom());
	float vy = rect_.getBottom() - rect_.height * std::clamp(volume, 0.0f, 1.0f);
	ofSetColor(80, 160, 255);
	ofDrawLine(rect_.x, vy, rect_.getRight(), vy);

	ofSetColor(selected ? kSelected : kFrame);
	ofSetLineWidth(selected ? 3.0f : 1.0f);
	ofDrawRectangle(rect_);
	ofSetColor(kText);
	ofDrawBitmapString(title, rect_.x + 6, rect_.y + 14);
	ofPopStyle();
}

// ------------------------------------------------------------------ SensorView

void SensorView::draw(const ControlInputs & inputs, Curve curve, float threshold) {
	ofPushStyle();
	const float gap = 8.0f;
	const int bars = kNumSensors + 5; // sensors, total, tilt x/y/any, shake
	float curveSize = std::min(rect_.height, 160.0f);
	ofRectangle barArea(rect_.x, rect_.y, rect_.width - curveSize - gap, rect_.height);
	float barWidth = (barArea.width - gap * (bars - 1)) / bars;

	const char * labels[] = { "1", "2", "3", "4", "5", "6", "sum", "tx", "ty", "tlt", "shk" };
	for (int i = 0; i < bars; ++i) {
		float value = i < kNumSensors ? inputs.sensor(i)
			: i == kNumSensors ? inputs.get(ModSource::Total)
			: inputs.get(static_cast<ModSource>(static_cast<int>(ModSource::TiltX) + (i - kNumSensors - 1)));
		ofRectangle bar(barArea.x + i * (barWidth + gap), barArea.y, barWidth, barArea.height - 14);
		ofNoFill();
		ofSetColor(80);
		ofDrawRectangle(bar);
		ofFill();
		ofSetColor(i < kNumSensors ? ofColor(0, 200, 120) : ofColor(200, 120, 0));
		float h = bar.height * std::clamp(value, 0.0f, 1.0f);
		ofDrawRectangle(bar.x, bar.getBottom() - h, bar.width, h);
		// Labels only when they fit (bitmap font: 8 px per character).
		if (barWidth + gap >= 8.0f * std::strlen(labels[i])) {
			ofSetColor(200);
			ofDrawBitmapString(labels[i], bar.x + 2, rect_.getBottom());
		}
	}
	drawCurve(ofRectangle(rect_.getRight() - curveSize, rect_.y, curveSize, curveSize), curve, threshold);
	ofPopStyle();
}

void SensorView::drawCurve(const ofRectangle & area, Curve curve, float threshold) {
	ofNoFill();
	ofSetColor(80);
	ofDrawRectangle(area);
	ofSetColor(255, 80, 80);
	ofPolyline line;
	for (int i = 0; i <= 64; ++i) {
		float x = i / 64.0f;
		float y = x < threshold ? 0.0f : applyCurve(curve, x);
		line.addVertex(area.x + x * area.width, area.getBottom() - y * area.height);
	}
	line.draw();
	ofSetColor(200);
	ofDrawBitmapString(curveLabel(curve), area.x + 4, area.y + 14);
}

} // namespace gm
