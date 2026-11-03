#include "GuiBuilder.h"

#include "../core/ControlInputs.h"

#include "ofSystemUtils.h"

#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

namespace gm {

template <typename T>
ofParameter<T> & GuiBuilder::makeParam() {
	auto param = std::make_unique<ofParameter<T>>();
	ofParameter<T> & ref = *param;
	params_.push_back(std::move(param));
	return ref;
}

void GuiBuilder::clear() {
	listeners_.unsubscribeAll();
	controls_.clear();
	params_.clear();
}

ofxGuiGroup * GuiBuilder::group(const std::string & name, bool minimized) {
	auto g = std::make_unique<ofxGuiGroup>();
	g->setup(name);
	if (minimized) g->minimize();
	ofxGuiGroup * raw = g.get();
	controls_.push_back(std::move(g));
	return raw;
}

void GuiBuilder::floatSlider(ofxGuiGroup & g, const std::string & label, float & target, float lo, float hi, Callback onChange) {
	auto & p = makeParam<float>();
	p.set(label, target, lo, hi);
	listeners_.push(p.newListener([&target, onChange](float & v) {
		target = v;
		if (onChange) onChange();
	}));
	g.add(p);
}

void GuiBuilder::intSlider(ofxGuiGroup & g, const std::string & label, int & target, int lo, int hi, Callback onChange) {
	auto & p = makeParam<int>();
	p.set(label, target, lo, hi);
	listeners_.push(p.newListener([&target, onChange](int & v) {
		target = v;
		if (onChange) onChange();
	}));
	g.add(p);
}

void GuiBuilder::toggle(ofxGuiGroup & g, const std::string & label, bool & target, Callback onChange) {
	auto & p = makeParam<bool>();
	p.set(label, target);
	listeners_.push(p.newListener([&target, onChange](bool & v) {
		target = v;
		if (onChange) onChange();
	}));
	g.add(p);
}

void GuiBuilder::text(ofxGuiGroup & g, const std::string & label, std::string & target, Callback onChange) {
	auto & p = makeParam<std::string>();
	p.set(label, target);
	listeners_.push(p.newListener([&target, onChange](std::string & v) {
		target = v;
		if (onChange) onChange();
	}));
	g.add(p);
}

void GuiBuilder::button(ofxGuiGroup & g, const std::string & label, Callback onClick) {
	auto & p = makeParam<void>();
	p.setName(label);
	listeners_.push(p.newListener([onClick]() {
		if (onClick) onClick();
	}));
	g.add(p);
}

void GuiBuilder::label(ofxGuiGroup & g, const std::string & text) {
	auto & p = makeParam<std::string>();
	p.set(text, "");
	auto control = std::make_unique<ofxLabel>(p);
	g.add(control.get());
	controls_.push_back(std::move(control));
}

void GuiBuilder::choice(ofxGuiGroup & g, const std::string & label, int current, const std::vector<std::string> & names,
	std::function<void(int)> onPick) {
	if (names.empty()) return;
	auto & index = makeParam<int>();
	index.set(label, current, 0, static_cast<int>(names.size()) - 1);
	auto & shown = makeParam<std::string>();
	// Each label needs its own name: ofParameterGroup warns about duplicates.
	shown.set("  " + label + " =", names[std::clamp(current, 0, static_cast<int>(names.size()) - 1)]);
	listeners_.push(index.newListener([&shown, names, onPick](int & i) {
		shown.set(names[std::clamp(i, 0, static_cast<int>(names.size()) - 1)]);
		if (onPick) onPick(i);
	}));
	g.add(index);
	auto nameLabel = std::make_unique<ofxLabel>(shown);
	g.add(nameLabel.get());
	controls_.push_back(std::move(nameLabel));
}

std::string GuiBuilder::adoptFile(const std::string & path, const std::string & baseDir, std::string & error) {
	std::error_code ec;
	fs::path source(path);
	fs::path base(baseDir);
	fs::create_directories(base, ec);
	fs::path relative = fs::relative(source, base, ec);
	bool inside = !ec && !relative.empty() && relative.native()[0] != '.';
	if (inside) {
		return relative.generic_string();
	}
	fs::path target = base / source.filename();
	if (!fs::exists(target, ec)) {
		fs::copy_file(source, target, ec);
		if (ec) {
			error = "cannot copy " + path + " to " + target.string() + ": " + ec.message();
			return "";
		}
	}
	return source.filename().string();
}

void GuiBuilder::pathField(ofxGuiGroup & g, const std::string & label, std::string & target, const std::string & baseDir,
	Callback onChange) {
	auto & p = makeParam<std::string>();
	p.set(label, target);
	listeners_.push(p.newListener([&target, onChange](std::string & v) {
		target = v;
		if (onChange) onChange();
	}));
	g.add(p);
	button(g, "browse...", [&p, baseDir]() {
		ofFileDialogResult result = ofSystemLoadDialog("Choose an audio file (wav, flac, mp3, ogg)", false, baseDir);
		if (!result.bSuccess) return;
		std::string error;
		std::string name = adoptFile(result.getPath(), baseDir, error);
		if (name.empty()) {
			ofLogError("gaugemancy") << error;
			return;
		}
		p.set(name); // fires the listener above
	});
}

std::vector<std::string> GuiBuilder::sourceNames() {
	std::vector<std::string> names;
	for (int i = 0; i < kNumModSources; ++i) {
		names.push_back(modSourceLabel(static_cast<ModSource>(i)));
	}
	return names;
}

ofxGuiGroup * GuiBuilder::modulated(ofxGuiGroup & g, const ParamSpec & spec, ModulatedParam & target, Callback onChange,
	bool allowSources) {
	ofxGuiGroup * sub = group(spec.label, true);
	floatSlider(*sub, "value", target.value, spec.lowest, spec.highest, onChange);
	if (allowSources) {
		floatSlider(*sub, "min (source at 0)", target.min, spec.lowest, spec.highest, onChange);
		floatSlider(*sub, "max (source at 1)", target.max, spec.lowest, spec.highest, onChange);
		choice(*sub, "source", static_cast<int>(target.source), sourceNames(), [&target, onChange](int i) {
			target.source = static_cast<ModSource>(i);
			if (onChange) onChange();
		});
	}
	g.add(sub);
	return sub;
}

} // namespace gm
