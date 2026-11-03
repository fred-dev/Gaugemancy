#include "DeviceView.h"

#include <algorithm>
#include <cstdio>

namespace gm {

namespace {
// Model dimensions (arbitrary units; the camera frames them).
constexpr float kBodyRadius = 60.0f;
constexpr float kBodyHeight = 200.0f;
constexpr float kBallRadius = 34.0f;
constexpr float kSideBallHeight = 48.0f; // upper/lower side balls, from the centre
constexpr float kMaxSquash = 0.45f;      // a full squeeze flattens a ball by 45%
constexpr float kSqueezeRiseSeconds = 0.35f;

const ofColor kBodyColour(205, 205, 210);
const ofColor kPlainBall(92, 92, 102);
const ofColor kSensorBallIdle(70, 125, 145);
const ofColor kSensorBallActive(255, 150, 40);

// Device coordinates: +Z is the instrument's long axis (up when level), which
// matches the accelerometer's default "up" axis. World coordinates: +Y is up.
const glm::quat kDeviceToWorldBase = glm::angleAxis(glm::radians(-90.0f), glm::vec3(1, 0, 0));
} // namespace

DeviceView::DeviceView() {
	const float sideOffset = kBodyRadius;
	auto side = [](float degrees, float z) {
		float r = glm::radians(degrees);
		return glm::normalize(glm::vec3(std::cos(r) * kBodyRadius, std::sin(r) * kBodyRadius, z));
	};
	// Top and bottom sit on the ends of the body; side balls on the four faces.
	balls_ = { {
		{ { 0, 0, 1 }, kBodyHeight * 0.5f, 0 },
		{ { 0, 0, -1 }, kBodyHeight * 0.5f, 1 },
		{ side(0, kSideBallHeight), sideOffset, 2 },     // side A upper
		{ side(0, -kSideBallHeight), sideOffset, -1 },   // side A lower
		{ side(90, kSideBallHeight), sideOffset, -1 },   // side B upper
		{ side(90, -kSideBallHeight), sideOffset, 3 },   // side B lower
		{ side(180, kSideBallHeight), sideOffset, 4 },   // side C upper
		{ side(180, -kSideBallHeight), sideOffset, -1 }, // side C lower
		{ side(270, kSideBallHeight), sideOffset, -1 },  // side D upper
		{ side(270, -kSideBallHeight), sideOffset, 5 },  // side D lower
	} };
	bodyMaterial_.setDiffuseColor(kBodyColour);
	bodyMaterial_.setAmbientColor(ofFloatColor(kBodyColour) * 0.5f);
	bodyMaterial_.setSpecularColor(ofFloatColor(0.6f, 0.6f, 0.6f));
	bodyMaterial_.setShininess(30.0f);
	// Key light from the upper front left, softer fill from the right.
	keyLight_.setPointLight();
	keyLight_.setPosition(-350.0f, 420.0f, 520.0f);
	keyLight_.setDiffuseColor(ofFloatColor(1.0f, 0.98f, 0.95f));
	keyLight_.setSpecularColor(ofFloatColor(0.9f, 0.9f, 0.9f));
	keyLight_.setAmbientColor(ofFloatColor(0.28f, 0.28f, 0.34f));
	fillLight_.setPointLight();
	fillLight_.setPosition(420.0f, 80.0f, 300.0f);
	fillLight_.setDiffuseColor(ofFloatColor(0.35f, 0.38f, 0.55f));
	fillLight_.setSpecularColor(ofFloatColor(0.0f, 0.0f, 0.0f));
	camera_.setFov(38.0f);
	camera_.setNearClip(10.0f);
	camera_.setFarClip(5000.0f);
	level();
}

void DeviceView::level() {
	orientation_ = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
	orientationChanged_ = true;
}

void DeviceView::orientTo(const AccelSample & accel) {
	// Rotate the model so the measured "up" (device coordinates) points up.
	glm::vec3 measured = glm::normalize(kDeviceToWorldBase * glm::vec3(accel.x, accel.y, accel.z));
	orientation_ = glm::rotation(measured, glm::vec3(0, 1, 0));
}

glm::quat DeviceView::deviceToWorld() const {
	return orientation_ * kDeviceToWorldBase;
}

glm::vec3 DeviceView::ballCentre(const Ball & ball, float squash) const {
	// The ball rests against the body; squashing pushes its centre inwards.
	float distance = ball.offset + kBallRadius * (1.0f - squash);
	return deviceToWorld() * (ball.direction * distance);
}

// ----------------------------------------------------------------- update

void DeviceView::update(float dtSeconds, bool simulating, SensorSimulator & simulator, const AccelSample & measuredAccel) {
	simulating_ = simulating;
	for (int i = 0; i < kNumSensors; ++i) {
		bool held = squeezedBall_ >= 0 && balls_[squeezedBall_].sensor == i;
		float step = dtSeconds / kSqueezeRiseSeconds;
		squeeze_[i] = std::clamp(squeeze_[i] + (held ? step : -2.0f * step), 0.0f, 1.0f);
	}

	if (simulating) {
		const AccelSample & current = simulator.accel();
		bool setElsewhere = current.x != lastWrittenAccel_.x || current.y != lastWrittenAccel_.y || current.z != lastWrittenAccel_.z;
		if (orientationChanged_) {
			// The accelerometer measures "up" (the reaction to gravity) in device
			// coordinates: world up rotated back into the device.
			glm::vec3 up = glm::inverse(deviceToWorld()) * glm::vec3(0, 1, 0);
			simulator.setAccel(up.x, up.y, up.z);
			orientationChanged_ = false;
		} else if (setElsewhere) {
			orientTo(current); // tilted remotely (OSC): show it
		}
		lastWrittenAccel_ = simulator.accel();
		// Only write levels that changed, so the "squeeze by hand" sliders keep
		// working for balls that are not being pressed here.
		for (int i = 0; i < kNumSensors; ++i) {
			if (squeeze_[i] != sentSqueeze_[i]) {
				simulator.setManual(i, squeeze_[i]);
				sentSqueeze_[i] = squeeze_[i];
			}
		}
	} else if (measuredAccel.valid) {
		orientTo(measuredAccel); // show the real tilt
	}
}

// ------------------------------------------------------------------- draw

void DeviceView::draw(const ControlInputs & inputs) {
	ofPushStyle();
	drawBackdrop();

	camera_.setPosition(0.0f, 170.0f, 640.0f);
	camera_.lookAt(glm::vec3(0, -10.0f, 0));

	ofEnableDepthTest();
	camera_.begin(rect_);
	ofSetSmoothLighting(true);
	ofEnableLighting();
	keyLight_.enable();
	fillLight_.enable();
	drawBody();
	for (const Ball & ball : balls_) {
		drawBall(ball, ball.sensor >= 0 ? inputs.sensor(ball.sensor) : 0.0f);
	}
	keyLight_.disable();
	fillLight_.disable();
	ofDisableLighting();
	camera_.end();
	ofDisableDepthTest();

	drawOverlay(inputs);
	ofNoFill();
	ofSetColor(80);
	ofDrawRectangle(rect_);
	ofPopStyle();
}

void DeviceView::drawBackdrop() {
	// Night-blue gradient, like the manufacturing render.
	ofMesh mesh;
	mesh.setMode(OF_PRIMITIVE_TRIANGLE_STRIP);
	const ofFloatColor top(0.03f, 0.03f, 0.08f);
	const ofFloatColor bottom(0.30f, 0.30f, 0.62f);
	mesh.addVertex({ rect_.x, rect_.y, 0 });
	mesh.addColor(top);
	mesh.addVertex({ rect_.getRight(), rect_.y, 0 });
	mesh.addColor(top);
	mesh.addVertex({ rect_.x, rect_.getBottom(), 0 });
	mesh.addColor(bottom);
	mesh.addVertex({ rect_.getRight(), rect_.getBottom(), 0 });
	mesh.addColor(bottom);
	mesh.draw();
}

void DeviceView::drawBody() {
	ofPushMatrix();
	glm::mat4 rotation = glm::toMat4(deviceToWorld());
	ofMultMatrix(rotation);
	bodyMaterial_.begin();
	// ofDrawCylinder is along Y; turn it onto the device Z axis.
	ofPushMatrix();
	ofRotateXDeg(90.0f);
	ofDrawCylinder(0.0f, 0.0f, 0.0f, kBodyRadius, kBodyHeight);
	ofPopMatrix();
	// Collars where the balls mount.
	for (const Ball & ball : balls_) {
		ofPushMatrix();
		glm::vec3 p = ball.direction * ball.offset;
		ofTranslate(p);
		ofDrawSphere(0, 0, 0, kBallRadius * 0.45f);
		ofPopMatrix();
	}
	bodyMaterial_.end();
	ofPopMatrix();
}

void DeviceView::drawBall(const Ball & ball, float pressure) {
	float squash = ball.sensor >= 0 ? kMaxSquash * std::clamp(pressure, 0.0f, 1.0f) : 0.0f;
	glm::vec3 centre = ballCentre(ball, squash);
	glm::vec3 outward = glm::normalize(deviceToWorld() * ball.direction);

	// Squash along the outward direction, bulge a little sideways.
	glm::quat align = glm::rotation(glm::vec3(0, 0, 1), outward);
	ofPushMatrix();
	ofTranslate(centre);
	ofMultMatrix(glm::toMat4(align));
	ofScale(1.0f + 0.35f * squash, 1.0f + 0.35f * squash, 1.0f - squash);

	ofMaterial material;
	ofColor colour = kPlainBall;
	if (ball.sensor >= 0) {
		colour = kSensorBallIdle.getLerped(kSensorBallActive, std::clamp(pressure, 0.0f, 1.0f));
		material.setEmissiveColor(ofFloatColor(colour) * (0.08f + 0.45f * pressure));
	}
	material.setDiffuseColor(colour);
	material.setAmbientColor(ofFloatColor(colour) * 0.6f);
	material.setShininess(60.0f);
	material.setSpecularColor(ofFloatColor(0.4f, 0.4f, 0.4f));
	material.begin();
	ofDrawSphere(0, 0, 0, kBallRadius);
	material.end();
	ofPopMatrix();

	// The pressure sensor inside: a small core that glows when activated.
	if (ball.sensor >= 0) {
		ofDisableLighting();
		glm::vec3 core = deviceToWorld() * (ball.direction * (ball.offset + kBallRadius * 0.25f));
		ofSetColor(ofColor(255, 230, 160).getLerped(ofColor(60, 60, 60), 1.0f - std::clamp(pressure * 1.5f, 0.0f, 1.0f)));
		ofDrawSphere(core, 5.0f + 5.0f * pressure);
		ofEnableLighting();
	}
}

void DeviceView::drawOverlay(const ControlInputs & inputs) {
	// Sensor numbers next to the sensor balls.
	ofSetColor(235);
	for (const Ball & ball : balls_) {
		if (ball.sensor < 0) continue;
		glm::vec3 screen = camera_.worldToScreen(ballCentre(ball, 0.0f) + glm::vec3(0, kBallRadius, 0), rect_);
		ofDrawBitmapString(std::to_string(ball.sensor + 1), screen.x - 4, screen.y);
	}
	char text[160];
	std::snprintf(text, sizeof(text), "tilt x %.2f y %.2f any %.2f", inputs.get(ModSource::TiltX),
		inputs.get(ModSource::TiltY), inputs.get(ModSource::Tilt));
	ofDrawBitmapString(text, rect_.x + 8, rect_.getBottom() - 38);
	std::snprintf(text, sizeof(text), "shake %.2f", inputs.get(ModSource::Shake));
	ofDrawBitmapString(text, rect_.x + 8, rect_.getBottom() - 24);
	ofSetColor(170);
	ofDrawBitmapString(simulating_ ? "hold ball: squeeze  drag: tilt" : "live sensors and tilt", rect_.x + 8, rect_.getBottom() - 8);
	if (simulating_) ofDrawBitmapString("double-click: level", rect_.x + 8, rect_.y + 16);
}

// ------------------------------------------------------------------ mouse

int DeviceView::ballAt(float x, float y) {
	int best = -1;
	float bestDepth = 1e9f;
	for (size_t i = 0; i < balls_.size(); ++i) {
		if (balls_[i].sensor < 0) continue;
		glm::vec3 centre = ballCentre(balls_[i], 0.0f);
		glm::vec3 screen = camera_.worldToScreen(centre, rect_);
		glm::vec3 edge = camera_.worldToScreen(centre + glm::vec3(kBallRadius, 0, 0), rect_);
		float radius = glm::distance(glm::vec2(screen), glm::vec2(edge));
		float depth = glm::distance(camera_.getPosition(), centre);
		if (glm::distance(glm::vec2(screen), glm::vec2(x, y)) < radius && depth < bestDepth) {
			best = static_cast<int>(i);
			bestDepth = depth;
		}
	}
	return best;
}

bool DeviceView::mousePressed(float x, float y) {
	if (!inside(x, y) || !simulating_) return false;
	uint64_t now = ofGetElapsedTimeMillis();
	if (now - lastClickMs_ < 300) {
		level();
		lastClickMs_ = 0;
		return true;
	}
	lastClickMs_ = now;
	squeezedBall_ = ballAt(x, y);
	dragging_ = squeezedBall_ < 0;
	lastMouse_ = { x, y };
	return true;
}

bool DeviceView::mouseDragged(float x, float y) {
	if (!dragging_) return squeezedBall_ >= 0;
	glm::vec2 delta = glm::vec2(x, y) - lastMouse_;
	lastMouse_ = { x, y };
	// Horizontal drag rolls the instrument sideways, vertical drag tips it
	// towards / away from the viewer (both change what the accelerometer reads).
	glm::quat roll = glm::angleAxis(glm::radians(-delta.x * 0.5f), glm::vec3(0, 0, 1));
	glm::quat pitch = glm::angleAxis(glm::radians(delta.y * 0.5f), glm::vec3(1, 0, 0));
	orientation_ = glm::normalize(roll * pitch * orientation_);
	orientationChanged_ = true;
	return true;
}

bool DeviceView::mouseReleased(float, float) {
	bool used = dragging_ || squeezedBall_ >= 0;
	dragging_ = false;
	squeezedBall_ = -1;
	return used;
}

} // namespace gm
