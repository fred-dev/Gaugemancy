// Unit tests for the openFrameworks-free core of Gaugemancy.
// Build and run: make -C tests

#include "TestFramework.h"

#include "core/ButtonClickDetector.h"
#include "core/Curve.h"
#include "core/HitGestureDetector.h"
#include "core/LedPattern.h"
#include "core/MotionProcessor.h"
#include "core/Smoother.h"
#include "core/Options.h"
#include "core/ModeController.h"
#include "core/Playhead.h"
#include "core/SensorProcessor.h"
#include "core/SensorRecording.h"
#include "core/SensorSimulator.h"
#include "core/SettingsJson.h"
#include "core/SettingsStore.h"

#include <filesystem>
#include <fstream>

using namespace gm;

// ======================================================================= curves

TEST(curves_map_0_to_0_and_1_to_1) {
	for (int i = 0; i < static_cast<int>(Curve::Count); ++i) {
		Curve c = static_cast<Curve>(i);
		CHECK_NEAR(applyCurve(c, 0.0f), 0.0f, 1e-6);
		CHECK_NEAR(applyCurve(c, 1.0f), 1.0f, 1e-6);
	}
}

TEST(curves_have_expected_shape) {
	CHECK_NEAR(applyCurve(Curve::Linear, 0.25f), 0.25f, 1e-6);
	CHECK_NEAR(applyCurve(Curve::SquareRoot, 0.25f), 0.5f, 1e-6);
	CHECK_NEAR(applyCurve(Curve::Quartic, 0.5f), 0.0625f, 1e-6);
	CHECK(applyCurve(Curve::Exponential, 0.5f) < 0.05f);
}

TEST(curves_clamp_input) {
	CHECK_NEAR(applyCurve(Curve::Linear, -1.0f), 0.0f, 1e-6);
	CHECK_NEAR(applyCurve(Curve::SquareRoot, 2.0f), 1.0f, 1e-6);
}

TEST(curve_ids_round_trip) {
	for (int i = 0; i < static_cast<int>(Curve::Count); ++i) {
		Curve c = static_cast<Curve>(i);
		CHECK(curveFromId(curveId(c)) == c);
	}
	CHECK(curveFromId("nonsense") == Curve::Linear);
}

// ============================================================ modulated params

TEST(fixed_param_ignores_inputs) {
	ModulatedParam p;
	p.value = 0.3f;
	ControlInputs in;
	in.setSensor(0, 1.0f);
	CHECK_NEAR(p.resolve(in), 0.3f, 1e-6);
}

TEST(modulated_param_interpolates_min_to_max) {
	ModulatedParam p;
	p.min = 100.0f;
	p.max = 300.0f;
	p.source = ModSource::Sensor3;
	ControlInputs in;
	in.setSensor(2, 0.5f);
	CHECK_NEAR(p.resolve(in), 200.0f, 1e-4);
}

TEST(modulated_param_can_be_inverted) {
	ModulatedParam p;
	p.min = 1.0f;
	p.max = 0.0f;
	p.source = ModSource::Total;
	ControlInputs in;
	in.set(ModSource::Total, 0.25f);
	CHECK_NEAR(p.resolve(in), 0.75f, 1e-6);
}

TEST(mod_source_ids_round_trip_and_legacy_indices) {
	for (int i = 0; i < kNumModSources; ++i) {
		ModSource s = static_cast<ModSource>(i);
		CHECK(modSourceFromId(modSourceId(s)) == s);
	}
	CHECK(modSourceFromLegacyIndex(0) == ModSource::None);
	CHECK(modSourceFromLegacyIndex(1) == ModSource::Sensor1);
	CHECK(modSourceFromLegacyIndex(6) == ModSource::Sensor6);
	CHECK(modSourceFromLegacyIndex(7) == ModSource::None);
}

// ============================================================ sensor pipeline

TEST(sensor_normalise_maps_zero_to_max) {
	CHECK_NEAR(SensorProcessor::normalise(200, 200, 600), 0.0f, 1e-6);
	CHECK_NEAR(SensorProcessor::normalise(400, 200, 600), 0.5f, 1e-6);
	CHECK_NEAR(SensorProcessor::normalise(900, 200, 600), 1.0f, 1e-6);
	CHECK_NEAR(SensorProcessor::normalise(100, 200, 600), 0.0f, 1e-6);
	CHECK_NEAR(SensorProcessor::normalise(700, 700, 600), 0.0f, 1e-6); // zero above max
}

TEST(sensor_threshold_silences_noise) {
	CHECK_NEAR(SensorProcessor::shape(0.02f, 0.035f, Curve::Linear), 0.0f, 1e-6);
	CHECK_NEAR(SensorProcessor::shape(0.5f, 0.035f, Curve::Linear), 0.5f, 1e-6);
}

TEST(sensor_calibration_averages_first_frames) {
	SensorSettings settings;
	settings.calibrationFrames = 4;
	settings.maxRaw = 600;
	settings.activeThreshold = 0.0f;
	SensorProcessor proc;
	proc.configure(settings, MotionSettings {});
	proc.beginCalibration();
	RawSensorFrame rest { 200, 210, 190, 200, 200, 200 };
	RawSensorFrame rest2 { 204, 214, 194, 200, 200, 200 };
	for (int i = 0; i < 2; ++i) proc.process(rest, {}, 0.01f);
	for (int i = 0; i < 2; ++i) proc.process(rest2, {}, 0.01f);
	CHECK(!proc.isCalibrating());
	CHECK(proc.zeroPoints()[0] == 202);
	CHECK(proc.zeroPoints()[1] == 212);

	RawSensorFrame squeezed { 401, 212, 192, 200, 200, 200 };
	ControlInputs in = proc.process(squeezed, {}, 0.01f);
	CHECK_NEAR(in.sensor(0), (401.0f - 202.0f) / (600.0f - 202.0f), 1e-5);
	CHECK_NEAR(in.sensor(1), 0.0f, 1e-6);
}

TEST(sensor_outputs_zero_while_calibrating) {
	SensorProcessor proc;
	SensorSettings settings;
	settings.calibrationFrames = 3;
	proc.configure(settings, MotionSettings {});
	proc.beginCalibration();
	ControlInputs in = proc.process({ 1000, 1000, 1000, 1000, 1000, 1000 }, {}, 0.01f);
	CHECK(!in.anySensorActive());
}

TEST(total_pressure_uses_denominator_and_clamps) {
	ControlInputs in;
	in.setSensor(0, 0.5f);
	in.setSensor(1, 1.0f);
	CHECK_NEAR(SensorProcessor::totalPressure(in, 3.0f), 0.5f, 1e-6);
	CHECK_NEAR(SensorProcessor::totalPressure(in, 1.0f), 1.0f, 1e-6);
}

TEST(no_accelerometer_reads_level) {
	SensorProcessor proc;
	proc.setZeroPoints({});
	ControlInputs in = proc.processNormalised({}, AccelSample {}, 0.01f);
	CHECK_NEAR(in.get(ModSource::TiltX), 0.5f, 1e-6);
	CHECK_NEAR(in.get(ModSource::Tilt), 0.0f, 1e-6);
	CHECK_NEAR(in.get(ModSource::Shake), 0.0f, 1e-6);
}

// ================================================================ smoothing

TEST(smoother_takes_first_value_and_follows_time_constant) {
	Smoother s;
	s.setTime(100.0f);
	CHECK_NEAR(s.process(0.0f, 0.01f), 0.0f, 1e-6); // first value as-is
	float v = 0.0f;
	for (int i = 0; i < 10; ++i) v = s.process(1.0f, 0.01f); // 100 ms
	CHECK_NEAR(v, 1.0f - std::exp(-1.0f), 1e-3);              // ~63%
	Smoother off;
	off.process(0.0f, 0.01f);
	CHECK_NEAR(off.process(1.0f, 0.01f), 1.0f, 1e-6);         // 0 ms = no smoothing
}

TEST(smoothing_is_frame_rate_independent) {
	Smoother fast, slow;
	fast.setTime(50.0f);
	slow.setTime(50.0f);
	fast.process(0.0f, 0.005f);
	slow.process(0.0f, 0.02f);
	float a = 0.0f, b = 0.0f;
	for (int i = 0; i < 40; ++i) a = fast.process(1.0f, 0.005f); // 200 ms at 200 Hz
	for (int i = 0; i < 10; ++i) b = slow.process(1.0f, 0.02f);  // 200 ms at 50 Hz
	CHECK_NEAR(a, b, 1e-4);
}

TEST(smoothing_bank_keeps_sources_separate) {
	SmoothingBank bank;
	bank.setTime(ModSource::Sensor1, 1000.0f);
	ControlInputs first;
	bank.process(first, 0.01f);
	ControlInputs in;
	in.setSensor(0, 1.0f);
	in.setSensor(1, 1.0f);
	bank.process(in, 0.01f);
	CHECK(in.sensor(0) < 0.05f);         // heavily smoothed
	CHECK_NEAR(in.sensor(1), 1.0f, 1e-6); // not smoothed
}

// =================================================================== motion

namespace {
// Accelerometer reading (gravity) for a device tilted by `degrees` about its Y axis.
Vec3 tiltedAboutY(float degrees) {
	float r = degrees / 57.29578f;
	return { std::sin(r), 0.0f, std::cos(r) };
}

MotionOutput settle(MotionProcessor & m, const Vec3 & g, int frames = 300) {
	MotionOutput out;
	for (int i = 0; i < frames; ++i) out = m.process(g.x, g.y, g.z, true, 0.01f);
	return out;
}
} // namespace

TEST(motion_angles_around_up) {
	Vec3 up { 0.0f, 0.0f, 1.0f };
	auto a = MotionProcessor::anglesFor(tiltedAboutY(30.0f), up);
	CHECK_NEAR(a.x, 30.0f, 0.1);
	CHECK_NEAR(a.y, 0.0f, 0.1);
	CHECK_NEAR(a.total, 30.0f, 0.1);
	CHECK_NEAR(MotionProcessor::signedToUnit(0.0f, 3.0f, 45.0f), 0.5f, 1e-6);
	CHECK_NEAR(MotionProcessor::signedToUnit(45.0f, 3.0f, 45.0f), 1.0f, 1e-6);
	CHECK_NEAR(MotionProcessor::signedToUnit(-90.0f, 3.0f, 45.0f), 0.0f, 1e-6);
	CHECK_NEAR(MotionProcessor::signedToUnit(2.0f, 3.0f, 45.0f), 0.5f, 1e-6); // dead zone
}

TEST(motion_zero_at_startup_uses_mounting_orientation) {
	MotionSettings s;
	s.zeroAtStartup = true;
	s.calibrationFrames = 10;
	s.deadzoneDegrees = 0.0f;
	MotionProcessor m;
	m.configure(s);
	m.beginCalibration();
	// Board mounted sideways: gravity along +x at rest.
	for (int i = 0; i < 10; ++i) m.process(1.0f, 0.0f, 0.0f, true, 0.01f);
	CHECK(!m.isCalibrating());
	MotionOutput rest = settle(m, { 1.0f, 0.0f, 0.0f });
	CHECK_NEAR(rest.tilt, 0.0f, 1e-3);
	CHECK_NEAR(rest.tiltX, 0.5f, 1e-3);
	CHECK_NEAR(rest.shake, 0.0f, 1e-3);
}

TEST(motion_absolute_reference_reports_tilt) {
	MotionSettings s;
	s.zeroAtStartup = false;
	s.upAxis = "+z";
	s.rangeDegrees = 45.0f;
	s.deadzoneDegrees = 0.0f;
	MotionProcessor m;
	m.configure(s);
	m.beginCalibration();
	MotionOutput out = settle(m, tiltedAboutY(22.5f));
	CHECK_NEAR(out.tilt, 0.5f, 0.01);
	CHECK(out.tiltX > 0.7f || out.tiltX < 0.3f); // tilted one way on X
	CHECK_NEAR(out.tiltY, 0.5f, 0.01);
	CHECK(axisFromId("-x").x == -1.0f);
}

TEST(motion_shake_rises_and_falls) {
	MotionSettings s;
	s.zeroAtStartup = false;
	s.shakeSensitivity = 0.5f;
	s.shakeReleaseMs = 100.0f;
	MotionProcessor m;
	m.configure(s);
	m.beginCalibration();
	settle(m, { 0.0f, 0.0f, 1.0f });
	MotionOutput jolt = m.process(0.6f, 0.0f, 1.0f, true, 0.01f);
	CHECK(jolt.shake > 0.9f);
	MotionOutput calm = settle(m, { 0.0f, 0.0f, 1.0f }, 200);
	CHECK(calm.shake < 0.05f);
}

TEST(legacy_accel_ids_map_to_tilt) {
	CHECK(modSourceFromId("accelX") == ModSource::TiltX);
	CHECK(modSourceFromId("accelZ") == ModSource::Tilt);
	CHECK(modSourceFromId("shake") == ModSource::Shake);
}

// ================================================================== button

TEST(button_single_double_triple_clicks) {
	ButtonClickDetector b;
	b.setClickTimeout(200);
	uint64_t t = 0;
	auto tap = [&](int count) {
		ButtonEvent last = ButtonEvent::None;
		for (int i = 0; i < count; ++i) {
			b.update(true, t); t += 50;
			b.update(false, t); t += 100;
		}
		for (int i = 0; i < 10 && last == ButtonEvent::None; ++i) {
			last = b.update(false, t);
			t += 50;
		}
		t += 1000;
		return last;
	};
	CHECK(tap(1) == ButtonEvent::SingleClick);
	CHECK(tap(2) == ButtonEvent::DoubleClick);
	CHECK(tap(3) == ButtonEvent::TripleClick);
}

TEST(button_long_and_very_long_press) {
	ButtonClickDetector b;
	b.update(true, 0);
	CHECK(b.update(false, 3500) == ButtonEvent::LongPress);
	b.update(true, 10000);
	CHECK(b.update(false, 16500) == ButtonEvent::VeryLongPress);
	CHECK(b.update(false, 20000) == ButtonEvent::None); // nothing left pending
}

// ============================================================== hit gesture

TEST(hit_gesture_counts_quick_pumps) {
	GestureSettings g;
	g.requiredHits = 3;
	g.hitThreshold = 0.25f;
	g.troughThreshold = 0.08f;
	g.maxPeakMs = 200;
	g.maxTroughMs = 200;
	g.minSensorsPerHit = 2;
	HitGestureDetector d;
	d.configure(g);

	ControlInputs squeeze;
	squeeze.setSensor(0, 0.6f);
	squeeze.setSensor(3, 0.6f);
	ControlInputs released;

	uint64_t t = 0;
	bool fired = false;
	for (int hit = 0; hit < 3; ++hit) {
		fired |= d.update(squeeze, t); t += 100;
		fired |= d.update(released, t); t += 100;
	}
	CHECK(fired);
	CHECK(d.hitsCounted() == 0); // reset after firing
}

TEST(hit_gesture_ignores_slow_squeezes_and_single_balls) {
	GestureSettings g;
	g.requiredHits = 2;
	g.minSensorsPerHit = 2;
	HitGestureDetector d;
	d.configure(g);

	ControlInputs one;
	one.setSensor(0, 1.0f);
	ControlInputs two = one;
	two.setSensor(1, 1.0f);
	ControlInputs released;

	uint64_t t = 0;
	CHECK(!d.update(one, t)); // only one ball: not a hit
	t += 100;
	CHECK(!d.update(two, t)); // starts a peak
	t += 1000;
	CHECK(!d.update(two, t)); // held too long: reset
	CHECK(d.hitsCounted() == 0);
	CHECK(!d.update(released, t + 10));
}

// ============================================================ mode controller

namespace {
struct RecordingActions : ModeActions {
	std::vector<std::string> log;
	void playNarration() override { log.push_back("play"); }
	void pauseNarration() override { log.push_back("pause"); }
	void stopNarration() override { log.push_back("stop"); }
	void setNarrationGlitch(bool active) override { log.push_back(active ? "glitch on" : "glitch off"); }
	void modeChanged(Mode mode) override { log.push_back(std::string("mode ") + modeName(mode)); }
};
} // namespace

TEST(mode_goes_straight_to_granular_without_narration) {
	RecordingActions actions;
	ModeController mc(actions);
	NarrationSettings n;
	n.enabled = false;
	mc.start(n, true, 0);
	CHECK(mc.mode() == Mode::Granular);
}

TEST(mode_full_narration_cycle) {
	RecordingActions actions;
	ModeController mc(actions);
	NarrationSettings n;
	n.enabled = true;
	n.startWithSensor = true;
	n.glitchThreshold = 0.1f;
	n.glitchStrand = 0.1f;
	mc.start(n, true, 0);
	CHECK(mc.mode() == Mode::WaitForNarration);

	ControlInputs idle;
	ControlInputs pressed;
	pressed.setSensor(2, 0.5f);

	mc.update(idle, {}, 10);
	CHECK(mc.mode() == Mode::WaitForNarration);
	mc.update(pressed, {}, 20);
	CHECK(mc.mode() == Mode::PlayNarration);
	mc.update(pressed, { 0.1f, false }, 25); // the starting squeeze does not glitch
	CHECK(mc.mode() == Mode::PlayNarration);

	mc.update(idle, { 0.4f, false }, 30);
	mc.update(pressed, { 0.5f, false }, 40);
	CHECK(mc.mode() == Mode::NarrationGlitch);
	CHECK(mc.glitchSensor() == 2);
	mc.update(pressed, { 0.5f, false }, 50);
	CHECK_NEAR(mc.glitchPosition(), 0.45f, 1e-5); // 0.5 - 0.1 * (1 - 0.5)

	mc.update(idle, { 0.5f, false }, 60);
	CHECK(mc.mode() == Mode::PlayNarration);
	mc.update(idle, { 1.0f, true }, 70);
	CHECK(mc.mode() == Mode::Granular);
	CHECK(actions.log.back() == "mode granular");
}

TEST(mode_auto_start_after_delay) {
	RecordingActions actions;
	ModeController mc(actions);
	NarrationSettings n;
	n.enabled = true;
	n.startWithSensor = false;
	n.autoStartDelayMs = 1000;
	mc.start(n, true, 0);
	mc.update({}, {}, 999);
	CHECK(mc.mode() == Mode::WaitForNarration);
	mc.update({}, {}, 1000);
	CHECK(mc.mode() == Mode::PlayNarration);
}

TEST(mode_skip_narration) {
	RecordingActions actions;
	ModeController mc(actions);
	NarrationSettings n;
	n.enabled = true;
	mc.start(n, true, 0);
	mc.skipNarration();
	CHECK(mc.mode() == Mode::Granular);
}

TEST(mode_missing_narration_file_goes_to_granular) {
	RecordingActions actions;
	ModeController mc(actions);
	NarrationSettings n;
	n.enabled = true;
	mc.start(n, false, 0);
	CHECK(mc.mode() == Mode::Granular);
}

// =================================================== simulator / playhead / leds

TEST(simulator_ramp_rises_and_falls_once) {
	SensorSimulator sim;
	SensorSimulator::RampSettings r;
	r.speed = 1.0f;
	r.extent = 1.0f;
	sim.setRamp(r);
	sim.startRamp();
	auto v = sim.update(0.5f);
	CHECK_NEAR(v[0], 0.5f, 1e-6);
	CHECK_NEAR(v[1], 0.0f, 1e-6);
	sim.update(0.6f); // reaches the top
	sim.update(2.0f); // back to zero
	CHECK(!sim.rampRunning());
}

TEST(simulator_manual_levels) {
	SensorSimulator sim;
	sim.setManual(4, 0.7f);
	CHECK_NEAR(sim.update(0.01f)[4], 0.7f, 1e-6);
}

TEST(playhead_moves_only_while_pressed_and_stops_at_end) {
	Playhead p;
	p.advance(1.0f, false, 0.5f);
	CHECK_NEAR(p.position(), 0.0f, 1e-6);
	p.advance(1.0f, true, 0.5f);
	CHECK_NEAR(p.position(), 0.5f, 1e-6);
	p.advance(10.0f, true, 0.5f);
	CHECK_NEAR(p.position(), Playhead::kDefaultEnd, 1e-6);
	p.rewind();
	CHECK_NEAR(p.position(), 0.0f, 1e-6);
}

TEST(led_preset_colours_never_go_dark) {
	// No preset's colour-mix cycle may ever show "off" - that is the whole
	// point of replacing the old blink-to-black scheme.
	for (int i = 0; i < 6; ++i) {
		LedPattern p = LedPattern::forPreset(i);
		int total = p.blueMs + p.bothMs + p.redMs;
		CHECK(total > 0);
		for (int t = 0; t < total; t += 37) {
			LedPattern now = p.at(static_cast<uint64_t>(t));
			CHECK(now.blue || now.red);
		}
	}
}

TEST(led_preset_colours_differ_and_are_consistent) {
	LedPattern p0 = LedPattern::forPreset(0);
	LedPattern p1 = LedPattern::forPreset(1);
	CHECK(p0.blueMs != p1.blueMs || p0.redMs != p1.redMs); // visibly different mixes
	CHECK(p0.blueMs > p0.redMs);                           // preset 0 reads as "mostly blue"
	CHECK(LedPattern::forPreset(0).blueMs == p0.blueMs);   // same index -> same mix every time
}

TEST(led_error_still_blinks_to_black) {
	LedPattern e = LedPattern::error();
	CHECK(e.blinkMs > 0);
	CHECK(e.at(0).red);
	CHECK(!e.at(static_cast<uint64_t>(e.blinkMs)).red);
}

// ================================================================ recording

TEST(recording_parses_lines) {
	RecordedFrame f;
	CHECK(!SensorPlayback::parseLine("time_ms,s1,s2,s3,s4,s5,s6", f));
	CHECK(!SensorPlayback::parseLine("# comment", f));
	CHECK(SensorPlayback::parseLine("120,1,2,3,4,5,2000", f));
	CHECK(f.timeMs == 120 && f.raw[0] == 1 && f.raw[5] == 1023);
	CHECK(!f.accel.valid);
	CHECK(SensorPlayback::parseLine("0,1,2,3,4,5,6,0.1,0.2,0.9", f));
	CHECK(f.accel.valid);
	CHECK_NEAR(f.accel.z, 0.9f, 1e-6);
}

TEST(recording_round_trip_and_playback_timing) {
	std::string path = (std::filesystem::temp_directory_path() / "gm_test_recording.csv").string();
	std::string error;
	SensorRecorder rec;
	CHECK(rec.open(path, error));
	rec.write(1000, { 1, 1, 1, 1, 1, 1 }, {});
	rec.write(1100, { 2, 2, 2, 2, 2, 2 }, {});
	rec.write(1200, { 3, 3, 3, 3, 3, 3 }, {});
	rec.close();

	SensorPlayback play;
	CHECK(play.load(path, error));
	CHECK(play.size() == 3);
	CHECK(play.durationMs() == 200);
	CHECK(play.frameAt(0, false).raw[0] == 1);
	CHECK(play.frameAt(150, false).raw[0] == 2);
	CHECK(play.frameAt(999, false).raw[0] == 3);
	CHECK(play.frameAt(201 + 150, true).raw[0] == 2); // looped
	std::filesystem::remove(path);
}

// ================================================================= settings

TEST(effect_catalog_is_consistent) {
	for (const EffectInfo & info : EffectCatalog::all()) {
		CHECK(!info.params.empty());
		for (const ParamSpec & spec : info.params) {
			CHECK(spec.lowest <= spec.defaultValue && spec.defaultValue <= spec.highest);
		}
		EffectSettings e = EffectSettings::make(info.id);
		CHECK(e.params.size() == info.params.size());
	}
	CHECK(EffectCatalog::find("nope") == nullptr);
}

TEST(grain_specs_match_settings) {
	CHECK(static_cast<int>(grainParamSpecs().size()) == GrainSettings::count());
	GrainSettings g;
	CHECK_NEAR(g.length.value, 500.0f, 1e-6);
	CHECK(&g.at(7) == &g.position);
}

TEST(preset_json_round_trip) {
	PresetSettings p;
	p.name = "Birds";
	p.playheadSpeed = 0.05f;
	p.slots[0].file = "BIRDS_1.wav";
	p.slots[0].window = WindowType::Sine;
	p.slots[0].gate = GateMode::Source;
	p.slots[0].gateSource = ModSource::Sensor2;
	p.slots[0].grain.pitch.source = ModSource::Sensor4;
	p.slots[0].grain.pitch.min = -7.0f;
	p.slots[0].effects.push_back(EffectSettings::make("filter"));
	p.slots[0].effects.back().params[0].value = 1234.0f;
	p.masterEffects.push_back(EffectSettings::make("reverb"));

	Warnings w;
	PresetSettings back = presetFromJson(toJson(p), w);
	CHECK(w.empty());
	CHECK(back.name == "Birds");
	CHECK(back.slots[0].file == "BIRDS_1.wav");
	CHECK(back.slots[0].window == WindowType::Sine);
	CHECK(back.slots[0].gate == GateMode::Source && back.slots[0].gateSource == ModSource::Sensor2);
	CHECK(back.slots[0].grain.pitch == p.slots[0].grain.pitch);
	CHECK(back.slots[0].effects.size() == 1);
	CHECK_NEAR(back.slots[0].effects[0].params[0].value, 1234.0f, 1e-3);
	CHECK(back.masterEffects.size() == 1 && back.masterEffects[0].type == "reverb");
}

TEST(profile_json_round_trip) {
	ProfileSettings p;
	p.name = "Fred";
	p.playMode = PlayMode::Accumulated;
	p.sensors.curve = Curve::Quartic;
	p.audio.deviceName = "USB Audio";
	p.hardware.adcChannels = { 5, 4, 3, 2, 1, 0 };
	p.narration.enabled = true;
	p.narration.file = "narr.wav";
	p.presetFiles = { "presets/a.json", "presets/b.json" };
	Warnings w;
	ProfileSettings back = profileFromJson(toJson(p), w);
	CHECK(w.empty());
	CHECK(back.playMode == PlayMode::Accumulated);
	CHECK(back.sensors.curve == Curve::Quartic);
	CHECK(back.audio.deviceName == "USB Audio");
	CHECK(back.hardware.adcChannels[0] == 5);
	CHECK(back.narration.enabled && back.narration.file == "narr.wav");
	CHECK(back.presetFiles.size() == 2);
}

TEST(json_reader_is_forgiving) {
	Json bad = Json::parse(R"({
		"slots": [ { "file": 42, "grain": { "pitch": { "value": 99, "source": "elbow" } },
		             "effects": [ { "type": "flanger" }, { "type": "gain", "params": { "gain": 2 } } ] } ],
		"master": { "gain": "loud" }
	})");
	Warnings w;
	PresetSettings p = presetFromJson(bad, w);
	CHECK(!w.empty());
	CHECK(p.slots[0].file.empty());                       // wrong type: default kept
	CHECK_NEAR(p.slots[0].grain.pitch.value, 20.0f, 1e-6); // clamped to range
	CHECK(p.slots[0].grain.pitch.source == ModSource::None);
	CHECK(p.slots[0].effects.size() == 1);                // unknown effect dropped
	CHECK_NEAR(p.slots[0].effects[0].params[0].value, 2.0f, 1e-6); // plain number shorthand
	CHECK_NEAR(p.masterGain, 1.0f, 1e-6);
}

TEST(store_saves_and_loads_profiles) {
	namespace fs = std::filesystem;
	fs::path root = fs::temp_directory_path() / "gm_test_store";
	fs::remove_all(root);
	SettingsStore store(root.string());
	std::string error;
	CHECK(store.createProfile("Tester", error));
	CHECK(!store.createProfile("Tester", error)); // never overwrites
	RootSettings rs;
	rs.activeProfile = "Tester";
	CHECK(store.saveRoot(rs, error));

	LoadedProfile loaded = store.loadActiveProfile();
	CHECK(loaded.profileFileFound);
	CHECK(loaded.profile.name == "Tester");
	CHECK(loaded.presets.size() == 1);

	loaded.presets[0].name = "Changed";
	CHECK(store.savePreset(loaded.profile, 0, loaded.presets[0], error));
	CHECK(store.loadProfile("Tester").presets[0].name == "Changed");
	CHECK(!fs::exists(store.profileFilePath("Tester") + ".tmp"));

	auto names = store.listProfiles();
	CHECK(names.size() == 1 && names[0] == "Tester");
	CHECK(store.audioPath("Tester", "a.wav") == (root / "Tester" / "audio" / "a.wav").lexically_normal().string());
	fs::remove_all(root);
}

TEST(store_reports_missing_and_broken_files) {
	namespace fs = std::filesystem;
	fs::path root = fs::temp_directory_path() / "gm_test_store_broken";
	fs::remove_all(root);
	fs::create_directories(root / "Broken");
	std::ofstream(root / "Broken" / "profile.json") << "{ not json";
	SettingsStore store(root.string());
	LoadedProfile loaded = store.loadProfile("Broken");
	CHECK(!loaded.profileFileFound);
	CHECK(!loaded.warnings.empty());
	CHECK(loaded.presets.size() == 1); // defaults still give a playable preset
	fs::remove_all(root);
}

// ================================================================== options

TEST(options_defaults) {
	Options o = parseCommandLine({ "Gaugemancy" });
	CHECK(o.command == Command::Run);
	CHECK(o.sensors == SensorMode::Auto);
	CHECK(o.errors.empty());
}

TEST(options_parse_everything) {
	Options o = parseCommandLine({ "app", "--data", "/media/Data", "--profile", "Fred", "--headless",
		"--sensors", "replay:take.csv", "--no-loop", "--record", "out.csv", "--log", "verbose", "--benchmark", "5" });
	CHECK(o.errors.empty());
	CHECK(o.dataRoot == "/media/Data" && o.profile == "Fred" && o.headless);
	CHECK(o.sensors == SensorMode::Replay && o.replayFile == "take.csv" && !o.loopReplay);
	CHECK(o.recordFile == "out.csv" && o.logLevel == "verbose");
	CHECK(o.command == Command::Benchmark);
	CHECK_NEAR(o.benchmarkSeconds, 5.0f, 1e-6);
}

TEST(options_report_mistakes) {
	Options o = parseCommandLine({ "app", "--bogus", "--sensors", "elbow", "--data" });
	CHECK(o.errors.size() == 3);
	Options xcode = parseCommandLine({ "app", "-NSDocumentRevisionsDebugMode", "YES" });
	CHECK(xcode.errors.empty());
}

int main() {
	return testing::runAll();
}
