#pragma once

#include "ofMain.h"
#include "ofxPDSP.h"
#include "ofxGui.h"
#include "ofxXmlSettings.h"
#include "ofxOsc.h"

// Core components
#include "core/AppConstants.h"
#include "core/ModeManager.h"
#include "core/PresetManager.h"
#include "sensors/SensorDataProcessor.h"
#include "audio/GrainCloudManager.h"
#include "AudioPlayer.h"

// Effects units
#include "EFFBitCrushUnit.h"
#include "EFFDecimatorUnit.h"
#include "EFFDelayUnit.h"
#include "EFFFilterUnit.h"
#include "EFFChorusUnit.h"
#include "EFFReverbUnit.h"
#include "EFFCompressorUnit.h"

// Platform-specific includes
#ifndef TARGET_OSX
#ifndef TARGET_WIN32
#define HAS_ADC
#endif
#endif

#ifdef HAS_ADC
#include "ofxGPIO.h"
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <wiringPi.h>
#else
#include "onScreenButton.h"
#endif

/**
 * ofApp - Main Application Class
 * 
 * This is the main application class for openFrameworks.
 * It coordinates all subsystems and handles the main lifecycle.
 * 
 * Refactored structure:
 * - ModeManager: Handles mode/state transitions
 * - PresetManager: Manages presets and file loading
 * - SensorDataProcessor: Handles sensor input and processing
 * - GrainCloudManager: Manages granular synthesis voices
 * - Audio subsystem: PDSP audio engine
 * - Effect processors: Individual effect units
 */
class ofApp : public ofBaseApp {
public:
    // Lifecycle
    void setup();
    void update();
    void draw();
    void exit();
    
    // Input handlers
    void keyPressed(int key);
    void keyReleased(int key);
    void mouseMoved(int x, int y);
    void mouseDragged(int x, int y, int button);
    void mousePressed(int x, int y, int button);
    void mouseReleased(int x, int y, int button);
    void windowResized(int w, int h);
    void dragEvent(ofDragInfo dragInfo);
    void gotMessage(ofMessage msg);
    
private:
    // =========================================================================
    // Subsystem Managers
    // =========================================================================
    
    ModeManager modeManager;
    PresetManager presetManager;
    SensorDataProcessor sensorProcessor;
    
    // =========================================================================
    // Audio System
    // =========================================================================
    
    ofxPDSPEngine engine;
    std::vector<std::unique_ptr<GrainCloudManager>> grainClouds;
    
    // Narration player
    AudioPlayer narration;
    
    // Effect processors (per slot)
    static constexpr int MaxSlots = 6;
    std::vector<EFFBitCrushUnit> bitCrusherParams;
    std::vector<EFFDecimatorUnit> decimatorParams;
    std::vector<EFFDelayUnit> delayParams;
    std::vector<EFFFilterUnit> filterParams;
    std::vector<EFFChorusUnit> chorusParams;
    std::vector<EFFReverbUnit> reverbParams;
    std::vector<EFFCompressorUnit> compressorParams;
    
    // =========================================================================
    // Configuration & State
    // =========================================================================
    
    std::string filePathPrefix;
    std::string unitID;
    int presetIndex;
    bool firstRun;
    
    // App settings (loaded from XML via PresetManager)
    int logLevel;
    
    // =========================================================================
    // Initialization Methods
    // =========================================================================
    
    void initParameters();
    void setupParamsFromXML();
    void setupFilePaths();
    void setupGrainCloud(const std::vector<std::string>& paths, const std::string& presetPath);
    
    // =========================================================================
    // Platform-Specific Setup (Raspberry Pi)
    // =========================================================================
    
#ifdef HAS_ADC
    void setupLIS3DH();
    void updateLIS3DH();
    void setupADC();
    void setupButton();
    void initLedBlue();
    void initLedRed();
    void setupSpeakerControl();
    void syncSpeaker();
    void buttonStateMachine();
    void deviceOnlyUpdateRoutine();
    
    // GPIO pins
    int redLedPin, blueLedPin, relayPin, buttonPin;
    
    // ADC data
    MCP a2d;
    int a2dVal[6];
    int a2dChannel[6];
    unsigned char data[8][6];
    
    // I2C accelerometer
    I2c* bus;
    float accelScaleFactor;
    float xg, yg, zg;
    
    // Button state machine
    bool buttonValue;
    int clicks;
    int click1Time, click2Time, click3Time, click1ReleaseTime;
    bool waitingForClick, clickReleased;
    bool shutdownPress, doShutdown;
    int state_button;
#endif
    
    // =========================================================================
    // Platform-Specific Setup (Desktop/Laptop)
    // =========================================================================
    
#ifndef HAS_ADC
    // GUI
    ofxPanel mainGui;
    ofTrueTypeFont messageFont;
    
    // On-screen buttons per mode
    onScreenButton waitForNarrButtons[6];
    onScreenButton playNarrationButtons[6];
    onScreenButton switchPresetsButtons[6];
    onScreenButton multiGrainModeButtons[6];
    onScreenButton singleGrainModeButtons[6];
    onScreenButton narrGuiModeButtons[6];
    onScreenButton simulationMultiButtons[6];
    onScreenButton simulationSingleButtons[6];
    onScreenButton narrationGlitchButtons[6];
    
    // Simulation
    ofParameter<int> compressionSimSpeed;
    ofParameter<int> compressionSimExtent;
    ofParameter<int> simulatedInput;
    ofParameter<float> compressionSims[6];
    ofParameter<bool> sensorSimActive[6];
    ofParameter<bool> runSimulation;
    
    bool simulationRunning, simulationRising, simulationFalling;
    void generateSimulatedData();
    void mapSimulatedDataSingle();
    void mapSimulatedDataMulti();
    void simulationGFXUpdateRoutine();
    
    // Visualizer
    void setupVisualiser();
    void setupGraphicEnv();
    void setupCurvesDisplay();
    void setupAllButtons();
    
    ofBoxPrimitive barsEmpty[6];
    ofBoxPrimitive barsFull[6];
    float sensorValues[6];
    ofEasyCam cam;
    ofParameter<float> camDistance;
    int barLength, barWidth;
    ofFbo barsFbo;
    
    // Curves display
    ofFbo curvesFbo;
    ofPolyline curvesLine;
    std::string curvesNames[4];
    ofParameter<int> curveSelector;
    ofRectangle simulationArea;
    bool simulationNotCurve;
    
    void generateCurvedLine();
    void drawCurvesDisplay(int x, int y, int width, int height);
    void drawCurvesDisplaySimulationSingle(int x, int y, int width, int height);
    void drawCurvesDisplaySimulationMulti(int x, int y, int width, int height);
#endif
    
    // =========================================================================
    // Utility Methods
    // =========================================================================
    
    // Curve processing
    float exponentialEaseIn(float value);
    float quarticEaseIn(float value);
    float exponentialEaseOut(float value);
    float quarticEaseOut(float value);
    
    // Parameter updates
    void updateParametersFromValuesSingle();
    void updateParametersFromValuesMulti();
    void applyDynamicValuesToParameters(
        int& k, 
        std::vector<ofParameter<int>> connectTo, 
        int v, 
        std::vector<ofParameter<float>> parameter,
        std::vector<ofParameter<float>> parameterMin,
        std::vector<ofParameter<float>> parameterMax,
        std::string paramName
    );
    
    // Preset switching
    void switchPresets();
    int presetSwitchTimer;
    bool firstPresetSwitch;
    int previousOperationMode;
    
    // OSC communication
    ofxOscSender sender;
    ofxOscMessage recyclingMessage;
    bool setLocalIp, setLocalToDHCP;
    std::string localIpFromXML;
    int localOSCPport, remoteOSCPort;
    std::string remoteOSCIp;
    bool oscDebug, oscLive;
    
    // XML settings
    ofxXmlSettings fileSettingsXML;
    ofxXmlSettings appSettingsXML;
    ofxXmlSettings usernameXML;
    ofxXmlSettings effectsPatchXML;
    
    // Calibration
    void calibrateOnStart();
    std::vector<int> baseSensorValues;
    std::vector<int> zeroValues;
    int maxSensorValue;
    float normalisedA2DValues[6];
    float normalisedA2DValuesMin;
    
    // Hit detection
    void checkForHits();
    void onHitRoutine();
    bool fireHitSwitch, isCountingHits, isCheckingHitPeaks, isCheckingHitTroughs;
    bool hadHitPeak[6], hadHitTrough[6];
    bool useHitGesture;
    int timeBetweenHits, timeSinceLastHitPeak, timeSinceLastHitTrough;
    int oldValues[6], hitsCollected, hitPeakChecker, maxPeakDuration;
    int hitTroughsCollected, hitTroughChecker, maxTroughDuration;
    int completedFullHits, requiredHits;
    float hitThreshold, troughThreshold;
    
    // Accumulated pressure
    bool useAccumulatedPressure;
    float accumulatedPressureNormalised;
    float accumulatedPressure;
    float accumulationDenominator;
    void getAccumulatedPressure();
    
    // Playhead timer
    void UpdatePlayheadWithTimer();
    float timeAdvanceInterval[4], positionFromTime;
    bool timeUpdateFromKey;
    
    // Load routine
    void loadRoutine(int target);
    int currentTarget;
    int grainOperationModeTranslate;
    int simulationOperationModeTranslate;
    int numberOfSlots;
    int buttonPressTimeOut;
    
    // Control UI
    void controlOn(int x, int y);
    void controlOnNarr(int x, int y);
    
    // Reset values
    void resetValuesAfterChanges();
};
