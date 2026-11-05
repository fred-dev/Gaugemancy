#pragma once

// -----------------------------------------------------------------------------
// ProgrammingGui: the desktop window used to program the instrument.
//
// Layout - optimised for use, not exploration: the panels you need while
//   playing or editing are always on screen, always in the same place, at a
//   fixed size proportional to the window (PanelLayout) - they cannot be
//   moved, resized or collapsed, and never overlap each other, so muscle
//   memory for "where is X" keeps working no matter the window size.
//   A thin menu bar is pinned at the very top. Below it, fixed in a grid:
//     left column, top to bottom : Transport, Simulation, Master output
//     right column               : Inspector (one slot, or Narration, at a time)
//     centre                     : six slot waveforms (click/drag = audition),
//                                   narration waveform, live sensor bars +
//                                   curve preview, 3D device view
//   Content that doesn't fit a panel scrolls in place; the panel itself never
//   has to move or grow for you to reach it.
//   Settings (profile switching, audio/hardware/network/calibration - set-once
//   stuff, a different context from the slots you edit every session) and the
//   message log (troubleshooting, not normal playing/editing) are hidden by
//   default and opened as ordinary, user-movable floating windows - they are
//   occasional dialogs, not part of the fixed working layout.
//
// Unlike the old ofxGui version, nothing here is a retained panel: every
// frame's draw() call draws the current state of the settings directly, so
// there is no "rebuild the panel" step when switching what the editor shows.
//
// Shortcuts (function keys, so they never clash with typing in text fields):
//   F1..F6 select slot   F7 toggle Settings   F8 narration
//   F9 squeeze ramp      F10 toggle message log
//   F11 fullscreen       F12 save everything
// (Master output is always visible in its own panel, not a shortcut target.)
// Drop an audio file on a waveform to load it into that slot.
// -----------------------------------------------------------------------------

#include "DeviceView.h"
#include "GuiBuilder.h"
#include "PanelLayout.h"
#include "Views.h"

#include "../app/Instrument.h"

#include "ofMain.h"
#include "ofxImGui.h"

#include <array>
#include <functional>

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
	// What the Inspector window's one open section shows. Master output has
	// its own always-visible window (drawMasterWindow()), not a section here;
	// Profile management lives in its own toggleable Settings window, not here.
	enum class Target { Slot, Narration };

	void layout();
	void select(Target target, int slot = 0) {
		target_ = target;
		selectedSlot_ = std::clamp(slot, 0, kMaxSlots - 1);
	}

	// Window drawers: called every frame from draw(), between gui_.begin()/end().
	// Pinned at the top always; returns its height so the grid below can clear it.
	float drawMenuBar();
	// Transport/Simulation/Master/Inspector are fixed panels (PanelLayout):
	// pinned at exactly the rect layout() computed this frame, every frame -
	// not movable, resizable or collapsible, so "where is X" never changes.
	void drawTransportWindow();
	void drawSimulationWindow();
	void drawMasterWindow(); ///< always visible, not part of the Inspector accordion
	// Hidden by default; profile switching plus the set-once device/hardware/
	// network/calibration settings that don't belong among the slots you
	// edit every session (see drawProfileSection()). A different context, so
	// it is hidden until toggled - but still a fixed panel: it takes over
	// exactly the centre region (draw() skips the slot grid while this is
	// open), so it never overlaps the always-on panels around it.
	void drawSettingsWindow();
	// Slot 1-6 and (if used) Narration, stacked in one fixed panel; clicking a
	// collapsed header opens it and closes whichever section was open before
	// - only one is ever expanded at a time.
	void drawInspectorWindow();
	/// Draws one section's clickable header (highlighted and not clickable
	/// when `active`, a plain Selectable calling `onSelect` otherwise) plus
	/// a trailing separator. Content is drawn by the caller right after,
	/// only when `active`.
	void drawInspectorSection(const std::string & title, bool active, const std::function<void()> & onSelect);
	void drawProfileSection();
	void drawMasterSection();
	void drawSlotEditor(SlotSettings & slot, int index);
	void drawNarrationEditor(NarrationSettings & narration);
	void drawEffectChainEditor(const std::string & idScope, EffectChainSettings & chain);

	void structureChanged() { structureChanged_ = true; }

	void drawSlots();
	void drawNarration();
	void drawStatus();
	std::string audioDir() const;

	Instrument & instrument_;

	ofxImGui::Gui gui_;
	GuiBuilder builder_;
	PanelLayout panels_;

	std::array<WaveformView, kMaxSlots> slotViews_;
	WaveformView narrationView_;
	SensorView sensorView_;
	DeviceView deviceView_;
	uint64_t lastUpdateMs_ = 0;
	ofRectangle statusRect_;

	// This frame's fixed-grid rects (set by layout(), consumed by the
	// draw*Window() panels and by draw() for the centre content's header).
	float menuBarHeight_ = 30.0f;
	ofRectangle transportRect_, simulationRect_, masterRect_, inspectorRect_, centerRect_;

	Target target_ = Target::Slot;
	int selectedSlot_ = 0;
	int auditionSlot_ = -1;
	bool glitchByMouse_ = false;

	// "Free run" per synth slot: an ephemeral, not-saved override (same
	// mechanism as click-to-audition) so a synth/generator source can be
	// heard and tuned continuously without needing the sensor simulator
	// running or a ball pressed.
	std::array<bool, kMaxSlots> freeRun_ {};

	// Hidden-by-default windows, toggled from the menu bar or F7/F10.
	bool showSettings_ = false;
	bool showLog_ = false;

	// Deferred actions: applied once per frame in update(), not mid-draw -
	// switching presets or reloading a sample changes what other widgets in
	// the same frame would be reading/writing.
	bool presetChanged_ = false;
	bool structureChanged_ = false;
	std::vector<int> samplesToReload_;
	bool reloadNarration_ = false;

	// Values edited in the GUI that are not settings fields.
	int presetChoice_ = 0;
	int sourceChoice_ = 0;
	int addEffectChoice_ = 0;
	std::string recordFile_ = "recording.csv";
	std::string newProfileName_;
	bool newProfileFromCurrent_ = true;
	SensorSimulator::RampSettings ramp_;
	std::array<float, kNumSensors> simLevels_ {};
};

} // namespace gm
