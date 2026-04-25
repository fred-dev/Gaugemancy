#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup() {
    ofLogNotice("ofApp") << "=== Application Starting ===";
    
    // Set file path prefix based on platform
#ifdef HAS_ADC
    filePathPrefix = "/media/Data/";
    ofLogNotice("ofApp") << "Running on Raspberry Pi with ADC";
#else
    filePathPrefix = ofToDataPath("");
    ofLogNotice("ofApp") << "Running on Desktop/Laptop";
#endif
    
    // Initialize parameters
    initParameters();
    
    // Load settings from XML
    setupParamsFromXML();
    
    // Setup preset manager
    presetManager.setup(filePathPrefix, unitID);
    
    // Load file paths from XML
    setupFilePaths();
    
    // Setup grain clouds for current preset
    auto paths = presetManager.getFilePathsForPreset(presetIndex);
    setupGrainCloud(paths, unitID + "_preset_" + ofToString(presetIndex) + ".xml");
    
#ifndef HAS_ADC
    // Desktop-only initialization
    setupVisualiser();
    setupGraphicEnv();
    setupCurvesDisplay();
    generateCurvedLine();
    setupAllButtons();
#endif
    
#ifdef HAS_ADC
    // Raspberry Pi hardware initialization
    wiringPiSetup();
    redLedPin = 31;
    blueLedPin = 21;
    relayPin = 23;
    buttonPin = 24;
    
    setupButton();
    setupLIS3DH();
    setupADC();
    initLedBlue();
    initLedRed();
    setupSpeakerControl();
    syncSpeaker();
    
    // Set LED state based on preset
    if (presetIndex == 1 || presetIndex == 3) {
        digitalWrite(blueLedPin, HIGH);
        digitalWrite(redLedPin, LOW);
    } else if (presetIndex == 2 || presetIndex == 4) {
        digitalWrite(blueLedPin, HIGH);
        digitalWrite(redLedPin, HIGH);
    }
#endif
    
    // Initialize sensor processor
    sensorProcessor.setup();
    
    // Setup narration if needed
    if (!presetManager.hasNarrationTrack()) {
        modeManager.changeMode(OperationMode::MultiGrain);
    } else {
        // Setup narration player
        narration.load(presetManager.getNarrationFilePath());
        modeManager.changeMode(OperationMode::WaitForNarration);
    }
    
    // Setup audio engine
    engine.listDevices();
    engine.setChannels(0, 2);
    engine.setDeviceID(presetManager.getAudioDeviceId());
    engine.setup(44100, presetManager.getEngineBufferSize(), presetManager.getNumberOfBuffers());
    
    ofLogNotice("ofApp") << "=== Application Ready ===";
}

//--------------------------------------------------------------
void ofApp::update() {
    // Update mode manager (handles mode-specific updates)
    modeManager.update();
    
    // Update sensor data
    sensorProcessor.update();
    
#ifdef HAS_ADC
    // Hardware-specific updates
    updateLIS3DH();
    buttonStateMachine();
    deviceOnlyUpdateRoutine();
#endif
    
#ifndef HAS_ADC
    // Desktop simulation updates
    if (simulationRunning) {
        simulationGFXUpdateRoutine();
    }
#endif
}

//--------------------------------------------------------------
void ofApp::draw() {
#ifndef HAS_ADC
    ofBackground(0);
    
    // Mode-specific drawing is handled via callbacks in ModeManager
    // This keeps the draw loop clean and organized
    
    // Draw messages overlay
    // drawMessages();
#endif
}

//--------------------------------------------------------------
void ofApp::exit() {
    ofLogNotice("ofApp") << "Application exiting...";
    
    // Cleanup grain clouds
    grainClouds.clear();
    
    // Stop audio engine - pdsp engine cleanup is handled automatically
    // when the engine object goes out of scope
    
    ofLogNotice("ofApp") << "Goodbye!";
}

//--------------------------------------------------------------
void ofApp::initParameters() {
    // Initialize all application parameters with defaults
    presetIndex = 1;
    firstRun = true;
    previousOperationMode = 0;
    presetSwitchTimer = 0;
    firstPresetSwitch = true;
    
    // Audio engine settings
    engineBufferSize = 512;
    numberOfBuffers = 4;
    audioDeviceId = 0;
    
    // Sensor settings
    maxSensorValue = 4095;
    normalisedA2DValuesMin = 0.0f;
    useAccumulatedPressure = true;
    accumulatedPressure = 0.0f;
    accumulatedPressureNormalised = 0.0f;
    accumulationDenominator = 6.0f * 4095.0f;
    
    // Hit detection settings
    useHitGesture = true;
    hitThreshold = 0.8f;
    troughThreshold = 0.2f;
    requiredHits = 3;
    timeBetweenHits = 500;
    maxPeakDuration = 200;
    maxTroughDuration = 200;
    
    // Curve settings
    curveSelector = 0;
    
    // OSC settings
    oscDebug = false;
    oscLive = false;
    localOSCPport = 8000;
    remoteOSCPort = 9000;
    
    // UI settings
    numberOfSlots = 6;
    buttonPressTimeOut = 500;
    
    ofLogNotice("ofApp") << "Parameters initialized";
}

//--------------------------------------------------------------
void ofApp::setupParamsFromXML() {
    // Load application settings from XML via PresetManager
    std::string appSettingsPath = filePathPrefix + "appSettings.xml";
    
    if (appSettingsXML.load(appSettingsPath)) {
        unitID = appSettingsXML.getValue("UNIT_ID", std::string("unknown"));
        logLevel = appSettingsXML.getValue("LOG_LEVEL", 3);
        
        localIpFromXML = appSettingsXML.getValue("LOCAL_IP", std::string(""));
        remoteOSCIp = appSettingsXML.getValue("REMOTE_IP", std::string("127.0.0.1"));
        
        ofSetLogLevel(ofLogLevel(logLevel));
        ofLogNotice("ofApp") << "Loaded app settings for unit: " << unitID;
    } else {
        ofLogWarning("ofApp") << "Could not load app settings, using defaults";
        unitID = "default";
    }
}

//--------------------------------------------------------------
void ofApp::setupFilePaths() {
    // File paths are loaded by PresetManager
    for (int i = 0; i < 4; i++) {
        presetManager.loadPresetConfig(i);
    }
}

//--------------------------------------------------------------
void ofApp::setupGrainCloud(const std::vector<std::string>& paths, const std::string& presetPath) {
    // Clear existing clouds
    grainClouds.clear();
    
    int numVoices = std::min(paths.size(), static_cast<size_t>(6));
    
    for (size_t i = 0; i < numVoices; i++) {
        auto cloud = std::make_unique<GrainCloudManager>();
        cloud->setup(i);
        
        if (i < paths.size() && !paths[i].empty()) {
            cloud->loadAudioFile(paths[i]);
        }
        
        grainClouds.push_back(std::move(cloud));
    }
    
    ofLogNotice("ofApp") << "Setup " << numVoices << " grain clouds";
}

// ===========================================================================
// Platform-specific implementations would go here
// These are stubs that delegate to the appropriate subsystem
// ===========================================================================

#ifdef HAS_ADC
void ofApp::setupLIS3DH() { /* I2C accelerometer setup */ }
void ofApp::updateLIS3DH() { /* Read accelerometer data */ }
void ofApp::setupADC() { /* SPI ADC setup */ }
void ofApp::setupButton() { /* GPIO button setup */ }
void ofApp::initLedBlue() { pinMode(blueLedPin, OUTPUT); }
void ofApp::initLedRed() { pinMode(redLedPin, OUTPUT); }
void ofApp::setupSpeakerControl() { pinMode(relayPin, OUTPUT); }
void ofApp::syncSpeaker() { digitalWrite(relayPin, HIGH); }
void ofApp::buttonStateMachine() { /* Handle button presses */ }
void ofApp::deviceOnlyUpdateRoutine() { /* Device-specific updates */ }
#endif

#ifndef HAS_ADC
void ofApp::setupVisualiser() {
    barLength = 500;
    barWidth = 30;
    for (int i = 0; i < 6; i++) {
        barsEmpty[i].set(barWidth, barLength, barWidth);
        barsFull[i].set(barWidth + 2, barLength + 2, barWidth + 2);
    }
    // Position bars in 3D cross pattern
    barsEmpty[0].setPosition(0, -barLength / 2, 0);
    barsEmpty[1].setPosition(0, barLength / 2, 0);
    barsFull[0].setPosition(0, -barLength / 2, 0);
    barsFull[1].setPosition(0, barLength / 2, 0);
    barsEmpty[2].setPosition(-barLength / 2, 0, 0);
    barsEmpty[2].rotateDeg(90, 0, 0, 1);
    barsFull[2].setPosition(-barLength / 2, 0, 0);
    barsFull[2].rotateDeg(90, 0, 0, 1);
    barsEmpty[3].setPosition(barLength / 2, 0, 0);
    barsEmpty[3].rotateDeg(-90, 0, 0, 1);
    barsFull[3].setPosition(barLength / 2, 0, 0);
    barsFull[3].rotateDeg(-90, 0, 0, 1);
    barsEmpty[4].setPosition(0, 0, -barLength / 2);
    barsEmpty[4].rotateDeg(90, 1, 0, 0);
    barsFull[4].setPosition(0, 0, -barLength / 2);
    barsFull[4].rotateDeg(90, 1, 0, 0);
    barsEmpty[5].setPosition(0, 0, barLength / 2);
    barsEmpty[5].rotateDeg(-90, 1, 0, 0);
    barsFull[5].setPosition(0, 0, barLength / 2);
    barsFull[5].rotateDeg(-90, 1, 0, 0);
}

void ofApp::setupGraphicEnv() {
    ofSetVerticalSync(true);
    ofBackground(0);
    barsFbo.allocate(600, 600);
    cam.setOrientation(glm::vec3(20, 20, 20));
}

void ofApp::setupCurvesDisplay() {
    curvesFbo.allocate(500, 500);
    curvesNames[0] = "Linear";
    curvesNames[1] = "Sqrt";
    curvesNames[2] = "Quartic";
    curvesNames[3] = "Exponential";
}

void ofApp::setupAllButtons() {
    // Button setup delegated to individual mode handlers
}

void ofApp::generateCurvedLine() {
    curvesLine.clear();
    float width = curvesFbo.getWidth();
    
    for (int i = 0; i < width; i++) {
        float t = ofMap(i, 0, width, 0.0f, 1.0f);
        float y = 0;
        
        switch (curveSelector) {
            case 0: y = t; break;
            case 1: y = sqrt(t); break;
            case 2: y = quarticEaseIn(t); break;
            case 3: y = exponentialEaseIn(t); break;
        }
        
        curvesLine.addVertex(i, width - y * width);
    }
}

void ofApp::simulationGFXUpdateRoutine() {
    // Generate simulated sensor data for testing
    static float simTime = 0;
    simTime += 0.01f;
    
    for (int i = 0; i < 6; i++) {
        sensorValues[i] = (sin(simTime + i * 0.5) + 1.0f) * 0.5f;
    }
}

void ofApp::generateSimulatedData() {
    // Map simulated values to normalized ADC values
    for (int i = 0; i < 6; i++) {
        normalisedA2DValues[i] = sensorValues[i];
    }
}

void ofApp::mapSimulatedDataSingle() {
    // Single mode mapping
}

void ofApp::mapSimulatedDataMulti() {
    // Multi mode mapping
}
#endif

// ===========================================================================
// Utility methods
// ===========================================================================

float ofApp::exponentialEaseIn(float value) {
    if (value == 0.0f) return 0.0f;
    return pow(2.0f, 10.0f * (value - 1.0f));
}

float ofApp::quarticEaseIn(float value) {
    return value * value * value * value;
}

float ofApp::exponentialEaseOut(float value) {
    if (value == 0.0f) return 1.0f;
    return -pow(2.0f, -10.0f * value) + 1.0f;
}

float ofApp::quarticEaseOut(float value) {
    float t = value - 1.0f;
    return -(t * t * t * t - 1.0f);
}

void ofApp::switchPresets() {
    presetManager.switchToNextPreset();
    presetIndex = presetManager.getCurrentPresetIndex();
    
    // Reload grain clouds with new preset files
    auto paths = presetManager.getFilePathsForPreset(presetIndex);
    setupGrainCloud(paths, unitID + "_preset_" + ofToString(presetIndex + 1) + ".xml");
}

void ofApp::getAccumulatedPressure() {
    accumulatedPressure = 0.0f;
    for (int i = 0; i < 6; i++) {
        accumulatedPressure += sensorProcessor.getRawValue(i);
    }
    accumulatedPressureNormalised = accumulatedPressure / accumulationDenominator;
}

void ofApp::checkForHits() {
    // Delegated to SensorDataProcessor with threshold parameters
    sensorProcessor.checkForHits(hitThreshold, troughThreshold);
}

void ofApp::onHitRoutine() {
    ofLogNotice("ofApp") << "Hit gesture detected!";
    // Trigger appropriate response based on current mode
}

void ofApp::UpdatePlayheadWithTimer() {
    // Time-based playhead advancement for narration
}

void ofApp::controlOn(int x, int y) {
    // Granular parameter control UI
}

void ofApp::controlOnNarr(int x, int y) {
    // Narration parameter control UI
}

void ofApp::resetValuesAfterChanges() {
    // Reset parameters after preset/mode changes
}

void ofApp::applyDynamicValuesToParameters(
    int& k,
    std::vector<ofParameter<int>> connectTo,
    int v,
    std::vector<ofParameter<float>> parameter,
    std::vector<ofParameter<float>> parameterMin,
    std::vector<ofParameter<float>> parameterMax,
    std::string paramName
) {
    // Apply sensor values to parameters based on connection mapping
}

void ofApp::calibrateOnStart() {
    // Initialize base values with zeros for first calibration
    std::vector<int> baseValues(6, 0);
    sensorProcessor.calibrate(baseValues);
}

void ofApp::loadRoutine(int target) {
    // Load samples for specific voice
}

// Input handlers - delegate to mode manager or handle globally
void ofApp::keyPressed(int key) {
    if (key == ' ') {
        switchPresets();
    }
}

void ofApp::keyReleased(int key) {}
void ofApp::mouseMoved(int x, int y) {}
void ofApp::mouseDragged(int x, int y, int button) {}
void ofApp::mousePressed(int x, int y, int button) {}
void ofApp::mouseReleased(int x, int y, int button) {}
void ofApp::windowResized(int w, int h) {}
void ofApp::dragEvent(ofDragInfo dragInfo) {}

void ofApp::gotMessage(ofMessage msg) {
    int message = ofToInt(msg.message);
    
    // Handle button messages
    switch (message) {
        case 14: // Switch presets
            switchPresets();
            break;
        // Add other button handlers as needed
    }
}
