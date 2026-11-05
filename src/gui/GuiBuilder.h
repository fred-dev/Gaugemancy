#pragma once

// -----------------------------------------------------------------------------
// GuiBuilder: thin wrapper over ImGui widgets, bound directly to settings
// fields. Unlike the old ofxGui-based version, nothing here is retained
// across frames - every call just draws one widget for the current value and
// writes straight back into the bound reference when it changes. There is no
// group/control/listener lifetime to manage and no "rebuild the panel" dance;
// call these every frame from inside an ImGui window.
//
//   if (b.group("Sensors")) {
//       b.floatSlider("Threshold", settings.activeThreshold, 0, 1);
//       b.endGroup();
//   }
//
// Callers looping over repeated items (slots, effects) must wrap the loop
// body in ImGui::PushID(index)/PopID() themselves so widget labels don't
// collide across iterations - GuiBuilder does not do this for you.
// -----------------------------------------------------------------------------

#include "../core/ModulatedParam.h"

#include <functional>
#include <string>
#include <vector>

namespace gm {

class GuiBuilder {
public:
	using Callback = std::function<void()>;

	/// Collapsible section. Only draw contents (and call endGroup()) if this
	/// returns true - mirrors ImGui::TreeNode()/TreePop() pairing.
	bool group(const std::string & name, bool defaultOpen = true);
	void endGroup();

	bool floatSlider(const std::string & label, float & target, float lo, float hi, Callback onChange = {});
	bool intSlider(const std::string & label, int & target, int lo, int hi, Callback onChange = {});
	bool toggle(const std::string & label, bool & target, Callback onChange = {});
	bool text(const std::string & label, std::string & target, Callback onChange = {});
	bool button(const std::string & label);
	void label(const std::string & text);

	/// Dropdown over `names`; returns true and updates `current` on pick.
	bool choice(const std::string & label, int & current, const std::vector<std::string> & names);

	/// A file name field with a "browse" button alongside. Picked files
	/// outside `baseDir` are copied into it, so a profile folder stays
	/// self-contained.
	bool pathField(const std::string & label, std::string & target, const std::string & baseDir, Callback onChange = {});

	/// value / min / max sliders and a source choice for one ModulatedParam,
	/// in a collapsible section titled with spec.label. Only draws the
	/// min/max/source controls when allowSources is true.
	bool modulated(const ParamSpec & spec, ModulatedParam & target, Callback onChange = {}, bool allowSources = true);

	/// Labels of every modulation source, in enum order.
	static std::vector<std::string> sourceNames();

	/// Copy `path` into `baseDir` if it is not already there; returns the file
	/// name to store in the settings, or "" on failure.
	static std::string adoptFile(const std::string & path, const std::string & baseDir, std::string & error);
};

} // namespace gm
