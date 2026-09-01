#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup(){
    
    //setup function for the whole system, run once on startup
    
    //set the root path for all settings and audio (different for the PI - USB stick mounted via FSTAB, or the laptop version - relative to the exe
#ifdef HAS_ADC
    filePathPrefix = "/media/Data/";
#else
    filePathPrefix = ofToDataPath("");
#endif
    
#ifndef HAS_ADC
    // prepare graphic components for the simulation visualiser - laptop version only
    setupVisualiser();
#endif
    // init parameters we need
    initParameters();
    
    // read settings from JSON, user, all user settings and
    setupParamsFromSettings();

    // based on the settings populate the vectors with blanks
    // (populateVectors() calls populateEffectVectors() itself at the end --
    // this used to also call it a second time here, redundantly)
    populateVectors();

    // read the audio file paths from the settings so we know what to load
    setupFilePaths();
    
    // create the granualr objects we need and setup thier parameters from the ofParameter class
    setupGraincloud(filePathsSet[presetIndex-1], unitID + "_preset_" + ofToString(presetIndex) + ".xml");
    
#ifndef HAS_ADC
    // setup graphic envirnment variables and FBOs - latop only
    setupGraphicEnv();
    // setup the curves preview environment, this is a visualiser for viewing the curve applied to the sensor data- latop only
    setupCurvesDisplay();
    // setup the curves preview line, this can change in app- latop only
    generateCurvedLine();
    // setup the on screen buttons- latop only, each mode has its own buttons
    setupAllButtons();
#endif
    
    
#ifdef HAS_ADC
    
    //setup the pin to accept input from the button- raspberry pi only
    setupButton();
    
    //setup the i2c accelermeter - raspberry pi only
    setupLIS3DH();
    
    //setup the SPI ADC - raspberry pi only
    setupADC();
    
    //setup the Blue LED pin- raspberry pi only
    initLedBlue();
    
    //setup the red LED pin- raspberry pi only
    initLedRed();
    
    //setup the relay to control the speaker- raspberry pi only
    setupSpeakerControl();
    
    //Turn on the speaker via the relay- raspberry pi only
    syncSpeaker();
    
    
    
    if (presetIndex ==1) {
        blueLed.setval_gpio("1");
        redLed.setval_gpio("0");
    }
    if (presetIndex == 2) {
        blueLed.setval_gpio("1");
        redLed.setval_gpio("1");
    }
    if (presetIndex ==3) {
        blueLed.setval_gpio("1");
        redLed.setval_gpio("0");
    }
    if (presetIndex ==4) {
        blueLed.setval_gpio("1");
        redLed.setval_gpio("1");
    }
#else
    
    // setup the font for showing messages on screen - latop only
    messageFont.load("verdana.ttf", 40);
#endif
    
    // In the show some of the devices have a narration file, which will play when the performers touch the balls and increase pressure readings past a threshold. If it does not have a narration track it goes directly to the granular mode
    if (!hasNarration)
    {
        goToMode(grainOperationModeTranslate);
    }
    if (hasNarration)
    {
        setupNarration();
    }
    
    //-----------------init and start audio subsystem-------------
    std::vector<ofSoundDevice> availableAudioDevices = engine.listDevices();

    // AUDIO_DEVICE_ID must be one of the ofSoundDevice::deviceID values
    // printed just above by listDevices() (format "[API: deviceID] name").
    // On this build (RtAudio >= 6) those IDs come straight from
    // RtAudio::getDeviceIds(), which on macOS/CoreAudio are opaque handles
    // assigned per-machine (not small sequential 0/1/2/... indices) -- so a
    // value tuned on one machine/OS install will very likely not match on
    // another. This block exists to make that mismatch obvious instead of
    // silently falling back to whatever device RtAudio picks.
    ofLogNotice() << "Requested AUDIO_DEVICE_ID = " + ofToString(audioDeviceId);
    bool audioDeviceIdMatched = false;
    for (const auto & dev : availableAudioDevices) {
        if (dev.deviceID == audioDeviceId) {
            audioDeviceIdMatched = true;
            ofLogNotice() << "  -> matches \"" + dev.name + "\" (in:" + ofToString(dev.inputChannels) + " out:" + ofToString(dev.outputChannels) + ")"
                + (dev.isDefaultOutput ? " [default out]" : "");
            if (dev.outputChannels < 2) {
                ofLogError() << "  -> this device only has " + ofToString(dev.outputChannels) + " output channel(s) but the engine requests 2 (stereo) -- audio will fail to start on it.";
            }
        }
    }
    if (!audioDeviceIdMatched) {
        ofLogError() << "AUDIO_DEVICE_ID " + ofToString(audioDeviceId) + " does not match any audio device on this machine. Update it in " + unitID + "_appSettings.json. Output-capable devices found here:";
        for (const auto & dev : availableAudioDevices) {
            if (dev.outputChannels >= 2) {
                ofLogError() << "  ID " + ofToString(dev.deviceID) + ": \"" + dev.name + "\" (out:" + ofToString(dev.outputChannels) + ")"
                    + (dev.isDefaultOutput ? " [default out]" : "");
            }
        }
    }

    engine.setChannels(0,2);

    engine.setDeviceID(audioDeviceId); // see AUDIO_DEVICE_ID diagnostic above
    engine.setup(44100, engineBufferSize, numberOfBuffers);
}

//--------------------------------------------------------------
void ofApp::update(){
    // here we have the update loop with a switch for the different modes, runs as fast as possible
    switch (operationMode)
    {
        case OP_MODE_SETUP:
            updateInitMode();
            break;
        case OP_MODE_WAIT_FOR_NARRATION:
            updateWaitForNarrMode();
            break;
        case OP_MODE_PLAY_NARRATION:
            updatePlayNarrMode();
            break;
        case OP_MODE_SWITCH_PRESETS:
            updateSwitchPresetsMode();
            break;
        case OP_MODE_MULTI_GRAIN_MODE:
            updateMultiGrainMode();
            break;
        case OP_MODE_SINGLE_GRAIN_MODE:
            updateSingleGrainMode();
            break;
#ifndef HAS_ADC
        case OP_MODE_NARRATION_GUI:
            updateNarrGuiMode();
            break;
        case OP_MODE_SIMULATION_MULTI:
            updateSimulationMultiMode();
            break;
        case OP_MODE_SIMULATION_SINGLE:
            updateSimulationSingleMode();
            break;
#endif
        case OP_MODE_NARRATION_GLITCH:
            updateNarrationGlitchMode();
            break;
    }
}

// here we have the draw loop with a switch for the different modes, runs at the screens refresh rate, - laptop only

//--------------------------------------------------------------
void ofApp::draw(){
#ifndef HAS_ADC
    
    switch (operationMode)
    {
        case OP_MODE_SETUP:
            drawInitMode();
            break;
        case OP_MODE_WAIT_FOR_NARRATION:
            drawWaitForNarrMode();
            break;
        case OP_MODE_PLAY_NARRATION:
            drawPlayNarrMode();
            break;
        case OP_MODE_SWITCH_PRESETS:
            drawSwitchPresetsMode();
            break;
        case OP_MODE_MULTI_GRAIN_MODE:
            drawMultiGrainMode();
            break;
        case OP_MODE_SINGLE_GRAIN_MODE:
            drawSingleGrainMode();
            break;
        case OP_MODE_NARRATION_GUI:
            drawNarrGuiMode();
            break;
        case OP_MODE_SIMULATION_MULTI:
            drawSimulationMultiMode();
            break;
        case OP_MODE_SIMULATION_SINGLE:
            drawSimulationSingleMode();
            break;
        case OP_MODE_NARRATION_GLITCH:
            drawNarrationGlitchMode();
            break;
    }
    drawMessages();
#endif
}


//processes sensor and simulation input to an exponential curve
float ofApp::exponentialEaseIn (float value){
    float t = value;
    return (t==0.0) ? 0.0 : 1.0 * pow(2.0, 10 * (t/1.0 - 1.0));
}

//processes sensor and simulation input to an quartic curve
float ofApp::quarticEaseIn(float value){
    return 1.0*(value/=1.0) * value * value * value;
}

//processes sensor and simulation input to an inverted exponential curve
float ofApp::exponentialEaseOut (float value){
    
    return (value==0) ? 1.0 : 1.0 * (-pow(2.0, -10.0 * value/0.0) + 1.0) ;
    
}

//processes sensor and simulation input to an inverted quartic curve
float ofApp::quarticEaseOut(float value){
    return -1.0 * ((value=value/0-1.0)*value*value*value - 1.0);
}


#ifndef HAS_ADC

void ofApp::setupVisualiser() {
    // create the 2 colour 3d cross that visualises simulated sensor input
    ofLogNotice() << "setup visualiser" << endl;
    barLength = 500;
    barWidth = 30;
    for (int i = 0; i < NUMBER_OF_SENSORS; i++) {
        barsEmpty[i].set(barWidth, barLength, barWidth);
        barsFull[i].set(barWidth + 2, barLength + 2, barWidth + 2);
        
    }
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
    ofLogNotice() << "Display setup" << endl;
}

void ofApp::setupGraphicEnv()
{
    // jst setting up screen stuff
    ofLogNotice() << "setup grapic environment" << endl;
    ofSetVerticalSync(true);
    ofBackground(0);
    barsFbo.allocate(600, 600);
    cam.setOrientation(glm::vec3(20, 20, 20));
}



void ofApp::setupCurvesDisplay(){
    // allocate FBO for the curves preview
    curvesFbo.allocate(500,500);
}


void ofApp::generateCurvedLine(){
    //incoming data can have a curve applied to change the way the sensor data feels, the curves line is a preview that visualises this, here we use the curves selector to create the appropriate curve for the preview
    curvesLine.clear();
    switch (curveSelector){
        case 0:
            for(int i = 0; i<curvesFbo.getWidth(); i++){
                curvesLine.addVertex(i, curvesFbo.getWidth() - i);
            }
            break;
        case 1:
            for(int i = 0; i<curvesFbo.getWidth(); i++){
                curvesLine.addVertex(i, curvesFbo.getWidth() - ofMap(sqrt(ofMap(i, 0, curvesFbo.getWidth(), 0.0, 1.0)), 0.0, 1.0, 0, 500));
            }
            break;
        case 2:
            for(int i = 0; i<curvesFbo.getWidth(); i++){
                
                curvesLine.addVertex(i, curvesFbo.getWidth() - ofMap(quarticEaseIn(ofMap(i, 0, curvesFbo.getWidth(), 0.0, 1.0)), 0.0, 1.0, 0, 500));
            }
            break;
        case 3:
            for(int i = 0; i<curvesFbo.getWidth(); i++){
                curvesLine.addVertex(i, curvesFbo.getWidth() - ofMap(exponentialEaseIn(ofMap(i, 0, curvesFbo.getWidth(), 0.0, 1.0)), 0.0, 1.0, 0, 500));
            }
            break;
    }
}

void ofApp::curveTypeChanged(int &curve){
    // this is a callback from the GUI, it is run if the curves slector is changed from the GUI and it genertes anew curve preview
    generateCurvedLine();
}


void ofApp::drawCurvesDisplay(int x, int y, int width, int height){
    //just draws the curves dispaly on screen, in position
    curvesFbo.begin();
    ofClear(0,0,0);
    ofPushStyle();
    ofSetColor(255, 0, 0);
    ofSetLineWidth(6.0);
    curvesLine.draw();
    curvesFbo.end();
    curvesFbo.draw(x,y,width,height);
    ofPushStyle();
    ofNoFill();
    ofSetColor(0, 0, 255);
    ofDrawRectangle(simulationArea);
    ofSetColor(255);
    ofDrawBitmapString(curvesNames[curveSelector], simulationArea.x + 5, simulationArea.y + 15);
    
    ofPopStyle();
}
void ofApp::drawCurvesDisplaySimulationSingle(int x, int y, int width, int height){
    // not implemented yet, should show a white ball at the current position on the curve during simulation fo single mode
    curvesFbo.begin();
    ofClear(0,0,0);
    ofPushStyle();
    ofSetColor(255, 0, 0);
    ofSetLineWidth(6.0);
    ofSetColor(255);
    curvesLine.draw();
    
    switch (curveSelector){
        case 0:
            ofDrawCircle(ofMap(x, 0, 1.0, 500, 0), ofMap(x, 0, 1.0, 500, 0), 15);
            break;
        case 1:
            ofDrawCircle(ofMap(x, 0, 1.0, 500, 0), ofMap(x, 0, 1.0, 500, 0), 15);
            
            //normalisedA2DValues[j]  = sqrt(normalisedA2DValues[j] );
            ofDrawCircle(x, y, 15);
            break;
        case 2:
            ofDrawCircle(ofMap(x, 0, 1.0, 500, 0), ofMap(x, 0, 1.0, 500, 0), 15);
            
            //normalisedA2DValues[j]  = quarticEaseIn(normalisedA2DValues[j] );
            ofDrawCircle(x, y, 15);
            break;
        case 3:
            ///ofDrawCircle(ofMap(x, 0, 1.0, 500, 0), ofMap(x, 0, 1.0, 500, 0), 15);
            break;
    }
    
    curvesFbo.end();
    curvesFbo.draw(x,y,width,height);
    ofPushStyle();
    ofNoFill();
    ofSetColor(0, 0, 255);
    ofDrawRectangle(simulationArea);
    ofSetColor(255);
    ofDrawBitmapString(curvesNames[curveSelector], simulationArea.x + 5, simulationArea.y + 15);
    
    ofPopStyle();
}
void ofApp::drawCurvesDisplaySimulationMulti(int x, int y, int width, int height){
    // not implemented yet, should show a white ball at the current position on the curve during simulation fo multi mode
    curvesFbo.begin();
    ofClear(0,0,0);
    ofPushStyle();
    ofSetColor(255, 0, 0);
    ofSetLineWidth(6.0);
    curvesLine.draw();
    ofSetColor(255);
    ofDrawCircle(x, y, 15);
    
    
    curvesFbo.end();
    curvesFbo.draw(x,y,width,height);
    ofPushStyle();
    ofNoFill();
    ofSetColor(0, 0, 255);
    ofDrawRectangle(simulationArea);
    ofSetColor(255);
    ofDrawBitmapString(curvesNames[curveSelector], simulationArea.x + 5, simulationArea.y + 15);
    
    ofPopStyle();
}
#endif


void ofApp::loadEffectPatchSettings()
{
    // read effects settings from JSON adn see whcih effects each slot needs, each slot has an effects settins struct to hod the data
    if (!firstRun) {
        for (int i = 0; i < numberOfSlots; i++)
        {
            effBitCrushersParams[i].DoClear();
            effDecimatorsParams[i].DoClear();
            effChorussParams[i].DoClear();
            effFiltersParams[i].DoClear();
            effDelaysParams[i].DoClear();
            effReverbsParams[i].DoClear();
            effCompressorsParams[i].DoClear();
            
            slots[i].cloud->disconnectAll();
            slots[i].ampControl->disconnectAll();
            slots[i].outputAmpL->disconnectAll();
            slots[i].outputAmpR->disconnectAll();
            
            ofLogNotice()<< "Removing bitcrusher from slot " + ofToString(i+1) << endl;
            slots[i].bitCrusherLs->disconnectAll();
            slots[i].bitCrusherRs->disconnectAll();
            
            ofLogNotice()<< "Removing decimator from slot " + ofToString(i+1) << endl;
            slots[i].decimatorLs->disconnectAll();
            slots[i].decimatorRs->disconnectAll();
            
            ofLogNotice()<< "Removing chorus from slot " + ofToString(i+1) << endl;
            slots[i].choruss->disconnectAll();
            
            ofLogNotice()<< "Removing filter from slot " + ofToString(i+1) << endl;
            slots[i].multiLadderFilterLs->disconnectAll();
            slots[i].multiLadderFilterRs->disconnectAll();
            
            ofLogNotice()<< "Removing delay from slot " + ofToString(i+1) << endl;
            slots[i].delayLs->disconnectAll();
            slots[i].delayRs->disconnectAll();
            slots[i].delaySends->disconnectAll();
            
            ofLogNotice()<< "Removing reverb from slot " + ofToString(i+1) << endl;
            slots[i].reverbs->disconnectAll();
            slots[i].reverbSends->disconnectAll();
        }
        populateEffectVectors();
    }

    if (firstRun) {
        for (int j = 0; j < NUMBER_OF_PRESETS; j++)
        {
            ofFile effectSettingsFile(filePathPrefix + unitID + "_effectSettings_preset_" + ofToString(j+1)+".json");
            if (!effectSettingsFile.exists()) {
                ofLogNotice() << "Effect settings preset " + ofToString(j+1) + " not loaded" << endl;
            }
            else {
                ofJson effectsPatchJson = ofLoadJson(effectSettingsFile.getAbsolutePath());
                ofLogNotice() << "Effect settings preset " + ofToString(j+1) + " loaded" << endl;

                for (int i = 0; i < numberOfSlots; i++)
                {
                    ofJson slot = effectsPatchJson.value("SLOT_" + ofToString(i + 1), ofJson::object());

                    effectsPatching[j][i].hasBitCrusher = slot.value("BIT_CRUSH", 0);
                    ofLogNotice() << "SLOT_" + ofToString(i + 1) + ":BIT_CRUSH " + ofToString(effectsPatching[j][i].hasBitCrusher) << endl;

                    effectsPatching[j][i].hasDecimator = slot.value("DECIMATOR", 0);
                    ofLogNotice() << "SLOT_" + ofToString(i + 1) + ":DECIMATOR " + ofToString(effectsPatching[j][i].hasDecimator) << endl;

                    effectsPatching[j][i].hasChorus = slot.value("CHORUS", 0);
                    ofLogNotice() << "SLOT_" + ofToString(i + 1) + ":CHORUS " + ofToString(effectsPatching[j][i].hasChorus) << endl;

                    effectsPatching[j][i].hasFilter = slot.value("FILTER", 0);
                    ofLogNotice() << "SLOT_" + ofToString(i + 1) + ":FILTER " + ofToString(effectsPatching[j][i].hasFilter) << endl;

                    effectsPatching[j][i].hasDelay = slot.value("DELAY", 0);
                    ofLogNotice() << "SLOT_" + ofToString(i + 1) + ":DELAY " + ofToString(effectsPatching[j][i].hasDelay) << endl;

                    effectsPatching[j][i].hasReverb = slot.value("REVERB", 0);
                    ofLogNotice() << "SLOT_" + ofToString(i + 1) + ":REVERB " + ofToString(effectsPatching[j][i].hasReverb) << endl;
                }
            }
        }
    }
    
    //create and add the parameters for each slot, not finished or implemented
    
    for (int j = 0; j < numberOfSlots; j++)
    {
        
        effectsPanels[j]->clear();
        
        
        effectsPanels[j]->setup("Effects Slot "+ ofToString(j+1),filePathPrefix + unitID + "_effectParameterSettings_preset_" + ofToString(presetIndex) + ".xml");
        
        if(effectsPatching[presetIndex-1][j].hasBitCrusher){
            effBitCrushersParams[j].setup();
            ofLogNotice()<< "adding bit crusher to slot " + ofToString(j+1) << endl;
            effBitCrushersParams[j].setParameterGroupName("Bitcrusher slot " + ofToString(j+1));
            effectsPanels[j]->add(effBitCrushersParams[j].getParamGroup());
        }
        
        if(effectsPatching[presetIndex-1][j].hasDecimator){
            effDecimatorsParams[j].setup();
            ofLogNotice()<< "adding decimator to slot " + ofToString(j+1) << endl;
            effDecimatorsParams[j].setParameterGroupName("Decimator slot " + ofToString(j+1));
            effectsPanels[j]->add(effDecimatorsParams[j].getParamGroup());
        }
        if(effectsPatching[presetIndex-1][j].hasChorus){
            effChorussParams[j].setup();
            ofLogNotice()<< "adding chorus to slot " + ofToString(j) << endl;
            effChorussParams[j].setParameterGroupName("Chorus slot " + ofToString(j+1));
            effectsPanels[j]->add(effChorussParams[j].getParamGroup());
        }
        if(effectsPatching[presetIndex-1][j].hasFilter){
            effFiltersParams[j].setup();
            ofLogNotice()<< "adding filter to slot " + ofToString(j) << endl;
            effFiltersParams[j].setParameterGroupName("Filter slot " + ofToString(j+1));
            effectsPanels[j]->add(effFiltersParams[j].getParamGroup());
        }
        if(effectsPatching[presetIndex-1][j].hasDelay){
            effDelaysParams[j].setup();
            ofLogNotice()<< "adding delay to slot " + ofToString(j) << endl;
            effDelaysParams[j].setParameterGroupName("Delay slot " + ofToString(j+1));
            effectsPanels[j]->add(effDelaysParams[j].getParamGroup());
        }
        if(effectsPatching[presetIndex-1][j].hasReverb){
            effReverbsParams[j].setup();
            ofLogNotice()<< "adding reverb to slot " + ofToString(j) << endl;
            effReverbsParams[j].setParameterGroupName("Reverb slot " + ofToString(j+1));
            effectsPanels[j]->add(effReverbsParams[j].getParamGroup());
        }
        
        effCompressorsParams[j].setup();
        ofLogNotice()<< "adding compressor to slot " + ofToString(j) << endl;
        effCompressorsParams[j].setParameterGroupName("Compressor slot " + ofToString(j+1));
        effectsPanels[j]->add(effCompressorsParams[j].getParamGroup());
        
        
        effectsPanels[j]->loadFromFile(filePathPrefix + unitID + "_effectParameterSettings_preset_" + ofToString(presetIndex) + ".xml");
        
        
        
        
    }
    
    // Each insert effect (bitcrush/decimator/chorus/filter) is a Patchable
    // ofxPDSP unit, and Patchable >> Patchable works generically for any of
    // them, so instead of hand patching every possible combination of six
    // on/off flags (previously ~30 near-duplicate if/else branches, several
    // with copy/paste channel bugs) we walk the enabled inserts in a fixed
    // order and build the graph for each slot/channel from that.
    for (int e = 0; e < numberOfSlots; e++)
    {
        const ChannelEffects & fx = effectsPatching[presetIndex-1][e];
        bool hasAnyEffect = fx.hasBitCrusher || fx.hasDecimator || fx.hasChorus || fx.hasFilter || fx.hasDelay || fx.hasReverb;
        ofLogNotice() << "Patching chain " + string(hasAnyEffect ? "with effects" : "with no effects") << endl;

        for (int ch = 0; ch < 2; ch++)
        {
            pdsp::Amp * outputAmp = (ch == 0) ? slots[e].outputAmpL.get() : slots[e].outputAmpR.get();

            // fixed insert order: bitcrush -> decimator -> chorus -> filter
            pdsp::Patchable * node = &(slots[e].cloud->ch(ch) >> slots[e].ampControl->ch(ch));

            if (fx.hasBitCrusher) {
                pdsp::Bitcruncher * bitCrusher = (ch == 0) ? slots[e].bitCrusherLs.get() : slots[e].bitCrusherRs.get();
                *node >> *bitCrusher;
                node = bitCrusher;
            }
            if (fx.hasDecimator) {
                pdsp::Decimator * decimator = (ch == 0) ? slots[e].decimatorLs.get() : slots[e].decimatorRs.get();
                *node >> *decimator;
                node = decimator;
            }
            if (fx.hasChorus) {
                pdsp::Patchable & chorus = slots[e].choruss->ch(ch);
                *node >> chorus;
                node = &chorus;
            }
            if (fx.hasFilter) {
                pdsp::MultiLadder4 * filter = (ch == 0) ? slots[e].multiLadderFilterLs.get() : slots[e].multiLadderFilterRs.get();
                *node >> *filter;
                node = filter;
            }

            // dry path is always connected, delay/reverb are parallel sends off the same insert-chain output
            *node >> outputAmp->out_signal() >> slots[e].compressors->ch(ch) >> engine.audio_out(ch);

            if (fx.hasDelay) {
                pdsp::Delay * delay = (ch == 0) ? slots[e].delayLs.get() : slots[e].delayRs.get();
                *node >> *slots[e].delaySends >> *delay >> outputAmp->out_signal() >> slots[e].compressors->ch(ch) >> engine.audio_out(ch);
            }
            if (fx.hasReverb) {
                *node >> *slots[e].reverbSends >> slots[e].reverbs->ch(ch) >> outputAmp->out_signal() >> slots[e].compressors->ch(ch) >> engine.audio_out(ch);
            }
        }
    }
}


void ofApp::exit() {
    ofLogNotice() << "Starting the EXIT function" << endl;

    // this is called when the app exits, we unload the audio files
    for (int i = 0; i < numberOfSlots; i++) {
        slots[i].grainVoices = slots[i].cloud->getVoicesNum();
        if (slots[i].sampleData->loaded())
        {
            slots[i].ampControl->set(0.0f);
            slots[i].sampleData->unLoad();
        }
    }
    
#ifdef HAS_ADC
    // on exit we turn our lights, off, turn off the speaker and if it is set on the XML sutdown the raspberry pi
    blueLed.setval_gpio("0");
    redLed.setval_gpio("0");
    relayOut.setval_gpio("1");
    ofSleepMillis(400);
    relayOut.setval_gpio("0");
    ofLogNotice() << "Speaker turned off" << endl;
    
    if (doShutdown) {
        string cmd = "sudo shutdown -h now";           // create the command
        ofLogNotice() << "Sending system shutown command" << endl;

        ofSystem(cmd.c_str());
    }
#endif
    ofLogNotice() << "Completing the EXIT function" << endl;

}

void ofApp::initParameters()
{
    //gettting some simple stuff ready for running
    firstRun = true;
    presetIndex = 1;
    narrationIsPlaying = false;
    presetSwitchTimer = ofGetElapsedTimeMillis();
    
    
    
#ifndef HAS_ADC
    windowTypeNames[0]="Rectangular";
    windowTypeNames[1]= "Triangular";
    windowTypeNames[2]= "Hann";
    windowTypeNames[3]= "Hamming";
    windowTypeNames[4]= "Blackman";
    windowTypeNames[5]= "BlackmanHarris";
    windowTypeNames[6]= "SineWindow";
    windowTypeNames[7]= "Welch";
    windowTypeNames[8]= "Gaussian";
    windowTypeNames[9]= "Tukey";
    
    curvesNames[0] = "Direct";
    curvesNames[1] = "Power of two";
    curvesNames[2] = "Quartic";
    curvesNames[3] = "Exponential";
    simulationNotCurve =true;
#endif
    timeUpdateFromKey=false;
}

// so far init mode is not used or implemented
void ofApp::settupInitMode()
{
}

void ofApp::updateInitMode()
{
}

void ofApp::drawInitMode()
{
}

void ofApp::exitInitMode()
{
}

void ofApp::setupWaitForNarrMode()
{
    // each mode has a setup method called when we switch to that mode, only the laptop version uses a screen so only that has the setup buttons
    ofLogNotice() << "setup wait for narration mode" << endl;
#ifndef HAS_ADC
    enableModeButtons(waitForNarrButtons, totalButtonsModeWaitForNarr);
#endif
    
}

void ofApp::updateWaitForNarrMode()
{
    
    // if it is a raspbery pi we are waiting either for an amount of time to pass, or for a sensor to be pushed past the threshold, when that happens we switch modes to playing narration
#ifdef HAS_ADC
    deviceOnlyUpdateRoutine(); // this polls all the electronics on the Raspberry pi
    if (hasNarration && shouldTriggerNarrationPlay && narrationUsesSensor)
    {
        for (int i = 0; i < NUMBER_OF_SENSORS; i++)
        {
            if (normalisedA2DValues[i]>narrationGlitchThreshold)
            {
                narration.play();
                shouldTriggerNarrationPlay = false;
                narrationIsPlaying = true;
                i = NUMBER_OF_SENSORS;
                ofLogNotice() << "Triggered Narration play" << endl;
                goToMode(OP_MODE_PLAY_NARRATION);
            }
        }
    }
#endif
    
    if (ofGetElapsedTimeMillis()>5000 && !narrationUsesSensor)
    {
        narration.play();
        shouldTriggerNarrationPlay = false;
        narrationIsPlaying = true;
        ofLogNotice() << "Triggered Narration play" << endl;
        goToMode(OP_MODE_PLAY_NARRATION);
    }
}

#ifndef HAS_ADC
// we just draw the waveforom and wait
void ofApp::drawWaitForNarrMode()
{
    ofPushStyle();
    ofSetColor(ofColor(0, 255, 255));
    ofNoFill();
    ofSetRectMode(OF_RECTMODE_CORNER);
    ofDrawRectangle(narrUiX, narrUiY, narrUiWidth, narrUiHeigth);
    narrWaveformGraphics.draw(narrUiX, narrUiY);
    ofPopStyle();
    drawModeButtons(waitForNarrButtons, totalButtonsModeWaitForNarr);
    
}
#endif

void ofApp::exitWaitForNarrMode()
{
    // when we exit a mode we disale the buttons
#ifndef HAS_ADC
    disableModeButtons(waitForNarrButtons, totalButtonsModeWaitForNarr);
#endif
}

#ifndef HAS_ADC

// every mod ehas its own createButtons method (these are called in the initial setup, as I need to give them unique names I made this shitty way to do it, but it looks way to big to do what it does.
void ofApp::createWaitForNarrModeButtons()
{
    for (int i = 0; i < totalButtonsModeWaitForNarr; i++)
    {
        switch (i)
        {
            case 0:
                waitForNarrButtons[i].setup("Play", BTN_MSG_M_OP_MODE_PLAY_NARRATION);
                waitForNarrButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                waitForNarrButtons[i].disableAllEvents();
                break;
            case 1:
                waitForNarrButtons[i].setup("Run Simulation", BTN_MSG_M_OP_MODE_SIMULATION);
                waitForNarrButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                waitForNarrButtons[i].disableAllEvents();
                break;
            case 2:
                waitForNarrButtons[i].setup("Play Narration", BTN_MSG_M_OP_MODE_PLAY_NARRATION);
                waitForNarrButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                waitForNarrButtons[i].disableAllEvents();
                break;
            case 3:
                waitForNarrButtons[i].setup("Program Narration", BTN_MSG_M_OP_MODE_NARRATION_GUI);
                waitForNarrButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                waitForNarrButtons[i].disableAllEvents();
                break;
            case 4:
                waitForNarrButtons[i].setup("Program Grain", BTN_MSG_M_OP_MODE_GRAIN_MODE);
                waitForNarrButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                waitForNarrButtons[i].disableAllEvents();
                break;
            case 5:
                waitForNarrButtons[i].setup("Switch Presets", BTN_MSG_M_OP_MODE_SWITCH_PRESETS);
                waitForNarrButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                waitForNarrButtons[i].disableAllEvents();
                break;
        }
    }
}
#endif
void ofApp::setupPlayNarrMode()
{
    //again a setup method for a mode, this time playing narration, it is like this so I can come and go to that mode no matter where I am in the software
    ofLogNotice() << "setup play narration mode" << endl;
    if (narrationIsPlaying)
    {
        if (narration.getIsPaused())
        {
            narration.pause(false);
            ofLogNotice() << "Unpausing narration" << endl;
        }
    }
    if (!narrationIsPlaying && !narration.getIsPaused())
    {
        narration.play();
        shouldTriggerNarrationPlay = false;
        narrationIsPlaying = true;
        ofLogNotice() << "Triggered Narration play" << endl;
    }
#ifndef HAS_ADC
    enableModeButtons(playNarrationButtons, totalButtonsModePlayNarration);
#endif
}

void ofApp::updatePlayNarrMode()
{
    // if the narration is finished (I dont have a real callback) we need to switch modes
    if (narration.getPosition()>0.998)
    {
        narrationIsPlaying = false;
        ofLogNotice() << "Narration is over setting up granulars" << endl;
        narrAmpControl.set(0.0f);
        
#ifndef HAS_ADC
        goToMode(grainOperationModeTranslate);
#endif
#ifdef HAS_ADC
        narration.disconnectAll();
        if (presetIndex ==1) {
            blueLed.setval_gpio("1");
            redLed.setval_gpio("0");
        }
        if (presetIndex == 2) {
            blueLed.setval_gpio("1");
            redLed.setval_gpio("1");
        }
        if (presetIndex ==3) {
            blueLed.setval_gpio("1");
            redLed.setval_gpio("0");
        }
        if (presetIndex ==4) {
            blueLed.setval_gpio("1");
            redLed.setval_gpio("1");
        }
        goToMode(grainOperationModeTranslate);
        
#endif
        hasNarration = false;
    }
    
#ifdef HAS_ADC
    // poll the device
    
    deviceOnlyUpdateRoutine();
    
    // if the narration is playing and a sensor is pushed it takes us to the narration granular (glitch) mode. It pauses the narration and uses that position as the base for the playhead position in the narration granualr
    for (int k = 0; k < NUMBER_OF_SENSORS; k++) {
        if (normalisedA2DValues[k] > narrationGlitchThreshold && !hasGlitchSource)
        {
            ofLogNotice() << "Sensor " + ofToString(k) + " is triggered doing narration glitch with a value of " + ofToString(normalisedA2DValues[k]) << endl;
            narrrationGlitchSensor = k;
            doNarrationGlitch = true;
            narration.pause(true);
            narrAmpControl.set(1.0);
            hasGlitchSource = true;
            k = NUMBER_OF_SENSORS;
            goToMode(OP_MODE_NARRATION_GLITCH);
            
        }
    }
#endif
}

#ifndef HAS_ADC
void ofApp::drawPlayNarrMode()
{
    //just shows us a waveform and prgress playead line
    ofPushStyle();
    ofSetColor(ofColor(0, 255, 255));
    ofNoFill();
    ofSetRectMode(OF_RECTMODE_CORNER);
    ofDrawRectangle(narrUiX, narrUiY, narrUiWidth, narrUiHeigth);
    narrWaveformGraphics.draw(narrUiX, narrUiY);
    ofSetColor(255);
    ofDrawLine(ofMap(narration.getPosition(), 0.0, 1.0, narrUiX, narrUiMaxX), narrUiY, ofMap(narration.getPosition(), 0.0, 1.0, narrUiX, narrUiMaxX), narrUiMaxY);
    ofPopStyle();
    drawModeButtons(playNarrationButtons, totalButtonsModePlayNarration);
    
}
#endif

void ofApp::exitPlayNarrMode()
{
#ifndef HAS_ADC
    disableModeButtons(playNarrationButtons, totalButtonsModePlayNarration);
#endif
    if (narrationIsPlaying)
    {
        if (!narration.getIsPaused())
        {
            narration.pause(true);
            ofLogNotice() << "Pausing narration" << endl;
        }
    }
}

#ifndef HAS_ADC
void ofApp::createPlayNarrationModeButtons()
{
    // another button setup
    for (int i = 0; i < totalButtonsModePlayNarration; i++)
    {
        switch (i)
        {
            case 0:
                playNarrationButtons[i].setup("Toggle Glitch", BTN_MSG_M_OP_MODE_NARRATION_GLITCH);
                playNarrationButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                playNarrationButtons[i].disableAllEvents();
                break;
            case 1:
                playNarrationButtons[i].setup("Restart Narration", BTN_MSG_A_RESTART_NARRATION);
                playNarrationButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                playNarrationButtons[i].disableAllEvents();
                break;
            case 2:
                playNarrationButtons[i].setup("Pause Narration", BTN_MSG_A_PAUSE_NARRATION);
                playNarrationButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                playNarrationButtons[i].disableAllEvents();
                break;
            case 3:
                playNarrationButtons[i].setup("Program Narration", BTN_MSG_M_OP_MODE_NARRATION_GUI);
                playNarrationButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                playNarrationButtons[i].disableAllEvents();
                break;
            case 4:
                playNarrationButtons[i].setup("Program Grain", BTN_MSG_M_OP_MODE_GRAIN_MODE);
                playNarrationButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                playNarrationButtons[i].disableAllEvents();
                break;
            case 5:
                playNarrationButtons[i].setup("Switch Presets", BTN_MSG_M_OP_MODE_SWITCH_PRESETS);
                playNarrationButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                playNarrationButtons[i].disableAllEvents();
                break;
        }
    }
}
#endif

void ofApp::setupSwitchPresetsMode()
{
    // This does not really have to be a mode but I kept the pattern, here I can switch from one preset to another, the preset contains files to be loaded to the granualr and settings to be loaded that define how the granualr reacts to the sensor data.
    ofLogNotice() << "setup switch presets" << endl;
#ifndef HAS_ADC
    enableModeButtons(switchPresetsButtons, totalButtonsModSwitchPresets);
#endif
    
    switchPresets();
    goToMode(grainOperationModeTranslate);
    
}

void ofApp::updateSwitchPresetsMode()
{
}

#ifndef HAS_ADC
void ofApp::drawSwitchPresetsMode()
{
    drawModeButtons(switchPresetsButtons, totalButtonsModSwitchPresets);
    
}
#endif

void ofApp::exitSwitchPresetsMode()
{
#ifndef HAS_ADC
    disableModeButtons(switchPresetsButtons, totalButtonsModSwitchPresets);
#endif
    ofLogNotice() << "exit switch presets" << endl;
    
}


#ifndef HAS_ADC
void ofApp::createSwitchPresetsModeButtons()
{
    
    for (int i = 0; i < totalButtonsModSwitchPresets; i++)
    {
        switch (i)
        {
            case 0:
                switchPresetsButtons[i].setup("Save", BTN_MSG_A_SAVE_GRANULAR);
                switchPresetsButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                switchPresetsButtons[i].disableAllEvents();
                break;
            case 1:
                switchPresetsButtons[i].setup("Run Simulation", BTN_MSG_M_OP_MODE_SIMULATION);
                switchPresetsButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                switchPresetsButtons[i].disableAllEvents();
                break;
            case 2:
                switchPresetsButtons[i].setup("Play Narration", BTN_MSG_M_OP_MODE_PLAY_NARRATION);
                switchPresetsButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                switchPresetsButtons[i].disableAllEvents();
                break;
            case 3:
                switchPresetsButtons[i].setup("Program Narration", BTN_MSG_M_OP_MODE_NARRATION_GUI);
                switchPresetsButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                switchPresetsButtons[i].disableAllEvents();
                break;
            case 4:
                switchPresetsButtons[i].setup("Program Grain", BTN_MSG_M_OP_MODE_GRAIN_MODE);
                switchPresetsButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                switchPresetsButtons[i].disableAllEvents();
                break;
            case 5:
                switchPresetsButtons[i].setup("Switch Presets", BTN_MSG_M_OP_MODE_SWITCH_PRESETS);
                switchPresetsButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                switchPresetsButtons[i].disableAllEvents();
                break;
        }
    }
}
#endif

void ofApp::setupMultiGrainMode()
{
    ofLogNotice() << "setup multi mode" << endl;
#ifndef HAS_ADC
    enableModeButtons(multiGrainModeButtons, totalButtonsModeMultiGrain);
#endif
}

void ofApp::updateMultiGrainMode() {
    
#ifdef HAS_ADC
    deviceOnlyUpdateRoutine();
#endif // HAS_ADC
    // just need to check if it is the first time, to know what we have to setup if we change presets
    if (firstRun)
    {
        firstRun = false;
    }
    
    // as long as we are not pressing the mouse lets see what the parameters should be
    if (!ofGetMousePressed())
    {
        updateParametersFromValuesMulti();
        
    }
    updateEffects();
}
#ifndef HAS_ADC
void ofApp::mapSimulatedDataSingle()
{
    // in signle grain mode we need to fake our ADC input, if the sensor is part of the simulation we give it a value, we then map that value back to the range we want as input
    for (int j = 0; j < NUMBER_OF_SENSORS; j++) {
        if (sensorSimActive[j]) {
            a2dVal[j] = simulatedInput;
            normalisedA2DValues[j] = ofMap(a2dVal[j], 0, 1024, 0.0, 1.0, true);
            compressionSims[j] = normalisedA2DValues[j];
        }
    }
    
    // we need to apply the parameters from the simulated sensor data to each of the granulars
    for (int s = 0; s < numberOfSlots; s++)
    {
        controlX[s] = ofMap(accumulatedPressureNormalised, 0.0, 1.0, uiX[s], uiMaxX[s], true);
        controlY[s] = ofMap(_volume[s], 0.0, 1.0, uiMaxY[s], uiY[s]);
        accumulatedPressureNormalised >> slots[s].cloud->in_position();
        _spread[s] >> slots[s].cloud->in_position_jitter();
        _in_length[s] >> slots[s].cloud->in_length();
        _in_density[s] >> slots[s].cloud->in_density();
        _in_distance_jitter[s] >> slots[s].cloud->in_distance_jitter();
        _in_pitch_jitter[s] >> slots[s].cloud->in_pitch_jitter();
        _grainDirection[s] >> slots[s].cloud->in_direction();
        _in_pitch[s] >> slots[s].cloud->in_pitch();
    }
}

void ofApp::mapSimulatedDataMulti()
{
    // in multi mode the data is different, again we map the simulation instead of the incomg data from the ADC, but here we apply the curve to the data as well
    for (int j = 0; j < NUMBER_OF_SENSORS; j++) {
        if (sensorSimActive[j]) {
            a2dVal[j] = simulatedInput;
            normalisedA2DValues[j] = ofMap(a2dVal[j], 0, 1024, 0.0, 1.0);
            switch (curveSelector){
                case 0:
                    break;
                case 1:
                    normalisedA2DValues[j]  = sqrt(normalisedA2DValues[j] );
                    break;
                case 2:
                    normalisedA2DValues[j]  = quarticEaseIn(normalisedA2DValues[j] );
                    break;
                case 3:
                    normalisedA2DValues[j]  = exponentialEaseIn(normalisedA2DValues[j] );
                    break;
            }
            compressionSims[j] = normalisedA2DValues[j];
        }
    }
    
    // we need to apply the parameters from the simulated sensor data to each of the granulars
    
    for (int s = 0; s < numberOfSlots; s++)
    {
        controlX[s] = ofMap(_posX[s], 0.0, 1.0, uiX[s], uiMaxX[s]);
        controlY[s] = ofMap(_volume[s], 0.0, 1.0, uiMaxY[s], uiY[s]);
        _posX[s] >> slots[s].cloud->in_position();
        _spread[s] >> slots[s].cloud->in_position_jitter();
        _in_length[s] >> slots[s].cloud->in_length();
        _in_density[s] >> slots[s].cloud->in_density();
        _in_distance_jitter[s] >> slots[s].cloud->in_distance_jitter();
        _in_pitch_jitter[s] >> slots[s].cloud->in_pitch_jitter();
        _grainDirection[s] >> slots[s].cloud->in_direction();
        _in_pitch[s] >> slots[s].cloud->in_pitch();
        
    }
}

void ofApp::generateSimulatedData()
{
    // we generate simulated data, it is int based and can have a max limit and speed set by the GUI, when the simulation is done we go back to the granular mode I use grainOperationModeTranslate, instead of the granular mode directly as this way the ssytem knows if it is single or multi and takes us to and from the crrect simulation or granular mode form any mode.
    if (simulationRising && simulationRunning) {
        simulatedInput += compressionSimSpeed;
        
        if (simulatedInput>compressionSimExtent) {
            simulationRising = false;
        }
    }
    if (!simulationRising && simulationRunning) {
        simulatedInput -= compressionSimSpeed;
        
        if (simulatedInput <= 0) {
            goToMode(grainOperationModeTranslate);
            
        }
    }
}

void ofApp::drawMultiGrainMode()
{
    // we can either draw the curve preview or a simulation of the sensor data
    if(simulationNotCurve){
        drawSimulationBars();
        
    }
    else {
        drawCurvesDisplay(simulationArea.x,simulationArea.y,simulationArea.width,simulationArea.height);
        
    }
    drawGrainClouds();
    mainGui.draw();
    drawModeButtons(multiGrainModeButtons, totalButtonsModeMultiGrain);
    
}

void ofApp::drawMessages()
{
    // we draw the name and preset to the screen
    ofPushStyle();
    ofSetColor(255);
    
    messageFont.drawString("Gaugemancy Control 1.0       User: " + unitID + " Preset: " + ofToString(presetIndex), 40, 60);
    
    ofPopStyle();
}

void ofApp::drawGrainClouds()
{
    //this draws each granular module, with a waveform and display of the individual grains
    ofPushView();
    ofSetLineWidth(1.0f);
    for (int j = 0; j < numberOfSlots; ++j) {
        ofPushStyle();
        ofSetColor(ofColor(0, 255, 255));
        ofNoFill();
        ofSetRectMode(OF_RECTMODE_CORNER);
        ofDrawRectangle(uiX[j], uiY[j], uiWidth[j], uiHeigth[j]);
        
        waveformGraphics[j]->draw(uiX[j], uiY[j]);
        
        ofDrawBitmapString(fileNamesSet[presetIndex-1][j], uiX[j] + 5, uiY[j] + 15);
        
        
        
        if (drawGrains[j]) {
            //draw position crossdraw grains
            
            ofDrawLine(ofMap(positionFromTime, 0, 1, uiX[j], uiMaxX[j]), uiY[j], ofMap(positionFromTime, 0, 1, uiX[j], uiMaxX[j]), uiMaxY[j]);
            ofDrawLine(uiX[j], controlY[j], uiMaxX[j], controlY[j]);
            //draw grains
            ofSetRectMode(OF_RECTMODE_CENTER);
            int grainsY = uiY[j] + uiHeigth[j] / 2;
            for (int k = 0; k < slots[j].grainVoices; ++k) {
                float xpos = uiX[j] + (uiWidth[j] * slots[j].cloud->meter_position(k));
                float dimensionX = slots[j].cloud->meter_env(k) * 10;
                float dimensionY = slots[j].cloud->meter_env(k) * 50;
                ofDrawRectangle(xpos, grainsY, dimensionX, dimensionY);
            }
        }
        ofSetColor(255);
        ofDrawBitmapString("Slot " + ofToString(j + 1), uiX[j] + 5, uiHeigth[j] + uiY[j] - 10);
        ofPopStyle();
        if(drawEffects){
            effectsPanels[j]->draw();
            
        }
        if(!drawEffects){
            samplePanels[j]->draw();
            
        }
    }
    ofPopView();
}

void ofApp::drawSimulationBars()
{
    //this draws the sensor simulation, it is in a 3D environment (you can rotate and zoom on the preview with a mouse, inside the interaction area rectangle
    ofBackground(0);
    cam.setDistance(camDistance);
    simulationArea.set(ofRectangle(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * totalButtonsModeSimulationAccu, BUTTON_WIDTH, BUTTON_WIDTH));
    cam.setControlArea(simulationArea);
    
    ofPushStyle();
    barsFbo.begin();
    ofPushView();
    ofEnableDepthTest();
    ofClear(0, 0, 0);
    cam.begin();
    for (int i = 0; i < NUMBER_OF_SENSORS; i++) {
        ofSetColor(255);
        barsFull[i].draw();
    }
    
    for (int i = 0; i < NUMBER_OF_SENSORS; i++) {
        ofSetColor(255, 0, 0);
        barsEmpty[i].draw();
    }
    cam.end();
    ofPopView();
    ofDisableDepthTest();
    barsFbo.end();
    ofPopStyle();
    
    ofPushStyle();
    barsFbo.draw(simulationArea);
    ofPopStyle();
    
    ofPushStyle();
    ofSetColor(0, 0, 255);
    ofNoFill();
    ofDrawRectangle(simulationArea);
    ofPopStyle();
    
    
}
#endif

void ofApp::exitMultiGrainMode()
{
    ofLogNotice() << "exit multi mode" << endl;
#ifndef HAS_ADC
    
    disableModeButtons(multiGrainModeButtons, totalButtonsModeMultiGrain);
#endif
}

#ifndef HAS_ADC
void ofApp::createMultiGrainModeButtons()
{
    for (int i = 0; i < totalButtonsModeMultiGrain; i++)
    {
        switch (i)
        {
            case 0:
                multiGrainModeButtons[i].setup("Save", BTN_MSG_A_SAVE_GRANULAR);
                multiGrainModeButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                multiGrainModeButtons[i].disableAllEvents();
                break;
            case 1:
                multiGrainModeButtons[i].setup("Run Simulation", BTN_MSG_M_OP_MODE_SIMULATION);
                multiGrainModeButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                multiGrainModeButtons[i].disableAllEvents();
                break;
            case 2:
                multiGrainModeButtons[i].setup("Play Narration", BTN_MSG_M_OP_MODE_PLAY_NARRATION);
                multiGrainModeButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                multiGrainModeButtons[i].disableAllEvents();
                break;
            case 3:
                multiGrainModeButtons[i].setup("Effects or Granular", BTN_MSG_A_EFFECTS_OR_GRANULAR);
                multiGrainModeButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                multiGrainModeButtons[i].disableAllEvents();
                break;
            case 4:
                multiGrainModeButtons[i].setup("Simulation/Curve", BTN_MSG_A_CURVES_OR_SIMULATION_DISPLAY);
                multiGrainModeButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                multiGrainModeButtons[i].disableAllEvents();
                break;
            case 5:
                multiGrainModeButtons[i].setup("Switch Presets", BTN_MSG_M_OP_MODE_SWITCH_PRESETS);
                multiGrainModeButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                multiGrainModeButtons[i].disableAllEvents();
                break;
        }
    }
}
#endif

void ofApp::setupSingleGrainMode()
{
    ofLogNotice() << "setup single grain mode" << endl;
#ifndef HAS_ADC
    enableModeButtons(SingleGrainModeButtons, totalButtonsModeSingleGrain);
#endif
}

void ofApp::updateSingleGrainMode()
{
#ifdef HAS_ADC
    deviceOnlyUpdateRoutine();
#endif // HAS_ADC
    
    if (firstRun)
    {
        firstRun = false;
    }
    
    if (!ofGetMousePressed())
    {
        updateParametersFromValuesSingle();
        
    }
    updateEffects();
}

#ifndef HAS_ADC
void ofApp::drawSingleGrainMode()
{
    if(simulationNotCurve){
        drawSimulationBars();
        
    }
    else {
        drawCurvesDisplay(simulationArea.x,simulationArea.y,simulationArea.width,simulationArea.height);
        
    }
    drawGrainClouds();
    mainGui.draw();
    drawModeButtons(SingleGrainModeButtons, totalButtonsModeSingleGrain);
    
}
#endif

void ofApp::exitSingleGrainMode()
{
    ofLogNotice() << "exit Single mode" << endl;
#ifndef HAS_ADC
    disableModeButtons(SingleGrainModeButtons, totalButtonsModeSingleGrain);
#endif
}

#ifndef HAS_ADC
void ofApp::createSingleGrainModeModeButtons()
{
    for (int i = 0; i < totalButtonsModeSingleGrain; i++)
    {
        switch (i)
        {
            case 0:
                SingleGrainModeButtons[i].setup("Save", BTN_MSG_A_SAVE_GRANULAR);
                SingleGrainModeButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                SingleGrainModeButtons[i].disableAllEvents();
                break;
            case 1:
                SingleGrainModeButtons[i].setup("Run Simulation", BTN_MSG_M_OP_MODE_SIMULATION);
                SingleGrainModeButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                SingleGrainModeButtons[i].disableAllEvents();
                break;
            case 2:
                SingleGrainModeButtons[i].setup("Play Narration", BTN_MSG_M_OP_MODE_PLAY_NARRATION);
                SingleGrainModeButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                SingleGrainModeButtons[i].disableAllEvents();
                break;
            case 3:
                SingleGrainModeButtons[i].setup("Effects or Granular", BTN_MSG_A_EFFECTS_OR_GRANULAR);
                SingleGrainModeButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                SingleGrainModeButtons[i].disableAllEvents();
                break;
            case 4:
                SingleGrainModeButtons[i].setup("Simulation/Curve", BTN_MSG_A_CURVES_OR_SIMULATION_DISPLAY);
                SingleGrainModeButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                SingleGrainModeButtons[i].disableAllEvents();
                break;
            case 5:
                SingleGrainModeButtons[i].setup("Switch Presets", BTN_MSG_M_OP_MODE_SWITCH_PRESETS);
                SingleGrainModeButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                SingleGrainModeButtons[i].disableAllEvents();
                break;
        }
    }
}



void ofApp::setupNarrGuiMode()
{
    ofLogNotice() << "setup narration gui mode" << endl;
    enableModeButtons(narrGuiModeButtons, totalButtonsModeNarrGui);
}

void ofApp::updateNarrGuiMode()
{
    // when we are programming the narration granular we need to aplpy the settings
    narr_in_position_jitter >> narrCloud.in_position_jitter();
    narr_in_length >> narrCloud.in_length();
    narr_in_density >> narrCloud.in_density();
    narr_in_distance_jitter >> narrCloud.in_distance_jitter();
    narr_in_pitch_jitter >> narrCloud.in_pitch_jitter();
    narr_in_pitch >> narrCloud.in_pitch();
    narr_grainDirection >> narrCloud.in_direction();
    narrCompressorControls._e_compressor_in_gain >> narrOutControlL.in_mod();
    narrCompressorControls._e_compressor_in_gain >> narrOutControlR.in_mod();
    narrCompressorControls._e_compressor_in_attack >> narrOutCompressor.in_attack();
    narrCompressorControls._e_compressor_in_knee >> narrOutCompressor.in_knee();
    narrCompressorControls._e_compressor_in_ratio >> narrOutCompressor.in_ratio();
    narrCompressorControls._e_compressor_in_release >> narrOutCompressor.in_release();
    narrCompressorControls._e_compressor_in_threshold >> narrOutCompressor.in_threshold();
    narrCompressorControls._e_compressor_comp_meter.set(narrOutCompressor.meter_GR());
    
}

void ofApp::drawNarrGuiMode()
{
    // when we are programming the narration granular we need to see the waveform and the GUI
    
    ofPushStyle();
    ofSetColor(ofColor(0, 255, 255));
    ofNoFill();
    ofSetRectMode(OF_RECTMODE_CORNER);
    ofDrawRectangle(narrUiX, narrUiY, narrUiWidth, narrUiHeigth);
    narrWaveformGraphics.draw(narrUiX, narrUiY);
    
    if (narrDrawGrains) {
        ofDrawLine(narrControlX, narrUiY, narrControlX, narrUiMaxY);
        ofDrawLine(narrUiX, narrControlY, narrUiMaxX, narrControlY);
        ofSetRectMode(OF_RECTMODE_CENTER);
        int grainsY = narrUiY + narrUiHeigth / 2;
        for (int k = 0; k < narrGrainVoices; ++k) {
            float xpos = narrUiX + (narrUiWidth * narrCloud.meter_position(k));
            float dimensionX = narrCloud.meter_env(k) * 10;
            float dimensionY = narrCloud.meter_env(k) * 50;
            ofDrawRectangle(xpos, grainsY, dimensionX, dimensionY);
        }
    }
    
    ofPopStyle();
    
    narrPanel.draw();
    drawModeButtons(narrGuiModeButtons, totalButtonsModeNarrGui);
    
}

void ofApp::exitNarrGuiMode()
{
    disableModeButtons(narrGuiModeButtons, totalButtonsModeNarrGui);
}

void ofApp::createNarrGuiModeButtons()
{
    for (int i = 0; i < totalButtonsModeNarrGui; i++)
    {
        switch (i)
        {
            case 0:
                narrGuiModeButtons[i].setup("Save", BTN_MSG_A_SAVE_NARRATION);
                narrGuiModeButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                narrGuiModeButtons[i].disableAllEvents();
                break;
            case 1:
                narrGuiModeButtons[i].setup("Run Simulation", BTN_MSG_M_OP_MODE_SIMULATION);
                narrGuiModeButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                narrGuiModeButtons[i].disableAllEvents();
                break;
            case 2:
                narrGuiModeButtons[i].setup("Play Narration", BTN_MSG_M_OP_MODE_PLAY_NARRATION);
                narrGuiModeButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                narrGuiModeButtons[i].disableAllEvents();
                break;
            case 3:
                narrGuiModeButtons[i].setup("Program Narration", BTN_MSG_M_OP_MODE_NARRATION_GUI);
                narrGuiModeButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                narrGuiModeButtons[i].disableAllEvents();
                break;
            case 4:
                narrGuiModeButtons[i].setup("Program Grain", BTN_MSG_M_OP_MODE_GRAIN_MODE);
                narrGuiModeButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                narrGuiModeButtons[i].disableAllEvents();
                break;
            case 5:
                narrGuiModeButtons[i].setup("Switch Presets", BTN_MSG_M_OP_MODE_SWITCH_PRESETS);
                narrGuiModeButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                narrGuiModeButtons[i].disableAllEvents();
                break;
        }
    }
}


void ofApp::setupSimulationMultiMode()
{
    ofLogNotice() << "setup simulation multi mode" << endl;
    for (int s = 0; s < numberOfSlots; s++)
    {
        drawGrains[s] = true;
    }
    enableModeButtons(simulationMultiButtons, totalButtonsModeSimulationMulti);
}

void ofApp::updateSimulationMultiMode()
{
    
    if (firstRun)
    {
        firstRun = false;
    }
    
    simulationGFXUpdateRoutine();
    
    
    updateParametersFromValuesMulti();
    updateEffectsSimulation();
    
    generateSimulatedData();
    mapSimulatedDataMulti();
    
}

void ofApp::drawSimulationMultiMode()
{
    if(simulationNotCurve){
        drawSimulationBars();
        
    }
    else {
        drawCurvesDisplaySimulationMulti(simulationArea.x,simulationArea.y,simulationArea.width,simulationArea.height);
        
    }
    drawGrainClouds();
    mainGui.draw();
    
    drawModeButtons(simulationMultiButtons, totalButtonsModeSimulationMulti);
}


void ofApp::exitSimulationMultiMode()
{
    ofLogNotice() << "Exit simulation multi mode" << endl;
    
    simulatedInput = 0;
    runSimulation = false;
    simulationRising = true;
    simulationRunning = false;
    disableModeButtons(simulationMultiButtons, totalButtonsModeSimulationMulti);
    resetValuesAfterChanges();
    for (int s = 0; s < numberOfSlots; s++)
    {
        drawGrains[s] = false;
    }
}

void ofApp::createSimulationMultiModeButtons()
{
    for (int i = 0; i < totalButtonsModeSimulationMulti; i++)
    {
        switch (i)
        {
            case 0:
                simulationMultiButtons[i].setup("Save", BTN_MSG_A_SAVE_GRANULAR);
                simulationMultiButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                simulationMultiButtons[i].disableAllEvents();
                break;
            case 1:
                simulationMultiButtons[i].setup("Pause Simulation", BTN_MSG_A_PAUSE_SIMULATION);
                simulationMultiButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                simulationMultiButtons[i].disableAllEvents();
                break;
            case 2:
                simulationMultiButtons[i].setup("Play Narration", BTN_MSG_M_OP_MODE_PLAY_NARRATION);
                simulationMultiButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                simulationMultiButtons[i].disableAllEvents();
                break;
            case 3:
                simulationMultiButtons[i].setup("Effects or Granular", BTN_MSG_A_EFFECTS_OR_GRANULAR);
                simulationMultiButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                simulationMultiButtons[i].disableAllEvents();
                break;
            case 4:
                simulationMultiButtons[i].setup("Simulation/Curve", BTN_MSG_A_CURVES_OR_SIMULATION_DISPLAY);
                simulationMultiButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                simulationMultiButtons[i].disableAllEvents();
                break;
            case 5:
                simulationMultiButtons[i].setup("Switch Presets", 666);
                simulationMultiButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                simulationMultiButtons[i].disableAllEvents();
                break;
        }
    }
}

void ofApp::setupSimulationSingleMode()
{
    ofLogNotice() << "setup simulation single mode" << endl;
    for (int s = 0; s < numberOfSlots; s++)
    {
        drawGrains[s] = true;
    }
    enableModeButtons(simulationSingleButtons, totalButtonsModeSimulationAccu);
}

void ofApp::updateSimulationSingleMode()
{
    drawModeButtons(simulationSingleButtons, totalButtonsModeSimulationAccu);
    
    if (firstRun)
    {
        firstRun = false;
    }
    
    simulationGFXUpdateRoutine();
    
    
    
    
    generateSimulatedData();
    mapSimulatedDataSingle();
    updateParametersFromValuesSingle();
    updateEffectsSimulation();
}

void ofApp::drawSimulationSingleMode()
{
    if(simulationNotCurve){
        drawSimulationBars();
        
    }
    else {
        drawCurvesDisplaySimulationSingle(simulationArea.x,simulationArea.y,simulationArea.width,simulationArea.height);
        
    }
    drawGrainClouds();
    mainGui.draw();
    drawModeButtons(simulationSingleButtons, totalButtonsModeSimulationAccu);
    
}

void ofApp::exitSimulationSingleMode()
{
    ofLogNotice() << "Exit simulation single mode" << endl;
    resetValuesAfterChanges();
    simulatedInput = 0;
    runSimulation = false;
    simulationRising = true;
    simulationRunning = false;
    for (int s = 0; s < numberOfSlots; s++)
    {
        drawGrains[s] = false;
    }
    disableModeButtons(simulationSingleButtons, totalButtonsModeSimulationAccu);
}

void ofApp::createSimulationSingleModeButtons()
{
    for (int i = 0; i < totalButtonsModeSimulationAccu; i++)
    {
        switch (i)
        {
            case 0:
                simulationSingleButtons[i].setup("Save", BTN_MSG_A_SAVE_GRANULAR);
                simulationSingleButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                simulationSingleButtons[i].disableAllEvents();
                break;
            case 1:
                simulationSingleButtons[i].setup("Pause Simulation", BTN_MSG_A_PAUSE_SIMULATION);
                simulationSingleButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                simulationSingleButtons[i].disableAllEvents();
                break;
            case 2:
                simulationSingleButtons[i].setup("Play Narration", BTN_MSG_M_OP_MODE_PLAY_NARRATION);
                simulationSingleButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                simulationSingleButtons[i].disableAllEvents();
                break;
            case 3:
                simulationSingleButtons[i].setup("Effects or Granular", BTN_MSG_A_EFFECTS_OR_GRANULAR);
                simulationSingleButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                simulationSingleButtons[i].disableAllEvents();
                break;
            case 4:
                simulationSingleButtons[i].setup("Simulation/Curve", BTN_MSG_A_CURVES_OR_SIMULATION_DISPLAY);
                simulationSingleButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                simulationSingleButtons[i].disableAllEvents();
                break;
            case 5:
                simulationSingleButtons[i].setup("Switch Presets", 666);
                simulationSingleButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                simulationSingleButtons[i].disableAllEvents();
                break;
        }
    }
}

#endif
void ofApp::setupNarrationGlitchMode()
{
#ifndef HAS_ADC
    enableModeButtons(narrationGlitchButtons, totalButtonsModeNarrationGlitch);
#endif
    narr_in_position_jitter >> narrCloud.in_position_jitter();
    narr_in_length >> narrCloud.in_length();
    narr_in_density >> narrCloud.in_density();
    narr_in_distance_jitter >> narrCloud.in_distance_jitter();
    narr_in_pitch_jitter >> narrCloud.in_pitch_jitter();
    narr_in_pitch >> narrCloud.in_pitch();
    narr_grainDirection >> narrCloud.in_direction();
    narrAmpControl.set(1.0);
    narrCompressorControls._e_compressor_in_gain >> narrOutControlL.in_mod();
    narrCompressorControls._e_compressor_in_gain >> narrOutControlR.in_mod();
    narrCompressorControls._e_compressor_in_attack >> narrOutCompressor.in_attack();
    narrCompressorControls._e_compressor_in_knee >> narrOutCompressor.in_knee();
    narrCompressorControls._e_compressor_in_ratio >> narrOutCompressor.in_ratio();
    narrCompressorControls._e_compressor_in_release >> narrOutCompressor.in_release();
    narrCompressorControls._e_compressor_in_threshold >> narrOutCompressor.in_threshold();
    narrCompressorControls._e_compressor_comp_meter.set(narrOutCompressor.meter_GR());
    ofLogNotice() << "setup narration glitch mode" << endl;
    
}

void ofApp::updateNarrationGlitchMode()
{
    // we get to narration glitch mode on the device by pressing one of the sensors, that sensor value is then used to move the playhead of the narraiton granular. If the sensor values goes below the threshold we go back to the  narration play mode
#ifdef HAS_ADC
    deviceOnlyUpdateRoutine();
    (narration.getPosition() + ofMap(normalisedA2DValues[narrrationGlitchSensor], 0.0, 1.0, -1.0 * narrationGlitchStrand, 0)) >> narrCloud.in_position();
    
    if (normalisedA2DValues[narrrationGlitchSensor] < narrationGlitchThreshold)
    {
        goToMode(OP_MODE_PLAY_NARRATION);
    }
    // if we are on a laptop the sensor data is take from the mouse x position
#else
    (narration.getPosition() + ofMap(narrationGlitchPlayheadPos, 0.0, 1.0, -1.0 * narrationGlitchStrand, 0, true))  >> narrCloud.in_position();
    narrControlX = ofMap(narration.getPosition() + ofMap(narrationGlitchPlayheadPos, 0.0, 1.0, -1.0 * narrationGlitchStrand, 0), 0, 1, narrUiX, narrUiMaxX);
    ofLogNotice() << ofToString(narrControlX) << endl;
#endif
    
    
    
    
}

#ifndef HAS_ADC
void ofApp::drawNarrationGlitchMode()
{
    // again for the glitch, or narration programming mode we need to see the waveform and GUI
    ofPushView();
    ofPushStyle();
    ofSetColor(ofColor(0, 255, 255));
    ofNoFill();
    ofSetRectMode(OF_RECTMODE_CORNER);
    ofDrawRectangle(narrUiX, narrUiY, narrUiWidth, narrUiHeigth);
    narrWaveformGraphics.draw(narrUiX, narrUiY);
    ofSetColor(255);
    ofDrawLine(narrControlX, narrUiY, narrControlX, narrUiMaxY);
    ofSetRectMode(OF_RECTMODE_CENTER);
    int grainsY = narrUiY + narrUiHeigth / 2;
    for (int k = 0; k < narrGrainVoices; ++k) {
        float xpos = narrUiX + (narrUiWidth * narrCloud.meter_position(k));
        float dimensionX = narrCloud.meter_env(k) * 10;
        float dimensionY = narrCloud.meter_env(k) * 50;
        ofSetColor(255);
        ofDrawRectangle(xpos, grainsY, dimensionX, dimensionY);
    }
    ofPopStyle();
    ofPopView();
    drawModeButtons(narrationGlitchButtons, totalButtonsModeNarrationGlitch);
}
#endif

void ofApp::exitNarrationGlitchMode()
{
#ifndef HAS_ADC
    disableModeButtons(narrationGlitchButtons, totalButtonsModeNarrationGlitch);
#endif
    0.0 >> narrCloud.in_position();
    narrAmpControl.set(0.0);
    hasGlitchSource = false;
    
}

#ifndef HAS_ADC
void ofApp::createNarrationGlitchModeButtons()
{
    for (int i = 0; i < totalButtonsModeNarrationGlitch; i++)
    {
        switch (i)
        {
            case 0:
                narrationGlitchButtons[i].setup("Toggle Glitch", BTN_MSG_M_OP_MODE_PLAY_NARRATION);
                narrationGlitchButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                narrationGlitchButtons[i].disableAllEvents();
                break;
            case 1:
                narrationGlitchButtons[i].setup("Run Simulation", BTN_MSG_M_OP_MODE_SIMULATION);
                narrationGlitchButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                narrationGlitchButtons[i].disableAllEvents();
                break;
            case 2:
                narrationGlitchButtons[i].setup("Play Narration", BTN_MSG_M_OP_MODE_PLAY_NARRATION);
                narrationGlitchButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                narrationGlitchButtons[i].disableAllEvents();
                break;
            case 3:
                narrationGlitchButtons[i].setup("Program Narration", BTN_MSG_M_OP_MODE_NARRATION_GUI);
                narrationGlitchButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                narrationGlitchButtons[i].disableAllEvents();
                break;
            case 4:
                narrationGlitchButtons[i].setup("Program Grain", BTN_MSG_M_OP_MODE_GRAIN_MODE);
                narrationGlitchButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                narrationGlitchButtons[i].disableAllEvents();
                break;
            case 5:
                narrationGlitchButtons[i].setup("Switch Presets", BTN_MSG_M_OP_MODE_SWITCH_PRESETS);
                narrationGlitchButtons[i].set(ofGetWidth() - (BUTTON_WIDTH + 10), 40 + 65 * i, BUTTON_WIDTH, BUTTON_HEIGHT);
                narrationGlitchButtons[i].disableAllEvents();
                break;
        }
    }
}
#endif

// This is the method that changes modes, we tell it the mode we want to go to, first it exits the current mode with the exitAnyMode(); method and then changes the operation mode and runs the setup for the new mode
void ofApp::goToMode(int mode)
{
    previousOperationMode = operationMode;
    exitAnyMode();
    operationMode = mode;
    switch (mode)
    {
        case OP_MODE_SETUP:
            settupInitMode();
            break;
        case OP_MODE_WAIT_FOR_NARRATION:
            setupWaitForNarrMode();
            break;
        case OP_MODE_PLAY_NARRATION:
            setupPlayNarrMode();
            break;
        case OP_MODE_SWITCH_PRESETS:
            setupSwitchPresetsMode();
            break;
        case OP_MODE_MULTI_GRAIN_MODE:
            setupMultiGrainMode();
            break;
        case OP_MODE_SINGLE_GRAIN_MODE:
            setupSingleGrainMode();
            break;
#ifndef HAS_ADC
        case OP_MODE_NARRATION_GUI:
            setupNarrGuiMode();
            break;
        case OP_MODE_SIMULATION_MULTI:
            setupSimulationMultiMode();
            break;
        case OP_MODE_SIMULATION_SINGLE:
            setupSimulationSingleMode();
            break;
#endif
        case OP_MODE_NARRATION_GLITCH:
            setupNarrationGlitchMode();
            break;
    }
    
}


void ofApp::exitAnyMode()
{
    switch (operationMode)
    {
        case OP_MODE_SETUP:
            exitInitMode();
            break;
        case OP_MODE_WAIT_FOR_NARRATION:
            exitWaitForNarrMode();
            break;
        case OP_MODE_PLAY_NARRATION:
            exitPlayNarrMode();
            break;
        case OP_MODE_SWITCH_PRESETS:
            exitSwitchPresetsMode();
            break;
        case OP_MODE_MULTI_GRAIN_MODE:
            exitMultiGrainMode();
            break;
        case OP_MODE_SINGLE_GRAIN_MODE:
            exitSingleGrainMode();
            break;
#ifndef HAS_ADC
        case OP_MODE_NARRATION_GUI:
            exitNarrGuiMode();
            break;
        case OP_MODE_SIMULATION_MULTI:
            exitSimulationMultiMode();
            break;
        case OP_MODE_SIMULATION_SINGLE:
            exitSimulationSingleMode();
            break;
#endif
        case OP_MODE_NARRATION_GLITCH:
            exitNarrationGlitchMode();
            break;
    }
}


void ofApp::setupParamsFromSettings()
{
    // partty!! this is where I read all the settings for the system. To make in manageable the settings are named with the performers names as a prefix, the user profile is set in the username.json. This is then used to decide which settings to use. becuase the system runs on a raspberry pi and that pie is stuck deep inside the unit, and I dont want thme to have to log in, conect to wife etc, I just map a USP stick to a fixed path on the PI. the settings and audio files are altered day to day by replacing them or adding them to the USB stick and starting the device.
    // get the username
    ofJson usernameJson = ofLoadJson(filePathPrefix + "username.json");
    if (usernameJson.empty()) {
        ofLogNotice() << "Username settings not loaded" << endl;

    }
    else {
        ofLogNotice() << "Username settings loaded" << endl;
    }

    unitID = loadUnitId(usernameJson);
    ofLogNotice() << "UNIT_ID = " + ofToString(unitID) << endl;
    filePathPrefix = filePathPrefix + unitID + "/";
    ofLogNotice()<< "Root user data path is " + filePathPrefix <<endl;

    // based on the username load that app settings profile
    ofJson appSettingsJson = ofLoadJson(filePathPrefix  + unitID + "_appSettings.json");
    if (appSettingsJson.empty()) {
        ofLogNotice() << "App settings not loaded" << endl;

    }
    else {
        ofLogNotice() << "App settings loaded" << endl;
    }

    // every field below is read (with its default) by AppSettings::loadFrom
    // -- see src/AppSettings.h/cpp. What's here is applying those values to
    // ofApp's own members/objects and the side effects that go with them
    // (log level, derived slot/mode counts, the two extracted state
    // machines' configs, OSC setup, network commands) -- none of which is
    // "read a field with a default," so none of it moved into the loader.
    AppSettings settings = AppSettings::loadFrom(appSettingsJson);

    // I can change log levels to give me different kinds of feedback from the system when I am debugging (via ssh or VNC)
    logLevel = settings.logLevel;
    switch (logLevel)
    {
        case 0:
            ofSetLogLevel(OF_LOG_VERBOSE);
            break;
        case 1:
            ofSetLogLevel(OF_LOG_ERROR);
            break;
        case 2:
            ofSetLogLevel(OF_LOG_NOTICE);
            break;
        case 3:
            ofSetLogLevel(OF_LOG_SILENT);
            break;

    }

    //do we have a narration file
    hasNarration = settings.hasNarration;
    ofLogNotice() << "HAS_NARRATION = " + ofToString(hasNarration) << endl;

    //what volume to we play the narration at
    narrationVolume = settings.narrationVolume;
    ofLogNotice() << "NARRATION_VOLUME = " + ofToString(narrationVolume) << endl;

    // sensor input threshold for moving between narration play and narration granular
    narrationGlitchThreshold = settings.narrationGlitchThreshold;
    ofLogNotice() << "NARRATION_GLITCH_THRESH = " + ofToString(narrationGlitchThreshold) << endl;

    // when the sensor data is used to manipulate the narration granular how far should it move the playhead
    narrationGlitchStrand = settings.narrationGlitchStrand;
    ofLogNotice() << "NARRATION_GLITCH_STRAND = " + ofToString(narrationGlitchStrand) << endl;

    // if the unit has narration we have an option of how it is started, it can run automatically after some time, or it can be triggered by pushing the sensor
    narrationUsesSensor = settings.narrationUsesSensor;
    ofLogNotice() << "NARR_PLAY_WITH_SENSOR = " + ofToString(narrationUsesSensor) << endl;

    // what is the maximum value we think a performer can get to, we will use this for the top end of the value scaling
    maxSensorValue = settings.maxSensorValue;
    ofLogNotice() << "MAX_SENSOR_VALUE = " + ofToString(maxSensorValue) << endl;

    // sensor input threshold for engaging interaction (otherise the units will make random sounds from noise in the ADC
    normalisedA2DValuesMin = settings.normalisedA2DValuesMin;
    ofLogNotice() << "ACTIVE_THRESHOLD = " + ofToString(normalisedA2DValuesMin) << endl;

    // chose a curve to transform incoming sensor or simulation data
    curveSelector = settings.curveSelector;
    ofLogNotice() << "EASING_SELECTOR = " + ofToString(curveSelector) << endl;

    // accumulated or multi mode uses the combination of all sensor input to move the plahead of the granular objects. So the full postion is only reached by pressing all the balls in at once. The performers cannot push all in at once so instead of dividing the aggregated sensor data by 6 I divide it by this (it was 4 as that is how many sensors they can engage at once, but it was still too hard to for them to reach the end point so here it is a shitty hack
    accumulationDenominator = settings.accumulationDenominator;
    ofLogNotice() << "ACCUMULATION_DENOMINATOR = " + ofToString(accumulationDenominator) << endl;

    // On some systems I can reduce the latency by reducing the audio buffer size
    engineBufferSize = settings.engineBufferSize;
    ofLogNotice() << "BUFFER_SIZE = " + ofToString(engineBufferSize) << endl;

    // On some systems I can reduce the latency by reducing the number of buffers
    numberOfBuffers = settings.numberOfBuffers;
    ofLogNotice() << "NUMBER_OF_BUFFERS = " + ofToString(numberOfBuffers) << endl;

    // Here we can change the audio device for different soundcards
    audioDeviceId = settings.audioDeviceId;
    ofLogNotice() << "AUDIO_DEVICE_ID = " + ofToString(audioDeviceId) << endl;


    // this is for a gestural input, where the performers squeeze several sensors rapidly, this is the timeout period between letting of the push and the next push
    useHitGesture = settings.useHitGesture;
    ofLogNotice() << "HIT_TO_CHANGE_PRESETS = " + ofToString(useHitGesture) << endl;
    // this is for a gestural input, where the performers squeeze several sensors rapidly, this is the timeout period between letting of the push and the next push
    maxTroughDuration = settings.maxTroughDuration;
    ofLogNotice() << "MAX_TROUGH_DURATION = " + ofToString(maxTroughDuration) << endl;

    // this is for a gestural input, where the performers squeeze several sensors rapidly, this is the timeout period between a push and the letting off the push
    maxPeakDuration = settings.maxPeakDuration;
    ofLogNotice() << "MAX_PEAK_DURATION = " + ofToString(maxPeakDuration) << endl;

    // this is for a gestural input, where the performers squeeze several sensors rapidly, number of pushes need to trigger an action
    requiredHits = settings.requiredHits;
    ofLogNotice() << "REQUIRED_HITS = " + ofToString(requiredHits) << endl;

    // this is for a gestural input, where the performers squeeze several sensors rapidly, how hard does a press have to be
    hitThreshHold = settings.hitThreshHold;
    ofLogNotice() << "HIT_THRESHOLD = " + ofToString(hitThreshHold) << endl;

    // this is for a gestural input, where the performers squeeze several sensors rapidly, how soft does a non press have to be
    troughThreshold = settings.troughThreshold;
    ofLogNotice() << "TROUGH_THRESHOLD = " + ofToString(troughThreshold) << endl;

    HitGestureDetector::Config hitGestureConfig;
    hitGestureConfig.hitThreshold = hitThreshHold;
    hitGestureConfig.troughThreshold = troughThreshold;
    hitGestureConfig.maxPeakDurationMs = maxPeakDuration;
    hitGestureConfig.maxTroughDurationMs = maxTroughDuration;
    hitGestureConfig.requiredHits = requiredHits;
    hitGestureDetector = HitGestureDetector(hitGestureConfig);

    // in accumulated pressure we go to single grain mode
    useAccumulatedPressure = settings.useAccumulatedPressure;
    ofLogNotice() << "ACCUMULATED_PRESSURE = " + ofToString(useAccumulatedPressure) << endl;

    for(int i = 0; i < NUMBER_OF_PRESETS; i++){
        timeAdvanceInterval[i] = settings.timeAdvanceInterval[i];
        ofLogNotice() << "TIME_ADVANCE_INTERVAL = " + ofToString(timeAdvanceInterval[i]) << endl;
    }



    // change the number of slots and the simulation and grain modes depending on the accumulated pressure setting
    if (useAccumulatedPressure)
    {
        numberOfSlots = 1;
        ofLogNotice() << "Using 1 slot and accumulated pressure" << endl;
        grainOperationModeTranslate = OP_MODE_SINGLE_GRAIN_MODE;
        simulationOperationModeTranslate = OP_MODE_SIMULATION_SINGLE;
    }
    else if (!useAccumulatedPressure)
    {
        numberOfSlots = 6;
        ofLogNotice() << "Using 6 slots and mappable pressure pressure" << endl;

        grainOperationModeTranslate = OP_MODE_MULTI_GRAIN_MODE;
        simulationOperationModeTranslate = OP_MODE_SIMULATION_MULTI;
    }



    //not implemented yet, but if the systems need to on a network and talking to me or each other should I set the IP
    setLocalIp = settings.setLocalIp;
    ofLogNotice() << "SET_LOCAL_IP = " + ofToString(setLocalIp) << endl;

    //not implemented yet, but if the systems need to on a network and talking to me and I should set the IP what should I set it to
    localIpFromXML = settings.localIp;
    ofLogNotice() << "LOCAL_IP = " + ofToString(localIpFromXML) << endl;

    //not implemented yet, but if the systems need to on a network and talking to me should I set the IP to DHCP
    setLocalToDHCP = settings.setLocalToDHCP;
    ofLogNotice() << "SET_DHCP = " + ofToString(setLocalToDHCP) << endl;

    //not implemented yet, but if the systems need to on a network and talking to me I use OSC, which is packaged UDP messages, this is the local port
    localOSCPport = settings.localOSCPport;
    ofLogNotice() << "LOCAL_OSC_PORT = " + ofToString(localOSCPport) << endl;

    //This is implemented, I can send OSC data (for now debug data), this is the remote port
    remoteOSCPort = settings.remoteOSCPort;
    ofLogNotice() << "REMOTE_OSC_PORT = " + ofToString(remoteOSCPort) << endl;

    //This is implemented, I can send OSC data (for now debug data), this is the remote IP
    remoteOSCIp = settings.remoteOSCIp;
    ofLogNotice() << "REMOTE_OSC_IP = " + ofToString(remoteOSCIp) << endl;

    //If OSC debug is on, it will send OSC messages as well as console debug messages
    oscDebug = settings.oscDebug;
    ofLogNotice() << "OSC_DEBUG = " + ofToString(oscDebug) << endl;

    //not implemented if OSC live is on, it will send OSC messages to allow me to duplicate the interaction on a laptop in real time when the room is too big for the built in speakers
    oscLive = settings.oscLive;
    ofLogNotice() << "OSC_LIVE =" + ofToString(oscLive) << endl;

#ifdef HAS_ADC
    //device only if we are counting double or trupple presses on the soft button, how long to wait before we reset counting
    buttonPressTimeOut = settings.buttonPressTimeOut;
    ofLogNotice() << "BUTTON_PRESS_MAX_WAIT = " + ofToString(buttonPressTimeOut) << endl;

    ButtonClickClassifier::Config buttonClickConfig;
    buttonClickConfig.clickWindowMs = buttonPressTimeOut;
    buttonClickClassifier = ButtonClickClassifier(buttonClickConfig);

    // should the application close from a button interaction
    shutdownPress = settings.shutdownPress;
    ofLogNotice() << "SH_P = " + ofToString(shutdownPress) << endl;

    // should the application closing shut town the raspberry pi
    doShutdown = settings.doShutdown;
    ofLogNotice() << "D_SH = " + ofToString(doShutdown) << endl;
#endif

    // if we have OSCdebug on setup the packet sender and then send the data below
    if (oscDebug) {
        sender.setup(remoteOSCIp, remoteOSCPort);
    }
    
    if (oscDebug) {
        recyclingMessage.clear();
        recyclingMessage.setAddress("/" + ofToString(unitID) + "/localOSCPport");
        recyclingMessage.addIntArg(localOSCPport);
        sender.sendMessage(recyclingMessage, false);
    }
#ifdef HAS_ADC
    if (oscDebug) {
        recyclingMessage.clear();
        recyclingMessage.setAddress("/" + ofToString(unitID) + "/shutdownPress");
        recyclingMessage.addIntArg(shutdownPress);
        sender.sendMessage(recyclingMessage, false);
    }
    
    if (oscDebug) {
        recyclingMessage.clear();
        recyclingMessage.setAddress("/" + ofToString(unitID) + "/doShutdown");
        recyclingMessage.addIntArg(doShutdown);
        sender.sendMessage(recyclingMessage, false);
    }
#endif
    if (setLocalIp) {
        ofSystem("dhclient -v -r");
        ofSystem("ifconfig eth0 " + localIpFromXML);
    }
    
    if (setLocalToDHCP) {
        ofSystem("dhclient eth0 -v");
    }
    
    //ofToString(unitID) + "/" +
    if (oscDebug) {
        recyclingMessage.clear();
        recyclingMessage.setAddress("/" + ofToString(unitID) + "/unitID");
        recyclingMessage.addStringArg(unitID);
        sender.sendMessage(recyclingMessage, false);
    }
    
    if (oscDebug) {
        recyclingMessage.clear();
        recyclingMessage.setAddress("/" + ofToString(unitID) + "/filePathPrefix");
        recyclingMessage.addStringArg(filePathPrefix);
        sender.sendMessage(recyclingMessage, false);
    }
}

void ofApp::setupFilePaths()
{
    // reads the file paths of the audio files we need for te preset
    ofJson fileSettingsJson = ofLoadJson(filePathPrefix + unitID + "_fileSettings.json");
    if (!fileSettingsJson.empty()) {
        ofLogNotice() << "File settings loaded" << endl;
        for (int i = 0; i < numberOfSlots; i++) {
            ofLogNotice() << "Getting audio file paths from " + unitID +  "_fileSettings: Slot " + ofToString(i + 1)<< endl;

            for (int j=0; j < NUMBER_OF_PRESETS; j++) {
                filePathsSet[j][i] = filePathPrefix + "audio/" + fileSettingsJson.value("FILE_" + ofToString(i + 1) + "_" + ofToString(j+1), "Grain_" + ofToString(i + 1) + ".mp3");
                ofLogNotice() << "Preset " +ofToString(j)+ " File path slot " + ofToString(i + 1) + " = " + filePathsSet[j][i] << endl;
                fileNamesSet[j][i] = fileSettingsJson.value("FILE_" + ofToString(i + 1) + "_" + ofToString(j+1), "Grain_" + ofToString(i + 1) + ".mp3");
                ofLogNotice() << "Preset " +ofToString(j)+ " File name slot " + ofToString(i + 1) + " = " + fileNamesSet[j][i] << endl;

                if (oscDebug) {
                    // if we are using OSC debug send the names
                    recyclingMessage.clear();
                    recyclingMessage.setAddress("/" + ofToString(unitID) + "/filePaths");
                    recyclingMessage.addStringArg(filePathsSet[j][i]);
                }
            }
        }
        if (oscDebug) {
            sender.sendMessage(recyclingMessage, false);
        }
    }
    else
        ofLogNotice() << "File settings not loaded" << endl;

    narrationFilePath = filePathPrefix + "audio/" + fileSettingsJson.value("NARRATION_FILE", "Mono_20hz_-20db_netV.wav");
}

#ifdef HAS_ADC

void ofApp::setupLIS3DH(){
    bus = new I2c("/dev/i2c-1");
    bus->addressSet(0x18); // the default address of the LIS3DH
    
    // Read the WHO_AM_I register to check if the device is present and responding
    unsigned char whoAmI = bus->readByte(0x0F);
    if (whoAmI != 0x33) {
        ofLogNotice() << "LIS3DH not found, WHO_AM_I register = " << hex << whoAmI << endl;
    }
    else{
        ofLogNotice()  << "LIS3DH found, WHO_AM_I register = " << hex << whoAmI << endl;
    }
}
void ofApp::updateLIS3DH(){
    // Read the X, Y, and Z acceleration registers
    unsigned char xL = bus->readByte(0x28);
    unsigned char xH = bus->readByte(0x29);
    unsigned char yL = bus->readByte(0x2A);
    unsigned char yH = bus->readByte(0x2B);
    unsigned char zL = bus->readByte(0x2C);
    unsigned char zH = bus->readByte(0x2D);
    
    // Combine the high and low bytes to get the full 16-bit acceleration values
    short x = (xH << 8) | xL;
    short y = (yH << 8) | yL;
    short z = (zH << 8) | zL;
    
    // Convert the 16-bit values to floating-point values with a full-scale range of 2 g and a resolution of 12 bits
    accelScaleFactor = 2.0f / (float)((1 << 12) - 1);
    xg = x * accelScaleFactor;
    yg = y * accelScaleFactor;
    zg = z * accelScaleFactor;
    
    // Print the acceleration values
    ofLogVerbose() << "X = " << xg << " g, Y = " << yg << " g, Z = " << zg << " g" << endl;
}
void ofApp::setupADC()
{
    //setup the SPI interface for rading sensor data - raspberry pi only
    a2d.setup("/dev/spidev0.0", SPI_MODE_0, 1000000, 8);
    
    for (int i = 0; i < NUMBER_OF_SENSORS; i++) {
        a2dChannel[i] = i;
        
    }
    
    if (oscDebug) {
        recyclingMessage.clear();
        recyclingMessage.setAddress("/" + ofToString(unitID) + "/a2dSetup");
        recyclingMessage.addIntArg(1);
        sender.sendMessage(recyclingMessage, false);
    }
    
    ofLogNotice() << "adc setup" << endl;
    
}

void ofApp::setupButton()

{
    //setup the soft button for interaction - raspberry pi only
    button.setup("19");
    button.export_gpio();
    ofSleepMillis(100);
    blueLed.setdir_gpio("in");
    
    if (oscDebug) {
        recyclingMessage.clear();
        recyclingMessage.setAddress("/" + ofToString(unitID) + "/button");
        recyclingMessage.addIntArg(1);
        sender.sendMessage(recyclingMessage, false);
    }
    
    ofLogNotice() << "Button setup" << endl;

}


void ofApp::initLedBlue()
{
    //setup the blue LED pin for feedback - raspberry pi only
    blueLed.setup("5");
    blueLed.export_gpio();
    ofSleepMillis(100);
    blueLed.setdir_gpio("out");
    
    blueLed.setval_gpio("1");
    ofSleepMillis(500);
    blueLed.setval_gpio("0");
    
    if (oscDebug) {
        recyclingMessage.clear();
        recyclingMessage.setAddress("/" + ofToString(unitID) + "/blueLed");
        recyclingMessage.addIntArg(1);
        sender.sendMessage(recyclingMessage, false);
    }
    ofLogNotice() << "Blue LED setup" << endl;
}

void ofApp::initLedRed()
{
    //setup the red LED pin for feedback - raspberry pi only
    redLed.setup("6");
    redLed.export_gpio();
    ofSleepMillis(100);
    redLed.setdir_gpio("out");
    
    redLed.setval_gpio("1");
    ofSleepMillis(200);
    redLed.setval_gpio("0");
    
    if (oscDebug) {
        recyclingMessage.clear();
        recyclingMessage.setAddress("/" + ofToString(unitID) + "/redLed");
        recyclingMessage.addIntArg(1);
        sender.sendMessage(recyclingMessage, false);
    }
    
    ofLogNotice() << "Red LED setup" << endl;
}

void ofApp::setupSpeakerControl()
{
    //setup the relay to turn the speaker on and off - raspberry pi only
    ofLogNotice() << "setup speaker control" << endl;
    relayOut.setup("13");
    relayOut.export_gpio();
    ofSleepMillis(100);
    relayOut.setdir_gpio("out");
    
    
    if (oscDebug) {
        recyclingMessage.clear();
        recyclingMessage.setAddress("/" + ofToString(unitID) + "/relayOut");
        recyclingMessage.addIntArg(1);
        sender.sendMessage(recyclingMessage, false);
    }
    ofLogNotice() << "relay setup" << endl;
}

void ofApp::syncSpeaker()
{
    //Turn on the speaker - raspberry pi only
    relayOut.setval_gpio("1");
    ofSleepMillis(400);
    relayOut.setval_gpio("0");
    ofLogNotice() << "Sync Speaker" << endl;

}

#endif // HAS_ADC


void ofApp::updateParametersFromValuesSingle()
{
    // go through the parameters of all the granular objects, if a parameters is connected to incoming sensor data, map that data appropriately, can also come from the simulator
    for (int k = 0; k < numberOfSlots; k++) {
        for (int v = 0; v < NUMBER_OF_SENSORS; v++) {
            applyDynamicValuesToParameters(k, in_length_connect, v, _in_length, _in_lengthMin, _in_lengthMax, "grain length");
            applyDynamicValuesToParameters(k, in_density_connect, v, _in_density, _in_densityMin, _in_densityMax, "density" );
            applyDynamicValuesToParameters(k, in_distJit_connect, v, _in_distance_jitter, _in_distance_jitterMin, _in_distance_jitterMax, "distance jitter");
            applyDynamicValuesToParameters(k, in_pitch_connect, v, _in_pitch, _in_pitchMin, _in_pitchMax, "pitch");
            applyDynamicValuesToParameters(k, in_pitchJit_connect, v, _in_pitch_jitter, _in_pitch_jitterMin, _in_pitch_jitterMax, "pitch jitter");
            applyDynamicValuesToParameters(k, _volume_connect, v, _volume, _volumeMin, _volumeMax, "volume");
            applyDynamicValuesToParameters(k, _spread_connect, v, _spread, _spreadMin, _spreadMax, "spread");
            
            
        }
    }
    getAccumulatedPressure();
}

void ofApp::updateParametersFromValuesMulti()
{
    // go through the parameters of all the granular objects, if a parameters is connected to incoming sensor data, map that data appropriately, can also come from the simulator
    for (int k = 0; k < numberOfSlots; k++) {
        for (int v = 0; v < NUMBER_OF_SENSORS; v++) {
            applyDynamicValuesToParameters(k, in_length_connect, v, _in_length, _in_lengthMin, _in_lengthMax, "grain length");
            applyDynamicValuesToParameters(k, in_density_connect, v, _in_density, _in_densityMin, _in_densityMax, "density" );
            applyDynamicValuesToParameters(k, in_distJit_connect, v, _in_distance_jitter, _in_distance_jitterMin, _in_distance_jitterMax, "distance jitter");
            applyDynamicValuesToParameters(k, in_pitch_connect, v, _in_pitch, _in_pitchMin, _in_pitchMax, "pitch");
            applyDynamicValuesToParameters(k, in_pitchJit_connect, v, _in_pitch_jitter, _in_pitch_jitterMin, _in_pitch_jitterMax, "pitch jitter");
            applyDynamicValuesToParameters(k, _volume_connect, v, _volume, _volumeMin, _volumeMax, "volume");
            applyDynamicValuesToParameters(k, _spread_connect, v, _spread, _spreadMin, _spreadMax, "spread");
            applyDynamicValuesToParameters(k, _posX_connect, v, _posX, _posXMin, _posXMax, "position");
            
        }
    }
    for (int i = 0; i < numberOfSlots; i++) {
        _posX[i] >> slots[i].cloud->in_position();
        _spread[i] >> slots[i].cloud->in_position_jitter();
        _in_length[i] >> slots[i].cloud->in_length();
        _in_density[i] >> slots[i].cloud->in_density();
        _in_distance_jitter[i] >> slots[i].cloud->in_distance_jitter();
        _in_pitch_jitter[i] >> slots[i].cloud->in_pitch_jitter();
        _grainDirection[i] >> slots[i].cloud->in_direction();
        _in_pitch[i] >> slots[i].cloud->in_pitch();
    }
}
void ofApp::UpdatePlayheadWithTimer(){
    positionFromTime += timeAdvanceInterval[presetIndex-1];
    if(positionFromTime > 0.985){
        positionFromTime = 0.985;
    }
    positionFromTime >> slots[0].cloud->in_position();
    _in_length[0] >> slots[0].cloud->in_length();
    _in_density[0] >> slots[0].cloud->in_density();
    _in_distance_jitter[0] >> slots[0].cloud->in_distance_jitter();
    _in_pitch_jitter[0] >> slots[0].cloud->in_pitch_jitter();
    _grainDirection[0] >> slots[0].cloud->in_direction();
    _in_pitch[0] >> slots[0].cloud->in_pitch();
    _spread[0] >> slots[0].cloud->in_position_jitter();
}
void ofApp::getAccumulatedPressure()
{
    // if we are in accumulated pressure or single grain mode, we get our playhead position from the total of all the sensors
    accumulatedPressure = 0;
    
    for (int p = 0; p < NUMBER_OF_SENSORS; p++)
    {
        if (normalisedA2DValues[p]>0.01)
        {
            accumulatedPressure += normalisedA2DValues[p];
        }
    }
    // and then we map it and normalise it- first we get how many sensors could be pushed at once and map it between that possible total and 0 and 1
    accumulatedPressureNormalised = ofMap(accumulatedPressure, 0.0, accumulationDenominator, 0.0, 1.0);
    
    //then we comensate for the normalisedA2DValuesMin, which is the threshold for engaging the playhead (we dont want to lose this bit of resolution in the file or have a jump
    accumulatedPressureNormalised = ofMap(accumulatedPressureNormalised, normalisedA2DValuesMin, 1.0, 0.0, 1.0);
    
    // if afer all that mapping we are over the threshold then apply the curves to the data
    if (accumulatedPressureNormalised>normalisedA2DValuesMin)
    {
        switch (curveSelector){
            case 0:
                accumulatedPressureNormalised = accumulatedPressureNormalised;
                break;
            case 1:
                accumulatedPressureNormalised = sqrt(accumulatedPressureNormalised);
                break;
            case 2:
                accumulatedPressureNormalised = quarticEaseIn(accumulatedPressureNormalised);
                break;
            case 3:
                accumulatedPressureNormalised = exponentialEaseIn(accumulatedPressureNormalised);
                break;
        }
        
        //then apply the curved data to the granular and set the volume as it should be (could be mapped to a sensor)
        //accumulatedPressureNormalised >> slots[0].cloud->in_position();
        UpdatePlayheadWithTimer();
        slots[0].ampControl->set(_volume[0]);
        ofLogVerbose() << "Accumulated pressure is " + ofToString(accumulatedPressureNormalised) << endl;
        
    }
    // of it does not pass the threshold then reset the volume and playhead position
    else if (accumulatedPressureNormalised<normalisedA2DValuesMin) {
        0.0 >> slots[0].cloud->in_position();
        slots[0].ampControl->set(0.0);
    }
#ifndef HAS_ADC
    if (timeUpdateFromKey) {
        UpdatePlayheadWithTimer();
        slots[0].ampControl->set(_volume[0]);
    }
#endif
    
}

void ofApp::applyDynamicValuesToParameters(int &k, std::vector<ofParameter<int>> connectTo, int v, std::vector<ofParameter<float>> parameter, std::vector<ofParameter<float>> parameterMin, std::vector<ofParameter<float>> parameterMax, string paramName)
{
    // if a parameter is mapped to a sensor then apply the data and map it between the min and max
    if (connectTo[k] == v + 1) {
        
        if (normalisedA2DValues[v]<normalisedA2DValuesMin)
        {
            slots[k].ampControl->set(0.0);
        }
        else
        {
            //drawGrains[k] = true;
            parameter[k] = ofMap(normalisedA2DValues[v], 0.0, 1.0, parameterMin[k], parameterMax[k]);
            slots[k].ampControl->set(_volume[k]);
        }
        
        ofLogVerbose() << paramName + " on slot " + ofToString(k) + " = " + ofToString(parameter[k]) + " mapped between " + ofToString(parameterMin[k]) + " and " + ofToString(parameterMax[k]) + " on sensor " + ofToString(v+1)<< endl;
    }
}




void ofApp::updateEffectParametersFromValues()
{
    // go through the parameters of all the granular objects, if a parameters is connected to incoming sensor data, map that data appropriately, can also come from the simulator
    for (int k = 0; k < numberOfSlots; k++) {
        for (int v = 0; v < NUMBER_OF_SENSORS; v++) {
            applyDynamicValuesToBitCrusherParameters(k, v);
            applyDynamicValuesToDecimatorParameters(k, v);
            applyDynamicValuesToFilterParameters(k, v);
            applyDynamicValuesToChorusParameters(k, v);
            applyDynamicValuesToReverbParameters(k, v);
            applyDynamicValuesToDelayParameters(k, v);
        }
    }
}

void ofApp::transferEfffectParamtersToUnits(){
    for (int j = 0; j < numberOfSlots; j++)
    {
        if(effectsPatching[presetIndex-1][j].hasBitCrusher){
            effBitCrushersParams[j]._e_bitcrush_in_bits >> slots[j].bitCrusherLs->in_bits();
            effBitCrushersParams[j]._e_bitcrush_in_bits >> slots[j].bitCrusherRs->in_bits();
        }
        
        if(effectsPatching[presetIndex-1][j].hasDecimator){
            
            effDecimatorsParams[j]._e_decomator_in_rate >> slots[j].decimatorLs->in_freq();
            effDecimatorsParams[j]._e_decomator_in_rate >> slots[j].decimatorRs->in_freq();
        }
        if(effectsPatching[presetIndex-1][j].hasChorus){
            
            effChorussParams[j]._e_chorus_in_depth >> slots[j].choruss->in_depth();
            effChorussParams[j]._e_chorus_in_speed >> slots[j].choruss->in_speed();
            effChorussParams[j]._e_chorus_in_delay >> slots[j].choruss->in_delay();
            
        }
        if(effectsPatching[presetIndex-1][j].hasFilter){
            
            effFiltersParams[j]._e_MLAD_in_freq >> slots[j].multiLadderFilterLs->in_freq();
            effFiltersParams[j]._e_MLAD_in_reso >> slots[j].multiLadderFilterLs->in_reso();
            
            effFiltersParams[j]._e_MLAD_in_freq >> slots[j].multiLadderFilterRs->in_freq();
            effFiltersParams[j]._e_MLAD_in_reso >> slots[j].multiLadderFilterRs->in_reso();
            
        }
        if(effectsPatching[presetIndex-1][j].hasDelay){
            
            effDelaysParams[j]._e_delay_in_send >> slots[j].delaySends->in_mod();
            
            effDelaysParams[j]._e_delay_in_time >> slots[j].delayLs->in_time();
            effDelaysParams[j]._e_delay_in_damping >> slots[j].delayLs->in_damping();
            effDelaysParams[j]._e_delay_in_feedback >> slots[j].delayLs->in_feedback();
            
            effDelaysParams[j]._e_delay_in_time >> slots[j].delayRs->in_time();
            effDelaysParams[j]._e_delay_in_damping >> slots[j].delayRs->in_damping();
            effDelaysParams[j]._e_delay_in_feedback >> slots[j].delayRs->in_feedback();
            
        }
        if(effectsPatching[presetIndex-1][j].hasReverb){
            
            effReverbsParams[j]._e_reverb_in_mix >> slots[j].reverbSends->in_mod();
            effReverbsParams[j]._e_reverb_in_time >> slots[j].reverbs->in_time();
            effReverbsParams[j]._e_reverb_in_damping >> slots[j].reverbs->in_damping();
            effReverbsParams[j]._e_reverb_in_density >> slots[j].reverbs->in_density();
            effReverbsParams[j]._e_reverb_in_hiCut >> slots[j].reverbs->in_hi_cut();
            effReverbsParams[j]._e_reverb_in_modFreq >> slots[j].reverbs->in_mod_freq();
            effReverbsParams[j]._e_reverb_in_modAmount >> slots[j].reverbs->in_mod_amount();
            
            
        }
        effCompressorsParams[j]._e_compressor_in_gain >> slots[j].outputAmpL->in_mod();
        effCompressorsParams[j]._e_compressor_in_gain >> slots[j].outputAmpR->in_mod();
        
        effCompressorsParams[j]._e_compressor_in_attack >> slots[j].compressors->in_attack();
        effCompressorsParams[j]._e_compressor_in_knee >> slots[j].compressors->in_knee();
        effCompressorsParams[j]._e_compressor_in_ratio >> slots[j].compressors->in_ratio();
        effCompressorsParams[j]._e_compressor_in_release >> slots[j].compressors->in_release();
        effCompressorsParams[j]._e_compressor_in_threshold >> slots[j].compressors->in_threshold();
        effCompressorsParams[j]._e_compressor_comp_meter.set(slots[j].compressors->meter_GR());
        
    }
}

void ofApp::updateEffects(){
    //updateEffectParametersFromValues();
    transferEfffectParamtersToUnits();
    
}
void ofApp::updateEffectsSimulation(){
    updateEffectParametersFromValues();
    transferEfffectParamtersToUnits();
    
}

void ofApp::applyDynamicValuesToDecimatorParameters(int k, int v){
    if (effectsPatching[presetIndex-1][k].hasDecimator) {
        if (effDecimatorsParams[k]._e_decomator_in_rateConnectTo == v + 1) {
            effDecimatorsParams[k]._e_decomator_in_rate = ofMap(normalisedA2DValues[v], 0.0, 1.0, effDecimatorsParams[k]._e_decomator_in_rateMin, effDecimatorsParams[k]._e_decomator_in_rateMax);
            ofLogVerbose() << "Decimator control on slot " + ofToString(k) + " = " + ofToString(effDecimatorsParams[k]._e_decomator_in_rate) + " mapped between " + ofToString(effDecimatorsParams[k]._e_decomator_in_rateMin) + " and " + ofToString(effDecimatorsParams[k]._e_decomator_in_rateMax)+ " on sensor " + ofToString(v+1) << endl;
        }
    }
}
void ofApp::applyDynamicValuesToDelayParameters(int k, int v){
    if (effectsPatching[presetIndex-1][k].hasDelay) {
        if (effDelaysParams[k]._e_delay_in_sendConnectTo == v + 1) {
            effDelaysParams[k]._e_delay_in_send = ofMap(normalisedA2DValues[v], 0.0, 1.0, effDelaysParams[k]._e_delay_in_sendMin, effDelaysParams[k]._e_delay_in_sendMax);
            ofLogVerbose() << "Delay send on slot " + ofToString(k) + " = " + ofToString(effDelaysParams[k]._e_delay_in_send) + " mapped between " + ofToString(effDelaysParams[k]._e_delay_in_sendMin) + " and " + ofToString(effDelaysParams[k]._e_delay_in_sendMax) + " on sensor " + ofToString(v+1) << endl;
        }
        
        if (effDelaysParams[k]._e_delay_in_timeConnectTo == v + 1) {
            effDelaysParams[k]._e_delay_in_time = ofMap(normalisedA2DValues[v], 0.0, 1.0, effDelaysParams[k]._e_delay_in_timeMin, effDelaysParams[k]._e_delay_in_timeMax);
            ofLogVerbose() << "Delay time on slot " + ofToString(k) + " = " + ofToString(effDelaysParams[k]._e_delay_in_time) + " mapped between " + ofToString(effDelaysParams[k]._e_delay_in_timeMin) + " and " + ofToString(effDelaysParams[k]._e_delay_in_timeMax)+ " on sensor " + ofToString(v+1) << endl;
        }
        
        if (effDelaysParams[k]._e_delay_in_feedbackConnectTo == v + 1) {
            effDelaysParams[k]._e_delay_in_feedback = ofMap(normalisedA2DValues[v], 0.0, 1.0, effDelaysParams[k]._e_delay_in_feedbackMin, effDelaysParams[k]._e_delay_in_feedbackMax);
            ofLogVerbose() << "Delay Feedback on slot " + ofToString(k) + " = " + ofToString(effDelaysParams[k]._e_delay_in_feedback) + " mapped between " + ofToString(effDelaysParams[k]._e_delay_in_feedbackMin) + " and " + ofToString(effDelaysParams[k]._e_delay_in_feedbackMax) + " on sensor " + ofToString(v+1)<< endl;
        }
        
        if (effDelaysParams[k]._e_delay_in_dampingConnectTo == v + 1) {
            effDelaysParams[k]._e_delay_in_damping = ofMap(normalisedA2DValues[v], 0.0, 1.0, effDelaysParams[k]._e_delay_in_dampingMin, effDelaysParams[k]._e_delay_in_dampingMax);
            ofLogVerbose() << "Delay damping on slot " + ofToString(k) + " = " + ofToString(effDelaysParams[k]._e_delay_in_damping) + " mapped between " + ofToString(effDelaysParams[k]._e_delay_in_dampingMin) + " and " + ofToString(effDelaysParams[k]._e_delay_in_dampingMax) + " on sensor " + ofToString(v+1)<< endl;
        }
    }
}
void ofApp::applyDynamicValuesToFilterParameters(int k, int v){
    if (effectsPatching[presetIndex-1][k].hasFilter) {
        if (effFiltersParams[k]._e_MLAD_in_freqConnectTo == v + 1) {
            effFiltersParams[k]._e_MLAD_in_freq = ofMap(normalisedA2DValues[v], 0.0, 1.0, effFiltersParams[k]._e_MLAD_in_freqMin, effFiltersParams[k]._e_MLAD_in_freqMax);
            ofLogVerbose() << "Filter freq on slot " + ofToString(k) + " = " + ofToString(effFiltersParams[k]._e_MLAD_in_freq) + " mapped between " + ofToString(effFiltersParams[k]._e_MLAD_in_freqMin) + " and " + ofToString(effFiltersParams[k]._e_MLAD_in_freqMax) + " on sensor " + ofToString(v+1)<< endl;
        }
        
        if (effFiltersParams[k]._e_MLAD_in_resoConnectTo == v + 1) {
            effFiltersParams[k]._e_MLAD_in_reso = ofMap(normalisedA2DValues[v], 0.0, 1.0, effFiltersParams[k]._e_MLAD_in_resoMin, effFiltersParams[k]._e_MLAD_in_resoMax);
            ofLogVerbose() << "Filter reso on slot " + ofToString(k) + " = " + ofToString(effFiltersParams[k]._e_MLAD_in_reso) + " mapped between " + ofToString(effFiltersParams[k]._e_MLAD_in_resoMin) + " and " + ofToString(effFiltersParams[k]._e_MLAD_in_resoMax)+ " on sensor " + ofToString(v+1) << endl;
        }
    }
}
void ofApp::applyDynamicValuesToChorusParameters(int k, int v){
    if (effectsPatching[presetIndex-1][k].hasChorus) {
        if (effChorussParams[k]._e_chorus_in_depthConnectTo == v + 1) {
            effChorussParams[k]._e_chorus_in_depth = ofMap(normalisedA2DValues[v], 0.0, 1.0, effChorussParams[k]._e_chorus_in_depthMin, effChorussParams[k]._e_chorus_in_depthMax);
            ofLogVerbose() << "Chorus depth on slot " + ofToString(k) + " = " + ofToString(effChorussParams[k]._e_chorus_in_depth) + " mapped between " + ofToString(effChorussParams[k]._e_chorus_in_depthMin) + " and " + ofToString(effChorussParams[k]._e_chorus_in_depthMax) + " on sensor " + ofToString(v+1)<< endl;
        }
        if (effChorussParams[k]._e_chorus_in_speedConnectTo == v + 1) {
            effChorussParams[k]._e_chorus_in_speed = ofMap(normalisedA2DValues[v], 0.0, 1.0, effChorussParams[k]._e_chorus_in_speedMin, effChorussParams[k]._e_chorus_in_speedMax);
            ofLogVerbose() << "Chorus Speed on slot " + ofToString(k) + " = " + ofToString(effChorussParams[k]._e_chorus_in_speed) + " mapped between " + ofToString(effChorussParams[k]._e_chorus_in_speedMin) + " and " + ofToString(effChorussParams[k]._e_chorus_in_speedMax) + " on sensor " + ofToString(v+1)<< endl;
        }
        if (effChorussParams[k]._e_chorus_in_delayConnectTo == v + 1) {
            effChorussParams[k]._e_chorus_in_delay = ofMap(normalisedA2DValues[v], 0.0, 1.0, effChorussParams[k]._e_chorus_in_delayMin, effChorussParams[k]._e_chorus_in_delayMax);
            ofLogVerbose() << "Chorus delay on slot " + ofToString(k) + " = " + ofToString(effChorussParams[k]._e_chorus_in_delay) + " mapped between " + ofToString(effChorussParams[k]._e_chorus_in_delayMin) + " and " + ofToString(effChorussParams[k]._e_chorus_in_delayMax) + " on sensor " + ofToString(v+1)<< endl;
        }
    }
}
void ofApp::applyDynamicValuesToReverbParameters(int k, int v){
    if (effectsPatching[presetIndex-1][k].hasReverb) {
        if (effReverbsParams[k]._e_reverb_in_mixConnectTo == v + 1) {
            effReverbsParams[k]._e_reverb_in_mix = ofMap(normalisedA2DValues[v], 0.0, 1.0, effReverbsParams[k]._e_reverb_in_mixMin, effReverbsParams[k]._e_reverb_in_mixMax);
            ofLogVerbose() << "Reverb mix on slot " + ofToString(k) + " = " + ofToString(effReverbsParams[k]._e_reverb_in_mix) + " mapped between " + ofToString(effReverbsParams[k]._e_reverb_in_mixMin) + " and " + ofToString(effReverbsParams[k]._e_reverb_in_mixMax) + " on sensor " + ofToString(v+1)<< endl;
        }
        if (effReverbsParams[k]._e_reverb_in_timeConnectTo == v + 1) {
            effReverbsParams[k]._e_reverb_in_time = ofMap(normalisedA2DValues[v], 0.0, 1.0, effReverbsParams[k]._e_reverb_in_timeMin, effReverbsParams[k]._e_reverb_in_timeMax);
            ofLogVerbose() << "Reverb time on slot " + ofToString(k) + " = " + ofToString(effReverbsParams[k]._e_reverb_in_time) + " mapped between " + ofToString(effReverbsParams[k]._e_reverb_in_timeMin) + " and " + ofToString(effReverbsParams[k]._e_reverb_in_timeMax) + " on sensor " + ofToString(v+1)<< endl;
        }
        if (effReverbsParams[k]._e_reverb_in_dampingConnectTo == v + 1) {
            effReverbsParams[k]._e_reverb_in_damping = ofMap(normalisedA2DValues[v], 0.0, 1.0, effReverbsParams[k]._e_reverb_in_dampingMin, effReverbsParams[k]._e_reverb_in_dampingMax);
            ofLogVerbose() << "Reverb damping on slot " + ofToString(k) + " = " + ofToString(effReverbsParams[k]._e_reverb_in_damping) + " mapped between " + ofToString(effReverbsParams[k]._e_reverb_in_dampingMin) + " and " + ofToString(effReverbsParams[k]._e_reverb_in_dampingMax) + " on sensor " + ofToString(v+1)<< endl;
        }
        if (effReverbsParams[k]._e_reverb_in_densityConnectTo == v + 1) {
            effReverbsParams[k]._e_reverb_in_density = ofMap(normalisedA2DValues[v], 0.0, 1.0, effReverbsParams[k]._e_reverb_in_densityMin, effReverbsParams[k]._e_reverb_in_densityMax);
            ofLogVerbose() << "Reverb density on slot " + ofToString(k) + " = " + ofToString(effReverbsParams[k]._e_reverb_in_density) + " mapped between " + ofToString(effReverbsParams[k]._e_reverb_in_densityMin) + " and " + ofToString(effReverbsParams[k]._e_reverb_in_densityMax) + " on sensor " + ofToString(v+1)<< endl;
        }
        if (effReverbsParams[k]._e_reverb_in_hiCutConnectTo == v + 1) {
            effReverbsParams[k]._e_reverb_in_hiCut = ofMap(normalisedA2DValues[v], 0.0, 1.0, effReverbsParams[k]._e_reverb_in_hiCutMin, effReverbsParams[k]._e_reverb_in_hiCutMax);
            ofLogVerbose() << "Reverb high cut on slot " + ofToString(k) + " = " + ofToString(effReverbsParams[k]._e_reverb_in_hiCut) + " mapped between " + ofToString(effReverbsParams[k]._e_reverb_in_hiCutMin) + " and " + ofToString(effReverbsParams[k]._e_reverb_in_hiCutMax) + " on sensor " + ofToString(v+1)<< endl;
        }
        if (effReverbsParams[k]._e_reverb_in_modFreqConnectTo == v + 1) {
            effReverbsParams[k]._e_reverb_in_modFreq = ofMap(normalisedA2DValues[v], 0.0, 1.0, effReverbsParams[k]._e_reverb_in_modFreqMin, effReverbsParams[k]._e_reverb_in_modFreqMax);
            ofLogVerbose() << "Reverb Mod freq on slot " + ofToString(k) + " = " + ofToString(effReverbsParams[k]._e_reverb_in_modFreq) + " mapped between " + ofToString(effReverbsParams[k]._e_reverb_in_modFreqMin) + " and " + ofToString(effReverbsParams[k]._e_reverb_in_modFreqMax) + " on sensor " + ofToString(v+1)<< endl;
        }
        if (effReverbsParams[k]._e_reverb_in_modAmountConnectTo == v + 1) {
            effReverbsParams[k]._e_reverb_in_modAmount = ofMap(normalisedA2DValues[v], 0.0, 1.0, effReverbsParams[k]._e_reverb_in_modAmountMin, effReverbsParams[k]._e_reverb_in_modAmountMax);
            ofLogVerbose() << "Reverb mod amount on slot " + ofToString(k) + " = " + ofToString(effReverbsParams[k]._e_reverb_in_modAmount) + " mapped between " + ofToString(effReverbsParams[k]._e_reverb_in_modAmountMin) + " and " + ofToString(effReverbsParams[k]._e_reverb_in_modAmountMax) + " on sensor " + ofToString(v+1)<< endl;
        }
    }
}

void ofApp::applyDynamicValuesToBitCrusherParameters(int k, int v)
{
    if (effectsPatching[presetIndex-1][k].hasBitCrusher) {
        if (effBitCrushersParams[k]._e_bitcrush_in_bitsConnectTo == v + 1) {
            effBitCrushersParams[k]._e_bitcrush_in_bits = ofMap(normalisedA2DValues[v], 0.0, 1.0, effBitCrushersParams[k]._e_bitcrush_in_bitsMin, effBitCrushersParams[k]._e_bitcrush_in_bitsMax);
            ofLogVerbose() << "Bitcrusher on slot " + ofToString(k) + " = " + ofToString(effBitCrushersParams[k]._e_bitcrush_in_bits) + " mapped between " + ofToString(effBitCrushersParams[k]._e_bitcrush_in_bitsMin) + " and " + ofToString(effBitCrushersParams[k]._e_bitcrush_in_bitsMax) + " on sensor " + ofToString(v+1)<< endl;
        }
    }
    
}



void ofApp::switchPresets()
{
    
    // I had a problem switching too fast on raspberry pi so I make a time to check it cannot happen within 10 seconds of the last switchPresets(), the raspberry pi changes the LED colour in this process
#ifdef HAS_ADC
    if (ofGetElapsedTimeMillis() - presetSwitchTimer > 4000)
    {
        
        
#endif // HAS_ADC
        timeUpdateFromKey=false;
        presetIndex++;
        if (presetIndex>NUMBER_OF_PRESETS) {
            presetIndex=1;
        }
        for(int i = 0; i < numberOfSlots; i++){
            drawGrains[i]=false;
            
        }
        
        setupGraincloud(filePathsSet[presetIndex-1], unitID + "_preset_" + ofToString(presetIndex) + ".xml");
        
        presetSwitchTimer = ofGetElapsedTimeMillis();
#ifdef HAS_ADC
        
    }
#endif // HAS_ADC
}


void ofApp::setupNarration() {
    // this is not modal, but the method for the first narration setup, load the file, setup the granualr for the narration and connect it to the audio engine.
    ofLogNotice() << "setup narration for system" << endl;
    
    narrationIsPlaying = false;
    if (hasNarration) {
        setupNarrationGrain();
        
        narration.load(narrationFilePath);
        
        if (narration.isLoaded()) {
            ofLogNotice() << "Loaded narration audio file from " + narrationFilePath << endl;
            shouldTriggerNarrationPlay = true;
            
            narration >> engine.audio_out(0);
            narration >> engine.audio_out(1);
        }
        else {
            ofLogNotice() << "Error loading narration audio file" << endl;
            shouldTriggerNarrationPlay = false;
        }
#ifdef HAS_ADC
        blueLed.setval_gpio("0");
        redLed.setval_gpio("1");
#endif
        goToMode(OP_MODE_WAIT_FOR_NARRATION);
    }
    
}
void ofApp::setupNarrationGrain()
{
    // called from setupNarration, it sets up the granular and its settings form XML for the narration (if it has it)
    ofLogNotice() << "setup narration grain" << endl;
    narrPanel.setup("Narration", filePathPrefix + unitID + "_narr.xml", 100, 100); // Seting up this GUI module and adding the parameters to it
    narrPanel.add(narr_window_type_id.set("Window type", 0, 0, 9));
#ifndef HAS_ADC
    narr_window_type_id.addListener(this, &ofApp::onNarrWindowTypeChanged);
    narrPanel.add(narr_window_type_name.set("Window name", "default"));
#endif
    narrPanel.add(narr_in_length.set("in length", 500, 10, 3000));
    narrPanel.add(narr_in_density.set("in density", 0.9, 0.1, 1.0));
    narrPanel.add(narr_in_distance_jitter.set("distance jitter", 0.0, 0.0, 1000.0));
    narrPanel.add(narr_in_pitch_jitter.set("pitch jitter", 0.0, 0.0, 200.0));
    narrPanel.add(narr_in_pitch.set("pitch ", 0.0, -10.0, 10.0));
    narrPanel.add(narr_in_position_jitter.set("spread", 0.0, 0.0, 1.0));
    narrPanel.add(narr_grainDirection.set("direction", 0.0, -1.0, 1.0));
    
    narrCompressorControls.setup();
    narrCompressorControls.setParameterGroupName("Narration Compressor");
    narrPanel.add(narrCompressorControls.getParamGroup());
    
    //    effDelaysParams[j].setup();
    //    ofLogNotice()<< "adding delay to slot " + ofToString(j) << endl;
    //    effDelaysParams[j].setParameterGroupName("Delay slot " + ofToString(j+1));
    //    effectsPanels[j]->add(effDelaysParams[j].getParamGroup());
    //    narrPanel.add(narrOutCompressor.
    
    // load settings from the XML
    narrPanel.loadFromFile(filePathPrefix + unitID + "_narr.xml");
    
    
    narrGrainVoices = narrCloud.getVoicesNum();
    // sampleData is an instance of pdsp::SampleBuffer
    narrSampleData.setVerbose(true);
    narrSampleData.load(narrationFilePath);
    
    switch (narr_window_type_id) {
        case 0:
            narrCloud.setWindowType(pdsp::Rectangular);
            ofLogNotice() << "Narration using window type: Rectangular" << endl;
            break;
        case 1:
            narrCloud.setWindowType(pdsp::Triangular);
            ofLogNotice() << "Narration using window type: Triangular" << endl;
            break;
        case 2:
            narrCloud.setWindowType(pdsp::Hann);
            ofLogNotice() << "Narration using window type: Hann" << endl;
            break;
        case 3:
            narrCloud.setWindowType(pdsp::Hamming);
            ofLogNotice() << "Narration using window type: Hamming" << endl;
            break;
        case 4:
            narrCloud.setWindowType(pdsp::Blackman);
            ofLogNotice() << "Narration using window type: Blackman" << endl;
            break;
        case 5:
            narrCloud.setWindowType(pdsp::BlackmanHarris);
            ofLogNotice() << "Narration using window type: BlackmanHarris" << endl;
            break;
        case 6:
            narrCloud.setWindowType(pdsp::SineWindow);
            ofLogNotice() << "Narration using window type: SineWindow" << endl;
            break;
        case 7:
            narrCloud.setWindowType(pdsp::Welch);
            ofLogNotice() << "Narration using window type: Welch" << endl;
            break;
        case 8:
            narrCloud.setWindowType(pdsp::Gaussian);
            ofLogNotice() << "Narration using window type: Gaussian" << endl;
            break;
        case 9:
            narrCloud.setWindowType(pdsp::Tukey);
            ofLogNotice() << "Narration using window type: Tukey" << endl;
            break;
    }
    
    narrCloud.setSample(&narrSampleData); // give to the pdsp::GrainCloud the pointer to the sample
    
    // prepare narration granular with the settings from the parameters
    0.00f >> narrPosX >> narrCloud.in_position();
    narr_in_position_jitter >> narrCloud.in_position_jitter();
    narr_in_length >> narrCloud.in_length();
    narr_in_density >> narrCloud.in_density();
    narr_in_distance_jitter >> narrCloud.in_distance_jitter();
    narr_in_pitch_jitter >> narrCloud.in_pitch_jitter();
    narr_in_pitch >> narrCloud.in_pitch();
    narr_grainDirection >> narrCloud.in_direction();
    narrCompressorControls._e_compressor_in_gain >> narrOutControlL.in_mod();
    narrCompressorControls._e_compressor_in_gain >> narrOutControlR.in_mod();
    narrCompressorControls._e_compressor_in_attack >> narrOutCompressor.in_attack();
    narrCompressorControls._e_compressor_in_knee >> narrOutCompressor.in_knee();
    narrCompressorControls._e_compressor_in_ratio >> narrOutCompressor.in_ratio();
    narrCompressorControls._e_compressor_in_release >> narrOutCompressor.in_release();
    narrCompressorControls._e_compressor_in_threshold >> narrOutCompressor.in_threshold();
    narrCompressorControls._e_compressor_comp_meter.set(narrOutCompressor.meter_GR());
    
#ifndef HAS_ADC
    // setup the graphic view of the narration granualr if we are on a laptop
    narrUiWidth = ofGetWidth() / 3;
    narrUiHeigth = ofGetHeight() / 3;
    narrUiX = (ofGetWidth() - narrUiWidth) / 2;
    narrUiY = (ofGetHeight() - narrUiHeigth) / 2;
    narrUiMaxX = narrUiX + narrUiWidth;
    narrUiMaxY = narrUiY + narrUiHeigth;
    narrDrawGrains = false;
    narrWaveformGraphics.setWaveform(narrSampleData, 0, ofColor(0, 100, 100, 255), narrUiWidth, narrUiHeigth);
#endif
    
    //connect the narration to the engine
    narrAmpControl.channels(2);
    narrAmpControl.enableSmoothing(50.0f);
    narrAmpControl.set(0.0f);
    
    narrCloud.ch(0) >> narrAmpControl.ch(0)  >> narrOutControlL.out_signal() >> narrOutCompressor.ch(0) >> engine.audio_out(0);
    narrCloud.ch(1) >> narrAmpControl.ch(1)  >> narrOutControlR.out_signal() >> narrOutCompressor.ch(1) >> engine.audio_out(1);
    
}

#ifndef HAS_ADC

void ofApp::enableModeButtons(onScreenButton buttons[], int arraySize)
{
    //enables buttons for any mode - laptop only
    for (int i = 0; i < arraySize; i++)
    {
        buttons[i].enabled = true;
        buttons[i].enableAllEvents();
    }
}

//--------------------------------------------------------------
void ofApp::disableModeButtons(onScreenButton buttons[], int arraySize)
{
    //disables buttons for any mode - laptop only
    for (int i = 0; i < arraySize; i++)
    {
        
        buttons[i].disableAllEvents();
        buttons[i].enabled = false;
    }
}

void ofApp::drawModeButtons(onScreenButton buttons[], int arraySize)
{
    // draw buttons for any mode- laptop only
    ofPushStyle();
    for (int i = 0; i < arraySize; i++)
    {
        buttons[i].draw();
    }
    ofPopStyle();
}

void ofApp::setupAllButtons()
{
    //creates buttons for modes one by one
    createWaitForNarrModeButtons();
    createPlayNarrationModeButtons();
    createSwitchPresetsModeButtons();
    createMultiGrainModeButtons();
    createSingleGrainModeModeButtons();
    createNarrGuiModeButtons();
    createSimulationMultiModeButtons();
    createSimulationSingleModeButtons();
    createNarrationGlitchModeButtons();
}

void ofApp::simulationGFXUpdateRoutine()
{
    // gives values to the preview of the simulation
    for (int i = 0; i < NUMBER_OF_SENSORS; i++) {
        
        barsFull[i].set(barWidth + 2, barLength * (1.0 - normalisedA2DValues[i]) + 2, barWidth + 2);
        
    }
    
    barsFull[0].setPosition(0, -((barLength * (1.0 - normalisedA2DValues[0])) / 2), 0);
    barsFull[1].setPosition(0, ((barLength * (1.0 - normalisedA2DValues[1])) / 2), 0);
    barsFull[2].setPosition(-((barLength * (1.0 - normalisedA2DValues[2])) / 2), 0, 0);
    barsFull[3].setPosition(((barLength * (1.0 - normalisedA2DValues[3]) / 2)), 0, 0);
    barsFull[4].setPosition(0, 0, -((barLength * (1.0 - normalisedA2DValues[4])) / 2));
    barsFull[5].setPosition(0, 0, ((barLength * (1.0 - normalisedA2DValues[5])) / 2));
}

void ofApp::controlOn(int x, int y) {
    // this method is run when we use the mouse to interact with the granular single, or multi on laptop only, it is called from the mousePressed() and mouseDragged() callbacks
    
    for (int z = 0; z < numberOfSlots; z++) {
        if (x > uiX[z] && x<uiMaxX[z] && y>uiY[z] && y < uiMaxY[z]) {
            ofMap(x, uiX[z], uiMaxX[z], 0.0f, 1.0f, true) >> slots[z].cloud->in_position();
            
            currentTarget = z;
            slots[z].ampControl->set(_volume[z]);
            drawGrains[z] = true;
            controlX[z] = x;
            controlY[z] = ofMap(_volume[z], 0, 1, uiMaxY[z], uiY[z]);
            //    controlY[z] = (uiY[z] - _volume[z] * uiMaxY[z] )+ uiMaxY[z];
            cam.disableMouseInput();
        }
        else {
            cam.enableMouseInput();
        }
    }
    
}
void ofApp::controlOnNarr(int x, int y)
{
    // this method is run when we use the mouse to interact with the narration granular on laptop only, it is called from the mousePressed() and mouseDragged() callbacks
    
    if (x > narrUiX && x<narrUiMaxX && y>narrUiY && y < narrUiMaxY) {
        cam.disableMouseInput();
        ofMap(float(x), narrUiX, narrUiMaxX, 0.0f, 1.0f, true) >> narrPosX;
        narrAmpControl.set(1.0f);
        narrDrawGrains = true;
        narrControlX = x;
        narrControlY = y;
    }
    else {
        cam.enableMouseInput();
    }
}
#endif

void ofApp::setupGraincloud(std::vector<string> paths, string presetPath)
{
    // this is the loader for files and parameters for the main granular system, single and multi
#ifdef HAS_ADC
    if (presetIndex ==1) {
        blueLed.setval_gpio("1");
        redLed.setval_gpio("0");
    }
    if (presetIndex == 2) {
        blueLed.setval_gpio("1");
        redLed.setval_gpio("1");
    }
    if (presetIndex ==3) {
        blueLed.setval_gpio("1");
        redLed.setval_gpio("0");
    }
    if (presetIndex ==4) {
        blueLed.setval_gpio("0");
        redLed.setval_gpio("1");
    }
    
#endif // HAS_ADC
    positionFromTime=0;
    loadEffectPatchSettings();
    //--------GRAINCLOUD-----------------------
    for (int i = 0; i < numberOfSlots; i++) {
        
        slots[i].grainVoices = slots[i].cloud->getVoicesNum();
        // if we have samples unload them
        if (slots[i].sampleData->loaded())
        {
            slots[i].ampControl->set(0.0f);
            slots[i].sampleData->unLoad();
        }
        
        slots[i].sampleData->setVerbose(true);
        slots[i].sampleData->load(paths[i]);
        
        slots[i].cloud->setSample(slots[i].sampleData.get()); // give to the pdsp::GrainCloud the pointer to the sample
        
        
        if (firstRun) {
            samplePanels[i]->setup("Slot " + ofToString(i + 1), filePathPrefix + presetPath); //we have a GUI panel for each granular
            //the parameters are split into groups to make it easier to see, each group has value, a min, a max, and connect to to connect it to the sensor
            _windowTypeGroup_group[i].add(_window_type_id[i].set("Window type " + ofToString(i + 1), 0, 0, 9));
#ifndef HAS_ADC
            _window_type_id[i].addListener(this, &ofApp::onWindowTypeChanged);
            _windowTypeGroup_group[i].add(_window_type_name[i].set("Windowing "+ ofToString(i + 1), "default"));
#endif
            _windowTypeGroup_group[i].setName("Window Type slot " + ofToString(i + 1));
            
            samplePanels[i]->add(_windowTypeGroup_group[i]);
            
            _in_length_group[i].add(_in_length[i].set("in length " + ofToString(i + 1), 500, 10, 3000));
            _in_length_group[i].add(_in_lengthMin[i].set("in length Min " + ofToString(i + 1), 100, 10, 3000));
            _in_length_group[i].add(_in_lengthMax[i].set("in length Max " + ofToString(i + 1), 3000, 10, 3000));
            _in_length_group[i].add(in_length_connect[i].set("IL Connect to " + ofToString(i + 1), 0, 0, 6));
            _in_length_group[i].setName("In Length");
            samplePanels[i]->add(_in_length_group[i]);
            
            _in_density_group[i].add(_in_density[i].set("in density " + ofToString(i + 1), 0.9, 0.1, 1.0));
            _in_density_group[i].add(_in_densityMin[i].set("in density Min " + ofToString(i + 1), 0.1, 0.1, 1.0));
            _in_density_group[i].add(_in_densityMax[i].set("in density Max " + ofToString(i + 1), 5.0, 0.1, 1.0));
            _in_density_group[i].add(in_density_connect[i].set("ID Connect to " + ofToString(i + 1), 0, 0, 6));
            _in_density_group[i].setName("density");
            samplePanels[i]->add(_in_density_group[i]);
            
            _in_distance_jitter_group[i].add(_in_distance_jitter[i].set("distance jitter " + ofToString(i + 1), 20.0, 0.0, 1000.0));
            _in_distance_jitter_group[i].add(_in_distance_jitterMin[i].set("distance jitter min " + ofToString(i + 1), 0.0, 0.0, 1000.0));
            _in_distance_jitter_group[i].add(_in_distance_jitterMax[i].set("distance jitter max " + ofToString(i + 1), 1000.0, 0.0, 1000.0));
            _in_distance_jitter_group[i].add(in_distJit_connect[i].set("DJ Connect to " + ofToString(i + 1), 0, 0, 6));
            _in_distance_jitter_group[i].setName("Distance Jitter");
            samplePanels[i]->add(_in_distance_jitter_group[i]);
            
            _in_pitch_jitter_group[i].add(_in_pitch_jitter[i].set("pitch jitter " + ofToString(i + 1), 0.0, -200.0, 200.0));
            _in_pitch_jitter_group[i].add(_in_pitch_jitterMin[i].set("pitch jitter min " + ofToString(i + 1), -200.0, -200.0, 200.0));
            _in_pitch_jitter_group[i].add(_in_pitch_jitterMax[i].set("pitch jitter max " + ofToString(i + 1), 200.0, -200.0, 200.0));
            _in_pitch_jitter_group[i].add(in_pitchJit_connect[i].set("PJ Connect to " + ofToString(i + 1), 0, 0, 6));
            _in_pitch_jitter_group[i].setName("Pitch Jitter");
            samplePanels[i]->add(_in_pitch_jitter_group[i]);
            
            _in_pitch_group[i].add(_in_pitch[i].set("pitch " + ofToString(i + 1), 0.0, -20.0, 20.0));
            _in_pitch_group[i].add(_in_pitchMin[i].set("pitch min " + ofToString(i + 1), -20.0, -20.0, 20.0));
            _in_pitch_group[i].add(_in_pitchMax[i].set("pitch max " + ofToString(i + 1), 20.0, -20.0, 20.0));
            _in_pitch_group[i].add(in_pitch_connect[i].set("Pitch Connect to " + ofToString(i + 1), 0, 0, 6));
            _in_pitch_group[i].setName("Pitch");
            samplePanels[i]->add(_in_pitch_group[i]);
            
            _spread_group[i].add(_spread[i].set("_spread " + ofToString(i + 1), 0.0, 0.0, 1.0));
            _spread_group[i].add(_spreadMin[i].set("_spread min " + ofToString(i + 1), 0.0, 0.0, 1.0));
            _spread_group[i].add(_spreadMax[i].set("_spread max " + ofToString(i + 1), 1.0, 0.0, 1.0));
            _spread_group[i].add(_spread_connect[i].set("S Connect to " + ofToString(i + 1), 0, 0, 6));
            _spread_group[i].setName("Spread");
            samplePanels[i]->add(_spread_group[i]);
            
            _volume_group[i].add(_volume[i].set("Volume " + ofToString(i + 1), 0.5, 0.0, 1.0));
            _volume_group[i].add(_volumeMin[i].set("Volume min " + ofToString(i + 1), 0.0, 0.0, 1.0));
            _volume_group[i].add(_volumeMax[i].set("Volume max " + ofToString(i + 1), 1.0, 0.0, 1.0));
            _volume_group[i].add(_volume_connect[i].set("V Connect to " + ofToString(i + 1), 0, 0, 6));
            _volume_group[i].setName("Volume");
            samplePanels[i]->add(_volume_group[i]);
            
            _grainDirection_group[i].add(_grainDirection[i].set("grain forwards " + ofToString(i + 1), 0.0, -1.0, 1.0));
            _grainDirection_group[i].setName("Direction");
            samplePanels[i]->add(_grainDirection_group[i]);
            
            if (!useAccumulatedPressure)
            {
                // in accumulated pressure mode (single granualr) the posX value is taken automatically from all sensors together so we only need this for the multi modes
                _posX_group[i].add(_posX[i].set("Play Position " + ofToString(i + 1), 0.5, 0.0, 1.0));
                _posX_group[i].add(_posXMin[i].set("Play Position min " + ofToString(i + 1), 0.0, 0.0, 1.0));
                _posX_group[i].add(_posXMax[i].set("Play Position max " + ofToString(i + 1), 1.0, 0.0, 1.0));
                _posX_group[i].add(_posX_connect[i].set("P Connect to " + ofToString(i + 1), 0, 0, 6));
                _posX_group[i].setName("Position");
                samplePanels[i]->add(_posX_group[i]);
            }
        }
        //load from the XML
        samplePanels[i]->loadFromFile(filePathPrefix + presetPath);
        //set the XML path manually so the native save and recal settings buttons work properly with our settings
        // samplePanels[i]->setFileName(filePathPrefix + presetPath);
        
        ofLogNotice() <<" _window_type_id " + ofToString(i+1) + " from settings load form XML is " + ofToString(_window_type_id[i]) <<endl;
        
        switch (_window_type_id[i]) {
            case 0:
                slots[i].cloud->setWindowType(pdsp::Rectangular);
                ofLogNotice() << "Slot " + ofToString(i +1) + " using window type: Rectangular" << endl;
                break;
            case 1:
                slots[i].cloud->setWindowType(pdsp::Triangular);
                ofLogNotice() << "Slot " + ofToString(i +1) + " using window type: Triangular" << endl;
                break;
            case 2:
                slots[i].cloud->setWindowType(pdsp::Hann);
                ofLogNotice() << "Slot " + ofToString(i +1) + " using window type: Hann" << endl;
                break;
            case 3:
                slots[i].cloud->setWindowType(pdsp::Hamming);
                ofLogNotice() << "Slot " + ofToString(i +1) + " using window type: Hamming" << endl;
                break;
            case 4:
                slots[i].cloud->setWindowType(pdsp::Blackman);
                ofLogNotice() << "Slot " + ofToString(i +1) + " using window type: Blackman" << endl;
                break;
            case 5:
                slots[i].cloud->setWindowType(pdsp::BlackmanHarris);
                ofLogNotice() << "Slot " + ofToString(i +1) + " using window type: BlackmanHarris" << endl;
                break;
            case 6:
                slots[i].cloud->setWindowType(pdsp::SineWindow);
                ofLogNotice() << "Slot " + ofToString(i +1) + " using window type: SineWindow" << endl;
                break;
            case 7:
                slots[i].cloud->setWindowType(pdsp::Welch);
                ofLogNotice() << "Slot " + ofToString(i +1) + " using window type: Welch" << endl;
                break;
            case 8:
                slots[i].cloud->setWindowType(pdsp::Gaussian);
                ofLogNotice() << "Slot " + ofToString(i +1) + " using window type: Gaussian" << endl;
                break;
            case 9:
                slots[i].cloud->setWindowType(pdsp::Tukey);
                ofLogNotice() << "Slot " + ofToString(i +1) + " using window type: Tukey" << endl;
                break;
        }
        
#ifndef HAS_ADC
        _window_type_name[i] = windowTypeNames[_window_type_id[i]];
#endif
        
        
        // apply all base settings to the grainular objects
        _posX[i] >> slots[i].cloud->in_position();
        _spread[i] >> slots[i].cloud->in_position_jitter();
        _in_length[i] >> slots[i].cloud->in_length();
        _in_density[i] >> slots[i].cloud->in_density();
        _in_distance_jitter[i] >> slots[i].cloud->in_distance_jitter();
        _in_pitch_jitter[i] >> slots[i].cloud->in_pitch_jitter();
        _grainDirection[i] >> slots[i].cloud->in_direction();
        _in_pitch[i] >> slots[i].cloud->in_pitch();
        
        if (firstRun) {
            // we onyl need to setup the audio engine when we start
            slots[i].ampControl->channels(2);
            //slots[i].ampControl->enableSmoothing(50.0f);
            slots[i].ampControl->set(0.0f);
            
            //outputAmp[i]->channels(2);
            
            
            
            //                slots[i].cloud->ch(0) >> (*slots[i].ampControl)[0]  >> engine.audio_out(0);
            //                slots[i].cloud->ch(1) >> (*slots[i].ampControl)[1]  >> engine.audio_out(1);
#ifndef HAS_ADC
            // create the sizes and positions of the waveform displays and interaction areas of the granulars
            //ui values-------------------------------
            uiWidth[i] = 250;
            uiHeigth[i] = 120;
            uiX[i] = i * uiWidth[i] + 40 + i * 20;
            uiY[i] = 75;
            uiMaxX[i] = uiX[i] + uiWidth[i];
            uiMaxY[i] = uiY[i] + uiHeigth[i];
#endif
            
        }
#ifndef HAS_ADC
        // setup the waveform preview and the position of the GUI modules
        drawGrains[i] = false;
        waveformGraphics[i]->setWaveform(*slots[i].sampleData, 0, ofColor(0, 100, 100, 255), uiWidth[i], uiHeigth[i]);
        samplePanels[i]->setPosition(uiX[i], uiMaxY[i] + 10);
        effectsPanels[i]->setPosition(uiX[i], uiMaxY[i] + 10);
        
        
#endif
        
        
    }
    
    //This will load up different effects to be chained to the output
    
    
    
    
    ofLogNotice() << "finished patching\n";
#ifndef HAS_ADC
    //this GUI is only for ocntrolling the simulator so it is onyl for the laptop version, there is no simulator on the raspberry pi
    mainGui.setup("Main Controls", "mainGui.xml");
    mainGui.add(compressionSimSpeed.set("Compression Speed", 1, 1, 180));
    mainGui.add(compressionSimExtent.set("Compression Limit", 1024, 0, 1024));
    mainGui.add(simulatedInput.set("Simulated input", 0, 0, 1024));
    
    //we can run each sensor on and off for a simulation run
    for (int l = 0; l < NUMBER_OF_SENSORS; l++) {
        mainGui.add(compressionSims[l].set("Compression simulation " + ofToString(l + 1), 0.0, 0.0, 1.0));
        mainGui.add(sensorSimActive[l].set("Include sensor " + ofToString(l + 1), false));
    }
    
    //callback if the run simuation button is pressed
    runSimulation.addListener(this, &ofApp::runSimulationMethod);
    mainGui.add(runSimulation.set("Run Simulation", false));
    
    //callback if the curves selector is changed
    curveSelector.addListener(this, &ofApp::curveTypeChanged);
    mainGui.add(curveSelector.set("Curve type", 0, 0,3));
    
    
    mainGui.add(camDistance.set("Cam distance", 1400.0, 0.0, 10000.0));
    mainGui.setPosition(ofGetWidth() - (mainGui.getWidth() + 40), (ofGetHeight() - mainGui.getHeight()) - 20);
    
    mainGui.loadFromFile("mainGui.xml");
    ofLogNotice() << "Load routine complete" << endl;
    
#endif
    
}
#ifndef HAS_ADC
void ofApp::onNarrWindowTypeChanged(int & windowType){
    switch (narr_window_type_id) {
        case 0:
            narrCloud.setWindowType(pdsp::Rectangular);
            ofLogNotice() << "Narration using window type: Rectangular" << endl;
            break;
        case 1:
            narrCloud.setWindowType(pdsp::Triangular);
            ofLogNotice() << "Narration using window type: Triangular" << endl;
            break;
        case 2:
            narrCloud.setWindowType(pdsp::Hann);
            ofLogNotice() << "Narration using window type: Hann" << endl;
            break;
        case 3:
            narrCloud.setWindowType(pdsp::Hamming);
            ofLogNotice() << "Narration using window type: Hamming" << endl;
            break;
        case 4:
            narrCloud.setWindowType(pdsp::Blackman);
            ofLogNotice() << "Narration using window type: Blackman" << endl;
            break;
        case 5:
            narrCloud.setWindowType(pdsp::BlackmanHarris);
            ofLogNotice() << "Narration using window type: BlackmanHarris" << endl;
            break;
        case 6:
            narrCloud.setWindowType(pdsp::SineWindow);
            ofLogNotice() << "Narration using window type: SineWindow" << endl;
            break;
        case 7:
            narrCloud.setWindowType(pdsp::Welch);
            ofLogNotice() << "Narration using window type: Welch" << endl;
            break;
        case 8:
            narrCloud.setWindowType(pdsp::Gaussian);
            ofLogNotice() << "Narration using window type: Gaussian" << endl;
            break;
        case 9:
            narrCloud.setWindowType(pdsp::Tukey);
            ofLogNotice() << "Narration using window type: Tukey" << endl;
            break;
    }
#ifndef HAS_ADC
    narr_window_type_name = windowTypeNames[narr_window_type_id];
#endif
}

void ofApp::onWindowTypeChanged(int & windowType){
    for (int i =0; i<numberOfSlots; i++) {
        switch (_window_type_id[i]) {
            case 0:
                slots[i].cloud->setWindowType(pdsp::Rectangular);
                ofLogNotice() << "Slot " + ofToString(i +1) + " using window type: Rectangular" << endl;
                break;
            case 1:
                slots[i].cloud->setWindowType(pdsp::Triangular);
                ofLogNotice() << "Slot " + ofToString(i +1) + " using window type: Triangular" << endl;
                break;
            case 2:
                slots[i].cloud->setWindowType(pdsp::Hann);
                ofLogNotice() << "Slot " + ofToString(i +1) + " using window type: Hann" << endl;
                break;
            case 3:
                slots[i].cloud->setWindowType(pdsp::Hamming);
                ofLogNotice() << "Slot " + ofToString(i +1) + " using window type: Hamming" << endl;
                break;
            case 4:
                slots[i].cloud->setWindowType(pdsp::Blackman);
                ofLogNotice() << "Slot " + ofToString(i +1) + " using window type: Blackman" << endl;
                break;
            case 5:
                slots[i].cloud->setWindowType(pdsp::BlackmanHarris);
                ofLogNotice() << "Slot " + ofToString(i +1) + " using window type: BlackmanHarris" << endl;
                break;
            case 6:
                slots[i].cloud->setWindowType(pdsp::SineWindow);
                ofLogNotice() << "Slot " + ofToString(i +1) + " using window type: SineWindow" << endl;
                break;
            case 7:
                slots[i].cloud->setWindowType(pdsp::Welch);
                ofLogNotice() << "Slot " + ofToString(i +1) + " using window type: Welch" << endl;
                break;
            case 8:
                slots[i].cloud->setWindowType(pdsp::Gaussian);
                ofLogNotice() << "Slot " + ofToString(i +1) + " using window type: Gaussian" << endl;
                break;
            case 9:
                slots[i].cloud->setWindowType(pdsp::Tukey);
                ofLogNotice() << "Slot " + ofToString(i +1) + " using window type: Tukey" << endl;
                break;
        }
        
        _window_type_name[i] = windowTypeNames[_window_type_id[i]];
    }
}

#endif
void ofApp::populateVectors()
{
    std::vector<ChannelEffects> tempChannelEffects1;
    std::vector<ChannelEffects> tempChannelEffects2;
    
    //just filling stuff with blanks based on sizes read from the XML
    for (int vC = 0; vC < numberOfSlots; vC++)
    {
        
#ifndef HAS_ADC
        ofxSampleBufferPlotter* tmp_waveformGraphics = new ofxSampleBufferPlotter();
        waveformGraphics.push_back(tmp_waveformGraphics);
#endif
        
        // sampleData/cloud/ampControl/outputAmpL/outputAmpR (plus the 12
        // effect objects, allocated below by populateEffectVectors() calling
        // Slot::resetEffects()) all live in one Slot per slot now -- see
        // src/Slot.h. (posX/jitY used to be allocated here too but were
        // never read anywhere after that -- dropped, not carried over.)
        slots.emplace_back();

        bool                tmp_drawGrains;
        drawGrains.push_back(tmp_drawGrains);
        int                    tmp_uiWidth;
        uiWidth.push_back(tmp_uiWidth);
        int                    tmp_uiHeigth;
        uiHeigth.push_back(tmp_uiHeigth);
        int                    tmp_uiX;
        uiX.push_back(tmp_uiX);
        int                    tmp_uiY;
        uiY.push_back(tmp_uiY);
        int                    tmp_uiMaxX;
        uiMaxX.push_back(tmp_uiMaxX);
        int                    tmp_uiMaxY;
        uiMaxY.push_back(tmp_uiMaxY);
        int                    tmp_controlX;
        controlX.push_back(tmp_controlX);
        int                    tmp_controlY;
        controlY.push_back(tmp_controlY);
        
        auto tempPanel = make_shared<ofxPanel>();
        samplePanels.push_back(tempPanel);
        
        
        
        ofParameter<int>        tmp__window_type_id;
        _window_type_id.push_back(tmp__window_type_id);
        
#ifndef HAS_ADC
        ofParameter<string>        _temp_window_type_name;
        _window_type_name.push_back(_temp_window_type_name);
#endif
        ofParameter<float>        tmp__in_length;
        _in_length.push_back(tmp__in_length);
        ofParameter<float>        tmp__in_lengthMin;
        _in_lengthMin.push_back(tmp__in_lengthMin);
        ofParameter<float>        tmp__in_lengthMax;
        _in_lengthMax.push_back(tmp__in_lengthMax);
        ofParameter<int>        tmp_in_length_connect;
        in_length_connect.push_back(tmp_in_length_connect);
        
        ofParameter<float>        tmp__in_density;
        _in_density.push_back(tmp__in_density);
        ofParameter<float>        tmp__in_densityMin;
        _in_densityMin.push_back(tmp__in_densityMin);
        ofParameter<float>        tmp__in_densityMax;
        _in_densityMax.push_back(tmp__in_densityMax);
        ofParameter<int>        tmp_in_density_connect;
        in_density_connect.push_back(tmp_in_density_connect);
        
        ofParameter<float>        tmp__in_distance_jitter;
        _in_distance_jitter.push_back(tmp__in_distance_jitter);
        ofParameter<float>        tmp__in_distance_jitterMin;
        _in_distance_jitterMin.push_back(tmp__in_distance_jitterMin);
        ofParameter<float>        tmp__in_distance_jitterMax;
        _in_distance_jitterMax.push_back(tmp__in_distance_jitterMax);
        ofParameter<int>        tmp_in_distJit_connect;
        in_distJit_connect.push_back(tmp_in_distJit_connect);
        
        ofParameter<float>        tmp__in_pitch_jitter;
        _in_pitch_jitter.push_back(tmp__in_pitch_jitter);
        ofParameter<float>        tmp__in_pitch_jitterMin;
        _in_pitch_jitterMin.push_back(tmp__in_pitch_jitterMin);
        ofParameter<float>        tmp__in_pitch_jitterMax;
        _in_pitch_jitterMax.push_back(tmp__in_pitch_jitterMax);
        ofParameter<int>        tmp_in_pitchJit_connect;
        in_pitchJit_connect.push_back(tmp_in_pitchJit_connect);
        
        ofParameter<float>        tmp__in_pitch;
        _in_pitch.push_back(tmp__in_pitch);
        ofParameter<float>        tmp__in_pitchMin;
        _in_pitchMin.push_back(tmp__in_pitchMin);
        ofParameter<float>        tmp__in_pitchMax;
        _in_pitchMax.push_back(tmp__in_pitchMax);
        ofParameter<int>        tmp_in_pitch_connect;
        in_pitch_connect.push_back(tmp_in_pitch_connect);
        
        ofParameter<float>        tmp__spread;
        _spread.push_back(tmp__spread);
        ofParameter<float>        tmp__spreadMin;
        _spreadMin.push_back(tmp__spreadMin);
        ofParameter<float>        tmp__spreadMax;
        _spreadMax.push_back(tmp__spreadMax);
        ofParameter<int>        tmp__spread_connect;
        _spread_connect.push_back(tmp__spread_connect);
        
        ofParameter<float>        tmp__posX;
        _posX.push_back(tmp__posX);
        ofParameter<float>        tmp__posXMin;
        _posXMin.push_back(tmp__posXMin);
        ofParameter<float>        tmp__posXMax;
        _posXMax.push_back(tmp__posXMax);
        ofParameter<int>        tmp__posX_connect;
        _posX_connect.push_back(tmp__posX_connect);
        
        ofParameter<float>        tmp__volume;
        _volume.push_back(tmp__volume);
        ofParameter<float>        tmp__volumeMin;
        _volumeMin.push_back(tmp__volumeMin);
        ofParameter<float>        tmp__volumeMax;
        _volumeMax.push_back(tmp__volumeMax);
        ofParameter<int>        tmp__volume_connect;
        _volume_connect.push_back(tmp__volume_connect);
        
        ofParameter<float>        tmp__grainDirection;
        _grainDirection.push_back(tmp__grainDirection);
        
        int                    tmp_baseSensorValues;
        baseSensorValues.push_back(tmp_baseSensorValues);
        int tmp_zeroValues;
        zeroValues.push_back(tmp_zeroValues);
        
        for(int i = 0; i < NUMBER_OF_PRESETS; i++){
            string tempString;
            filePathsSet[i].push_back(tempString);
            
            string tempString1;
            fileNamesSet[i].push_back(tempString1);
        }
        
        //        string tempString;
        //        filePathsSet1.push_back(tempString);
        //        string tempString2;
        //        filePathsSet2.push_back(tempString2);
        //        string tempString3;
        //        fileNamesSet1.push_back(tempString3);
        //        string tempString4;
        //        fileNamesSet2.push_back(tempString4);
        
        ChannelEffects thisEffectSet1;
        
        tempChannelEffects1.push_back(thisEffectSet1);
        
        
        ChannelEffects thisEffectSet2;
        
        tempChannelEffects2.push_back(thisEffectSet2);
        
        
        ofParameterGroup    _windowTypeGroup_group_temp;
        _windowTypeGroup_group.push_back(_windowTypeGroup_group_temp);
        
        ofParameterGroup    _in_length_group_temp;
        _in_length_group.push_back(_in_length_group_temp);
        
        ofParameterGroup        _in_density_group_temp;
        _in_density_group.push_back(_in_density_group_temp);
        
        ofParameterGroup    _in_distance_jitter_group_temp;
        _in_distance_jitter_group.push_back(_in_distance_jitter_group_temp);
        
        ofParameterGroup        _in_pitch_jitter_group_temp;
        _in_pitch_jitter_group.push_back(_in_pitch_jitter_group_temp);
        
        ofParameterGroup    _in_pitch_group_temp;
        _in_pitch_group.push_back(_in_pitch_group_temp);
        
        ofParameterGroup        _spread_group_temp;
        _spread_group.push_back(_spread_group_temp);
        
        ofParameterGroup        _posX_group_temp;
        _posX_group.push_back(_posX_group_temp);
        
        ofParameterGroup    _volume_group_temp;
        _volume_group.push_back(_volume_group_temp);
        
        ofParameterGroup    _grainDirection_group_temp;
        _grainDirection_group.push_back(_grainDirection_group_temp);
        
        
        //------------bit crusher
        EFFBitCrushUnit temp_effBitCrushersParams;
        effBitCrushersParams.push_back(temp_effBitCrushersParams);
        
        
        //------------decimator
        EFFDecimatorUnit tempeffDecimatorsParams;
        effDecimatorsParams.push_back(tempeffDecimatorsParams);
        
        
        //------------delay
        EFFDelayUnit tempeffDelaysParams;
        effDelaysParams.push_back(tempeffDelaysParams);
        
        
        //------------Filter
        EFFFilterUnit tempeffFiltersParams;
        effFiltersParams.push_back(tempeffFiltersParams);
        
        
        //------------chorus
        EFFChorusUnit tempeffChorus;
        effChorussParams.push_back(tempeffChorus);
        
        
        //------------reverb
        EFFReverbUnit tempeffReverbsParams;
        effReverbsParams.push_back(tempeffReverbsParams);
        
        
        //------------compressor
        EFFCompressorUnit tempeffCompressorsParams;
        effCompressorsParams.push_back(tempeffCompressorsParams);
        
        auto effectPanel = make_shared<ofxPanel>();
        effectsPanels.push_back(effectPanel);
        //        ofxPanel effectPanel;
        //        effectsPanels.push_back(effectPanel);
        
        
        
    }
    effectsPatching.push_back(tempChannelEffects1);
    effectsPatching.push_back(tempChannelEffects2);
    effectsPatching.push_back(tempChannelEffects1);
    effectsPatching.push_back(tempChannelEffects2);
    
    
    populateEffectVectors();
    
    
}
void ofApp::populateEffectVectors(){
    // the 12 pdsp effect objects per slot are (re)allocated by
    // Slot::resetEffects() -- RAII, so this replaces the old vector<T*>
    // .clear() + `new`-and-push_back dance (which leaked the discarded
    // objects on every non-first call, i.e. every preset switch -- see
    // ofApp::loadEffectPatchSettings()) with no behavior change beyond that
    // leak going away. What's left here is just the GUI-facing effect
    // parameter structs, not yet folded into Slot.
    for(int i =0; i<numberOfSlots; i++){
        slots[i].resetEffects();

        EFFBitCrushUnit temp_effBitCrushersParams;
        effBitCrushersParams.push_back(temp_effBitCrushersParams);

        EFFDecimatorUnit tempeffDecimatorsParams;
        effDecimatorsParams.push_back(tempeffDecimatorsParams);

        EFFDelayUnit tempeffDelaysParams;
        effDelaysParams.push_back(tempeffDelaysParams);

        EFFFilterUnit tempeffFiltersParams;
        effFiltersParams.push_back(tempeffFiltersParams);

        EFFChorusUnit tempeffChorus;
        effChorussParams.push_back(tempeffChorus);

        EFFReverbUnit tempeffReverbsParams;
        effReverbsParams.push_back(tempeffReverbsParams);

        EFFCompressorUnit tempeffCompressorsParams;
        effCompressorsParams.push_back(tempeffCompressorsParams);
    }
}
#ifdef HAS_ADC
void ofApp::calibrateOnStart() {
    // get the pressure in the balls when we start the unit, they can all be very different
    recyclingMessage.clear();
    recyclingMessage.setAddress("/" + ofToString(unitID) + "/zeroValues");
    
    for (int i = 0; i < NUMBER_OF_SENSORS; i++) {
        
        data[i][0] = 1;
        data[i][1] = 0b10000000 | (((a2dChannel[i] & 7) << 4));
        data[i][2] = 0;
        a2d.readWrite(data[i]);//sizeof(data) );
        a2dVal[i] = 0;
        a2dVal[i] = (data[i][1] << 8) & 0b1100000000;
        a2dVal[i] |= (data[i][2] & 0xff);
        zeroValues[i] = a2dVal[i];
        ofLogNotice() << ofToString(unitID) + " zerValues for " + ofToString(i) + " = " + ofToString(zeroValues[i]) << endl;
        if (oscDebug) {
            recyclingMessage.addIntArg(zeroValues[i]);
        }
    }
    if (oscDebug && firstRun) {
        sender.sendMessage(recyclingMessage, false);
    }
    
}

void ofApp::readADCValues()
{
    //read the values from the ADC (SPI serial) they come as integers, the resolution is 10bit
    for (int i = 0; i < NUMBER_OF_SENSORS; i++) {
        
        data[i][0] = 1;
        data[i][1] = 0b10000000 | (((a2dChannel[i] & 7) << 4));
        data[i][2] = 0;
        a2d.readWrite(data[i]);//sizeof(data) );
        a2dVal[i] = 0;
        a2dVal[i] = (data[i][1] << 8) & 0b1100000000;
        a2dVal[i] |= (data[i][2] & 0xff);
        
        ofLogVerbose() << " a to d reading " + ofToString(i) + " = " + ofToString(a2dVal[i]) << endl;
    }
}

void ofApp::normaliseADCValues()
{
    
    if (oscDebug) {
        recyclingMessage.clear();
        recyclingMessage.setAddress("/" + ofToString(unitID) + "_normalisedADC");
    }
    
    // get the raw readings fromt the sensors from the ADC and then map betwen the zero values and our maximum from the XML
    
    for (int m = 0; m < NUMBER_OF_SENSORS; m++) {
        normalisedA2DValues[m] = ofMap(a2dVal[m], zeroValues[m], maxSensorValue, 0.0, 1.0, true);
        if (normalisedA2DValues[m] < normalisedA2DValuesMin)
        {
            normalisedA2DValues[m] = 0;
        }
        else if(normalisedA2DValues[m] > normalisedA2DValuesMin){
            switch (curveSelector){
                case 0:
                    
                    break;
                case 1:
                    normalisedA2DValues[m] = sqrt(normalisedA2DValues[m]);
                    break;
                case 2:
                    normalisedA2DValues[m] = quarticEaseIn(normalisedA2DValues[m]);
                    break;
                case 3:
                    normalisedA2DValues[m] = exponentialEaseIn(normalisedA2DValues[m]);
                    break;
            }
        }
        if (oscDebug) {
            recyclingMessage.addFloatArg(normalisedA2DValues[m]);
        }
        ofLogVerbose() << "normalisedA2DValues " + ofToString(m + 1) + " = " + ofToString(normalisedA2DValues[m]) << endl;
    }
    
    if (oscDebug) {
        sender.sendMessage(recyclingMessage, false);
    }
}

void ofApp::onHitRoutine()
{
    // if they make enough hits in time then we run whatever is in here, for the moment it changes presets
    switchPresets();
}
#endif // HAS_ADC

#ifndef HAS_ADC

void ofApp::runSimulationMethod(bool & run)
{
    
    //callback from the run simulation button on the GUI -laptop only
    if (operationMode != simulationOperationModeTranslate)
    {
        timeUpdateFromKey=false;
        drawGrains[0]=false;
        goToMode(simulationOperationModeTranslate);
    }
    simulationRunning = runSimulation;
    
}

void ofApp::simulatedInputChanged(int & simulatedInput)
{
    //another GUI callback, not in use now -laptop only
}

void ofApp::singleCompSimChanged(const void * guiSender, int & value)
{
    //another GUI callback, not in use now -laptop only
}
#endif // HAS_ADC

//--------------------------------------------------------------
void ofApp::keyPressed(int key){
    // here is our key interaction, laptop only
#ifndef HAS_ADC
    if (key=='f')
    {
        ofToggleFullscreen();
    }
    if(key == '?'){
        
        OF_EXIT_APP(0);
    }
    switch (operationMode)
    {
        case OP_MODE_SETUP:
            break;
            
        case OP_MODE_WAIT_FOR_NARRATION:
            if (key==' ')
            {
                if (narrationUsesSensor)
                {
                    narration.play();
                    shouldTriggerNarrationPlay = false;
                    narrationIsPlaying = true;
                    ofLogNotice() << "Triggered Narration play" << endl;
                    goToMode(OP_MODE_PLAY_NARRATION);
                }
                
            }
            break;
        case OP_MODE_PLAY_NARRATION:
            if (key == 'g')
            {
                goToMode(OP_MODE_NARRATION_GLITCH);
            }
            if (key == ' ')
            {
                ofSendMessage(ofToString(BTN_MSG_A_PAUSE_NARRATION));
            }
            break;
        case OP_MODE_SWITCH_PRESETS:
            break;
        case OP_MODE_MULTI_GRAIN_MODE:
            if (key=='p')
            {
                goToMode(OP_MODE_SWITCH_PRESETS);
            }
            if (key == ' ')
            {
                timeUpdateFromKey=false;
                drawGrains[0]=false;
                ofSendMessage(ofToString(BTN_MSG_M_OP_MODE_SIMULATION));
            }
            switch (key) {
                case '1':
                    loadRoutine(0);
                    break;
                case '2':
                    loadRoutine(1);
                    break;
                case '3':
                    loadRoutine(2);
                    break;
                case '4':
                    loadRoutine(3);
                    break;
                case '5':
                    loadRoutine(4);
                    break;
                case '6':
                    loadRoutine(5);
                    break;
            }
            if(key == 'e'){
                drawEffects=!drawEffects;
            }
            break;
            
        case OP_MODE_SINGLE_GRAIN_MODE:
            if (key == 'p')
            {
                goToMode(OP_MODE_SWITCH_PRESETS);
            }
            if (key == ' ')
            {
                timeUpdateFromKey=false;
                drawGrains[0]=false;
                ofSendMessage(ofToString(BTN_MSG_M_OP_MODE_SIMULATION));
            }
            if(key=='R'){
                positionFromTime = 0;
            }
            switch (key) {
                case '1':
                    loadRoutine(0);
                    break;
                case'r':
                    timeUpdateFromKey=!timeUpdateFromKey;
                    drawGrains[0] = timeUpdateFromKey;
                    break;
            }
            
            if(key == 'e'){
                drawEffects=!drawEffects;
            }
            
            
        case OP_MODE_NARRATION_GUI:
            break;
        case OP_MODE_SIMULATION_MULTI:
            if (key == ' ')
            {
                ofSendMessage(ofToString(BTN_MSG_A_PAUSE_SIMULATION));
            }
            if(key == 'e'){
                drawEffects=!drawEffects;
            }
            break;
        case OP_MODE_SIMULATION_SINGLE:
            if (key == ' ')
            {
                ofSendMessage(ofToString(BTN_MSG_A_PAUSE_SIMULATION));
            }
            if(key == 'e'){
                drawEffects=!drawEffects;
            }
            break;
        case OP_MODE_NARRATION_GLITCH:
            if (key == 'g')
            {
                goToMode(OP_MODE_PLAY_NARRATION);
                
            }
            break;
    }
#endif
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){
#ifndef HAS_ADC
    switch (operationMode)
    {
        case OP_MODE_SETUP:
            break;
        case OP_MODE_WAIT_FOR_NARRATION:
            break;
        case OP_MODE_PLAY_NARRATION:
            break;
        case OP_MODE_SWITCH_PRESETS:
            break;
        case OP_MODE_MULTI_GRAIN_MODE:
            break;
        case OP_MODE_SINGLE_GRAIN_MODE:
            break;
        case OP_MODE_NARRATION_GUI:
            break;
        case OP_MODE_SIMULATION_MULTI:
            break;
        case OP_MODE_SIMULATION_SINGLE:
            break;
        case OP_MODE_NARRATION_GLITCH:
            break;
    }
#endif
}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ){
    //if we move the mouse (no button pushed
#ifndef HAS_ADC
    switch (operationMode)
    {
        case OP_MODE_SETUP:
            break;
        case OP_MODE_WAIT_FOR_NARRATION:
            break;
        case OP_MODE_PLAY_NARRATION:
            break;
        case OP_MODE_SWITCH_PRESETS:
            break;
        case OP_MODE_MULTI_GRAIN_MODE:
            break;
        case OP_MODE_SINGLE_GRAIN_MODE:
            break;
        case OP_MODE_NARRATION_GUI:
            break;
        case OP_MODE_SIMULATION_MULTI:
            break;
        case OP_MODE_SIMULATION_SINGLE:
            break;
        case OP_MODE_NARRATION_GLITCH:
            narrationGlitchPlayheadPos = ofMap(x, 0, ofGetWidth(), 0.0, 1.0);
            
            break;
    }
#endif
}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){
    //if we drag the mouse- we use this for previewing the granulars
#ifndef HAS_ADC
    switch (operationMode)
    {
        case OP_MODE_SETUP:
            break;
        case OP_MODE_WAIT_FOR_NARRATION:
            break;
        case OP_MODE_PLAY_NARRATION:
            break;
        case OP_MODE_SWITCH_PRESETS:
            break;
        case OP_MODE_MULTI_GRAIN_MODE:
            controlOn(x, y);
            break;
        case OP_MODE_SINGLE_GRAIN_MODE:
            controlOn(x, y);
            break;
        case OP_MODE_NARRATION_GUI:
            controlOnNarr(x, y);
            break;
        case OP_MODE_SIMULATION_MULTI:
            break;
        case OP_MODE_SIMULATION_SINGLE:
            break;
        case OP_MODE_NARRATION_GLITCH:
            narrationGlitchPlayheadPos = ofMap(x, 0, ofGetWidth(), 0.0, 1.0);
            break;
    }
#endif
}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){
    //if we click the mouse- we use this for previewing the granulars
#ifndef HAS_ADC
    switch (operationMode)
    {
        case OP_MODE_SETUP:
            break;
        case OP_MODE_WAIT_FOR_NARRATION:
            break;
        case OP_MODE_PLAY_NARRATION:
            break;
        case OP_MODE_SWITCH_PRESETS:
            break;
        case OP_MODE_MULTI_GRAIN_MODE:
            controlOn(x, y);
            break;
        case OP_MODE_SINGLE_GRAIN_MODE:
            controlOn(x, y);
            break;
        case OP_MODE_NARRATION_GUI:
            controlOnNarr(x, y);
            break;
        case OP_MODE_SIMULATION_MULTI:
            break;
        case OP_MODE_SIMULATION_SINGLE:
            break;
        case OP_MODE_NARRATION_GLITCH:
            narrationGlitchPlayheadPos = ofMap(x, 0, ofGetWidth(), 0.0, 0.1);
            break;
    }
#endif
}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){
    //when we let go of the mouse reset out granular stuff
#ifndef HAS_ADC
    switch (operationMode)
    {
        case OP_MODE_SETUP:
            break;
        case OP_MODE_WAIT_FOR_NARRATION:
            break;
        case OP_MODE_PLAY_NARRATION:
            break;
        case OP_MODE_SWITCH_PRESETS:
            break;
        case OP_MODE_MULTI_GRAIN_MODE:
            resetValuesAfterChanges();
            break;
        case OP_MODE_SINGLE_GRAIN_MODE:
            resetValuesAfterChanges();
            break;
        case OP_MODE_NARRATION_GUI:
            narrAmpControl.set(0.0f);
            narrDrawGrains = false;
            break;
        case OP_MODE_SIMULATION_MULTI:
            break;
        case OP_MODE_SIMULATION_SINGLE:
            break;
        case OP_MODE_NARRATION_GLITCH:
            break;
    }
#endif
}

void ofApp::resetValuesAfterChanges()
{
    //resets all the granualars parameters as the should be, volume is at 0 and playhead position at 0
    for (int i = 0; i < numberOfSlots; i++) {
        slots[i].ampControl->set(0.0f);
        0.0 >> slots[i].cloud->in_position();
        _in_length[i] >> slots[i].cloud->in_length();
        _in_density[i] >> slots[i].cloud->in_density();
        _in_distance_jitter[i] >> slots[i].cloud->in_distance_jitter();
        _in_pitch_jitter[i] >> slots[i].cloud->in_pitch_jitter();
        _grainDirection[i] >> slots[i].cloud->in_direction();
        _in_pitch[i] >> slots[i].cloud->in_pitch();
        _spread[i] >> slots[i].cloud->in_position_jitter();
        drawGrains[i] = false;
    }
    
}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y){
    
}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y){
    
}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){
    
}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){
    // crappy low budget events system, openframeworks has this built in, I can call ofSendMessage() from anywhere and it will be received with this callback. I use my button message and button action definitions as messages and they arrive here and appropriate action is taken.
    switch (ofToInt(msg.message))
    {
        case BTN_MSG_A_EFFECTS_OR_GRANULAR:
            drawEffects=!drawEffects;
            break;
            
        case BTN_MSG_A_SKIP_NARRATION:
            goToMode(grainOperationModeTranslate);
            break;
            
            
        case BTN_MSG_A_RESTART_NARRATION:
            narration.stop();
            narration.setPosition(0.0);
            narration.play();
            narrationIsPlaying = true;
            break;
            
        case BTN_MSG_A_PAUSE_NARRATION:
            if (narration.getIsPaused())
            {
                narration.pause(false);
                
            }
            else if (!narration.getIsPaused())
            {
                narration.pause(true);
                
            }
            break;
            
        case BTN_MSG_M_OP_MODE_NARRATION_GLITCH:
            goToMode(OP_MODE_NARRATION_GLITCH);
            break;
            
#ifndef HAS_ADC
        case BTN_MSG_A_CURVES_OR_SIMULATION_DISPLAY:
            simulationNotCurve=!simulationNotCurve;
            break;
            
        case BTN_MSG_A_PAUSE_SIMULATION:
            simulationRunning = !simulationRunning;
            runSimulation = simulationRunning;
            break;
        case BTN_MSG_M_OP_MODE_SIMULATION:
            if (operationMode != simulationOperationModeTranslate)
            {
                goToMode(simulationOperationModeTranslate);
            }
            simulationRunning = true;
            break;
        case BTN_MSG_M_OP_MODE_NARRATION_GUI:
            goToMode(OP_MODE_NARRATION_GUI);
            break;
            
#endif // !HAS_ADC
            
            
        case BTN_MSG_M_OP_MODE_PLAY_NARRATION:
            goToMode(OP_MODE_PLAY_NARRATION);
            break;
            
            
        case BTN_MSG_M_OP_MODE_SWITCH_PRESETS:
            goToMode(OP_MODE_SWITCH_PRESETS);
            break;
            
        case BTN_MSG_M_OP_MODE_GRAIN_MODE:
            goToMode(grainOperationModeTranslate);
            break;
            
        case BTN_MSG_A_SAVE_GRANULAR:
            
            for (int i = 0; i<numberOfSlots; i++) {
                samplePanels[i]->saveToFile(filePathPrefix + unitID + "_preset_"+ ofToString(presetIndex) + ".xml");
                effectsPanels[i]->saveToFile(filePathPrefix + unitID + "_effectParameterSettings_preset_"+ ofToString(presetIndex) + ".xml");
                
            }
            
            drawGrains[0] = timeUpdateFromKey;
            
            
            break;
            
        case BTN_MSG_A_SAVE_NARRATION:
            narrPanel.saveToFile(filePathPrefix + unitID + "_narr.xml");
            break;
    }
    
}

void ofApp::loadRoutine(int target) {
#ifndef HAS_ADC
    // in the laptop mode you can load other audio files to preview
    slots[target].ampControl->set(0.0f);
    drawGrains[target] = false;
    
    
    //Open the Open File Dialog
    ofFileDialogResult openFileResult = ofSystemLoadDialog("select an audio sample");
    
    //Check if the user opened a file
    if (openFileResult.bSuccess) {
        
        string path = openFileResult.getPath();
        
        slots[target].sampleData->load(path);
        waveformGraphics[target]->setWaveform(*slots[target].sampleData, 0, ofColor(0, 100, 100, 255), uiWidth[target], uiHeigth[target]);
        fileNamesSet[presetIndex][target] = openFileResult.getName();
        
        ofLogNotice("file loaded");
        
    }
    else {
        ofLogNotice("User hit cancel");
    }
#endif
}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){
    
}



#ifdef HAS_ADC

void ofApp::deviceOnlyUpdateRoutine()
{
    // just for the raspbery pi, we get the button state
    button.getval_gpio(state_button);
    if(state_button == 0){
        state_button = 1;
    }
    else{
        state_button = 0;
    }
    // check if the button is doing something interesting
    buttonStateMachine();
    // if it si first launch run the calibration
    if (firstRun) {
        calibrateOnStart();
        firstRun = false;
    }
    //read the values from the ADC all the time
    readADCValues();
    //normalise those vaules
    normaliseADCValues();
    // this checks for the gestural input (lots of squeazing rapidly
    if(useHitGesture){
        std::array<float, HitGestureDetector::kNumSensors> sensorFrame;
        for (int i = 0; i < HitGestureDetector::kNumSensors; i++) {
            sensorFrame[i] = normalisedA2DValues[i];
        }
        if (hitGestureDetector.update(sensorFrame, ofGetElapsedTimeMillis())) {
            onHitRoutine();
        }
    }
    updateLIS3DH();
    
    
}

void ofApp::buttonStateMachine() {
    //crappy state machine for getting more interaction out of a single button, we have click, double click, triple click and medium press 3 seconds and long press 6 seconds, works ok
    //the click/double-click/triple-click/long-press timing logic itself lives in ButtonClickClassifier (src/ButtonClickClassifier.h); this just reacts to whichever event it reports
    ButtonClickClassifier::Event event = buttonClickClassifier.update(state_button == 1, ofGetElapsedTimeMillis());

    switch (event)
    {
        case ButtonClickClassifier::Event::TripleClick:
        {
            relayOut.setval_gpio("1");
            ofSleepMillis(400);
            relayOut.setval_gpio("0");

            if (oscDebug) {
                ofxOscMessage m;
                m.setAddress("/" + ofToString(unitID) + "/button");
                m.addIntArg(3);
                sender.sendMessage(m, false);
            }
            ofLogNotice() << "triple click" << endl;

            if (oscDebug) {
                ofxOscMessage m;
                m.setAddress("/" + ofToString(unitID) + "/speaker");
                m.addIntArg(1);
                sender.sendMessage(m, false);
            }
            ofLogNotice() << "Speaker Sync" << endl;
            break;
        }

        case ButtonClickClassifier::Event::DoubleClick:
        {
            ofLogNotice() << "double click" << endl;

            if (oscDebug) {
                ofxOscMessage m;
                m.setAddress("/" + ofToString(unitID) + "/button");
                m.addIntArg(2);
                sender.sendMessage(m, false);
            }
            if (!hasNarration) {
                goToMode(OP_MODE_SWITCH_PRESETS);
            }
            if (hasNarration) {
                narration.stop();
                narrationIsPlaying = false;
                ofLogNotice() << "Narration is over setting up granulars" << endl;
                narrAmpControl.set(0.0f);
                hasNarration = false;

                if (presetIndex == 1) {
                    blueLed.setval_gpio("1");
                    redLed.setval_gpio("0");
                }
                if (presetIndex == 2) {
                    blueLed.setval_gpio("1");
                    redLed.setval_gpio("1");
                }
                if (presetIndex == 3) {
                    blueLed.setval_gpio("1");
                    redLed.setval_gpio("0");
                }
                if (presetIndex == 4) {
                    blueLed.setval_gpio("1");
                    redLed.setval_gpio("1");
                }
                narration.disconnectAll();
                goToMode(grainOperationModeTranslate);
            }
            break;
        }

        case ButtonClickClassifier::Event::SingleClick:
        {
            ofLogNotice() << "single click" << endl;
            if (oscDebug) {
                ofxOscMessage m;
                m.setAddress("/" + ofToString(unitID) + "/button");
                m.addIntArg(1);
                sender.sendMessage(m, false);
            }
            break;
        }

        case ButtonClickClassifier::Event::SixSecondHold:
        {
            ofLogNotice() << "6 second click" << endl;
            if (oscDebug) {
                ofxOscMessage m;
                m.setAddress("/" + ofToString(unitID) + "/button");
                m.addIntArg(6000);
                sender.sendMessage(m, false);
            }
            if (shutdownPress) {
                exit();
            }
            break;
        }

        case ButtonClickClassifier::Event::ThreeSecondHold:
        {
            ofLogNotice() << "3 second click" << endl;
            if (oscDebug) {
                ofxOscMessage m;
                m.setAddress("/" + ofToString(unitID) + "/button");
                m.addIntArg(3000);
                sender.sendMessage(m, false);
            }
            break;
        }

        case ButtonClickClassifier::Event::None:
        default:
            break;
    }
}
#endif
