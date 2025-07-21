#pragma once

#include "ofxPDSP.h"
#include "ofxGui.h"
#include "ofxXmlSettings.h"
#include "ofxOsc.h"

#include "SettingsManager.h"
#include "PresetManager.h"
#include "AudioManager.h"
#include "ISensorStrategy.h"
#include "SPIStrategy.h"
#include "SimulationStrategy.h"
#include "UIManager.h"

// Define TARGET_RASPBERRY_PI if not on OSX or Windows
#ifndef TARGET_OSX
#ifndef TARGET_WIN32
#define TARGET_RASPBERRY_PI
#endif
#endif

// Include onScreenButton only when not on Raspberry Pi
#ifndef TARGET_RASPBERRY_PI
#include "onScreenButton.h"
#endif

// Include Raspberry Pi-specific headers
#ifdef TARGET_RASPBERRY_PI
#include "ofxGPIO.h"
#include <stdio.h>
#include <stdlib.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <wiringPi.h>
#endif

#define NUMBER_OF_SENSORS 6
#define NUMBER_OF_EFFECTS 6
#define NUMBER_OF_PRESETS 4

// Operation modes
#define OP_MODE_SETUP 0
#define OP_MODE_WAIT_FOR_NARRATION 1
#define OP_MODE_PLAY_NARRATION 2
#define OP_MODE_NARRATION_GUI 6
#define OP_MODE_NARRATION_GLITCH 9
#define OP_MODE_SWITCH_PRESETS 3
#define OP_MODE_MULTI_GRAIN_MODE 4
#define OP_MODE_SINGLE_GRAIN_MODE 5
#define OP_MODE_SIMULATION_MULTI 7
#define OP_MODE_SIMULATION_SINGLE 8

// Button action messages
#define BTN_MSG_A_PAUSE_NARRATION 9
#define BTN_MSG_A_SKIP_NARRATION 10
#define BTN_MSG_A_RESTART_NARRATION 11
#define BTN_MSG_A_GLITCH_NARRATION 12
#define BTN_MSG_A_TOGGLE_SIMULATION 13
#define BTN_MSG_A_SWITCH_PRESETS 14
#define BTN_MSG_A_RETURN_TO_NARRATION 15
#define BTN_MSG_A_NARRATION_GUI 16
#define BTN_MSG_A_SAVE_GRANULAR 17
#define BTN_MSG_A_SAVE_NARRATION 18
#define BTN_MSG_A_PAUSE_SIMULATION 19
#define BTN_MSG_A_CURVES_OR_SIMULATION_DISPLAY 20
#define BTN_MSG_A_EFFECTS_OR_GRANULAR 28

// Button mode messages
#define BTN_MSG_M_OP_MODE_PLAY_NARRATION 21
#define BTN_MSG_M_OP_MODE_SWITCH_PRESETS 22
#define BTN_MSG_M_OP_MODE_GRAIN_MODE 23
#define BTN_MSG_M_OP_MODE_NARRATION_GUI 24
#define BTN_MSG_M_OP_MODE_SIMULATION 25
#define BTN_MSG_M_OP_MODE_NARRATION_GLITCH 26
#define BTN_MSG_M_EXIT 27

// Size of on-screen buttons
#define BUTTON_WIDTH 250
#define BUTTON_HEIGHT 45



class ofApp : public ofBaseApp {
public:
    void setup();
    void update();
    void draw();
    void exit();
    void keyPressed(int key);
    void keyReleased(int key);
    void mouseMoved(int x, int y);
    void mouseDragged(int x, int y, int button);
    void mousePressed(int x, int y, int button);
    void mouseReleased(int x, int y, int button);
    void mouseScrolled(int x, int y, float scrollX, float scrollY); // Added missing declaration
    void resetValuesAfterChanges();
    void mouseEntered(int x, int y);
    void mouseExited(int x, int y);
    void windowResized(int w, int h);
    void dragEvent(ofDragInfo dragInfo);
    void gotMessage(ofMessage msg);

private:
    SettingsManager settingsManager;
    PresetManager presetManager;
    AudioManager audioManager;
    std::shared_ptr<ISensorStrategy> sensorStrategy;
    std::unique_ptr<UIManager> uiManager;

    int operationMode;

#ifndef TARGET_RASPBERRY_PI
    std::vector<onScreenButton*> onScreenButtons;
    void setupButtons();
#endif
};
