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

// ----------------------------------------------------------- parameter table

bool GuiBuilder::beginParamTable(const std::string & id) {
	constexpr ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp;
	if (!ImGui::BeginTable(id.c_str(), 5, flags)) return false;
	ImGui::TableSetupColumn("parameter", ImGuiTableColumnFlags_WidthStretch, 1.5f);
	ImGui::TableSetupColumn("source", ImGuiTableColumnFlags_WidthStretch, 1.0f);
	ImGui::TableSetupColumn("value, or at 0", ImGuiTableColumnFlags_WidthStretch, 2.0f);
	ImGui::TableSetupColumn("at 1", ImGuiTableColumnFlags_WidthStretch, 2.0f);
	ImGui::TableSetupColumn("now", ImGuiTableColumnFlags_WidthStretch, 0.7f);
	ImGui::TableHeadersRow();
	tableRows_ = 0;
	return true;
}

bool GuiBuilder::paramRow(const ParamSpec & spec, ModulatedParam & target, float now, bool allowSources) {
	static const std::vector<std::string> sources = sourceNames();
	bool changed = false;
	bool discrete = !spec.choices.empty();
	ImGui::TableNextRow();
	ImGui::PushID(spec.id);

	ImGui::TableNextColumn();
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(spec.label);

	ImGui::TableNextColumn();
	if (allowSources) {
		int source = static_cast<int>(target.source);
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (choice("##source", source, sources)) {
			target.source = static_cast<ModSource>(source);
			changed = true;
		}
	}

	bool modulatedNow = allowSources && target.isModulated();
	ImGui::TableNextColumn();
	ImGui::SetNextItemWidth(-FLT_MIN);
	if (modulatedNow) {
		changed |= ImGui::SliderFloat("##min", &target.min, spec.lowest, spec.highest);
	} else if (discrete) {
		std::vector<std::string> names(spec.choices.begin(), spec.choices.end());
		int index = std::clamp(static_cast<int>(std::lround(target.value - spec.lowest)), 0, static_cast<int>(names.size()) - 1);
		if (choice("##value", index, names)) {
			target.value = spec.lowest + static_cast<float>(index);
			changed = true;
		}
	} else {
		changed |= ImGui::SliderFloat("##value", &target.value, spec.lowest, spec.highest);
	}

	ImGui::TableNextColumn();
	if (modulatedNow) {
		ImGui::SetNextItemWidth(-FLT_MIN);
		changed |= ImGui::SliderFloat("##max", &target.max, spec.lowest, spec.highest);
	}

	ImGui::TableNextColumn();
	ImGui::AlignTextToFramePadding();
	if (discrete) {
		int index = std::clamp(static_cast<int>(now - spec.lowest), 0, static_cast<int>(spec.choices.size()) - 1);
		ImGui::TextUnformatted(spec.choices[static_cast<size_t>(index)]);
	} else {
		ImGui::Text("%.4g", now);
	}

	ImGui::PopID();
	++tableRows_;
	return changed;
}

void GuiBuilder::endParamTable(int totalRows) {
	for (; tableRows_ < totalRows; ++tableRows_) {
		ImGui::TableNextRow(0, ImGui::GetFrameHeight());
		ImGui::TableNextColumn();
		ImGui::Dummy(ImVec2(1.0f, ImGui::GetFrameHeight()));
	}
	ImGui::EndTable();
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
			// A discrete parameter (e.g. a waveform pick) is just set, not
			// modulated: "min (source at 0) / max (source at 1)" sweeping
			// through named options by sensor pressure isn't a sensible
			// control, so there is no source picker here at all - picking a
			// value is the whole story.
			std::vector<std::string> names(spec.choices.begin(), spec.choices.end());
			int count = static_cast<int>(names.size());
			target.source = ModSource::None; // never modulated, even if loaded from an older save
			int index = std::clamp(static_cast<int>(std::lround(target.value - spec.lowest)), 0, count - 1);
			if (choice("value", index, names)) {
				target.value = spec.lowest + static_cast<float>(index);
				changed = true;
				if (onChange) onChange();
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
