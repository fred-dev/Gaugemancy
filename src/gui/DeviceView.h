#pragma once

// -----------------------------------------------------------------------------
// DeviceView: a 3D model of the instrument for simulation and monitoring.
//
// The instrument (see Intrument_model.png) is an upright body with ten small
// inflatable balls: one on top, one underneath, and two on each of its four
// sides (an upper and a lower one). Six balls contain a pressure sensor:
//   sensor 1 = top, sensor 2 = bottom, and on the four sides, going round,
//   sensor 3 = side A upper, 4 = side B lower, 5 = side C upper, 6 = side D lower.
// (This numbering is for the picture; the ADC channel of each sensor is set in
// the profile's hardware.adcChannels.)
//
// What it shows: sensor balls squash and light up with pressure, a small core
// inside each glows as its pressure sensor is activated, plain balls stay grey.
//
// Interaction (when the simulation is the sensor input):
//   press and hold a sensor ball   squeeze it (pressure rises while held)
//   drag anywhere else             tilt the instrument = simulated accelerometer
//   double-click                   back to level
// With hardware or replay input the model follows the measured tilt instead.
// -----------------------------------------------------------------------------

#include "../core/ControlInputs.h"
#include "../core/SensorSimulator.h"

#include "ofMain.h"

#include <array>

namespace gm {

class DeviceView {
public:
	DeviceView();

	void setRect(const ofRectangle & rect) { rect_ = rect; }
	const ofRectangle & rect() const { return rect_; }
	bool inside(float x, float y) const { return rect_.inside(x, y); }

	/// Advance squeezes and, when `simulating`, write the simulated accelerometer
	/// and ball levels into `simulator`. Otherwise follow `measuredAccel`.
	void update(float dtSeconds, bool simulating, SensorSimulator & simulator, const AccelSample & measuredAccel);

	void draw(const ControlInputs & inputs);

	// Mouse, in window coordinates. Return true when the event was used.
	bool mousePressed(float x, float y);
	bool mouseDragged(float x, float y);
	bool mouseReleased(float x, float y);

	/// Back to level.
	void level();

private:
	struct Ball {
		glm::vec3 direction; ///< unit vector from the body centre (device coordinates)
		float offset;        ///< distance from the centre to the ball surface contact
		int sensor;          ///< 0..5, or -1 for a plain ball
	};

	glm::quat deviceToWorld() const;
	glm::vec3 ballCentre(const Ball & ball, float squash) const;
	int ballAt(float x, float y);
	void drawBody();
	void drawBall(const Ball & ball, float pressure);
	void drawOverlay(const ControlInputs & inputs);
	void drawBackdrop();
	void orientTo(const AccelSample & accel);

	ofRectangle rect_;
	ofCamera camera_;
	ofLight keyLight_;
	ofLight fillLight_;
	ofMaterial bodyMaterial_;
	std::array<Ball, 10> balls_;

	glm::quat orientation_;          ///< user tilt (world space)
	glm::vec2 lastMouse_ {};
	bool dragging_ = false;
	int squeezedBall_ = -1;
	std::array<float, kNumSensors> squeeze_ {}; ///< mouse squeeze per sensor, 0..1
	std::array<float, kNumSensors> sentSqueeze_ {}; ///< last value written to the simulator
	bool simulating_ = true;
	bool orientationChanged_ = true; ///< the user tilted the model since the last update
	AccelSample lastWrittenAccel_;    ///< what this view last told the simulator
	uint64_t lastClickMs_ = 0;
};

} // namespace gm
