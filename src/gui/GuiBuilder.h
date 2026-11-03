#pragma once

// -----------------------------------------------------------------------------
// GuiBuilder: builds ofxGui controls that are bound directly to settings
// fields. Changing a control writes the field and calls an optional callback;
// nothing else has to be kept in sync.
//
//   GuiBuilder b;
//   ofxGuiGroup * g = b.group("Sensors");
//   b.floatSlider(*g, "Threshold", settings.activeThreshold, 0, 1);
//   b.choice(*g, "Curve", curveIndex, curveNames, [&](int i) { ... });
//   panel.add(g);
//
// The builder owns every parameter, control and listener it creates; clear()
// destroys them. Bound references must outlive the controls, so panels are
// rebuilt (clear + build again) whenever the structure they show changes.
// -----------------------------------------------------------------------------

#include "../core/ModulatedParam.h"

#include "ofxGui.h"

#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace gm {

class GuiBuilder {
public:
	using Callback = std::function<void()>;

	/// Destroy everything this builder made.
	void clear();

	/// A new (owned) group, to be added to a panel or another group.
	ofxGuiGroup * group(const std::string & name, bool minimized = false);

	void floatSlider(ofxGuiGroup & g, const std::string & label, float & target, float lo, float hi, Callback onChange = {});
	void intSlider(ofxGuiGroup & g, const std::string & label, int & target, int lo, int hi, Callback onChange = {});
	void toggle(ofxGuiGroup & g, const std::string & label, bool & target, Callback onChange = {});
	void text(ofxGuiGroup & g, const std::string & label, std::string & target, Callback onChange = {});
	void button(ofxGuiGroup & g, const std::string & label, Callback onClick);
	void label(ofxGuiGroup & g, const std::string & text);

	/// Pick one of `names` (shown as a slider plus the chosen name).
	void choice(ofxGuiGroup & g, const std::string & label, int current, const std::vector<std::string> & names,
		std::function<void(int)> onPick);

	/// A file name field with a "browse" button. Picked files outside
	/// `baseDir` are copied into it, so a profile folder stays self-contained.
	void pathField(ofxGuiGroup & g, const std::string & label, std::string & target, const std::string & baseDir,
		Callback onChange = {});

	/// value / min / max sliders and a source choice for one ModulatedParam.
	ofxGuiGroup * modulated(ofxGuiGroup & g, const ParamSpec & spec, ModulatedParam & target, Callback onChange = {},
		bool allowSources = true);

	/// Labels of every modulation source, in enum order.
	static std::vector<std::string> sourceNames();

	/// Copy `path` into `baseDir` if it is not already there; returns the file
	/// name to store in the settings, or "" on failure.
	static std::string adoptFile(const std::string & path, const std::string & baseDir, std::string & error);

private:
	template <typename T>
	ofParameter<T> & makeParam();

	std::deque<std::unique_ptr<ofAbstractParameter>> params_;
	std::vector<std::unique_ptr<ofxBaseGui>> controls_;
	ofEventListeners listeners_; ///< ofEventListener cannot live in a std::vector
};

} // namespace gm
