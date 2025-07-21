#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup() {
#ifdef TARGET_RASPBERRY_PI
    sensorStrategy = std::make_shared<SPIStrategy>();
#else
    sensorStrategy = std::make_shared<SimulationStrategy>();
    uiManager = std::make_unique<UIManager>(&audioManager);
    setupButtons(); // Setup buttons
#endif

    settingsManager.loadSettings("settings.xml");
    audioManager.setup(sensorStrategy, &settingsManager);
    presetManager.setup(&audioManager, &settingsManager);

    operationMode = OP_MODE_SETUP; // Initialize operation mode
}
//--------------------------------------------------------------
void ofApp::update() {
    sensorStrategy->updateSensorData();
    audioManager.updateEffects();

    if (uiManager) {
        uiManager->update();
    }

#ifndef TARGET_RASPBERRY_PI
    for (auto button : onScreenButtons) {
        button->update();
    }
#endif
}

//--------------------------------------------------------------
void ofApp::draw() {
    if (uiManager) {
        uiManager->draw();
    }

#ifndef TARGET_RASPBERRY_PI
    for (auto button : onScreenButtons) {
        button->draw();
    }
#endif
}

//--------------------------------------------------------------
void ofApp::exit() {
#ifndef TARGET_RASPBERRY_PI
    for (auto button : onScreenButtons) {
        button->disableMouseEvents();
        delete button;
    }
    onScreenButtons.clear();
#endif
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key) {
    // Handle key presses if necessary
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key) {
    // Handle key releases if necessary
}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y) {
    // Handle mouse movement if necessary
}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button) {
    // Handle mouse dragging if necessary
}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button) {
    // Handle mouse presses if necessary
}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button) {
    // Handle mouse releases if necessary
}

//--------------------------------------------------------------
void ofApp::mouseScrolled(int x, int y, float scrollX, float scrollY) {
    // Handle mouse scrolling if necessary
}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y) {
    // Handle mouse enter events if necessary
}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y) {
    // Handle mouse exit events if necessary
}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h) {
    // Handle window resize events if necessary
}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg) {
    int messageID = ofToInt(msg.message);
    switch (messageID) {
        case BTN_MSG_M_OP_MODE_PLAY_NARRATION:
            operationMode = OP_MODE_PLAY_NARRATION;
            // Implement behavior for playing narration
            ofLogVerbose() << "Switched to OP_MODE_PLAY_NARRATION";
            break;

        case BTN_MSG_M_OP_MODE_SWITCH_PRESETS:
            // Implement behavior for switching presets
            presetManager.applyPreset("NextPreset");
            ofLogVerbose() << "Applied next preset";
            break;

        // Handle other messages
        default:
            ofLogVerbose() << "Received unknown message: " << messageID;
            break;
    }
}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo) {
    // Handle drag-and-drop events if necessary
}

#ifndef TARGET_RASPBERRY_PI
void ofApp::setupButtons() {
    int x = 10;
    int y = 10;
    int spacing = BUTTON_HEIGHT + 10;

    onScreenButton* playNarrationButton = new onScreenButton();
    playNarrationButton->setup("Play Narration", BTN_MSG_M_OP_MODE_PLAY_NARRATION);
    playNarrationButton->setPosition(x, y);
    playNarrationButton->setSize(BUTTON_WIDTH, BUTTON_HEIGHT);
    onScreenButtons.push_back(playNarrationButton);
    y += spacing;

    onScreenButton* switchPresetsButton = new onScreenButton();
    switchPresetsButton->setup("Switch Presets", BTN_MSG_M_OP_MODE_SWITCH_PRESETS);
    switchPresetsButton->setPosition(x, y);
    switchPresetsButton->setSize(BUTTON_WIDTH, BUTTON_HEIGHT);
    onScreenButtons.push_back(switchPresetsButton);
    y += spacing;

    // Add more buttons as needed
}
#endif
