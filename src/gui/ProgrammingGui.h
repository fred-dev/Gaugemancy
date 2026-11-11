#pragma once

// -----------------------------------------------------------------------------
// ProgrammingGui: the desktop window used to program the instrument.
//
// One rule: every control has ONE permanent place. Nothing in the working
// layout collapses, scrolls or moves when something else changes, so the hand
// learns where things are. The window is a fixed grid (PanelLayout):
//
//   top bar      profile, preset buttons, save / reload, Settings, Log
//   left column  the instrument: 3D view, sensor faders (input AND meter),
//                squeeze ramp, performance buttons
//   tile row     Slot 1..6, Master, Narration - what the editor below shows
//   editor       Source (what plays) | Parameters (always 9 rows)
//                                    | Effects (always 8 places, 7 rows)
//   bottom bar   output level, mode, sensor input, last message
//
// Selecting a tile changes WHAT the editor shows, never WHERE its rows are:
// the tables are padded with blank rows and unused controls are greyed out
// instead of removed.
//
// Settings (set-once profile, device, hardware and network options) and the
// message log are a different context: each takes over the editor area while
// it is open, and selecting a tile closes it again.
//
// Nothing here is retained: every frame draws the current settings directly.
//
// Shortcuts (function keys, so they never clash with typing in text fields):
//   F1..F6 select slot   F7 Settings   F8 narration
//   F9 squeeze ramp      F10 message log
//   F11 fullscreen       F12 save everything
// Click or drag in a slot tile to audition it; drop an audio file on a slot
// tile to load it.
// -----------------------------------------------------------------------------

#include "DeviceView.h"
#include "GuiBuilder.h"
#include "PanelLayout.h"
#include "Views.h"

#include "../app/Instrument.h"

#include "ofMain.h"
#include "ofxImGui.h"

#include <array>

namespace gm {

class ProgrammingGui {
public:
	explicit ProgrammingGui(Instrument & instrument);

	void setup();
	void update();
	void draw();

	void keyPressed(int key);
	void mouseMoved(int x, int y);
	void mouseDragged(int x, int y, int button);
	void mousePressed(int x, int y, int button);
	void mouseReleased(int x, int y, int button);
	void windowResized(int w, int h);
	void dragEvent(const ofDragInfo & info);

private:
	/// What the editor shows.
	enum class Target { Slot, Master, Narration };
	static constexpr int kTargets = kMaxSlots + 2; ///< six slots, master, narration

	void layout();
	void select(Target target, int slot = 0);
	/// 0..5 for a slot, 6 for the master, 7 for the narration.
	int targetIndex() const;
	EffectChainSettings & selectedChain();

	// Bars and the instrument column.
	void drawTopBar();
	void drawBottomBar();
	void drawSensorsPanel();
	void drawRampPanel();
	void drawPerformancePanel();

	// The editor: three panels that keep their rows whatever is selected.
	void drawSourcePanel();
	void drawSlotSource(SlotSettings & slot, int index);
	void drawMasterSource();
	void drawNarrationSource(NarrationSettings & narration);
	void drawParametersPanel();
	void drawEffectsPanel();
	void drawEffectPlaces(EffectChainSettings & chain, int & selected);
	void drawEffectControls(EffectChainSettings & chain, int & selected);

	// Take over the editor area while open.
	void drawSettingsPanel();
	void drawLogPanel();
	void drawProfileSection();

	// Tiles (drawn with openFrameworks, not ImGui).
	void drawSlotTiles();
	void drawMasterTile();
	void drawNarrationTile();

	void structureChanged() { structureChanged_ = true; }
	std::string audioDir() const;

	Instrument & instrument_;

	ofxImGui::Gui gui_;
	GuiBuilder builder_;
	PanelLayout panels_;

	std::array<WaveformView, kMaxSlots> slotViews_;
	WaveformView narrationView_;
	DeviceView deviceView_;
	uint64_t lastUpdateMs_ = 0;

	// This frame's fixed grid (set by layout()).
	ofRectangle topRect_, bottomRect_;
	ofRectangle sensorsRect_, rampRect_, performanceRect_;
	ofRectangle masterTileRect_;
	ofRectangle editorRect_, sourceRect_, parametersRect_, effectsRect_;

	Target target_ = Target::Slot;
	int selectedSlot_ = 0;
	std::array<int, kTargets> selectedEffect_ {}; ///< per tile, so each remembers its own
	int auditionSlot_ = -1;
	bool glitchByMouse_ = false;

	// "Free run" per synth slot: an ephemeral, not-saved override (same
	// mechanism as click-to-audition) so a synth/generator source can be
	// heard and tuned continuously without needing the sensor simulator
	// running or a ball pressed.
	std::array<bool, kMaxSlots> freeRun_ {};

	// Toggled from the top bar or F7/F10.
	bool showSettings_ = false;
	bool showLog_ = false;

	// Deferred actions: applied once per frame in update(), not mid-draw -
	// switching presets or reloading a sample changes what other widgets in
	// the same frame would be reading/writing.
	int presetRequest_ = -1;
	bool structureChanged_ = false;
	std::vector<int> samplesToReload_;
	bool reloadNarration_ = false;

	// Values edited in the GUI that are not settings fields.
	int sourceChoice_ = 0;
	std::string recordFile_ = "recording.csv";
	std::string newProfileName_;
	bool newProfileFromCurrent_ = true;
	SensorSimulator::RampSettings ramp_;
	std::array<float, kNumSensors> simLevels_ {};
};

} // namespace gm
