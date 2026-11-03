#pragma once

// -----------------------------------------------------------------------------
// ProgrammingGui: the desktop window used to program the instrument.
//
// Layout
//   left    : Transport (presets, save/reload, sensor source, recording)
//             Simulation (fake squeezes for testing without the instrument)
//   centre  : six slot waveforms (click/drag = audition), narration waveform,
//             live sensor bars + curve preview, message log
//   right   : Editor (the selected slot, the master chain or the narration:
//             sample, window, gate, grain parameters, effect chain)
//             Profile (sensors, play mode, audio device, gestures, hardware, OSC)
//
// Shortcuts (function keys, so they never clash with typing in text fields):
//   F1..F6 select slot   F7 master   F8 narration   F9 squeeze ramp
//   F11 fullscreen       F12 save everything
// Drop an audio file on a waveform to load it into that slot.
// -----------------------------------------------------------------------------

#include "GuiBuilder.h"
#include "Views.h"

#include "../app/Instrument.h"

#include "ofMain.h"
#include "ofxGui.h"

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
	// What the editor panel shows.
	enum class Target { Slot, Master, Narration };

	void layout();
	void select(Target target, int slot = 0);

	// Panel builders (each clears and refills one panel).
	void buildTransport();
	void buildSimulation();
	void buildEditor();
	void buildProfile();
	void buildSlotEditor(SlotSettings & slot, int index);
	void buildNarrationEditor(NarrationSettings & narration);
	void buildEffectChainEditor(ofxGuiGroup & parent, EffectChainSettings & chain);

	// Deferred actions: GUI callbacks must not destroy the control that is
	// calling them, so rebuilds and structural changes run in update().
	void requestRebuild() { rebuildEditor_ = true; }
	void structureChanged() { structureChanged_ = true; }

	void drawSlots();
	void drawNarration();
	void drawStatus();
	std::string audioDir() const;

	Instrument & instrument_;

	// Builders own the controls the panels show, so they are declared first:
	// members are destroyed in reverse order, panels before their controls.
	GuiBuilder transportBuilder_, simBuilder_, editorBuilder_, profileBuilder_;
	ofxPanel transportPanel_, simPanel_, editorPanel_, profilePanel_;

	std::array<WaveformView, kMaxSlots> slotViews_;
	WaveformView narrationView_;
	SensorView sensorView_;
	ofRectangle statusRect_;

	Target target_ = Target::Slot;
	int selectedSlot_ = 0;
	int auditionSlot_ = -1;
	bool glitchByMouse_ = false;

	bool rebuildEditor_ = false;
	bool rebuildTransport_ = false;
	bool structureChanged_ = false;
	std::vector<int> samplesToReload_;
	bool reloadNarration_ = false;

	// Values edited in the GUI that are not settings fields.
	int presetChoice_ = 0;
	int sourceChoice_ = 0;
	int addEffectChoice_ = 0;
	std::string recordFile_ = "recording.csv";
	SensorSimulator::RampSettings ramp_;
	std::array<float, kNumSensors> simLevels_ {};
};

} // namespace gm
