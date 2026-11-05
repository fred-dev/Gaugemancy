#include "GuiBuilder.h"

#include "../core/ControlInputs.h"

#include "imgui.h"
#include "ofLog.h"
#include "ofSystemUtils.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

namespace fs = std::filesystem;

namespace gm {

bool GuiBuilder::group(const std::string & name, bool defaultOpen) {
	return ImGui::TreeNodeEx(name.c_str(), defaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : 0);
}

void GuiBuilder::endGroup() {
	ImGui::TreePop();
}

bool GuiBuilder::floatSlider(const std::string & label, float & target, float lo, float hi, Callback onChange) {
	bool changed = ImGui::SliderFloat(label.c_str(), &target, lo, hi);
	if (changed && onChange) onChange();
	return changed;
}

bool GuiBuilder::intSlider(const std::string & label, int & target, int lo, int hi, Callback onChange) {
	bool changed = ImGui::SliderInt(label.c_str(), &target, lo, hi);
	if (changed && onChange) onChange();
	return changed;
}

bool GuiBuilder::toggle(const std::string & label, bool & target, Callback onChange) {
	bool changed = ImGui::Checkbox(label.c_str(), &target);
	if (changed && onChange) onChange();
	return changed;
}

bool GuiBuilder::text(const std::string & label, std::string & target, Callback onChange) {
	char buf[256];
	std::snprintf(buf, sizeof(buf), "%s", target.c_str());
	bool changed = ImGui::InputText(label.c_str(), buf, sizeof(buf));
	if (changed) {
		target = buf;
		if (onChange) onChange();
	}
	return changed;
}

bool GuiBuilder::button(const std::string & label) {
	return ImGui::Button(label.c_str());
}

void GuiBuilder::label(const std::string & text) {
	ImGui::TextWrapped("%s", text.c_str());
}

bool GuiBuilder::choice(const std::string & label, int & current, const std::vector<std::string> & names) {
	if (names.empty()) return false;
	current = std::clamp(current, 0, static_cast<int>(names.size()) - 1);
	bool changed = false;
	if (ImGui::BeginCombo(label.c_str(), names[current].c_str())) {
		for (int i = 0; i < static_cast<int>(names.size()); ++i) {
			bool selected = (i == current);
			if (ImGui::Selectable(names[i].c_str(), selected)) {
				current = i;
				changed = true;
			}
			if (selected) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	return changed;
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

bool GuiBuilder::pathField(const std::string & label, std::string & target, const std::string & baseDir, Callback onChange) {
	bool changed = text(label, target, onChange);
	ImGui::SameLine();
	if (button((std::string("browse...##") + label).c_str())) {
		ofFileDialogResult result = ofSystemLoadDialog("Choose an audio file (wav, flac, mp3, ogg)", false, baseDir);
		if (result.bSuccess) {
			std::string error;
			std::string name = adoptFile(result.getPath(), baseDir, error);
			if (name.empty()) {
				ofLogError("gaugemancy") << error;
			} else {
				target = name;
				changed = true;
				if (onChange) onChange();
			}
		}
	}
	return changed;
}

std::vector<std::string> GuiBuilder::sourceNames() {
	std::vector<std::string> names;
	for (int i = 0; i < kNumModSources; ++i) {
		names.push_back(modSourceLabel(static_cast<ModSource>(i)));
	}
	return names;
}

bool GuiBuilder::modulated(const ParamSpec & spec, ModulatedParam & target, Callback onChange, bool allowSources) {
	bool changed = false;
	if (group(spec.label, false)) {
		if (!spec.choices.empty()) {
			// A discrete parameter (e.g. a waveform pick): named dropdown, not
			// a slider over raw numbers - still just a float under the hood,
			// so modulating it (min/max/source) works exactly as before.
			std::vector<std::string> names(spec.choices.begin(), spec.choices.end());
			int count = static_cast<int>(names.size());
			auto drawChoice = [&](const char * label, float & field) {
				int index = std::clamp(static_cast<int>(std::lround(field - spec.lowest)), 0, count - 1);
				if (choice(label, index, names)) {
					field = spec.lowest + static_cast<float>(index);
					changed = true;
					if (onChange) onChange();
				}
			};
			drawChoice("value", target.value);
			if (allowSources) {
				drawChoice("min (source at 0)", target.min);
				drawChoice("max (source at 1)", target.max);
				int source = static_cast<int>(target.source);
				if (choice("source", source, sourceNames())) {
					target.source = static_cast<ModSource>(source);
					changed = true;
					if (onChange) onChange();
				}
			}
		} else {
			changed |= floatSlider("value", target.value, spec.lowest, spec.highest, onChange);
			if (allowSources) {
				changed |= floatSlider("min (source at 0)", target.min, spec.lowest, spec.highest, onChange);
				changed |= floatSlider("max (source at 1)", target.max, spec.lowest, spec.highest, onChange);
				int source = static_cast<int>(target.source);
				if (choice("source", source, sourceNames())) {
					target.source = static_cast<ModSource>(source);
					changed = true;
					if (onChange) onChange();
				}
			}
		}
		endGroup();
	}
	return changed;
}

} // namespace gm
