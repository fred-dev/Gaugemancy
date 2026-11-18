#include "ProgrammingGui.h"

#include "imgui.h"

#include <algorithm>
#include <cmath>

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

// The fixed grid. Bars have a fixed height; everything else is a share of
// the window, decided in layout() - the one place panel sizes are set.
constexpr float kTopBarHeight = 48.0f;
constexpr float kBottomBarHeight = 44.0f;
constexpr float kLeftColumnFrac = 0.19f;  // the instrument column
constexpr float kDeviceFrac = 0.36f;      // share of the column's height: 3D view
constexpr float kSensorsFrac = 0.22f;     // sensor faders
constexpr float kRampFrac = 0.21f;        // squeeze ramp (performance gets the rest)
constexpr float kTileFrac = 0.12f;        // height of the tile row
constexpr float kSourceFrac = 0.27f;      // width of the Source panel in the editor
constexpr float kParametersFrac = 0.45f;  // height of Parameters (Effects gets the rest)

// Fixed numbers of places, so nothing shifts when a list grows.
constexpr int kPresetPlaces = 12;
constexpr int kEffectPlaces = 8;
constexpr int kParameterRows = 9;       // the grain engine has 9; synths have fewer
constexpr int kEffectParameterRows = 7; // the reverb has 7; others have fewer

const ImVec4 kSelectedColor(0.95f, 0.65f, 0.15f, 1.0f);
const ImVec4 kSelectedText(0.08f, 0.08f, 0.08f, 1.0f);

/// A button that is highlighted while `selected`.
bool selectableButton(const std::string & label, bool selected, const ImVec2 & size) {
	if (selected) {
		ImGui::PushStyleColor(ImGuiCol_Button, kSelectedColor);
		ImGui::PushStyleColor(ImGuiCol_Text, kSelectedText);
	}
	bool clicked = ImGui::Button(label.c_str(), size);
	if (selected) ImGui::PopStyleColor(2);
	return clicked;
}

/// Width a button with this label takes, including the gap after it.
float buttonWidth(const char * label) {
	const ImGuiStyle & style = ImGui::GetStyle();
	return ImGui::CalcTextSize(label, nullptr, true).x + 2.0f * style.FramePadding.x + style.ItemSpacing.x;
}

/// A vertical level meter filling the last drawn item from the bottom.
void overlayLevel(float level, ImU32 colour) {
	ImVec2 lo = ImGui::GetItemRectMin();
	ImVec2 hi = ImGui::GetItemRectMax();
	float top = hi.y - (hi.y - lo.y) * std::clamp(level, 0.0f, 1.0f);
	ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(lo.x, top), hi, colour);
}

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
	instrument_.audio().enableScopes(); // live scopes are only drawn here
	ofSetWindowTitle("Gaugemancy");
	ofBackground(18);
	// A new file name per layout generation: positions saved for an older
	// layout must not be applied to this one.
	gui_.iniFileName = ofToDataPath("imgui_v3.ini");
	gui_.setup(nullptr, false);
	applyAccessibleTheme();
	if (gui_.addFont(kFontPath, kFontSize, nullptr, nullptr, true) != nullptr) {
		ImGui::GetStyle().ScaleAllSizes(kStyleScale);
	}
	ramp_ = instrument_.simulator().ramp();
	layout();
}

void ProgrammingGui::layout() {
	const float w = static_cast<float>(ofGetWidth());
	const float h = static_cast<float>(ofGetHeight());
	const float m = kMargin;

	topRect_.set(m, m, std::max(w - 2.0f * m, 100.0f), kTopBarHeight);
	bottomRect_.set(m, h - m - kBottomBarHeight, topRect_.width, kBottomBarHeight);
	const float top = topRect_.getBottom() + m;
	const float height = std::max(bottomRect_.y - m - top, 100.0f);

	// Left column: the instrument, top to bottom.
	const float leftW = std::max(w * kLeftColumnFrac, 300.0f);
	float y = top;
	auto take = [&](float fraction) {
		ofRectangle rect(m, y, leftW, height * fraction - m);
		y += height * fraction;
		return rect;
	};
	deviceView_.setRect(take(kDeviceFrac));
	sensorsRect_ = take(kSensorsFrac);
	rampRect_ = take(kRampFrac);
	performanceRect_.set(m, y, leftW, top + height - y);

	// Tile row: eight equal tiles across the rest of the window.
	const float editorX = m + leftW + m;
	const float editorW = std::max(w - editorX - m, 200.0f);
	const float tileH = std::clamp(height * kTileFrac, 70.0f, 140.0f);
	const float tileW = (editorW - (kTargets - 1) * m) / kTargets;
	auto tile = [&](int index) { return ofRectangle(editorX + index * (tileW + m), top, tileW, tileH); };
	for (int i = 0; i < kMaxSlots; ++i) slotViews_[i].setRect(tile(i));
	masterTileRect_ = tile(kMaxSlots);
	narrationView_.setRect(tile(kMaxSlots + 1));

	// Editor: Source on the left, Parameters above Effects on the right.
	editorRect_.set(editorX, top + tileH + m, editorW, height - tileH - m);
	const float sourceW = std::max(editorW * kSourceFrac, 320.0f);
	const float parametersH = editorRect_.height * kParametersFrac;
	sourceRect_.set(editorRect_.x, editorRect_.y, sourceW, editorRect_.height);
	parametersRect_.set(editorRect_.x + sourceW + m, editorRect_.y, editorW - sourceW - m, parametersH - m * 0.5f);
	effectsRect_.set(parametersRect_.x, editorRect_.y + parametersH + m * 0.5f, parametersRect_.width,
		editorRect_.height - parametersH - m * 0.5f);
}

void ProgrammingGui::windowResized(int, int) {
	layout();
}

std::string ProgrammingGui::audioDir() const {
	return instrument_.store().audioDir(instrument_.profile().name);
}

void ProgrammingGui::select(Target target, int slot) {
	target_ = target;
	if (target == Target::Slot) selectedSlot_ = std::clamp(slot, 0, kMaxSlots - 1);
	// Picking something to edit always brings the editor back.
	showSettings_ = false;
	showLog_ = false;
}

int ProgrammingGui::targetIndex() const {
	if (target_ == Target::Master) return kMaxSlots;
	if (target_ == Target::Narration) return kMaxSlots + 1;
	return selectedSlot_;
}

EffectChainSettings & ProgrammingGui::chainOf(int index) {
	if (index == kMaxSlots) return instrument_.preset().masterEffects;
	if (index == kMaxSlots + 1) return instrument_.profile().narration.effects;
	return instrument_.preset().slots[std::clamp(index, 0, kMaxSlots - 1)].effects;
}

std::string ProgrammingGui::tileName(int index) {
	if (index == kMaxSlots) return "Master";
	if (index == kMaxSlots + 1) return "Narration";
	return "Slot " + std::to_string(index + 1);
}

void ProgrammingGui::tileClicked(int index) {
	constexpr uint64_t kDoubleClickMs = 350;
	uint64_t now = ofGetElapsedTimeMillis();
	bool doubleClick = index == lastTileClicked_ && now - lastTileClickMs_ < kDoubleClickMs;
	lastTileClicked_ = doubleClick ? -1 : index; // a third click starts a new pair
	lastTileClickMs_ = now;
	if (index < kMaxSlots) select(Target::Slot, index);
	else select(index == kMaxSlots ? Target::Master : Target::Narration);
	if (doubleClick) {
		effectsLock_ = effectsLock_ == index ? -1 : index;
		instrument_.note(effectsLock_ >= 0 ? "effects panel locked to " + tileName(index) : "effects panel follows the selected tile");
	}
}

void ProgrammingGui::drawLockMarker(const ofRectangle & tile) const {
	ofPushStyle();
	ofFill();
	ofSetColor(255, 190, 40);
	ofDrawRectangle(tile.x, tile.getBottom() - 16.0f, 68.0f, 16.0f);
	ofSetColor(20);
	ofDrawBitmapString("FX LOCK", tile.x + 6.0f, tile.getBottom() - 4.0f);
	ofPopStyle();
}

void ProgrammingGui::refreshAudioLists() {
	const ProfileSettings & p = instrument_.profile();
	deviceNames_ = AudioEngine::outputDeviceNames(p.audio.api);
	sampleRates_ = instrument_.supportedSampleRates();
	audioListsStale_ = false;
}

// ===================================================================== top bar

void ProgrammingGui::drawTopBar() {
	panels_.beginBar("##top", topRect_);
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(instrument_.profile().name.c_str());
	ImGui::SameLine();
	ImGui::TextDisabled("preset");

	// A fixed row of places: preset 3 is always the third button.
	const int count = static_cast<int>(instrument_.presets().size());
	const float place = ImGui::GetFrameHeight() * 1.35f;
	for (int i = 0; i < kPresetPlaces; ++i) {
		ImGui::SameLine();
		ImGui::PushID(i);
		ImGui::BeginDisabled(i >= count);
		if (selectableButton(std::to_string(i + 1), i == instrument_.presetIndex(), ImVec2(place, 0))) {
			presetRequest_ = i;
		}
		if (i < count && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", instrument_.presets()[i].name.c_str());
		ImGui::EndDisabled();
		ImGui::PopID();
	}
	ImGui::SameLine();
	if (ImGui::Button("+ copy")) {
		ProfileSettings & profile = instrument_.profile();
		instrument_.presets().push_back(instrument_.preset());
		profile.presetFiles.push_back("presets/preset_" + std::to_string(instrument_.presets().size()) + ".json");
		instrument_.note("added preset " + std::to_string(instrument_.presets().size()) + " (save to keep it)");
		presetRequest_ = static_cast<int>(instrument_.presets().size()) - 1;
	}
	ImGui::SameLine();
	ImGui::TextUnformatted((instrument_.preset().name + (instrument_.audio().isFading() ? "  (crossfading)" : "")).c_str());

	// Right-aligned: files and the two occasional panels, always in this order.
	const char * labels[] = { "Save all (F12)", "Save preset", "Reload (discard edits)", "Settings (F7)", "Log (F10)" };
	float width = 0.0f;
	for (const char * label : labels) width += buttonWidth(label);
	ImGui::SameLine(std::max(ImGui::GetWindowWidth() - width - ImGui::GetStyle().WindowPadding.x, ImGui::GetCursorPosX()));
	std::string error;
	if (ImGui::Button(labels[0])) instrument_.saveToDisk(error);
	ImGui::SameLine();
	if (ImGui::Button(labels[1])) instrument_.savePresetToDisk(error);
	ImGui::SameLine();
	if (ImGui::Button(labels[2])) instrument_.reloadFromDisk();
	ImGui::SameLine();
	if (selectableButton(labels[3], showSettings_, ImVec2(0, 0))) {
		audioListsStale_ = true; // ask the system again each time Settings opens
		showSettings_ = !showSettings_;
		showLog_ = false;
	}
	ImGui::SameLine();
	if (selectableButton(labels[4], showLog_, ImVec2(0, 0))) {
		showLog_ = !showLog_;
		showSettings_ = false;
	}
	panels_.end();
}

// ================================================================== bottom bar

void ProgrammingGui::drawBottomBar() {
	panels_.beginBar("##bottom", bottomRect_);
	const float meterW = ImGui::GetFrameHeight() * 5.0f;
	ImGui::AlignTextToFramePadding();
	ImGui::TextDisabled("out");
	ImGui::SameLine();
	ImGui::ProgressBar(std::clamp(meterPeak_[0], 0.0f, 1.0f), ImVec2(meterW, 0), "L");
	ImGui::SameLine();
	ImGui::ProgressBar(std::clamp(meterPeak_[1], 0.0f, 1.0f), ImVec2(meterW, 0), "R");
	ImGui::SameLine();
	if (instrument_.audioStalled() || !instrument_.audioOpen()) {
		// Nothing can be heard: say so where the eye already is.
		ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.3f, 1.0f), "%s",
			instrument_.audioStalled() ? "AUDIO NOT RUNNING: check Settings > Audio device  | " : "NO AUDIO DEVICE  | ");
		ImGui::SameLine();
	}
	ImGui::Text("%s  |  sensors: %s  |  dropped buffers: %llu%s", modeName(instrument_.mode()),
		instrument_.sensorSourceName().c_str(), static_cast<unsigned long long>(instrument_.audio().skippedCallbacks()),
		instrument_.isRecording() ? "  |  RECORDING" : "");
	if (!instrument_.messages().empty()) {
		ImGui::SameLine();
		ImGui::TextDisabled("|  %s", instrument_.messages().back().c_str());
	}
	panels_.end();
}

// ========================================================== instrument column

void ProgrammingGui::drawSensorsPanel() {
	panels_.begin("Sensors (drag = simulate, fill = live)", sensorsRect_);
	SensorSimulator & sim = instrument_.simulator();
	const ControlInputs & inputs = instrument_.inputs();
	const ImU32 live = ImGui::GetColorU32(ImVec4(0.2f, 0.75f, 0.95f, 0.55f));
	const char * extraNames[] = { "all", "tx", "ty", "tilt", "shk" };
	const ModSource extras[] = { ModSource::Total, ModSource::TiltX, ModSource::TiltY, ModSource::Tilt, ModSource::Shake };
	const int columns = kNumSensors + 5;

	if (ImGui::BeginTable("sensors", columns, ImGuiTableFlags_SizingStretchSame)) {
		float barH = std::max(ImGui::GetContentRegionAvail().y - ImGui::GetTextLineHeightWithSpacing() - 8.0f, 40.0f);
		ImGui::TableNextRow();
		for (int i = 0; i < kNumSensors; ++i) {
			ImGui::TableNextColumn();
			ImGui::PushID(i);
			simLevels_[i] = sim.manual(i);
			// The fader sets the simulated squeeze; the fill shows what the
			// instrument actually reads (also from hardware or a replay).
			if (ImGui::VSliderFloat("##fader", ImVec2(ImGui::GetContentRegionAvail().x, barH), &simLevels_[i], 0.0f, 1.0f, "")) {
				sim.setManual(i, simLevels_[i]);
			}
			overlayLevel(inputs.sensor(i), live);
			ImGui::PopID();
		}
		for (int i = 0; i < 5; ++i) {
			ImGui::TableNextColumn();
			ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x, barH));
			bool available = inputs.motionAvailable || !isMotionSource(extras[i]);
			ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
				ImGui::GetColorU32(ImGuiCol_Border, available ? 1.0f : 0.3f));
			if (available) overlayLevel(inputs.get(extras[i]), live);
			else if (ImGui::IsItemHovered()) ImGui::SetTooltip("this profile has no gyro (Settings)");
		}
		ImGui::TableNextRow();
		for (int i = 0; i < columns; ++i) {
			ImGui::TableNextColumn();
			if (i < kNumSensors) ImGui::Text("%d", i + 1);
			else ImGui::TextDisabled("%s", extraNames[i - kNumSensors]);
		}
		ImGui::EndTable();
	}
	panels_.end();
}

void ProgrammingGui::drawRampPanel() {
	panels_.begin("Squeeze ramp (F9)", rampRect_);
	SensorSimulator & sim = instrument_.simulator();
	if (selectableButton(sim.rampRunning() ? "stop ramp" : "start ramp", sim.rampRunning(), ImVec2(-FLT_MIN, 0))) {
		if (sim.rampRunning()) {
			sim.stopRamp();
		} else {
			sim.setRamp(ramp_);
			sim.startRamp();
		}
	}
	builder_.floatSlider("speed", ramp_.speed, 0.05f, 5.0f);
	builder_.floatSlider("peak", ramp_.extent, 0.0f, 1.0f);
	builder_.toggle("loop", ramp_.loop);
	ImGui::AlignTextToFramePadding();
	ImGui::TextDisabled("balls");
	for (int i = 0; i < kNumSensors; ++i) {
		ImGui::SameLine();
		ImGui::PushID(i);
		if (selectableButton(std::to_string(i + 1), ramp_.sensors[i], ImVec2(ImGui::GetFrameHeight(), 0))) {
			ramp_.sensors[i] = !ramp_.sensors[i];
		}
		ImGui::PopID();
	}
	panels_.end();
}

void ProgrammingGui::drawPerformancePanel() {
	panels_.begin("Performance", performanceRect_);
	const float half = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
	if (ImGui::Button("recalibrate", ImVec2(half, 0))) instrument_.recalibrate();
	ImGui::SameLine();
	ImGui::BeginDisabled(!instrument_.profile().narration.enabled || instrument_.profile().narration.skip);
	if (ImGui::Button("always skip narration", ImVec2(half, 0))) instrument_.skipNarration();
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
		ImGui::SetTooltip("Saved: this profile starts straight with the slots from now on");
	}
	ImGui::EndDisabled();
	// While the narration runs the six slots are silent, so squeezes in the
	// 3D view or the faders drive the narration, not the slots.
	bool narrationFirst = instrument_.mode() != Mode::Granular;
	ImGui::BeginDisabled(!narrationFirst);
	if (selectableButton(narrationFirst ? "narration first: play the slots now (not saved)" : "slots playing",
			narrationFirst, ImVec2(-FLT_MIN, 0))) {
		instrument_.playSlotsNow();
	}
	ImGui::EndDisabled();
	sourceChoice_ = instrument_.sensorSourceName() == "hardware" ? 1 : 0;
	if (builder_.choice("sensors", sourceChoice_, { "Simulation", "Hardware" })) {
		instrument_.useSensorMode(sourceChoice_ == 1 ? SensorMode::Hardware : SensorMode::Simulation);
	}
	builder_.text("file", recordFile_);
	if (selectableButton(instrument_.isRecording() ? "stop recording" : "record sensors", instrument_.isRecording(),
			ImVec2(-FLT_MIN, 0))) {
		if (instrument_.isRecording()) instrument_.stopRecording();
		else instrument_.startRecording(ofToDataPath(recordFile_, true));
	}
	panels_.end();
}

// ====================================================================== source

void ProgrammingGui::drawSourcePanel() {
	std::string title = target_ == Target::Master ? "Master" : target_ == Target::Narration ? "Narration"
		: "Slot " + std::to_string(selectedSlot_ + 1) + " source";
	panels_.begin(title + "###source", sourceRect_);
	if (target_ == Target::Master) drawMasterSource();
	else if (target_ == Target::Narration) drawNarrationSource(instrument_.profile().narration);
	else drawSlotSource(instrument_.preset().slots[selectedSlot_], selectedSlot_);
	drawMasterGains();
	panels_.end();
}

void ProgrammingGui::drawSlotSource(SlotSettings & slot, int index) {
	ImGui::PushID(index);
	bool granular = slot.sourceType == "granular";
	if (!slot.enabled) {
		// There is no separate "enabled" switch: "heard" below decides.
		slot.enabled = true;
		structureChanged();
	}

	int sourceIndex = sourceTypeIndex(slot.sourceType);
	if (builder_.choice("type", sourceIndex, sourceTypeNames())) {
		slot.sourceType = SourceCatalog::all()[sourceIndex].id;
		slot.synthParams = SourceCatalog::makeParams(slot.sourceType);
		granular = slot.sourceType == "granular";
		if (freeRun_[index]) {
			// Free run only exists for a synth source; never leave it stuck on.
			freeRun_[index] = false;
			instrument_.setAudition(index, false, 0.0f);
		}
		structureChanged();
	}

	// "Heard": tilt and shake are only offered when the instrument has a gyro
	// (one already in use stays visible, marked, so nothing changes silently).
	struct Gate {
		std::string label;
		GateMode mode;
		ModSource source;
	};
	std::vector<Gate> gates = { { "Auto (while its sensors are pressed)", GateMode::Auto, ModSource::None },
		{ "Always", GateMode::Always, ModSource::None } };
	const bool gyro = builder_.motionAvailable();
	for (int i = 1; i < kNumModSources; ++i) {
		ModSource source = static_cast<ModSource>(i);
		bool inUse = slot.gate == GateMode::Source && slot.gateSource == source;
		if (!gyro && isMotionSource(source) && !inUse) continue;
		gates.push_back({ "While " + modSourceLabel(source) + (!gyro && isMotionSource(source) ? " (no gyro: follows the balls)" : ""),
			GateMode::Source, source });
	}
	std::vector<std::string> gateNames;
	int gateIndex = 0;
	for (size_t i = 0; i < gates.size(); ++i) {
		gateNames.push_back(gates[i].label);
		if (gates[i].mode == slot.gate && (slot.gate != GateMode::Source || gates[i].source == slot.gateSource)) {
			gateIndex = static_cast<int>(i);
		}
	}
	if (builder_.choice("heard", gateIndex, gateNames)) {
		slot.gate = gates[gateIndex].mode;
		slot.gateSource = gates[gateIndex].source;
	}

	// Sample rows: greyed out, not removed, for a synth source.
	ImGui::BeginDisabled(!granular);
	if (builder_.pathField("file", slot.file, audioDir())) samplesToReload_.push_back(index);
	int windowIndex = static_cast<int>(slot.window);
	if (builder_.choice("grain window", windowIndex, windowNames())) {
		slot.window = static_cast<WindowType>(windowIndex);
		structureChanged();
	}
	builder_.intSlider("grain voices (cpu)", slot.voices, 1, 32, [this]() { structureChanged(); });
	ImGui::EndDisabled();

	// Free run row: greyed out for the granular source.
	ImGui::BeginDisabled(granular);
	if (builder_.toggle("free run (play continuously)", freeRun_[index])) {
		instrument_.setAudition(index, freeRun_[index], 0.0f);
	}
	ImGui::EndDisabled();

	ImGui::Separator();
	if (const SourceInfo * info = SourceCatalog::find(slot.sourceType)) builder_.label(info->description);
	ImGui::PopID();
}

void ProgrammingGui::drawMasterSource() {
	builder_.label("The effects on the right are applied to the mix of all six slots. The gains and the output "
				   "level are at the foot of this panel whatever tile is selected: preset gain is saved with the "
				   "preset, profile gain with the profile.");
}

void ProgrammingGui::drawMasterGains() {
	// Always shown, pinned to the bottom of the Source panel: a separator and
	// four rows (two sliders, two meters). If the rows above ever reach this
	// far, the block follows them instead of overlapping (the panel scrolls).
	const float blockH = 4.0f * ImGui::GetFrameHeightWithSpacing() + 2.0f;
	const float bottom = ImGui::GetCursorPosY() + ImGui::GetContentRegionAvail().y;
	ImGui::SetCursorPosY(std::max(ImGui::GetCursorPosY(), bottom - blockH));
	ImGui::Separator();
	builder_.floatSlider("preset gain", instrument_.preset().masterGain, 0.0f, 4.0f);
	builder_.floatSlider("profile gain", instrument_.profile().audio.masterGain, 0.0f, 4.0f);
	ImGui::ProgressBar(std::clamp(meterPeak_[0], 0.0f, 1.0f), ImVec2(-FLT_MIN, 0), "output L");
	ImGui::ProgressBar(std::clamp(meterPeak_[1], 0.0f, 1.0f), ImVec2(-FLT_MIN, 0), "output R");
}

void ProgrammingGui::drawNarrationSource(NarrationSettings & n) {
	builder_.toggle("enabled", n.enabled, [this]() { reloadNarration_ = true; });
	ImGui::BeginDisabled(!n.enabled);
	builder_.toggle("skip (start straight in granular)", n.skip, [this]() { reloadNarration_ = true; });
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
	ImGui::EndDisabled();
	ImGui::Separator();
	builder_.label("Drag in the narration tile to glitch by hand. The parameters on the right shape the glitch grains.");
}

// ================================================================== parameters

void ProgrammingGui::drawParametersPanel() {
	panels_.begin("Parameters###parameters", parametersRect_);
	const ControlInputs & inputs = instrument_.inputs();
	if (builder_.beginParamTable("parameters")) {
		ImGui::PushID(targetIndex());
		if (target_ == Target::Slot) {
			SlotSettings & slot = instrument_.preset().slots[selectedSlot_];
			if (slot.sourceType == "granular") {
				const auto & specs = grainParamSpecs();
				for (int i = 0; i < GrainSettings::count(); ++i) {
					builder_.paramRow(specs[i], slot.grain.at(i), slot.grain.at(i).resolve(inputs));
				}
			} else if (const SourceInfo * info = SourceCatalog::find(slot.sourceType)) {
				for (size_t i = 0; i < info->params.size() && i < slot.synthParams.size(); ++i) {
					builder_.paramRow(info->params[i], slot.synthParams[i], slot.synthParams[i].resolve(inputs));
				}
			}
		} else if (target_ == Target::Narration) {
			// The glitch grain uses fixed values; its position and volume
			// come from the glitch itself.
			NarrationSettings & n = instrument_.profile().narration;
			const auto & specs = grainParamSpecs();
			for (int i = 0; i < GrainSettings::count(); ++i) {
				if (std::string(specs[i].id) == "position" || std::string(specs[i].id) == "volume") continue;
				builder_.paramRow(specs[i], n.grain.at(i), n.grain.at(i).value, false);
			}
		}
		ImGui::PopID();
		builder_.endParamTable(kParameterRows);
	}
	panels_.end();
}

// ===================================================================== effects

void ProgrammingGui::drawEffectsPanel() {
	const int index = effectsIndex();
	std::string title = "Effects of " + tileName(index) + ", in order"
		+ (effectsLock_ >= 0 ? "   [LOCKED - double-click a tile to unlock or move the lock]" : "   (double-click a tile to lock)");
	panels_.begin(title + "###effects", effectsRect_);
	EffectChainSettings & chain = chainOf(index);
	int & selected = selectedEffect_[index];
	selected = std::clamp(selected, 0, std::max(static_cast<int>(chain.size()) - 1, 0));
	ImGui::PushID(index);

	drawEffectPlaces(chain, selected);
	drawEffectControls(chain, selected);

	bool hasEffect = selected < static_cast<int>(chain.size());
	const EffectInfo * info = hasEffect ? chain[selected].info() : nullptr;
	if (builder_.beginParamTable("effect")) {
		if (info != nullptr) {
			EffectSettings & effect = chain[selected];
			ImGui::PushID(selected);
			for (size_t p = 0; p < info->params.size() && p < effect.params.size(); ++p) {
				builder_.paramRow(info->params[p], effect.params[p], effect.params[p].resolve(instrument_.inputs()));
			}
			ImGui::PopID();
		}
		builder_.endParamTable(kEffectParameterRows);
	}
	ImGui::PopID();
	panels_.end();
}

void ProgrammingGui::drawEffectPlaces(EffectChainSettings & chain, int & selected) {
	// Eight places, always: effect 3 is always the third button.
	const float spacing = ImGui::GetStyle().ItemSpacing.x;
	const float placeW = (ImGui::GetContentRegionAvail().x - (kEffectPlaces - 1) * spacing) / kEffectPlaces;
	const int count = static_cast<int>(chain.size());
	for (int i = 0; i < kEffectPlaces; ++i) {
		if (i > 0) ImGui::SameLine();
		ImGui::PushID(i);
		if (i < count) {
			const EffectInfo * info = chain[i].info();
			std::string label = std::to_string(i + 1) + " " + (info != nullptr ? info->label : chain[i].type)
				+ (chain[i].enabled ? "" : " (off)");
			if (selectableButton(label, i == selected, ImVec2(placeW, 0))) selected = i;
		} else if (i == count) {
			if (ImGui::Button("+ add", ImVec2(placeW, 0))) ImGui::OpenPopup("add effect");
			if (ImGui::BeginPopup("add effect")) {
				for (const EffectInfo & info : EffectCatalog::all()) {
					std::string label = info.label + "  (cpu " + std::to_string(info.cpuCost) + ")";
					if (ImGui::Selectable(label.c_str())) {
						chain.push_back(EffectSettings::make(info.id));
						selected = static_cast<int>(chain.size()) - 1;
						structureChanged();
					}
				}
				ImGui::EndPopup();
			}
		} else {
			ImGui::BeginDisabled();
			ImGui::Button("##empty", ImVec2(placeW, 0));
			ImGui::EndDisabled();
		}
		ImGui::PopID();
	}
}

void ProgrammingGui::drawEffectControls(EffectChainSettings & chain, int & selected) {
	// One fixed row of controls and one line of description, above the
	// parameter rows so they never move with the number of parameters.
	const int count = static_cast<int>(chain.size());
	bool hasEffect = selected < count;
	bool enabled = hasEffect ? chain[selected].enabled : false;
	enum class Action { None, Left, Right, Remove } action = Action::None;

	ImGui::BeginDisabled(!hasEffect);
	if (ImGui::Checkbox("on", &enabled) && hasEffect) {
		chain[selected].enabled = enabled;
		structureChanged();
	}
	ImGui::SameLine();
	if (ImGui::Button("move left")) action = Action::Left;
	ImGui::SameLine();
	if (ImGui::Button("move right")) action = Action::Right;
	ImGui::SameLine();
	if (ImGui::Button("remove")) action = Action::Remove;
	ImGui::EndDisabled();

	const EffectInfo * info = hasEffect ? chain[selected].info() : nullptr;
	ImGui::SameLine();
	ImGui::TextDisabled("%s", info != nullptr ? info->description.c_str() : "no effect here: use + add");

	if (!hasEffect) return;
	if (action == Action::Left && selected > 0) {
		std::swap(chain[selected], chain[selected - 1]);
		--selected;
		structureChanged();
	} else if (action == Action::Right && selected + 1 < count) {
		std::swap(chain[selected], chain[selected + 1]);
		++selected;
		structureChanged();
	} else if (action == Action::Remove) {
		chain.erase(chain.begin() + selected);
		selected = std::max(std::min(selected, static_cast<int>(chain.size()) - 1), 0);
		structureChanged();
	}
}

// ============================================================ settings and log

void ProgrammingGui::drawSettingsPanel() {
	panels_.begin("Settings - profile: " + instrument_.profile().name + "###settings", editorRect_);
	drawProfileSection();
	panels_.end();
}

void ProgrammingGui::drawLogPanel() {
	panels_.begin("Message log###log", editorRect_);
	for (const std::string & line : instrument_.messages()) {
		ImGui::TextWrapped("%s", line.c_str());
	}
	panels_.end();
}

// ==================================================================== profile

void ProgrammingGui::drawProfileSection() {
	ProfileSettings & p = instrument_.profile();
	auto changed = [this]() { structureChanged(); };

	// Master switch, always visible: an instrument without the gyro has no
	// tilt or shake, so those sources disappear everywhere when this is off.
	builder_.toggle("Has gyro (tilt and shake sensors)", p.motion.enabled, changed);
	if (!p.motion.enabled) {
		ImGui::SameLine();
		ImGui::TextDisabled("off: tilt and shake cannot be chosen, and anything mapped to them keeps its fixed value");
	}
	ImGui::Separator();

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
		builder_.intSlider("preset crossfade (ms)", p.presetFadeMs, 0, 10000);
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

	if (builder_.group("Motion (gyro / accelerometer)", false)) {
		ImGui::BeginDisabled(!p.motion.enabled);
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
		ImGui::EndDisabled();
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
		if (audioListsStale_) refreshAudioLists();
		auto restart = [this]() {
			instrument_.restartAudio();
			audioListsStale_ = true;
		};
		builder_.text("api", p.audio.api, restart);
		int current = static_cast<int>(std::distance(deviceNames_.begin(), std::find(deviceNames_.begin(), deviceNames_.end(), p.audio.deviceName)));
		if (builder_.choice("device", current, deviceNames_) && current >= 0 && current < static_cast<int>(deviceNames_.size())) {
			p.audio.deviceName = deviceNames_[current];
			restart();
		}
		ImGui::SameLine();
		if (builder_.button("rescan")) audioListsStale_ = true;

		// Only the rates this device can really run at: any other rate is silence.
		std::vector<int> rates = sampleRates_.empty() ? std::vector<int> { 22050, 32000, 44100, 48000, 88200, 96000 } : sampleRates_;
		std::vector<std::string> rateNames;
		for (int rate : rates) rateNames.push_back(std::to_string(rate) + " Hz");
		int running = instrument_.audioOpen() ? instrument_.audio().sampleRate() : p.audio.sampleRate;
		int rateIndex = static_cast<int>(std::distance(rates.begin(), std::find(rates.begin(), rates.end(), running)));
		if (builder_.choice("sample rate", rateIndex, rateNames)) {
			p.audio.sampleRate = rates[rateIndex];
			restart();
		}
		if (sampleRates_.empty()) {
			builder_.label("This device does not list its sample rates; common ones are offered.");
		} else if (p.audio.sampleRate != running) {
			builder_.label("The saved rate " + std::to_string(p.audio.sampleRate) + " Hz is not supported by this device, so "
				+ std::to_string(running) + " Hz is used. Pick a rate to save it.");
		}

		const std::vector<int> sizes = { 64, 128, 256, 512, 1024, 2048, 4096 };
		std::vector<std::string> sizeNames;
		for (int size : sizes) sizeNames.push_back(std::to_string(size) + " samples");
		int sizeIndex = static_cast<int>(std::distance(sizes.begin(), std::lower_bound(sizes.begin(), sizes.end(), p.audio.bufferSize)));
		if (builder_.choice("buffer size", sizeIndex, sizeNames)) {
			p.audio.bufferSize = sizes[sizeIndex];
			restart();
		}
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
		builder_.label("the Narration tile (F8) has the file, glitch grain and effects");
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
	deviceView_.setTiltEnabled(instrument_.profile().motion.enabled);
	for (int ch = 0; ch < 2; ++ch) {
		// The meter reads single samples: hold the peak briefly so it is readable.
		meterPeak_[ch] = std::max(std::fabs(instrument_.audio().masterMeter(ch)), meterPeak_[ch] * 0.92f);
	}
	deviceView_.update(dt, simulating, instrument_.simulator(), instrument_.lastAccel());
	if (presetRequest_ >= 0) {
		if (presetRequest_ != instrument_.presetIndex()) {
			instrument_.selectPreset(presetRequest_);
		}
		presetRequest_ = -1;
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
	layout(); // cheap; keeps every rect in step with the window size
	builder_.setMotionAvailable(instrument_.profile().motion.enabled);

	drawSlotTiles();
	drawMasterTile();
	drawNarrationTile();
	if (effectsLock_ >= 0) {
		drawLockMarker(effectsLock_ < kMaxSlots ? slotViews_[effectsLock_].rect()
			: effectsLock_ == kMaxSlots ? masterTileRect_ : narrationView_.rect());
	}
	deviceView_.draw(instrument_.inputs());
	drawNarrationWarning();

	gui_.begin();
	drawTopBar();
	drawSensorsPanel();
	drawRampPanel();
	drawPerformancePanel();
	if (showSettings_) {
		drawSettingsPanel();
	} else if (showLog_) {
		drawLogPanel();
	} else {
		drawSourcePanel();
		drawParametersPanel();
		drawEffectsPanel();
	}
	drawBottomBar();
	gui_.end();
	gui_.draw();
}

void ProgrammingGui::drawSlotTiles() {
	bool accumulated = instrument_.profile().playMode == PlayMode::Accumulated;
	for (int i = 0; i < kMaxSlots; ++i) {
		GrainSlot & slot = instrument_.audio().slot(i);
		const SlotSettings & settings = instrument_.preset().slots[i];
		bool granular = settings.sourceType == "granular";
		const SourceInfo * source = SourceCatalog::find(settings.sourceType);
		std::string what = granular ? (settings.file.empty() ? "(empty)" : settings.file)
			: source != nullptr ? source->label : settings.sourceType;
		std::string title = std::to_string(i + 1) + ": " + what + (accumulated && i > 0 ? " [unused]" : "");
		bool selected = target_ == Target::Slot && selectedSlot_ == i;
		if (granular) {
			GrainMeter meter {
				[&slot]() { return slot.voices(); },
				[&slot](int v) { return slot.grainPosition(v); },
				[&slot](int v) { return slot.grainEnvelope(v); },
			};
			slotViews_[i].draw(slot.sample(), title, meter, slot.currentPosition(), slot.currentVolume(), selected, slot.gateOpen());
		} else {
			// No sample to plot for a synth source: trace its output instead.
			slotViews_[i].drawLive(slot.scopeBuffer(), title, slot.currentVolume(), selected, slot.gateOpen());
		}
	}
}

void ProgrammingGui::drawNarrationWarning() {
	if (instrument_.mode() == Mode::Granular) return;
	// Squeezes start or glitch the narration in these modes; the slots only
	// play after it, which looks like "no sound" while programming.
	const ofRectangle & r = deviceView_.rect();
	ofPushStyle();
	ofFill();
	ofSetColor(150, 30, 30, 220);
	ofDrawRectangle(r.x, r.getBottom() - 44.0f, r.width, 44.0f);
	ofSetColor(255);
	ofDrawBitmapString("NARRATION: slots are silent", r.x + 8.0f, r.getBottom() - 26.0f);
	ofDrawBitmapString("see Performance, play slots", r.x + 8.0f, r.getBottom() - 10.0f);
	ofPopStyle();
}

void ProgrammingGui::drawMasterTile() {
	const ofRectangle & r = masterTileRect_;
	bool selected = target_ == Target::Master;
	ofPushStyle();
	// Two output meters, left and right, filling the tile from the bottom.
	ofFill();
	ofSetColor(0, 140, 140);
	for (int ch = 0; ch < 2; ++ch) {
		float level = std::clamp(meterPeak_[ch], 0.0f, 1.0f);
		float barW = r.width * 0.5f - 12.0f;
		float x = r.x + 8.0f + ch * (barW + 8.0f);
		float barH = (r.height - 30.0f) * level;
		ofDrawRectangle(x, r.getBottom() - 6.0f - barH, barW, barH);
	}
	ofNoFill();
	ofSetColor(selected ? ofColor(255, 190, 40) : ofColor(0, 140, 140));
	ofSetLineWidth(selected ? 3.0f : 1.0f);
	ofDrawRectangle(r);
	ofSetColor(230);
	ofDrawBitmapString("Master", r.x + 6.0f, r.y + 14.0f);
	ofPopStyle();
}

void ProgrammingGui::drawNarrationTile() {
	NarrationVoice & n = instrument_.audio().narration();
	const NarrationSettings & settings = instrument_.profile().narration;
	GrainMeter meter {
		[&n]() { return n.glitchVoices(); },
		[&n](int v) { return n.glitchGrainPosition(v); },
		[&n](int v) { return n.glitchGrainEnvelope(v); },
	};
	bool glitch = instrument_.mode() == Mode::NarrationGlitch;
	float head = glitch ? instrument_.glitchPosition() : n.position();
	std::string title = !settings.enabled ? "Narration (off)" : settings.skip ? "Narration (skipped)" : "N: " + settings.file;
	// The tile keeps its place even when this profile has no narration.
	narrationView_.draw(settings.enabled ? n.sample() : nullptr, title, meter, head, 1.0f, target_ == Target::Narration, glitch);
}

// ====================================================================== input

void ProgrammingGui::keyPressed(int key) {
	if (key >= OF_KEY_F1 && key <= OF_KEY_F6) {
		select(Target::Slot, key - OF_KEY_F1);
	} else if (key == OF_KEY_F7) {
		audioListsStale_ = true;
		showSettings_ = !showSettings_;
		showLog_ = false;
	} else if (key == OF_KEY_F8) {
		select(Target::Narration);
	} else if (key == OF_KEY_F9) {
		SensorSimulator & s = instrument_.simulator();
		s.setRamp(ramp_);
		s.startRamp();
	} else if (key == OF_KEY_F10) {
		showLog_ = !showLog_;
		showSettings_ = false;
	} else if (key == OF_KEY_F11) {
		ofToggleFullscreen();
	} else if (key == OF_KEY_F12) {
		std::string error;
		instrument_.saveToDisk(error);
	}
}

void ProgrammingGui::mouseMoved(int, int) {}

void ProgrammingGui::mousePressed(int x, int y, int) {
	if (deviceView_.mousePressed(x, y)) return;
	for (int i = 0; i < kMaxSlots; ++i) {
		if (slotViews_[i].inside(x, y)) {
			tileClicked(i);
			auditionSlot_ = i;
			instrument_.setAudition(i, true, slotViews_[i].positionAt(x));
			return;
		}
	}
	if (masterTileRect_.inside(x, y)) {
		tileClicked(kMaxSlots);
	} else if (narrationView_.inside(x, y)) {
		tileClicked(kMaxSlots + 1);
		if (instrument_.profile().narration.enabled) {
			glitchByMouse_ = true;
			instrument_.setManualGlitch(true, narrationView_.positionAt(x));
		}
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
	if (info.files.empty()) return;
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
