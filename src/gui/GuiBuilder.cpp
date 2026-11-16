#include "GuiBuilder.h"

#include "../core/ControlInputs.h"

#include "imgui.h"
#include "ofLog.h"
#include "ofSystemUtils.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>

namespace fs = std::filesystem;

namespace gm {

bool GuiBuilder::group(const std::string & name, bool defaultOpen) {
	return ImGui::TreeNodeEx(name.c_str(), defaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : 0);
}

void GuiBuilder::endGroup() {
	ImGui::TreePop();
}

bool GuiBuilder::numberSlider(const char * label, float & target, float lo, float hi, const char * format) {
	const ImGuiID id = ImGui::GetID(label);

	// Typing mode: the slider is replaced by a text field until Enter, Escape
	// or a click elsewhere. Width and position are those of the slider, so
	// nothing moves.
	if (editingId_ == id) {
		if (editingNeedsFocus_) {
			ImGui::SetKeyboardFocusHere();
		}
		bool entered = ImGui::InputFloat(label, &editingValue_, 0.0f, 0.0f, "%.6g",
			ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
		bool done = entered || (!editingNeedsFocus_ && ImGui::IsItemDeactivated());
		bool cancelled = ImGui::IsKeyPressed(ImGuiKey_Escape);
		editingNeedsFocus_ = false;
		if (!done && !cancelled) return false;
		editingId_ = 0;
		float typed = std::clamp(editingValue_, std::min(lo, hi), std::max(lo, hi));
		if (cancelled || typed == target) return false;
		target = typed;
		return true;
	}

	const float before = target;
	bool changed = ImGui::SliderFloat(label, &target, lo, hi, format);
	if (ImGui::IsItemActivated() && !ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
		// Remember the value from before this click: the first click of a
		// double-click also moves the slider, which typing must not keep.
		preClickId_ = id;
		preClickValue_ = before;
	}
	if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
		if (preClickId_ == id) target = preClickValue_;
		editingId_ = id;
		editingNeedsFocus_ = true;
		editingValue_ = target;
		changed = true;
	}
	return changed;
}

bool GuiBuilder::sourceChoice(const char * label, ModSource & source) {
	// A source this instrument lacks is still shown when a parameter already
	// uses it (marked, and ignored by the audio), but cannot be picked.
	auto name = [&](ModSource s) {
		std::string text = modSourceLabel(s);
		return (!motionAvailable_ && isMotionSource(s)) ? text + " (no gyro)" : text;
	};
	bool changed = false;
	if (ImGui::BeginCombo(label, name(source).c_str())) {
		for (int i = 0; i < kNumModSources; ++i) {
			ModSource candidate = static_cast<ModSource>(i);
			if (!motionAvailable_ && isMotionSource(candidate)) continue;
			bool selected = candidate == source;
			if (ImGui::Selectable(name(candidate).c_str(), selected)) {
				source = candidate;
				changed = true;
			}
			if (selected) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	return changed;
}

bool GuiBuilder::floatSlider(const std::string & label, float & target, float lo, float hi, Callback onChange) {
	bool changed = numberSlider(label.c_str(), target, lo, hi);
	if (changed && onChange) onChange();
	return changed;
}

bool GuiBuilder::intSlider(const std::string & label, int & target, int lo, int hi, Callback onChange) {
	float value = static_cast<float>(target);
	bool changed = numberSlider(label.c_str(), value, static_cast<float>(lo), static_cast<float>(hi), "%.0f");
	int rounded = static_cast<int>(std::lround(value));
	changed = changed && rounded != target;
	if (changed) {
		target = rounded;
		if (onChange) onChange();
	}
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
	ImGui::TableSetupColumn("value, or at 0", ImGuiTableColumnFlags_WidthStretch, 1.8f);
	ImGui::TableSetupColumn("at 1", ImGuiTableColumnFlags_WidthStretch, 1.8f);
	ImGui::TableSetupColumn("now (moves with its source)", ImGuiTableColumnFlags_WidthStretch, 1.5f);
	ImGui::TableHeadersRow();
	tableRows_ = 0;
	return true;
}

bool GuiBuilder::paramRow(const ParamSpec & spec, ModulatedParam & target, float now, bool allowSources) {
	bool changed = false;
	bool discrete = !spec.choices.empty();
	ImGui::TableNextRow();
	ImGui::PushID(spec.id);

	ImGui::TableNextColumn();
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(spec.label);

	ImGui::TableNextColumn();
	if (allowSources) {
		ImGui::SetNextItemWidth(-FLT_MIN);
		changed |= sourceChoice("##source", target.source);
	}

	// A source the instrument lacks (no gyro) counts as "not connected".
	bool modulatedNow = allowSources && target.isModulated() && (motionAvailable_ || !isMotionSource(target.source));
	ImGui::TableNextColumn();
	ImGui::SetNextItemWidth(-FLT_MIN);
	if (modulatedNow) {
		changed |= numberSlider("##min", target.min, spec.lowest, spec.highest);
	} else if (discrete) {
		std::vector<std::string> names(spec.choices.begin(), spec.choices.end());
		int index = std::clamp(static_cast<int>(std::lround(target.value - spec.lowest)), 0, static_cast<int>(names.size()) - 1);
		if (choice("##value", index, names)) {
			target.value = spec.lowest + static_cast<float>(index);
			changed = true;
		}
	} else {
		changed |= numberSlider("##value", target.value, spec.lowest, spec.highest);
	}

	ImGui::TableNextColumn();
	if (modulatedNow) {
		ImGui::SetNextItemWidth(-FLT_MIN);
		changed |= numberSlider("##max", target.max, spec.lowest, spec.highest);
	}

	ImGui::TableNextColumn();
	ImGui::AlignTextToFramePadding();
	if (discrete) {
		int index = std::clamp(static_cast<int>(now - spec.lowest), 0, static_cast<int>(spec.choices.size()) - 1);
		ImGui::TextUnformatted(spec.choices[static_cast<size_t>(index)]);
	} else if (modulatedNow) {
		// A bar across the parameter's whole range that follows the source.
		char text[32];
		std::snprintf(text, sizeof(text), "%.4g", now);
		float span = spec.highest - spec.lowest;
		float fraction = span != 0.0f ? std::clamp((now - spec.lowest) / span, 0.0f, 1.0f) : 0.0f;
		ImGui::ProgressBar(fraction, ImVec2(-FLT_MIN, 0), text);
	} else {
		ImGui::TextDisabled("%.4g", now);
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
				if (sourceChoice("source", target.source)) {
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
