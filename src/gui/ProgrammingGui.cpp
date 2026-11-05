#include "ProgrammingGui.h"

#include "imgui.h"

#include <algorithm>

namespace gm {

namespace {
constexpr float kMargin = 10.0f; // outer margin and gutter between fixed panels

// A free, Univers/Helvetica-style grotesque already on macOS - no font to
// ship. Sized well above ImGui's tiny ~13px default per the user's request
// for a bigger GUI; style.ScaleAllSizes() grows paddings/spacing to match
// so widgets don't look cramped next to the larger text.
constexpr const char * kFontPath = "/System/Library/Fonts/Supplemental/Arial.ttf";
constexpr float kFontSize = 19.0f;
constexpr float kStyleScale = 1.2f;

// The fixed grid, as fractions of the canvas (the window, minus the menu
// bar): every number here is a panel's permanent share of the screen. This
// is the one place panel sizes are decided - see PanelLayout.h.
constexpr float kLeftColumnFrac = 0.19f; // Transport/Simulation/Master
constexpr float kRightColumnFrac = 0.27f; // Inspector
constexpr float kTransportFrac = 0.22f; // share of the left column's height
constexpr float kSimulationFrac = 0.40f; // Master gets the rest (0.38)
constexpr float kHeaderHeight = 20.0f; // one-line status strip above the slot grid

std::vector<std::string> windowNames() {
	std::vector<std::string> names;
	for (int i = 0; i < static_cast<int>(WindowType::Count); ++i) names.push_back(windowTypeLabel(static_cast<WindowType>(i)));
	return names;
}

std::vector<std::string> curveNames() {
	std::vector<std::string> names;
	for (int i = 0; i < static_cast<int>(Curve::Count); ++i) names.push_back(curveLabel(static_cast<Curve>(i)));
	return names;
}

const std::vector<std::string> kAxisIds = { "+z", "-z", "+x", "-x", "+y", "-y" };

std::vector<std::string> effectNames() {
	std::vector<std::string> names;
	for (const EffectInfo & info : EffectCatalog::all()) {
		names.push_back(info.label + " (cpu " + std::to_string(info.cpuCost) + ")");
	}
	return names;
}

std::vector<std::string> sourceTypeNames() {
	std::vector<std::string> names;
	for (const SourceInfo & info : SourceCatalog::all()) names.push_back(info.label);
	return names;
}

int sourceTypeIndex(const std::string & id) {
	const auto & sources = SourceCatalog::all();
	for (size_t i = 0; i < sources.size(); ++i) {
		if (sources[i].id == id) return static_cast<int>(i);
	}
	return 0;
}

// High-contrast dark theme: the default ImGui dark theme shades sliders,
// inputs and combo boxes only slightly lighter than the window background,
// so a slider's filled track/grab is hard to tell apart from its empty
// track at a glance. This instead gives every interactive control a clearly
// lighter "well" to sit in, and one bright, consistent accent colour (amber)
// for anything showing a current value or selection (slider grab, checkmark,
// active header/tab, buttons) - so "where is the control and what is it
// currently set to" is readable without leaning in.
void applyAccessibleTheme() {
	ImGuiStyle & style = ImGui::GetStyle();
	ImVec4 * c = style.Colors;

	const ImVec4 kAccent(0.95f, 0.65f, 0.15f, 1.0f);       // amber: value/selection
	const ImVec4 kAccentHot(1.0f, 0.75f, 0.30f, 1.0f);
	const ImVec4 kAccentActive(1.0f, 0.82f, 0.45f, 1.0f);
	const ImVec4 kWindowBg(0.11f, 0.11f, 0.12f, 1.0f);
	const ImVec4 kFieldBg(0.24f, 0.24f, 0.26f, 1.0f);      // sliders/inputs/combos: lighter well
	const ImVec4 kFieldBgHot(0.30f, 0.30f, 0.33f, 1.0f);
	const ImVec4 kFieldBgActive(0.34f, 0.34f, 0.37f, 1.0f);
	const ImVec4 kText(0.96f, 0.96f, 0.96f, 1.0f);
	const ImVec4 kBorder(0.55f, 0.55f, 0.58f, 0.6f);

	c[ImGuiCol_Text] = kText;
	c[ImGuiCol_TextDisabled] = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
	c[ImGuiCol_WindowBg] = kWindowBg;
	c[ImGuiCol_ChildBg] = kWindowBg;
	c[ImGuiCol_PopupBg] = ImVec4(0.14f, 0.14f, 0.15f, 1.0f);
	c[ImGuiCol_Border] = kBorder;

	c[ImGuiCol_FrameBg] = kFieldBg;
	c[ImGuiCol_FrameBgHovered] = kFieldBgHot;
	c[ImGuiCol_FrameBgActive] = kFieldBgActive;

	c[ImGuiCol_SliderGrab] = kAccent;
	c[ImGuiCol_SliderGrabActive] = kAccentActive;
	c[ImGuiCol_CheckMark] = kAccent;

	c[ImGuiCol_Button] = ImVec4(0.30f, 0.30f, 0.33f, 1.0f);
	c[ImGuiCol_ButtonHovered] = kAccentHot;
	c[ImGuiCol_ButtonActive] = kAccentActive;

	c[ImGuiCol_Header] = ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.45f);
	c[ImGuiCol_HeaderHovered] = ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.65f);
	c[ImGuiCol_HeaderActive] = ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.80f);

	c[ImGuiCol_Tab] = ImVec4(0.22f, 0.22f, 0.24f, 1.0f);
	c[ImGuiCol_TabHovered] = kAccentHot;
	c[ImGuiCol_TabActive] = ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.85f);

	c[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.08f, 0.09f, 1.0f);
	c[ImGuiCol_TitleBgActive] = ImVec4(0.16f, 0.16f, 0.18f, 1.0f);
	c[ImGuiCol_MenuBarBg] = ImVec4(0.08f, 0.08f, 0.09f, 1.0f);

	c[ImGuiCol_ScrollbarBg] = ImVec4(0.08f, 0.08f, 0.09f, 1.0f);
	c[ImGuiCol_ScrollbarGrab] = kFieldBgHot;
	c[ImGuiCol_ScrollbarGrabHovered] = kAccentHot;
	c[ImGuiCol_ScrollbarGrabActive] = kAccentActive;

	c[ImGuiCol_Separator] = kBorder;
	c[ImGuiCol_PlotHistogram] = kAccent;
	c[ImGuiCol_PlotHistogramHovered] = kAccentHot;

	style.FrameBorderSize = 1.0f; // every field gets a visible outline, not just a fill colour
	style.GrabMinSize = 10.0f;    // a slider grab you can actually see/land a click on
}
} // namespace

ProgrammingGui::ProgrammingGui(Instrument & instrument) : instrument_(instrument) {}

// ======================================================================= setup

void ProgrammingGui::setup() {
	ofSetWindowTitle("Gaugemancy");
	ofBackground(18);
	// A new file name (not the old "imgui.ini"): window positions saved
	// under the old, smaller font/layout no longer fit this one, so this
	// starts everyone fresh on the new layout once; from here on it's
	// remembered exactly like before.
	gui_.iniFileName = ofToDataPath("imgui_v2.ini");
	gui_.setup(nullptr, false);
	applyAccessibleTheme();
	if (gui_.addFont(kFontPath, kFontSize, nullptr, nullptr, true) != nullptr) {
		ImGui::GetStyle().ScaleAllSizes(kStyleScale);
	}
	ramp_ = instrument_.simulator().ramp();
	presetChoice_ = instrument_.presetIndex();
	layout();
}

void ProgrammingGui::layout() {
	float w = static_cast<float>(ofGetWidth());
	float h = static_cast<float>(ofGetHeight());

	// The one fixed grid everything else reads from: a canvas below the menu
	// bar, carved into permanent fractional regions (PanelLayout). Every
	// panel's share of the screen is decided here, once, each frame.
	ofRectangle canvas(kMargin, menuBarHeight_ + kMargin, std::max(w - 2.0f * kMargin, 100.0f), std::max(h - menuBarHeight_ - 2.0f * kMargin, 100.0f));
	panels_.setCanvas(canvas);

	ofRectangle leftColumn = panels_.region(0.0f, 0.0f, kLeftColumnFrac, 1.0f);
	transportRect_ = PanelLayout::inset(ofRectangle(leftColumn.x, leftColumn.y, leftColumn.width, leftColumn.height * kTransportFrac), kMargin * 0.5f);
	simulationRect_ = PanelLayout::inset(ofRectangle(leftColumn.x, leftColumn.y + leftColumn.height * kTransportFrac,
										leftColumn.width, leftColumn.height * kSimulationFrac), kMargin * 0.5f);
	masterRect_ = PanelLayout::inset(ofRectangle(leftColumn.x, leftColumn.y + leftColumn.height * (kTransportFrac + kSimulationFrac),
									leftColumn.width, leftColumn.height * (1.0f - kTransportFrac - kSimulationFrac)), kMargin * 0.5f);
	inspectorRect_ = PanelLayout::inset(panels_.region(1.0f - kRightColumnFrac, 0.0f, 1.0f, 1.0f), kMargin * 0.5f);
	centerRect_ = PanelLayout::inset(panels_.region(kLeftColumnFrac, 0.0f, 1.0f - kRightColumnFrac, 1.0f), kMargin * 0.5f);

	// Centre content (OF-drawn, not ImGui: the slot grid, narration waveform,
	// 3D view, sensor bars, log) fills centerRect_, with a one-line header
	// strip reserved at the top.
	float contentTop = centerRect_.y + kHeaderHeight;
	float slotW = std::min((centerRect_.width - 2.0f * kMargin) / 3.0f, 300.0f);
	float slotH = std::min(100.0f, h * 0.12f);
	for (int i = 0; i < kMaxSlots; ++i) {
		int row = i / 3;
		int col = i % 3;
		slotViews_[i].setRect(ofRectangle(centerRect_.x + col * (slotW + kMargin), contentTop + row * (slotH + kMargin), slotW, slotH));
	}
	float y = contentTop + 2.0f * (slotH + kMargin);
	narrationView_.setRect(ofRectangle(centerRect_.x, y, centerRect_.width, slotH * 0.7f));
	y += slotH * 0.7f + kMargin;
	// Bottom row: the 3D instrument on the left; sensor bars and messages right.
	float deviceSize = std::max(std::min({ centerRect_.getBottom() - y - kMargin, centerRect_.width * 0.45f, 420.0f }), 200.0f);
	deviceView_.setRect(ofRectangle(centerRect_.x, y, deviceSize, deviceSize));
	float rightX = centerRect_.x + deviceSize + kMargin;
	float rightW = std::max(centerRect_.width - deviceSize - kMargin, 200.0f);
	sensorView_.setRect(ofRectangle(rightX, y, rightW, 150.0f));
	statusRect_.set(rightX, y + 150.0f + kMargin, rightW, std::max(centerRect_.getBottom() - y - 150.0f - 2.0f * kMargin, 60.0f));
}

void ProgrammingGui::windowResized(int, int) {
	layout();
}

std::string ProgrammingGui::audioDir() const {
	return instrument_.store().audioDir(instrument_.profile().name);
}

// ===================================================================== menu

float ProgrammingGui::drawMenuBar() {
	ImGui::BeginMainMenuBar();
	ImGui::MenuItem("Settings (F7)", nullptr, &showSettings_);
	ImGui::MenuItem("Message log (F10)", nullptr, &showLog_);
	ImGui::EndMainMenuBar();
	return static_cast<float>(gui_.getMenuHeight());
}

// ================================================================== transport

void ProgrammingGui::drawTransportWindow() {
	panels_.begin("Gaugemancy - " + instrument_.profile().name, transportRect_);

	if (builder_.group("Presets")) {
		std::vector<std::string> presetNames;
		for (size_t i = 0; i < instrument_.presets().size(); ++i) {
			presetNames.push_back(std::to_string(i + 1) + ": " + instrument_.presets()[i].name);
		}
		if (builder_.choice("preset", presetChoice_, presetNames)) presetChanged_ = true;
		if (builder_.button("next preset")) {
			presetChoice_ = (instrument_.presetIndex() + 1) % static_cast<int>(instrument_.presets().size());
			presetChanged_ = true;
		}
		if (builder_.button("add preset (copy of this one)")) {
			ProfileSettings & profile = instrument_.profile();
			instrument_.presets().push_back(instrument_.preset());
			profile.presetFiles.push_back("presets/preset_" + std::to_string(instrument_.presets().size()) + ".json");
			instrument_.note("added preset " + std::to_string(instrument_.presets().size()) + " (save to keep it)");
			presetChoice_ = static_cast<int>(instrument_.presets().size()) - 1;
			presetChanged_ = true;
		}
		builder_.endGroup();
	}

	if (builder_.group("Files")) {
		if (builder_.button("save everything (F12)")) {
			std::string error;
			instrument_.saveToDisk(error);
		}
		if (builder_.button("save this preset")) {
			std::string error;
			instrument_.savePresetToDisk(error);
		}
		if (builder_.button("reload from disk (discard edits)")) {
			instrument_.reloadFromDisk();
			presetChoice_ = instrument_.presetIndex();
		}
		builder_.endGroup();
	}

	if (builder_.group("Performance")) {
		if (builder_.button("skip narration")) instrument_.skipNarration();
		if (builder_.button("recalibrate sensors")) instrument_.recalibrate();
		sourceChoice_ = instrument_.sensorSourceName() == "hardware" ? 1 : 0;
		if (builder_.choice("sensor input", sourceChoice_, { "Simulation", "Hardware" })) {
			instrument_.useSensorMode(sourceChoice_ == 1 ? SensorMode::Hardware : SensorMode::Simulation);
		}
		builder_.text("record file", recordFile_);
		if (builder_.button("start / stop recording")) {
			if (instrument_.isRecording()) instrument_.stopRecording();
			else instrument_.startRecording(ofToDataPath(recordFile_, true));
		}
		builder_.endGroup();
	}

	ImGui::End();
}

// ================================================================= simulation

void ProgrammingGui::drawSimulationWindow() {
	panels_.begin("Simulation", simulationRect_);
	SensorSimulator & sim = instrument_.simulator();

	if (builder_.group("Squeeze by hand")) {
		for (int i = 0; i < kNumSensors; ++i) {
			ImGui::PushID(i);
			simLevels_[i] = sim.manual(i);
			if (builder_.floatSlider("sensor " + std::to_string(i + 1), simLevels_[i], 0.0f, 1.0f)) {
				instrument_.simulator().setManual(i, simLevels_[i]);
			}
			ImGui::PopID();
		}
		builder_.endGroup();
	}

	if (builder_.group("Squeeze ramp (F9)")) {
		builder_.floatSlider("speed", ramp_.speed, 0.05f, 5.0f);
		builder_.floatSlider("peak", ramp_.extent, 0.0f, 1.0f);
		builder_.toggle("loop", ramp_.loop);
		// One "include sensor" heading plus a compact grid of numbered boxes,
		// instead of 6 full-width "include sensor N" rows - the whole panel
		// then fits without scrolling at 1920x1080.
		builder_.label("include sensor:");
		constexpr int kPerRow = 3;
		for (int i = 0; i < kNumSensors; ++i) {
			ImGui::PushID(i);
			builder_.toggle(std::to_string(i + 1), ramp_.sensors[i]);
			ImGui::PopID();
			if (i + 1 < kNumSensors && (i + 1) % kPerRow != 0) ImGui::SameLine();
		}
		if (builder_.button("start / stop ramp")) {
			SensorSimulator & s = instrument_.simulator();
			if (s.rampRunning()) {
				s.stopRamp();
			} else {
				s.setRamp(ramp_);
				s.startRamp();
			}
		}
		builder_.endGroup();
	}

	ImGui::End();
}

// ====================================================================== master

void ProgrammingGui::drawMasterWindow() {
	// Always visible, fixed in the left column with Transport/Simulation -
	// not part of the Inspector's single-open accordion, so selecting a slot
	// never hides the one place you can see and fix the overall output level.
	panels_.begin("Master output", masterRect_);
	drawMasterSection();
	ImGui::End();
}

// =================================================================== settings

void ProgrammingGui::drawSettingsWindow() {
	if (!showSettings_) return;
	// A different context from the always-on working layout (profile
	// switching, device/hardware/network/calibration), so it is hidden by
	// default - but still a fixed panel, not a free-floating window: it
	// takes over exactly the centre region the slot grid would otherwise
	// use (which draw() skips while this is open), so it never overlaps
	// Transport/Simulation/Master/Inspector, which all stay visible and usable.
	panels_.begin("Settings - profile: " + instrument_.profile().name, centerRect_);
	drawProfileSection();
	ImGui::End();
}

// ===================================================================== editor

void ProgrammingGui::drawInspectorWindow() {
	panels_.begin("Inspector", inspectorRect_);

	for (int i = 0; i < kMaxSlots; ++i) {
		bool active = target_ == Target::Slot && selectedSlot_ == i;
		ImGui::PushID(i);
		drawInspectorSection("Slot " + std::to_string(i + 1) + " (F" + std::to_string(i + 1) + ")", active,
			[this, i]() { select(Target::Slot, i); });
		if (active) drawSlotEditor(instrument_.preset().slots[i], i);
		ImGui::PopID();
	}

	// Narration is a legacy feature: only list it here once a profile
	// actually has it enabled, so it doesn't clutter every other profile.
	if (instrument_.profile().narration.enabled) {
		drawInspectorSection("Narration (F8)", target_ == Target::Narration, [this]() { select(Target::Narration); });
		if (target_ == Target::Narration) drawNarrationEditor(instrument_.profile().narration);
	} else if (target_ == Target::Narration) {
		select(Target::Slot, 0);
	}

	ImGui::End();
}

void ProgrammingGui::drawInspectorSection(const std::string & title, bool active, const std::function<void()> & onSelect) {
	if (active) {
		ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "%s", title.c_str());
	} else if (ImGui::Selectable(title.c_str())) {
		onSelect();
	}
	ImGui::Separator();
}

void ProgrammingGui::drawMasterSection() {
	if (builder_.group("Master", false)) {
		builder_.floatSlider("preset gain", instrument_.preset().masterGain, 0.0f, 4.0f);
		builder_.label("effects on the mix of all slots:");
		builder_.endGroup();
	}
	if (builder_.group("Output level", true)) {
		float left = instrument_.audio().masterMeter(0);
		float right = instrument_.audio().masterMeter(1);
		ImGui::ProgressBar(std::clamp(left, 0.0f, 1.0f), ImVec2(-1, 0), "L");
		ImGui::ProgressBar(std::clamp(right, 0.0f, 1.0f), ImVec2(-1, 0), "R");
		builder_.endGroup();
	}
	drawEffectChainEditor("master", instrument_.preset().masterEffects);
}

void ProgrammingGui::drawSlotEditor(SlotSettings & slot, int index) {
	bool granular = slot.sourceType == "granular";

	if (builder_.group("Source")) {
		int sourceIndex = sourceTypeIndex(slot.sourceType);
		if (builder_.choice("type", sourceIndex, sourceTypeNames())) {
			slot.sourceType = SourceCatalog::all()[sourceIndex].id;
			slot.synthParams = SourceCatalog::makeParams(slot.sourceType);
			granular = slot.sourceType == "granular";
			if (freeRun_[index]) {
				// The toggle below only exists for a synth source; switching
				// away from one must not leave it stuck audibly "on" with no
				// way left on screen to turn it back off.
				freeRun_[index] = false;
				instrument_.setAudition(index, false, 0.0f);
			}
			structureChanged();
		}
		if (const SourceInfo * info = SourceCatalog::find(slot.sourceType)) {
			builder_.label(info->description);
		}
		if (!granular) {
			if (builder_.toggle("free run (play continuously - for tuning by ear)", freeRun_[index])) {
				instrument_.setAudition(index, freeRun_[index], 0.0f);
			}
		}
		builder_.endGroup();
	}

	if (builder_.group("Playback")) {
		builder_.toggle("enabled", slot.enabled, [this]() { structureChanged(); });
		std::vector<std::string> gates = { "Auto (while its sensors are pressed)", "Always" };
		for (int i = 1; i < kNumModSources; ++i) gates.push_back("While " + modSourceLabel(static_cast<ModSource>(i)));
		int gateIndex = slot.gate == GateMode::Auto ? 0 : slot.gate == GateMode::Always ? 1 : 1 + static_cast<int>(slot.gateSource);
		if (builder_.choice("heard", gateIndex, gates)) {
			if (gateIndex == 0) slot.gate = GateMode::Auto;
			else if (gateIndex == 1) slot.gate = GateMode::Always;
			else {
				slot.gate = GateMode::Source;
				slot.gateSource = static_cast<ModSource>(gateIndex - 1);
			}
		}
		builder_.endGroup();
	}

	// File/window/voices are all granular-only - a synth source has nothing
	// to load or window, so showing them next to it is just clutter/confusion.
	if (granular && builder_.group("Sample")) {
		if (builder_.pathField("file", slot.file, audioDir())) samplesToReload_.push_back(index);
		int windowIndex = static_cast<int>(slot.window);
		if (builder_.choice("grain window", windowIndex, windowNames())) {
			slot.window = static_cast<WindowType>(windowIndex);
			structureChanged();
		}
		builder_.intSlider("grain voices (cpu)", slot.voices, 1, 32, [this]() { structureChanged(); });
		builder_.endGroup();
	}

	if (slot.sourceType == "granular") {
		if (builder_.group("Grain parameters")) {
			const auto & specs = grainParamSpecs();
			for (int i = 0; i < GrainSettings::count(); ++i) {
				ImGui::PushID(i);
				builder_.modulated(specs[i], slot.grain.at(i));
				ImGui::PopID();
			}
			builder_.endGroup();
		}
	} else if (const SourceInfo * info = SourceCatalog::find(slot.sourceType)) {
		if (builder_.group(info->label + " parameters")) {
			for (size_t i = 0; i < info->params.size() && i < slot.synthParams.size(); ++i) {
				ImGui::PushID(static_cast<int>(i));
				builder_.modulated(info->params[i], slot.synthParams[i]);
				ImGui::PopID();
			}
			builder_.endGroup();
		}
	}

	drawEffectChainEditor("slot" + std::to_string(index), slot.effects);
}

void ProgrammingGui::drawNarrationEditor(NarrationSettings & n) {
	if (builder_.group("Narration")) {
		builder_.toggle("enabled", n.enabled, [this]() { reloadNarration_ = true; });
		builder_.toggle("skip (sticks: starts straight in granular)", n.skip, [this]() { reloadNarration_ = true; });
		if (builder_.pathField("file", n.file, audioDir())) reloadNarration_ = true;
		builder_.floatSlider("volume", n.volume, 0.0f, 4.0f);
		builder_.toggle("start when a ball is squeezed", n.startWithSensor);
		builder_.intSlider("else start after (ms)", n.autoStartDelayMs, 0, 60000);
		builder_.floatSlider("glitch threshold", n.glitchThreshold, 0.0f, 1.0f);
		builder_.floatSlider("glitch reach (of file)", n.glitchStrand, 0.0f, 0.2f);
		int windowIndex = static_cast<int>(n.window);
		if (builder_.choice("glitch window", windowIndex, windowNames())) {
			n.window = static_cast<WindowType>(windowIndex);
			structureChanged();
		}
		builder_.label("drag in the narration view to glitch");
		builder_.endGroup();
	}

	if (builder_.group("Glitch grain")) {
		const auto & specs = grainParamSpecs();
		for (int i = 0; i < GrainSettings::count(); ++i) {
			if (std::string(specs[i].id) == "position" || std::string(specs[i].id) == "volume") continue;
			ImGui::PushID(i);
			builder_.modulated(specs[i], n.grain.at(i), {}, false);
			ImGui::PopID();
		}
		builder_.endGroup();
	}

	drawEffectChainEditor("narration", n.effects);
}

void ProgrammingGui::drawEffectChainEditor(const std::string & idScope, EffectChainSettings & chain) {
	ImGui::PushID(idScope.c_str());
	if (builder_.group("Effects (in order)", true)) {
		for (size_t e = 0; e < chain.size(); ++e) {
			EffectSettings & effect = chain[e];
			const EffectInfo * info = effect.info();
			if (info == nullptr) continue;
			ImGui::PushID(static_cast<int>(e));
			if (builder_.group(std::to_string(e + 1) + ". " + info->label, true)) {
				builder_.label(info->description);
				builder_.toggle("enabled", effect.enabled, [this]() { structureChanged(); });
				for (size_t p = 0; p < info->params.size() && p < effect.params.size(); ++p) {
					ImGui::PushID(static_cast<int>(p));
					builder_.modulated(info->params[p], effect.params[p]);
					ImGui::PopID();
				}
				if (builder_.button("move up") && e > 0) {
					std::swap(chain[e], chain[e - 1]);
					structureChanged();
				}
				ImGui::SameLine();
				if (builder_.button("move down") && e + 1 < chain.size()) {
					std::swap(chain[e], chain[e + 1]);
					structureChanged();
				}
				ImGui::SameLine();
				if (builder_.button("remove")) {
					chain.erase(chain.begin() + static_cast<long>(e));
					structureChanged();
					// Close every group/ID this frame opened before bailing:
					// this effect's own group, its PushID, the outer "Effects
					// (in order)" group, and the idScope PushID - leaving any
					// of those unclosed corrupts ImGui's stack for the rest
					// of the frame. Stop walking `chain`: it just resized.
					builder_.endGroup();
					ImGui::PopID();
					builder_.endGroup();
					ImGui::PopID();
					return;
				}
				builder_.endGroup();
			}
			ImGui::PopID();
		}
		builder_.choice("new effect", addEffectChoice_, effectNames());
		if (builder_.button("add effect at the end")) {
			chain.push_back(EffectSettings::make(EffectCatalog::all()[addEffectChoice_].id));
			structureChanged();
		}
		builder_.endGroup();
	}
	ImGui::PopID();
}

// ==================================================================== profile

void ProgrammingGui::drawProfileSection() {
	ProfileSettings & p = instrument_.profile();
	auto changed = [this]() { structureChanged(); };

	if (builder_.group("Switch / new profile", false)) {
		std::vector<std::string> names = instrument_.store().listProfiles();
		int current = static_cast<int>(std::distance(names.begin(), std::find(names.begin(), names.end(), p.name)));
		if (builder_.choice("profile", current, names) && current >= 0 && current < static_cast<int>(names.size())) {
			instrument_.switchProfile(names[current]);
		}
		builder_.text("new profile name", newProfileName_);
		builder_.toggle("copy current profile's settings into it", newProfileFromCurrent_);
		if (builder_.button("create") && !newProfileName_.empty()) {
			std::string error;
			bool ok = newProfileFromCurrent_ ? instrument_.store().duplicateProfile(p.name, newProfileName_, error)
												: instrument_.store().createProfile(newProfileName_, error);
			if (ok) {
				instrument_.switchProfile(newProfileName_);
				newProfileName_.clear();
			} else {
				instrument_.note("could not create profile: " + error, true);
			}
		}
		builder_.endGroup();
	}

	if (builder_.group("Playing", false)) {
		int modeIndex = static_cast<int>(p.playMode);
		if (builder_.choice("mode", modeIndex, { "Multi (6 slots)", "Accumulated (slot 1)" })) {
			p.playMode = modeIndex == 1 ? PlayMode::Accumulated : PlayMode::Multi;
		}
		builder_.floatSlider("accumulated playhead speed", instrument_.preset().playheadSpeed, 0.0f, 0.2f);
		builder_.intSlider("control rate (Hz)", p.controlRateHz, 20, 500);
		builder_.intSlider("min ms between preset changes", p.presetSwitchMinIntervalMs, 0, 20000);
		builder_.endGroup();
	}

	if (builder_.group("Sensors", false)) {
		builder_.intSlider("raw value at full squeeze", p.sensors.maxRaw, 1, 1023, changed);
		builder_.floatSlider("noise threshold", p.sensors.activeThreshold, 0.0f, 0.5f, changed);
		int curveIndex = static_cast<int>(p.sensors.curve);
		if (builder_.choice("response curve", curveIndex, curveNames())) {
			p.sensors.curve = static_cast<Curve>(curveIndex);
			changed();
		}
		builder_.floatSlider("smoothing (ms)", p.sensors.smoothingMs, 0.0f, 300.0f, changed);
		builder_.floatSlider("balls for 100% total", p.sensors.accumulationDenominator, 0.5f, 6.0f, changed);
		builder_.intSlider("calibration frames", p.sensors.calibrationFrames, 1, 200, changed);
		builder_.endGroup();
	}

	if (builder_.group("Motion (accelerometer)", false)) {
		builder_.toggle("enabled", p.motion.enabled, changed);
		builder_.toggle("level = position at startup", p.motion.zeroAtStartup, changed);
		int axisIndex = std::max(0, static_cast<int>(std::find(kAxisIds.begin(), kAxisIds.end(), p.motion.upAxis) - kAxisIds.begin()) % 6);
		if (builder_.choice("else 'up' axis", axisIndex, kAxisIds)) {
			p.motion.upAxis = kAxisIds[axisIndex];
			changed();
		}
		builder_.floatSlider("tilt range (deg)", p.motion.rangeDegrees, 5.0f, 180.0f, changed);
		builder_.floatSlider("dead zone (deg)", p.motion.deadzoneDegrees, 0.0f, 20.0f, changed);
		builder_.floatSlider("tilt smoothing (ms)", p.motion.smoothingMs, 0.0f, 1000.0f, changed);
		builder_.floatSlider("shake sensitivity (g)", p.motion.shakeSensitivity, 0.05f, 4.0f, changed);
		builder_.floatSlider("shake release (ms)", p.motion.shakeReleaseMs, 10.0f, 3000.0f, changed);
		if (builder_.button("set level now (recalibrate)")) instrument_.recalibrate();
		builder_.endGroup();
	}

	if (builder_.group("Hit gesture (change preset)", false)) {
		builder_.toggle("enabled", p.gestures.hitToChangePreset, changed);
		builder_.floatSlider("squeeze above", p.gestures.hitThreshold, 0.0f, 1.0f, changed);
		builder_.floatSlider("release below", p.gestures.troughThreshold, 0.0f, 1.0f, changed);
		builder_.intSlider("max squeeze ms", p.gestures.maxPeakMs, 10, 2000, changed);
		builder_.intSlider("max pause ms", p.gestures.maxTroughMs, 10, 2000, changed);
		builder_.intSlider("hits needed", p.gestures.requiredHits, 1, 20, changed);
		builder_.intSlider("balls per hit", p.gestures.minSensorsPerHit, 1, kNumSensors, changed);
		builder_.endGroup();
	}

	if (builder_.group("Audio device", false)) {
		auto restart = [this]() { instrument_.restartAudio(); };
		builder_.text("api", p.audio.api, restart);
		std::vector<std::string> deviceNames = AudioEngine::outputDeviceNames(p.audio.api);
		int current = static_cast<int>(std::distance(deviceNames.begin(), std::find(deviceNames.begin(), deviceNames.end(), p.audio.deviceName)));
		if (builder_.choice("device", current, deviceNames) && current >= 0 && current < static_cast<int>(deviceNames.size())) {
			p.audio.deviceName = deviceNames[current];
			instrument_.restartAudio();
		}
		builder_.intSlider("sample rate", p.audio.sampleRate, 22050, 96000, restart);
		builder_.intSlider("buffer size", p.audio.bufferSize, 64, 4096, restart);
		builder_.intSlider("buffers", p.audio.numBuffers, 1, 8, restart);
		builder_.floatSlider("master gain", p.audio.masterGain, 0.0f, 4.0f);
		builder_.endGroup();
	}

	// Above is used as-is on a laptop. On the Pi (Instrument::hardwareExpected()),
	// this overrides it instead - so the same profile file needs no editing when
	// copied between the laptop and the instrument's USB sound card.
	if (builder_.group("Audio device on the Pi (overrides above)", false)) {
		builder_.toggle("use this on the Pi", p.audio.pi.enabled);
		builder_.text("api", p.audio.pi.api);
		builder_.text("device name contains", p.audio.pi.deviceName);
		builder_.intSlider("device id (fallback)", p.audio.pi.deviceId, -1, 64);
		builder_.endGroup();
	}

	if (builder_.group("Raspberry Pi hardware", false)) {
		builder_.text("SPI device", p.hardware.spiDevice);
		builder_.intSlider("button pin (BCM)", p.hardware.buttonPin, 0, 27);
		builder_.toggle("button pulls to ground", p.hardware.buttonActiveLow);
		builder_.intSlider("blue LED pin", p.hardware.blueLedPin, -1, 27);
		builder_.intSlider("red LED pin", p.hardware.redLedPin, -1, 27);
		builder_.intSlider("speaker relay pin", p.hardware.relayPin, -1, 27);
		builder_.intSlider("multi-click gap (ms)", p.hardware.clickTimeoutMs, 50, 1000);
		builder_.toggle("6 s press quits", p.hardware.longPressQuits);
		builder_.toggle("power off on quit", p.hardware.shutdownOnQuit);
		builder_.toggle("accelerometer", p.hardware.accelerometer);
		builder_.endGroup();
	}

	if (builder_.group("Network (OSC)", false)) {
		builder_.toggle("accept commands", p.osc.control);
		builder_.intSlider("command port", p.osc.controlPort, 1024, 65535);
		builder_.toggle("send telemetry", p.osc.telemetry);
		builder_.text("telemetry host", p.osc.remoteHost);
		builder_.intSlider("telemetry port", p.osc.remotePort, 1024, 65535);
		builder_.endGroup();
	}

	if (builder_.group("Narration (legacy)", false)) {
		builder_.toggle("enabled", p.narration.enabled, [this]() { reloadNarration_ = true; });
		builder_.label("turn on to show its own section below, with the file/grain/effects editor");
		builder_.endGroup();
	}

	if (builder_.group("Logging", false)) {
		builder_.text("level", p.logLevel);
		builder_.endGroup();
	}
}

// ===================================================================== update

void ProgrammingGui::update() {
	uint64_t now = ofGetElapsedTimeMillis();
	float dt = lastUpdateMs_ == 0 ? 0.0f : static_cast<float>(now - lastUpdateMs_) / 1000.0f;
	lastUpdateMs_ = now;
	bool simulating = instrument_.sensorSourceName() == "simulation";
	deviceView_.update(dt, simulating, instrument_.simulator(), instrument_.lastAccel());
	if (presetChanged_) {
		presetChanged_ = false;
		if (presetChoice_ != instrument_.presetIndex()) {
			instrument_.selectPreset(presetChoice_);
		}
	}
	for (int slot : samplesToReload_) {
		instrument_.reloadSlotSample(slot);
	}
	samplesToReload_.clear();
	if (reloadNarration_) {
		reloadNarration_ = false;
		instrument_.reloadNarration();
	}
	if (structureChanged_) {
		structureChanged_ = false;
		instrument_.settingsChanged();
	}
}

// ======================================================================= draw

void ProgrammingGui::draw() {
	ofBackground(18);
	// Recompute the fixed grid every frame, off menuBarHeight_ as of last
	// frame (see the comment below) - cheap, and keeps every panel's rect
	// exactly in sync with the current window size.
	layout();

	// Settings is a different context that takes over this centre region
	// (see drawSettingsWindow()): skip drawing what it would otherwise
	// overlap, rather than overlap it.
	if (!showSettings_) {
		ofSetColor(220);
		std::string header = instrument_.profile().name + " | preset " + std::to_string(instrument_.presetIndex() + 1) + ": " +
			instrument_.preset().name + " | " + modeName(instrument_.mode()) + " | sensors: " + instrument_.sensorSourceName() +
			(instrument_.isRecording() ? " | RECORDING" : "");
		size_t headerChars = static_cast<size_t>(std::max(slotViews_[2].rect().getRight() - slotViews_[0].rect().x, 8.0f) / 8.0f);
		if (header.size() > headerChars) header = header.substr(0, headerChars - 1) + "~";
		ofDrawBitmapString(header, centerRect_.x, centerRect_.y + 14.0f);

		drawSlots();
		drawNarration();
		const ProfileSettings & p = instrument_.profile();
		sensorView_.draw(instrument_.inputs(), p.sensors.curve, p.sensors.activeThreshold);
		deviceView_.draw(instrument_.inputs());
		// Hidden by default (toggle: menu bar or F10) - troubleshooting detail,
		// not something you need in view while playing or editing.
		if (showLog_) drawStatus();
	}

	gui_.begin();
	drawTransportWindow();
	drawSimulationWindow();
	drawMasterWindow();
	drawInspectorWindow();
	drawSettingsWindow();
	// menuBarHeight_ is only known once this frame's menu bar is fully
	// submitted and measured, in gui_.end() (one-frame lag feeding next
	// frame's layout() - not visible in practice). Drawn last so it paints
	// on top of the fixed panels below it, as a menu bar should.
	menuBarHeight_ = drawMenuBar();
	gui_.end();
	gui_.draw();
}

void ProgrammingGui::drawSlots() {
	bool accumulated = instrument_.profile().playMode == PlayMode::Accumulated;
	for (int i = 0; i < kMaxSlots; ++i) {
		GrainSlot & slot = instrument_.audio().slot(i);
		const SlotSettings & settings = instrument_.preset().slots[i];
		GrainMeter meter {
			[&slot]() { return slot.voices(); },
			[&slot](int v) { return slot.grainPosition(v); },
			[&slot](int v) { return slot.grainEnvelope(v); },
		};
		std::string what = settings.sourceType == "granular" ? (settings.file.empty() ? "(empty)" : settings.file)
			: SourceCatalog::find(settings.sourceType) != nullptr ? SourceCatalog::find(settings.sourceType)->label : settings.sourceType;
		std::string title = std::to_string(i + 1) + ": " + what
			+ (settings.enabled ? "" : "  [off]") + (accumulated && i > 0 ? "  [unused in accumulated mode]" : "");
		bool selected = target_ == Target::Slot && selectedSlot_ == i;
		slotViews_[i].draw(slot.sample(), title, meter, slot.currentPosition(), slot.currentVolume(), selected, slot.gateOpen());
	}
}

void ProgrammingGui::drawNarration() {
	// Legacy feature: don't clutter the centre area with it unless this
	// profile actually has narration turned on.
	if (!instrument_.profile().narration.enabled) return;
	NarrationVoice & n = instrument_.audio().narration();
	GrainMeter meter {
		[&n]() { return n.glitchVoices(); },
		[&n](int v) { return n.glitchGrainPosition(v); },
		[&n](int v) { return n.glitchGrainEnvelope(v); },
	};
	bool glitch = instrument_.mode() == Mode::NarrationGlitch;
	float head = glitch ? instrument_.glitchPosition() : n.position();
	const NarrationSettings & settings = instrument_.profile().narration;
	std::string title = "narration: ";
	if (!settings.enabled) {
		title += "(this profile has no narration)";
	} else if (settings.skip) {
		title += "(no narration - skipped)";
	} else {
		title += settings.file;
	}
	narrationView_.draw(n.sample(), title, meter, head, 1.0f, target_ == Target::Narration, glitch);
}

void ProgrammingGui::drawStatus() {
	ofPushStyle();
	ofSetColor(40);
	ofDrawRectangle(statusRect_);
	ofSetColor(200);
	float y = statusRect_.y + 16;
	// Bitmap font: 8 px per character; long lines are cut to fit the box.
	size_t maxChars = static_cast<size_t>(std::max(statusRect_.width - 12.0f, 8.0f) / 8.0f);
	for (const std::string & line : instrument_.messages()) {
		if (y > statusRect_.getBottom()) break;
		ofDrawBitmapString(line.size() > maxChars ? line.substr(0, maxChars - 1) + "~" : line, statusRect_.x + 6, y);
		y += 14;
	}
	ofPopStyle();
}

// ====================================================================== input

void ProgrammingGui::keyPressed(int key) {
	if (key >= OF_KEY_F1 && key <= OF_KEY_F6) {
		select(Target::Slot, key - OF_KEY_F1);
	} else if (key == OF_KEY_F7) {
		showSettings_ = !showSettings_;
	} else if (key == OF_KEY_F8) {
		select(Target::Narration);
	} else if (key == OF_KEY_F9) {
		SensorSimulator & s = instrument_.simulator();
		s.setRamp(ramp_);
		s.startRamp();
	} else if (key == OF_KEY_F10) {
		showLog_ = !showLog_;
	} else if (key == OF_KEY_F11) {
		ofToggleFullscreen();
	} else if (key == OF_KEY_F12) {
		std::string error;
		instrument_.saveToDisk(error);
	}
}

void ProgrammingGui::mouseMoved(int, int) {}

void ProgrammingGui::mousePressed(int x, int y, int) {
	// The centre content isn't drawn while Settings covers that region (see
	// draw()), so it shouldn't react to clicks there either.
	if (showSettings_) return;
	if (deviceView_.mousePressed(x, y)) return;
	for (int i = 0; i < kMaxSlots; ++i) {
		if (slotViews_[i].inside(x, y)) {
			if (target_ != Target::Slot || selectedSlot_ != i) select(Target::Slot, i);
			auditionSlot_ = i;
			instrument_.setAudition(i, true, slotViews_[i].positionAt(x));
			return;
		}
	}
	if (narrationView_.inside(x, y)) {
		glitchByMouse_ = true;
		instrument_.setManualGlitch(true, narrationView_.positionAt(x));
	}
}

void ProgrammingGui::mouseDragged(int x, int y, int) {
	if (deviceView_.mouseDragged(x, y)) return;
	if (auditionSlot_ >= 0) {
		instrument_.setAudition(auditionSlot_, true, slotViews_[auditionSlot_].positionAt(x));
	}
	if (glitchByMouse_) {
		instrument_.setManualGlitch(true, narrationView_.positionAt(x));
	}
}

void ProgrammingGui::mouseReleased(int x, int y, int) {
	if (deviceView_.mouseReleased(x, y)) return;
	if (auditionSlot_ >= 0) {
		// Letting go of a click-held audition must not silence a slot whose
		// "free run" toggle is separately keeping it on.
		if (!freeRun_[auditionSlot_]) instrument_.setAudition(auditionSlot_, false, 0.0f);
		auditionSlot_ = -1;
	}
	if (glitchByMouse_) {
		glitchByMouse_ = false;
		instrument_.setManualGlitch(false, 0.0f);
	}
}

void ProgrammingGui::dragEvent(const ofDragInfo & info) {
	if (info.files.empty() || showSettings_) return;
	for (int i = 0; i < kMaxSlots; ++i) {
		if (!slotViews_[i].inside(info.position.x, info.position.y)) continue;
		std::string error;
		std::string name = GuiBuilder::adoptFile(info.files[0].string(), audioDir(), error);
		if (name.empty()) {
			instrument_.note(error, true);
			return;
		}
		instrument_.preset().slots[i].file = name;
		samplesToReload_.push_back(i);
		select(Target::Slot, i);
		return;
	}
}

} // namespace gm
