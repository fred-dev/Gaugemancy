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
    
// read settings from XML, user, all user settings and
	setupParamsFromXML();
    
// based on the XML settings populate the vectors with blanks
	populateVectors();
    
// read the audio file paths from the XML so we know what to load
	setupFilePaths();
    
// not  implemented, this will load up different effects to be chained to the output
	loadEffectPatchSettings();
    
// create the granualr objects we need and setup thier parameters from the ofParameter class
	setupGraincloud(filePathsSet1, unitID + "_preset_1.xml");
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
    
//setup the pin to accept input from the button- raspberry pi only
	setupButton();
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
	engine.listDevices();
	engine.setDeviceID(1); // REMEMBER TO SET THIS AT THE RIGHT INDEX!!!!
	engine.setup(44100, engineBufferSize, 3);
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
	cout << "setup visualiser" << endl;
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
	ofLogVerbose() << "Display setup" << endl;
}

void ofApp::setupGraphicEnv()
{
    // jst setting up screen stuff
	cout << "setup grapic environment" << endl;
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
// read effects settings from XML adn see whcih effects each slot needs, each slot has an effects settins struct to hod the data
	if (!effectsPatchXML.loadFile(filePathPrefix + unitID + "_effectSettings.xml")) {
		cout << "Effect settings not loaded" << endl;


	}
	else {
		cout << "Effect settings loaded" << endl;

		for (int i = 0; i < numberOfSlots; i++)
		{

				effectsPatching[i].hasPhaser = effectsPatchXML.getValue("SLOTS:SLOT_" + ofToString(i+1) + ":PHASER", 0);
				cout << "SLOTS:SLOT_" + ofToString(i + 1) + ":PHASER " + ofToString(effectsPatching[i].hasPhaser) << endl;

				effectsPatching[i].hasCompressor = effectsPatchXML.getValue("SLOTS:SLOT_" + ofToString(i + 1) + ":COMPRESSOR", 0);
				cout << "SLOTS:SLOT_" + ofToString(i + 1) + ":COMPRESSOR " + ofToString(effectsPatching[i].hasCompressor) << endl;

				effectsPatching[i].hasBitCruncher = effectsPatchXML.getValue("SLOTS:SLOT_" + ofToString(i + 1) + ":BIT_CRUNCH", 0);
				cout << "SLOTS:SLOT_" + ofToString(i + 1) + ":BIT_CRUNCH " + ofToString(effectsPatching[i].hasBitCruncher) << endl;

				effectsPatching[i].hasReverb = effectsPatchXML.getValue("SLOTS:SLOT_" + ofToString(i + 1) + ":REVERB", 0);
				cout << "SLOTS:SLOT_" + ofToString(i + 1) + ":REVERB " + ofToString(effectsPatching[i].hasReverb) << endl;

				effectsPatching[i].hasDelay = effectsPatchXML.getValue("SLOTS:SLOT_" + ofToString(i + 1) + ":DELAY", 0);
				cout << "SLOTS:SLOT_" + ofToString(i + 1) + ":DELAY " + ofToString(effectsPatching[i].hasDelay) << endl;

				effectsPatching[i].hasPhaseShifter = effectsPatchXML.getValue("SLOTS:SLOT_" + ofToString(i + 1) + ":PHASE_SHIFT", 0);
				cout << "SLOTS:SLOT_" + ofToString(i + 1) + ":PHASE_SHIFT " + ofToString(effectsPatching[i].hasPhaseShifter) << endl;
		}

	}

//create and add the parameters for each slot, not finished or implemented
	for (int j = 0; j < numberOfSlots; j++)
	{
		std::vector<ofParameterGroup> slotParams;

		if (effectsPatching[j].hasPhaser)
		{
			ofParameterGroup params;
			slotParams.push_back(params);
		}
		if (effectsPatching[j].hasCompressor)
		{
			ofParameterGroup params;
			slotParams.push_back(params);
		}
		if (effectsPatching[j].hasBitCruncher)
		{
			ofParameterGroup params;
			slotParams.push_back(params);
		}
		if (effectsPatching[j].hasReverb)
		{
			ofParameterGroup params;
			slotParams.push_back(params);
		}
		if (effectsPatching[j].hasDelay)
		{
			ofParameterGroup params;
			slotParams.push_back(params);
		}
		if (effectsPatching[j].hasPhaseShifter)
		{
			ofParameterGroup params;
			slotParams.push_back(params);
		}

		//osc1_group.setName("oscillator 1");
		effectsParamtersForGui.push_back(slotParams);
	}

// this is a stupid bit of code, the effects I can use need to be patched into each other in a single chain in one line, there is no way to insert them, so I need to take care of every possible combination of effects and patch the whole chain together at once.
	for (int e = 0; e < numberOfSlots; e++)
	{

		if (effectsPatching[e].hasPhaser)
		{

		}
		else if (effectsPatching[e].hasCompressor)
		{

		}
		else if (effectsPatching[e].hasBitCruncher)
		{

		}
		else if (effectsPatching[e].hasReverb)
		{

		}
		else if (effectsPatching[e].hasDelay)
		{

		}
		else if (effectsPatching[e].hasPhaseShifter)
		{

		}
		else if (effectsPatching[e].hasPhaser &&  effectsPatching[e].hasCompressor)
		{

		}
		else if (effectsPatching[e].hasPhaser &&  effectsPatching[e].hasBitCruncher)
		{

		}
		else if (effectsPatching[e].hasPhaser &&  effectsPatching[e].hasReverb)
		{

		}
		else if (effectsPatching[e].hasPhaser &&  effectsPatching[e].hasDelay)
		{

		}
		else if (effectsPatching[e].hasPhaser &&  effectsPatching[e].hasPhaseShifter)
		{

		}
		else if (effectsPatching[e].hasCompressor &&  effectsPatching[e].hasBitCruncher)
		{

		}
		else if (effectsPatching[e].hasCompressor &&  effectsPatching[e].hasReverb)
		{

		}
		else if (effectsPatching[e].hasCompressor &&  effectsPatching[e].hasDelay)
		{

		}
		else if (effectsPatching[e].hasCompressor &&  effectsPatching[e].hasPhaseShifter)
		{

		}
		else if (effectsPatching[e].hasBitCruncher &&  effectsPatching[e].hasReverb)
		{

		}
		else if (effectsPatching[e].hasBitCruncher &&  effectsPatching[e].hasDelay)
		{

		}
		else if (effectsPatching[e].hasBitCruncher &&  effectsPatching[e].hasPhaseShifter)
		{

		}
		else if (effectsPatching[e].hasReverb &&  effectsPatching[e].hasDelay)
		{

		}
		else if (effectsPatching[e].hasReverb &&  effectsPatching[e].hasPhaseShifter)
		{

		}
		else if (effectsPatching[e].hasDelay &&  effectsPatching[e].hasPhaseShifter)
		{

		}
		else if (effectsPatching[e].hasPhaser &&  effectsPatching[e].hasCompressor &&  effectsPatching[e].hasBitCruncher)
		{

		}
		else if (effectsPatching[e].hasPhaser &&  effectsPatching[e].hasCompressor &&  effectsPatching[e].hasReverb)
		{

		}
		else if (effectsPatching[e].hasPhaser &&  effectsPatching[e].hasCompressor &&  effectsPatching[e].hasDelay)
		{

		}
		else if (effectsPatching[e].hasPhaser &&  effectsPatching[e].hasCompressor &&  effectsPatching[e].hasPhaseShifter)
		{

		}
		else if (effectsPatching[e].hasPhaser &&  effectsPatching[e].hasBitCruncher &&  effectsPatching[e].hasReverb)
		{

		}
		else if (effectsPatching[e].hasPhaser &&  effectsPatching[e].hasBitCruncher &&  effectsPatching[e].hasDelay)
		{

		}
		else if (effectsPatching[e].hasPhaser &&  effectsPatching[e].hasBitCruncher &&  effectsPatching[e].hasPhaseShifter)
		{

		}
		else if (effectsPatching[e].hasPhaser &&  effectsPatching[e].hasReverb &&  effectsPatching[e].hasDelay)
		{

		}
		else if (effectsPatching[e].hasPhaser &&  effectsPatching[e].hasReverb &&  effectsPatching[e].hasPhaseShifter)
		{

		}
		else if (effectsPatching[e].hasPhaser &&  effectsPatching[e].hasDelay &&  effectsPatching[e].hasPhaseShifter)
		{

		}
		else if (effectsPatching[e].hasCompressor &&  effectsPatching[e].hasBitCruncher &&  effectsPatching[e].hasReverb)
		{

		}
		else if (effectsPatching[e].hasCompressor &&  effectsPatching[e].hasBitCruncher &&  effectsPatching[e].hasDelay)
		{

		}
		else if (effectsPatching[e].hasCompressor &&  effectsPatching[e].hasBitCruncher &&  effectsPatching[e].hasPhaseShifter)
		{

		}
		else if (effectsPatching[e].hasCompressor &&  effectsPatching[e].hasReverb &&  effectsPatching[e].hasDelay)
		{

		}
		else if (effectsPatching[e].hasCompressor &&  effectsPatching[e].hasReverb &&  effectsPatching[e].hasPhaseShifter)
		{

		}
		else if (effectsPatching[e].hasCompressor &&  effectsPatching[e].hasDelay &&  effectsPatching[e].hasPhaseShifter)
		{

		}
		else if (effectsPatching[e].hasBitCruncher &&  effectsPatching[e].hasReverb &&  effectsPatching[e].hasDelay)
		{

		}
		else if (effectsPatching[e].hasBitCruncher &&  effectsPatching[e].hasReverb &&  effectsPatching[e].hasPhaseShifter)
		{

		}
		else if (effectsPatching[e].hasBitCruncher &&  effectsPatching[e].hasDelay &&  effectsPatching[e].hasPhaseShifter)
		{

		}
		else if (effectsPatching[e].hasReverb &&  effectsPatching[e].hasDelay &&  effectsPatching[e].hasPhaseShifter)
		{

		}
		else if (effectsPatching[e].hasPhaser &&  effectsPatching[e].hasCompressor &&  effectsPatching[e].hasBitCruncher &&  effectsPatching[e].hasReverb)
		{

		}
		else if (effectsPatching[e].hasPhaser &&  effectsPatching[e].hasCompressor &&  effectsPatching[e].hasBitCruncher &&  effectsPatching[e].hasDelay)
		{

		}
		else if (effectsPatching[e].hasPhaser &&  effectsPatching[e].hasCompressor &&  effectsPatching[e].hasBitCruncher &&  effectsPatching[e].hasPhaseShifter)
		{

		}
		else if (effectsPatching[e].hasPhaser &&  effectsPatching[e].hasCompressor &&  effectsPatching[e].hasReverb &&  effectsPatching[e].hasDelay)
		{

		}
		else if (effectsPatching[e].hasPhaser &&  effectsPatching[e].hasCompressor &&  effectsPatching[e].hasReverb &&  effectsPatching[e].hasPhaseShifter)
		{

		}
		else if (effectsPatching[e].hasPhaser &&  effectsPatching[e].hasCompressor &&  effectsPatching[e].hasDelay &&  effectsPatching[e].hasPhaseShifter)
		{

		}
		else if (effectsPatching[e].hasPhaser &&  effectsPatching[e].hasBitCruncher &&  effectsPatching[e].hasReverb &&  effectsPatching[e].hasDelay)
		{

		}
		else if (effectsPatching[e].hasPhaser &&  effectsPatching[e].hasBitCruncher &&  effectsPatching[e].hasReverb &&  effectsPatching[e].hasPhaseShifter)
		{

		}
		else if (effectsPatching[e].hasPhaser &&  effectsPatching[e].hasBitCruncher &&  effectsPatching[e].hasDelay &&  effectsPatching[e].hasPhaseShifter)
		{

		}
		else if (effectsPatching[e].hasPhaser &&  effectsPatching[e].hasReverb &&  effectsPatching[e].hasDelay &&  effectsPatching[e].hasPhaseShifter)
		{

		}
		else if (effectsPatching[e].hasCompressor &&  effectsPatching[e].hasBitCruncher &&  effectsPatching[e].hasReverb &&  effectsPatching[e].hasDelay)
		{

		}
		else if (effectsPatching[e].hasCompressor &&  effectsPatching[e].hasBitCruncher &&  effectsPatching[e].hasReverb &&  effectsPatching[e].hasPhaseShifter)
		{

		}
		else if (effectsPatching[e].hasCompressor &&  effectsPatching[e].hasBitCruncher &&  effectsPatching[e].hasDelay &&  effectsPatching[e].hasPhaseShifter)
		{

		}
		else if (effectsPatching[e].hasCompressor &&  effectsPatching[e].hasReverb &&  effectsPatching[e].hasDelay &&  effectsPatching[e].hasPhaseShifter)
		{

		}
		else if (effectsPatching[e].hasBitCruncher &&  effectsPatching[e].hasReverb &&  effectsPatching[e].hasDelay &&  effectsPatching[e].hasPhaseShifter)
		{

		}
		else if (effectsPatching[e].hasPhaser &&  effectsPatching[e].hasCompressor &&  effectsPatching[e].hasBitCruncher &&  effectsPatching[e].hasReverb &&  effectsPatching[e].hasDelay)
		{

		}
		else if (effectsPatching[e].hasPhaser &&  effectsPatching[e].hasCompressor &&  effectsPatching[e].hasBitCruncher &&  effectsPatching[e].hasReverb &&  effectsPatching[e].hasPhaseShifter)
		{

		}
		else if (effectsPatching[e].hasPhaser &&  effectsPatching[e].hasCompressor &&  effectsPatching[e].hasBitCruncher &&  effectsPatching[e].hasDelay &&  effectsPatching[e].hasPhaseShifter)
		{

		}
		else if (effectsPatching[e].hasPhaser &&  effectsPatching[e].hasCompressor &&  effectsPatching[e].hasReverb &&  effectsPatching[e].hasDelay &&  effectsPatching[e].hasPhaseShifter)
		{

		}
		else if (effectsPatching[e].hasPhaser &&  effectsPatching[e].hasBitCruncher &&  effectsPatching[e].hasReverb &&  effectsPatching[e].hasDelay &&  effectsPatching[e].hasPhaseShifter)
		{

		}
		else if (effectsPatching[e].hasCompressor &&  effectsPatching[e].hasBitCruncher &&  effectsPatching[e].hasReverb &&  effectsPatching[e].hasDelay &&  effectsPatching[e].hasPhaseShifter)
		{

		}
		else if (effectsPatching[e].hasPhaser &&  effectsPatching[e].hasCompressor &&  effectsPatching[e].hasBitCruncher &&  effectsPatching[e].hasReverb &&  effectsPatching[e].hasDelay &&  effectsPatching[e].hasPhaseShifter)
		{

		}
		else if (!effectsPatching[e].hasPhaser &&  !effectsPatching[e].hasCompressor &&  !effectsPatching[e].hasBitCruncher &&  !effectsPatching[e].hasReverb &&  !effectsPatching[e].hasDelay && ! effectsPatching[e].hasPhaseShifter)
		{

		}
	}


}

void ofApp::exit() {

// this is called when the app exits, we unload the audio files
	for (int i = 0; i < numberOfSlots; i++) {
		grainVoices[i] = cloud[i]->getVoicesNum();
		if (sampleData[i]->loaded())
		{
			ampControl[i]->setv(0.0f);
			sampleData[i]->unLoad();
		}
	}

#ifdef HAS_ADC
// on exit we turn our lights, off, turn off the speaker and if it is set on the XML sutdown the raspberry pi
	blueLed.setval_gpio("0");
	redLed.setval_gpio("0");
	relayOut.setval_gpio("1");
	ofSleepMillis(400);
	relayOut.setval_gpio("0");
	ofLogVerbose() << "relay setup" << endl;

	if (doShutdown) {
		string cmd = "sudo shutdown -h now";           // create the command
		ofSystem(cmd.c_str());
	}
#endif
}

void ofApp::initParameters()
{
//gettting some simple stuff ready for running
	firstRun = true;
#ifdef HAS_ADC
	clicks = 0;
	waitingForClick = true;
#endif
	presetIndex = 1;
	isCheckingHitPeaks = false;
	isCheckingHitTroughs = false;
	narrationIsPlaying = false;
	presetSwitchTimer = ofGetElapsedTimeMillis();
    
#ifndef HAS_ADC
    curvesNames[0] = "Direct";
    curvesNames[1] = "Power of two";
    curvesNames[2] = "Quartic";
    curvesNames[3] = "Exponential";
    simulationNotCurve =true;
#endif

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
	cout << "setup wait for narration mode" << endl;
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
				cout << "Triggered Narration play" << endl;
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
		cout << "Triggered Narration play" << endl;
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
	cout << "setup play narration mode" << endl;
	if (narrationIsPlaying)
	{
		if (narration.getIsPaused())
		{
			narration.pause(false);
			cout << "Unpausing narration" << endl;
		}
	}
	if (!narrationIsPlaying && !narration.getIsPaused())
	{
		narration.play();
		shouldTriggerNarrationPlay = false;
		narrationIsPlaying = true;
		std::cout << "Triggered Narration play" << endl;
	}
#ifndef HAS_ADC
	enableModeButtons(playNarrationButtons, totalButtonsModePlayNarration);
#endif
}

void ofApp::updatePlayNarrMode()
{
// if the narration is finished (I dont have a real callback) we need to switch modes
	if (narration.getPosition()>0.995)
	{
		narrationIsPlaying = false;
		std::cout << "Narration is over setting up granulars" << endl;
		narrAmpControl.setv(0.0f);
		goToMode(grainOperationModeTranslate);
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
			narrAmpControl.setv(1.0);
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
			cout << "Pausing narration" << endl;
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
	cout << "setup switch presets" << endl;
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
	cout << "exit switch presets" << endl;

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
	cout << "setup multi mode" << endl;
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
		accumulatedPressureNormalised >> cloud[s]->in_position();
		_spread[s] >> cloud[s]->in_position_jitter();
		_in_length[s] >> cloud[s]->in_length();
		_in_density[s] >> cloud[s]->in_density();
		_in_distance_jitter[s] >> cloud[s]->in_distance_jitter();
		_in_pitch_jitter[s] >> cloud[s]->in_pitch_jitter();
		_grainDirection[s] >> cloud[s]->in_direction();
		_in_pitch[s] >> cloud[s]->in_pitch();
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
		_posX[s] >> cloud[s]->in_position();
		_spread[s] >> cloud[s]->in_position_jitter();
		_in_length[s] >> cloud[s]->in_length();
		_in_density[s] >> cloud[s]->in_density();
		_in_distance_jitter[s] >> cloud[s]->in_distance_jitter();
		_in_pitch_jitter[s] >> cloud[s]->in_pitch_jitter();
		_grainDirection[s] >> cloud[s]->in_direction();
		_in_pitch[s] >> cloud[s]->in_pitch();
		
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
	if (presetIndex == 1) {
		messageFont.drawString("Gaugemancy Control 1.0       User: " + unitID + " Preset: 1", 40, 60);
	}
	else if (presetIndex == 2) {
		messageFont.drawString("Gaugemancy Control 1.0       User: " + unitID + " Preset: 2", 40, 60);
	}
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
		if (presetIndex==1)
		{
			ofDrawBitmapString(fileNamesSet1[j], uiX[j] + 5, uiY[j] + 15);

		}
		if (presetIndex == 2)
		{
			ofDrawBitmapString(fileNamesSet2[j], uiX[j] + 5, uiY[j] + 15);
		}
		if (drawGrains[j]) {
			//draw position crossdraw grains 
			ofDrawLine(controlX[j], uiY[j], controlX[j], uiMaxY[j]);
			ofDrawLine(uiX[j], controlY[j], uiMaxX[j], controlY[j]);
			//draw grains
			ofSetRectMode(OF_RECTMODE_CENTER);
			int grainsY = uiY[j] + uiHeigth[j] / 2;
			for (int k = 0; k < grainVoices[j]; ++k) {
				float xpos = uiX[j] + (uiWidth[j] * cloud[j]->meter_position(k));
				float dimensionX = cloud[j]->meter_env(k) * 10;
				float dimensionY = cloud[j]->meter_env(k) * 50;
				ofDrawRectangle(xpos, grainsY, dimensionX, dimensionY);
			}
		}
		ofSetColor(255);
		ofDrawBitmapString("Slot " + ofToString(j + 1), uiX[j] + 5, uiHeigth[j] + uiY[j] - 10);
		ofPopStyle();
		samplePanels[j].draw();
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
	cout << "exit multi mode" << endl;
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
			multiGrainModeButtons[i].setup("Program Narration", BTN_MSG_M_OP_MODE_NARRATION_GUI);
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
	cout << "setup single grain mode" << endl;
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
	cout << "exit Single mode" << endl;
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
			SingleGrainModeButtons[i].setup("Program Narration", BTN_MSG_M_OP_MODE_NARRATION_GUI);
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
	cout << "setup narration gui mode" << endl;
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
	cout << "setup simulation mulit mode" << endl;
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
	cout << "Exit simulation mulit mode" << endl;

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
			simulationMultiButtons[i].setup("Program Narration", BTN_MSG_M_OP_MODE_NARRATION_GUI);
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
	cout << "setup simulation single mode" << endl;
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
	cout << "Exit simulation single mode" << endl;
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
			simulationSingleButtons[i].setup("Program Narration", BTN_MSG_M_OP_MODE_NARRATION_GUI);
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
	narrAmpControl.setv(1.0);
	cout << "setup narration glitch mode" << endl;

}

void ofApp::updateNarrationGlitchMode()
{
    // we get to narration glitch mode on the device by pressing one of the sensors, that sensor value is then used to move the playhead of the narraiton granular. If the sensor values goes below the threshold we go back to the  narration play mode
#ifdef HAS_ADC
	deviceOnlyUpdateRoutine();
	narration.getPosition() + ofMap(normalisedA2DValues[narrrationGlitchSensor], 0.0, 1.0, -1.0 * narrationGlitchStrand, 0) >> narrCloud.in_position();

	if (normalisedA2DValues[narrrationGlitchSensor] < narrationGlitchThreshold)
	{
		goToMode(OP_MODE_PLAY_NARRATION);
	}
	// if we are on a laptop the sensor data is take from the mouse x position
#else
	(narration.getPosition() + ofMap(narrationGlitchPlayheadPos, 0.0, 1.0, -1.0 * narrationGlitchStrand, 0, true))  >> narrCloud.in_position();
	narrControlX = ofMap(narration.getPosition() + ofMap(narrationGlitchPlayheadPos, 0.0, 1.0, -1.0 * narrationGlitchStrand, 0), 0, 1, narrUiX, narrUiMaxX);
	cout << ofToString(narrControlX) << endl;
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
	narrAmpControl.setv(0.0);

		
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


void ofApp::setupParamsFromXML()
{
    // partty!! this is where I read all the settings for the system. To make in manageable the settings are named with the performers names as a prefix, the user profile is set in the username.xml. This is then used to decide which settings to use. becuase the system runs on a raspberry pi and that pie is stuck deep inside the unit, and I dont want thme to have to log in, conect to wife etc, I just map a USP stick to a fixed path on the PI. the settings and audio files are altered day to day by replacing them or adding them to the USB stick and starting the device.
    // get the username
	if (!usernameXML.loadFile(filePathPrefix + "username.xml")) {
		cout << "Username settings not loaded" << endl;

	}
	else {
		cout << "Username settings loaded" << endl;
	}
    
	unitID = usernameXML.getValue("NAMES:USERNAME", "NO_UNIT_ID");
	cout << "UNIT_ID = " + ofToString(unitID) << endl;

// based on the username load that app settings profile
	if (!appSettingsXML.loadFile(filePathPrefix + unitID + "_appSettings.xml")) {
		cout << "App settings not loaded" << endl;

	}
	else {
		cout << "App settings loaded" << endl;
	}
    // I can change log levels to give me different kinds of feedback from the system when I am debugging (via ssh or VNC)
	logLevel = appSettingsXML.getValue("SETTINGS:LOG_LEVEL", 2);
	switch (logLevel)
	{
	case 0:
		ofSetLogLevel(OF_LOG_VERBOSE);
		break;
	case 1:
		ofSetLogLevel(OF_LOG_ERROR);
		break;
	case 2:
		ofSetLogLevel(OF_LOG_SILENT);
		break;

	}

//do we have a narration file
	hasNarration = appSettingsXML.getValue("SETTINGS:HAS_NARRATION", 0);
	cout << "HAS_NARRATION = " + ofToString(hasNarration) << endl;
    
//what volume to we play the narration at
	narrationVolume = appSettingsXML.getValue("SETTINGS:NARRATION_VOLUME", 1.0);
	cout << "NARRATION_VOLUME = " + ofToString(narrationVolume) << endl;

// sensor input threshold for moving between narration play and narration granular
    narrationGlitchThreshold = appSettingsXML.getValue("SETTINGS:NARRATION_GLITCH_THRESH", 0.025);
    cout << "NARRATION_GLITCH_THRESH = " + ofToString(narrationGlitchThreshold) << endl;
    
// when the sensor data is used to manipulate the narration granular how far should it move the playhead
    narrationGlitchStrand = appSettingsXML.getValue("SETTINGS:NARRATION_GLITCH_STRAND", 0.0025);
    cout << "NARRATION_GLITCH_STRAND = " + ofToString(narrationGlitchStrand) << endl;
    
// if the unit has narration we have an option of how it is started, it can run automatically after some time, or it can be triggered by pushing the sensor
    narrationUsesSensor = appSettingsXML.getValue("SETTINGS:NARR_PLAY_WITH_SENSOR", false);
    cout << "NARR_PLAY_WITH_SENSOR = " + ofToString(narrationUsesSensor) << endl;
    
// what is the maximum value we think a performer can get to, we will use this for the top end of the value scaling
	maxSensorValue = appSettingsXML.getValue("SETTINGS:MAX_SENSOR_VALUE", 520);
	cout << "MAX_SENSOR_VALUE = " + ofToString(maxSensorValue) << endl;

// sensor input threshold for engaging interaction (otherise the units will make random sounds from noise in the ADC
    normalisedA2DValuesMin = appSettingsXML.getValue("SETTINGS:ACTIVE_THRESHOLD", 0.0025);
    cout << "ACTIVE_THRESHOLD = " + ofToString(normalisedA2DValuesMin) << endl;
    
// chose a curve to transform incoming sensor or simulation data
    curveSelector = appSettingsXML.getValue("SETTINGS:EASING_SELECTOR", 0);
    cout << "EASING_SELECTOR = " + ofToString(curveSelector) << endl;

// accumulated or multi mode uses the combination of all sensor input to move the plahead of the granular objects. So the full postion is only reached by pressing all the balls in at once. The performers cannot push all in at once so instead of dividing the aggregated sensor data by 6 I divide it by this (it was 4 as that is how many sensors they can engage at once, but it was still too hard to for them to reach the end point so here it is a shitty hack
    accumulationDenominator = appSettingsXML.getValue("SETTINGS:ACCUMULATION_DENOMINATOR", 4);
    cout << "ACCUMULATION_DENOMINATOR = " + ofToString(accumulationDenominator) << endl;
    
// On some systems I can reduce the latency by reducing the audio buffer size
    engineBufferSize = appSettingsXML.getValue("SETTINGS:BUFFER_SIZE", 512);
    cout << "BUFFER_SIZE = " + ofToString(engineBufferSize) << endl;
    
// this is for a gestural input, where the performers squeeze several sensors rapidly, this is the timeout period between letting of the push and the next push
    maxTroughDuration = appSettingsXML.getValue("SETTINGS:MAX_TROUGH_DURATION", 250);
    cout << "MAX_TROUGH_DURATION = " + ofToString(maxTroughDuration) << endl;
    
// this is for a gestural input, where the performers squeeze several sensors rapidly, this is the timeout period between a push and the letting off the push
    maxPeakDuration = appSettingsXML.getValue("SETTINGS:MAX_PEAK_DURATION", 80);
    cout << "MAX_PEAK_DURATION = " + ofToString(maxPeakDuration) << endl;
    
// this is for a gestural input, where the performers squeeze several sensors rapidly, number of pushes need to trigger an action
    requiredHits = appSettingsXML.getValue("SETTINGS:REQUIRED_HITS", 8);
    cout << "REQUIRED_HITS = " + ofToString(requiredHits) << endl;
    
// this is for a gestural input, where the performers squeeze several sensors rapidly, how hard does a press have to be
    hitThreshHold = appSettingsXML.getValue("SETTINGS:HIT_THRESHOLD", 0.085);
    cout << "HIT_THRESHOLD = " + ofToString(hitThreshHold) << endl;
    
// this is for a gestural input, where the performers squeeze several sensors rapidly, how soft does a non press have to be
    troughThreshold = appSettingsXML.getValue("SETTINGS:TROUGH_THRESHOLD", 0.025);
    cout << "TROUGH_THRESHOLD = " + ofToString(troughThreshold) << endl;
    
// in accumulated pressure we go to single grain mode
    useAccumulatedPressure = appSettingsXML.getValue("SETTINGS:ACCUMULATED_PRESSURE", false);
    cout << "ACCUMULATED_PRESSURE = " + ofToString(useAccumulatedPressure) << endl;
    
    // change the number of slots and the simulation and grain modes depending on the accumulated pressure setting
    if (useAccumulatedPressure)
    {
        numberOfSlots = 1;
        cout << "Using 1 slot and accumulated pressure" << endl;
        grainOperationModeTranslate = OP_MODE_SINGLE_GRAIN_MODE;
        simulationOperationModeTranslate = OP_MODE_SIMULATION_SINGLE;
    }
    else if (!useAccumulatedPressure)
    {
        numberOfSlots = 6;
        cout << "Using 6 slots and mappable pressure pressure" << endl;
        
        grainOperationModeTranslate = OP_MODE_MULTI_GRAIN_MODE;
        simulationOperationModeTranslate = OP_MODE_SIMULATION_MULTI;
    }
    
//not implemented yet, but if the systems need to on a network and talking to me or each other should I set the IP
	setLocalIp = appSettingsXML.getValue("SETTINGS:SET_LOCAL_IP", false);
	cout << "SET_LOCAL_IP = " + ofToString(setLocalIp) << endl;

//not implemented yet, but if the systems need to on a network and talking to me and I should set the IP what should I set it to
	localIpFromXML = appSettingsXML.getValue("SETTINGS:XML_IP", "192.168.1.15");
	cout << "XML_IP = " + ofToString(localIpFromXML) << endl;

//not implemented yet, but if the systems need to on a network and talking to me should I set the IP to DHCP
	setLocalToDHCP = appSettingsXML.getValue("SETTINGS:SET_DHCP", false);
	cout << "SET_DHCP = " + ofToString(setLocalToDHCP) << endl;

//not implemented yet, but if the systems need to on a network and talking to me I use OSC, which is packaged UDP messages, this is the local port
    localOSCPport = appSettingsXML.getValue("SETTINGS:LOCAL_OSC_PORT", 1234);
	cout << "LOCAL_OSC_PORT = " + ofToString(localOSCPport) << endl;

//This is implemented, I can send OSC data (for now debug data), this is the remote port
	remoteOSCPort = appSettingsXML.getValue("SETTINGS:REMOTE_OSC_PORT", 1235);
	cout << "REMOTE_OSC_PORT = " + ofToString(remoteOSCPort) << endl;

//This is implemented, I can send OSC data (for now debug data), this is the remote IP
	remoteOSCIp = appSettingsXML.getValue("SETTINGS:REMOTE_OSC_IP", "192.168.178.236");
	cout << "REMOTE_OSC_IP = " + ofToString(remoteOSCIp) << endl;
    
//If OSC debug is on, it will send OSC messages as well as console debug messages
	oscDebug = appSettingsXML.getValue("SETTINGS:OSC_DEBUG", false);
	cout << "OSC_DEBUG = " + ofToString(oscDebug) << endl;

//not implemented if OSC live is on, it will send OSC messages to allow me to duplicate the interaction on a laptop in real time when the room is too big for the built in speakers
	oscLive = appSettingsXML.getValue("SETTINGS:OSC_LIVE", false);
	cout << "OSC_LIVE =" + ofToString(oscLive) << endl;
    
#ifdef HAS_ADC
//device only if we are counting double or trupple presses on the soft button, how long to wait before we reset counting
    buttonPressTimeOut = appSettingsXML.getValue("SETTINGS:BUTTON_PRESS_MAX_WAIT", 220);
    cout << "BUTTON_PRESS_MAX_WAIT = " + ofToString(buttonPressTimeOut) << endl;
    
// should the application close from a button interaction
    shutdownPress = appSettingsXML.getValue("SETTINGS:SH_P", false);
    cout << "SH_P = " + ofToString(shutdownPress) << endl;
    
// should the application closing shut town the raspberry pi
    doShutdown = appSettingsXML.getValue("SETTINGS:D_SH", false);
    cout << "D_SH = " + ofToString(doShutdown) << endl;
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
	if (fileSettingsXML.loadFile(filePathPrefix + unitID + "_fileSettings.xml")) {
		cout << "File settings loaded" << endl;
		for (int i = 0; i < numberOfSlots; i++) {
			cout << "Getting paths from file settings " + ofToString(i)<< endl;
			filePathsSet1[i] = filePathPrefix + "audio/" + fileSettingsXML.getValue("PATHS:FILE_" + ofToString(i + 1) + "A", "Grain_" + ofToString(i + 1) + ".mp3");
			fileNamesSet1[i] = fileSettingsXML.getValue("PATHS:FILE_" + ofToString(i + 1) + "A", "Grain_" + ofToString(i + 1) + ".mp3");
			cout << "File path " + ofToString(i + 1) + "A  = " + filePathsSet1[i] << endl;
			filePathsSet2[i] = filePathPrefix + "audio/" + fileSettingsXML.getValue("PATHS:FILE_" + ofToString(i + 1) + "B", "Grain_" + ofToString(i + 1) + ".mp3");
			fileNamesSet2[i] = fileSettingsXML.getValue("PATHS:FILE_" + ofToString(i + 1) + "B", "Grain_" + ofToString(i + 1) + ".mp3");
			cout << "File path " + ofToString(i + 1) + "B  = " + filePathsSet2[i] << endl;	

			if (oscDebug) {
                // if we are using OSC debug send the names
				recyclingMessage.clear();
				recyclingMessage.setAddress("/" + ofToString(unitID) + "/filePaths");
				recyclingMessage.addStringArg(filePathsSet1[i]);
				recyclingMessage.addStringArg(filePathsSet2[i]);
				sender.sendMessage(recyclingMessage, false);
			}
		}
	}
	else
		cout << "File settings not loaded" << endl;

	narrationFilePath = filePathPrefix + "audio/" + fileSettingsXML.getValue("PATHS:NARRATION_FILE", "Mono_20hz_-20db_netV.wav");
}

#ifdef HAS_ADC

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

	ofLogVerbose() << "adc setup" << endl;

}

void ofApp::setupButton()
{
    //setup the soft button for interaction - raspberry pi only
	button.setup("19", "in", "high");
	button.export_gpio();
	button.setdir_gpio("in");

	if (oscDebug) {
		recyclingMessage.clear();
		recyclingMessage.setAddress("/" + ofToString(unitID) + "/button");
		recyclingMessage.addIntArg(1);
		sender.sendMessage(recyclingMessage, false);
	}
}


void ofApp::initLedBlue()
{
    //setup the blue LED pin for feedback - raspberry pi only
	blueLed.setup("5");
	blueLed.export_gpio();
	blueLed.setdir_gpio("out");
	ofSleepMillis(200);
	blueLed.setval_gpio("1");

	if (oscDebug) {
		recyclingMessage.clear();
		recyclingMessage.setAddress("/" + ofToString(unitID) + "/blueLed");
		recyclingMessage.addIntArg(1);
		sender.sendMessage(recyclingMessage, false);
	}
	ofLogVerbose() << "Blue LED setup" << endl;
}

void ofApp::initLedRed()
{
    //setup the red LED pin for feedback - raspberry pi only
	redLed.setup("6");
	redLed.export_gpio();
	redLed.setdir_gpio("out");
	ofSleepMillis(200);
	redLed.setval_gpio("1");
	ofSleepMillis(200);
	redLed.setval_gpio("0");

	if (oscDebug) {
		recyclingMessage.clear();
		recyclingMessage.setAddress("/" + ofToString(unitID) + "/redLed");
		recyclingMessage.addIntArg(1);
		sender.sendMessage(recyclingMessage, false);
	}

	ofLogVerbose() << "Red LED setup" << endl;
}

void ofApp::setupSpeakerControl()
{
    //setup the relay to turn the speaker on and off - raspberry pi only
	cout << "setup speaker control" << endl;
	relayOut.setup("13");
	relayOut.export_gpio();
	relayOut.setdir_gpio("out");
	ofSleepMillis(200);

	if (oscDebug) {
		recyclingMessage.clear();
		recyclingMessage.setAddress("/" + ofToString(unitID) + "/relayOut");
		recyclingMessage.addIntArg(1);
		sender.sendMessage(recyclingMessage, false);
	}
	ofLogVerbose() << "relay setup" << endl;
}

void ofApp::syncSpeaker()
{
    //Turn on the speaker - raspberry pi only
	relayOut.setval_gpio("1");
	ofSleepMillis(400);
	relayOut.setval_gpio("0");

}

#endif // HAS_ADC


void ofApp::updateParametersFromValuesSingle()
{
    // go through the parameters of all the granular objects, if a parameters is connected to incoming sensor data, map that data appropriately, can also come from the simulator
	for (int k = 0; k < numberOfSlots; k++) {
		for (int v = 0; v < NUMBER_OF_SENSORS; v++) {				
			applyDynamicValuesToParameters(k, in_length_connect, v, _in_length, _in_lengthMin, _in_lengthMax);
			applyDynamicValuesToParameters(k, in_density_connect, v, _in_density, _in_densityMin, _in_densityMax);
			applyDynamicValuesToParameters(k, in_distJit_connect, v, _in_distance_jitter, _in_distance_jitterMin, _in_distance_jitterMax);
			applyDynamicValuesToParameters(k, in_pitch_connect, v, _in_pitch, _in_pitchMin, _in_pitchMax);
			applyDynamicValuesToParameters(k, in_pitchJit_connect, v, _in_pitch_jitter, _in_pitch_jitterMin, _in_pitch_jitterMax);
			applyDynamicValuesToParameters(k, _volume_connect, v, _volume, _volumeMin, _volumeMax);
            applyDynamicValuesToParameters(k, _spread_connect, v, _spread, _spreadMin, _spreadMax);

            
		}
	}
	getAccumulatedPressure();
}

void ofApp::updateParametersFromValuesMulti()
{
    // go through the parameters of all the granular objects, if a parameters is connected to incoming sensor data, map that data appropriately, can also come from the simulator
	for (int k = 0; k < numberOfSlots; k++) {
		for (int v = 0; v < NUMBER_OF_SENSORS; v++) {
			applyDynamicValuesToParameters(k, in_length_connect, v, _in_length, _in_lengthMin, _in_lengthMax);
			applyDynamicValuesToParameters(k, in_density_connect, v, _in_density, _in_densityMin, _in_densityMax);
			applyDynamicValuesToParameters(k, in_distJit_connect, v, _in_distance_jitter, _in_distance_jitterMin, _in_distance_jitterMax);
			applyDynamicValuesToParameters(k, in_pitch_connect, v, _in_pitch, _in_pitchMin, _in_pitchMax);
			applyDynamicValuesToParameters(k, in_pitchJit_connect, v, _in_pitch_jitter, _in_pitch_jitterMin, _in_pitch_jitterMax);
			applyDynamicValuesToParameters(k, _volume_connect, v, _volume, _volumeMin, _volumeMax);
			applyDynamicValuesToParameters(k, _posX_connect, v, _posX, _posXMin, _posXMax);
            applyDynamicValuesToParameters(k, _spread_connect, v, _spread, _spreadMin, _spreadMax);

		}
	}
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
			accumulatedPressureNormalised >> cloud[0]->in_position();
			ampControl[0]->setv(_volume[0]);
			ofLogVerbose() << "Accumulated pressure is " + ofToString(accumulatedPressureNormalised) << endl;

		}
    // of it does not pass the threshold then reset the volume and playhead position
		else if (accumulatedPressureNormalised<normalisedA2DValuesMin) {
			0.0 >> cloud[0]->in_position();
			ampControl[0]->setv(0.0);
		}
	
}

void ofApp::applyDynamicValuesToParameters(int &k, std::vector<ofParameter<int>> connectTo, int v, std::vector<ofParameter<float>> parameter, std::vector<ofParameter<float>> parameterMin, std::vector<ofParameter<float>> parameterMax)
{
    // if a parameter is mapped to a sensor then apply the data and map it between the min and max
	if (connectTo[k] == v + 1) {

		if (normalisedA2DValues[v]<normalisedA2DValuesMin)
		{
			ampControl[k]->setv(0.0);
		}
		else
		{
			//drawGrains[k] = true;
			parameter[k] = ofMap(normalisedA2DValues[v], 0.0, 1.0, parameterMin[k], parameterMax[k]);
			ampControl[k]->setv(_volume[k]);
		}

		ofLogVerbose() << "in length" + ofToString(k) + " = " + ofToString(parameter[k]) + " mapped between " + ofToString(parameterMin[k]) + " and " + ofToString(parameterMax[k]) << endl;
	}
}

void ofApp::switchPresets()
{
// I had a problem switching too fast on raspberry pi so I make a time to check it cannot happen within 10 seconds of the last switchPresets(), the raspberry pi changes the LED colour in this process
#ifdef HAS_ADC
	if (ofGetElapsedTimeMillis() - presetSwitchTimer > 10000)
	{
        redLed.setval_gpio("0");
#endif // HAS_ADC

		switch (presetIndex) {
		case 1:
			setupGraincloud(filePathsSet2, unitID + "_preset_2.xml");
			presetIndex = 2;
			cout << "Preset index 2 loaded" << endl;
			break;
		case 2:
			setupGraincloud(filePathsSet1, unitID + "_preset_1.xml");
			presetIndex = 1;
			cout << "Preset index 1 loaded" << endl;
		default:
			break;
		}
		presetSwitchTimer = ofGetElapsedTimeMillis();
#ifdef HAS_ADC
        redLed.setval_gpio("1");
	}
#endif // HAS_ADC
}


void ofApp::setupNarration() {
// this is not modal, but the method for the first narration setup, load the file, setup the granualr for the narration and connect it to the audio engine.
	cout << "setup narration for system" << endl;

	narrationIsPlaying = false;
	if (hasNarration) {
		setupNarrationGrain();

		narration.load(narrationFilePath);
		if (narration.isLoaded()) {
			cout << "Loaded narration audio file from " + narrationFilePath << endl;
			shouldTriggerNarrationPlay = true;

			narration.out("0") >> engine.audio_out(0);
			narration.out("0") >> engine.audio_out(1);
		}
		else {
			cout << "Error loading narration audio file" << endl;
			shouldTriggerNarrationPlay = false;
		}
		goToMode(OP_MODE_WAIT_FOR_NARRATION);
	}

}
void ofApp::setupNarrationGrain()
{
// called from setupNarration, it sets up the granular and its settings form XML for the narration (if it has it)
	cout << "setup narration grain" << endl;
	narrGrainVoices = narrCloud.getVoicesNum();
	// sampleData is an instance of pdsp::SampleBuffer
	narrSampleData.setVerbose(true);
	narrSampleData.load(narrationFilePath);
	narrCloud.setWindowType(pdsp::Triangular); // select the shape of the grain envelope
											   // available windows: Rectangular, Triangular, Hann, Hamming, Blackman, BlackmanHarris, SineWindow, Tukey, Welch
	narrCloud.setSample(&narrSampleData); // give to the pdsp::GrainCloud the pointer to the sample

	narrPanel.setup("Narration", filePathPrefix + unitID + "_narr.xml", 100, 100); // Seting up this GUI module and adding the parameters to it

	narrPanel.add(narr_in_length.set("in length", 500, 10, 3000));
	narrPanel.add(narr_in_density.set("in density", 0.9, 0.1, 1.0));
	narrPanel.add(narr_in_distance_jitter.set("distance jitter", 0.0, 0.0, 1000.0));
	narrPanel.add(narr_in_pitch_jitter.set("pitch jitter", 0.0, 0.0, 200.0));
	narrPanel.add(narr_in_pitch.set("pitch ", 0.0, -10.0, 10.0));
	narrPanel.add(narr_in_position_jitter.set("spread", 0.0, 0.0, 1.0));
	narrPanel.add(narr_grainDirection.set("direction", 0.0, -1.0, 1.0));

// load settings from the XML
	narrPanel.loadFromFile(filePathPrefix + unitID + "_narr.xml");

// prepare narration granular with the settings from the parameters
	0.00f >> narrPosX >> narrCloud.in_position();
	narr_in_position_jitter >> narrCloud.in_position_jitter();
	narr_in_length >> narrCloud.in_length();
	narr_in_density >> narrCloud.in_density();
	narr_in_distance_jitter >> narrCloud.in_distance_jitter();
	narr_in_pitch_jitter >> narrCloud.in_pitch_jitter();
	narr_in_pitch >> narrCloud.in_pitch();
	narr_grainDirection >> narrCloud.in_direction();

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
	narrAmpControl.setv(0.0f);

	narrCloud.out_L() >> narrAmpControl[0] * dB(12.0f) >> engine.audio_out(0);
	narrCloud.out_R() >> narrAmpControl[1] * dB(12.0f) >> engine.audio_out(1);
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
            ofMap(x, uiX[z], uiMaxX[z], 0.0f, 1.0f, true) >> cloud[z]->in_position();
            
            currentTarget = z;
            ampControl[z]->setv(_volume[z]);
            drawGrains[z] = true;
            controlX[z] = x;
            controlY[z] = ofMap(_volume[z], 0, 1, uiMaxY[z], uiY[z]);
            //	controlY[z] = (uiY[z] - _volume[z] * uiMaxY[z] )+ uiMaxY[z];
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
		narrAmpControl.setv(1.0f);
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
	ofSleepMillis(200);

#endif // HAS_ADC

	//--------GRAINCLOUD-----------------------
	for (int i = 0; i < numberOfSlots; i++) {

		grainVoices[i] = cloud[i]->getVoicesNum();
// if we have samples unload them
		if (sampleData[i]->loaded())
		{
			ampControl[i]->setv(0.0f);
			sampleData[i]->unLoad();
		}

		sampleData[i]->setVerbose(true);
		sampleData[i]->load(paths[i]);
		cloud[i]->setWindowType(pdsp::SineWindow, 2048);
		cloud[i]->setSample(sampleData[i]); // give to the pdsp::GrainCloud the pointer to the sample

		if (firstRun) {
			samplePanels[i].setup("Slot " + ofToString(i + 1)); //we have a GUI panel for each granular
            //the parameters are split into groups to make it easier to see, each group has value, a min, a max, and connect to to connect it to the sensor
			_in_length_group[i].add(_in_length[i].set("in length " + ofToString(i + 1), 500, 10, 3000));
			_in_length_group[i].add(_in_lengthMin[i].set("in length Min " + ofToString(i + 1), 100, 10, 3000));
			_in_length_group[i].add(_in_lengthMax[i].set("in length Max " + ofToString(i + 1), 3000, 10, 3000));
			_in_length_group[i].add(in_length_connect[i].set("IL Connect to " + ofToString(i + 1), 0, 0, 6));
			_in_length_group[i].setName("In Length");
			samplePanels[i].add(_in_length_group[i]);

			_in_density_group[i].add(_in_density[i].set("in density " + ofToString(i + 1), 0.9, 0.1, 1.0));
			_in_density_group[i].add(_in_densityMin[i].set("in density Min " + ofToString(i + 1), 0.1, 0.1, 1.0));
			_in_density_group[i].add(_in_densityMax[i].set("in density Max " + ofToString(i + 1), 5.0, 0.1, 1.0));
			_in_density_group[i].add(in_density_connect[i].set("ID Connect to " + ofToString(i + 1), 0, 0, 6));
			_in_density_group[i].setName("density");
			samplePanels[i].add(_in_density_group[i]);

			_in_distance_jitter_group[i].add(_in_distance_jitter[i].set("distance jitter " + ofToString(i + 1), 20.0, 0.0, 1000.0));
			_in_distance_jitter_group[i].add(_in_distance_jitterMin[i].set("distance jitter min " + ofToString(i + 1), 0.0, 0.0, 1000.0));
			_in_distance_jitter_group[i].add(_in_distance_jitterMax[i].set("distance jitter max " + ofToString(i + 1), 1000.0, 0.0, 1000.0));
			_in_distance_jitter_group[i].add(in_distJit_connect[i].set("DJ Connect to " + ofToString(i + 1), 0, 0, 6));
			_in_distance_jitter_group[i].setName("Distance Jitter");
			samplePanels[i].add(_in_distance_jitter_group[i]);

			_in_pitch_jitter_group[i].add(_in_pitch_jitter[i].set("pitch jitter " + ofToString(i + 1), 0.0, -200.0, 200.0));
			_in_pitch_jitter_group[i].add(_in_pitch_jitterMin[i].set("pitch jitter min " + ofToString(i + 1), -200.0, -200.0, 200.0));
			_in_pitch_jitter_group[i].add(_in_pitch_jitterMax[i].set("pitch jitter max " + ofToString(i + 1), 200.0, -200.0, 200.0));
			_in_pitch_jitter_group[i].add(in_pitchJit_connect[i].set("PJ Connect to " + ofToString(i + 1), 0, 0, 6));
			_in_pitch_jitter_group[i].setName("Pitch Jitter");
			samplePanels[i].add(_in_pitch_jitter_group[i]);

			_in_pitch_group[i].add(_in_pitch[i].set("pitch " + ofToString(i + 1), 0.0, -20.0, 20.0));
			_in_pitch_group[i].add(_in_pitchMin[i].set("pitch min " + ofToString(i + 1), -20.0, -20.0, 20.0));
			_in_pitch_group[i].add(_in_pitchMax[i].set("pitch max " + ofToString(i + 1), 20.0, -20.0, 20.0));
			_in_pitch_group[i].add(in_pitch_connect[i].set("Pitch Connect to " + ofToString(i + 1), 0, 0, 6));
			_in_pitch_group[i].setName("Pitch");
			samplePanels[i].add(_in_pitch_group[i]);

			_spread_group[i].add(_spread[i].set("_spread " + ofToString(i + 1), 0.0, 0.0, 1.0));
			_spread_group[i].add(_spreadMin[i].set("_spread min " + ofToString(i + 1), 0.0, 0.0, 1.0));
			_spread_group[i].add(_spreadMax[i].set("_spread max " + ofToString(i + 1), 1.0, 0.0, 1.0));
			_spread_group[i].add(_spread_connect[i].set("S Connect to " + ofToString(i + 1), 0, 0, 6));
			_spread_group[i].setName("Spread");
			samplePanels[i].add(_spread_group[i]);

			_volume_group[i].add(_volume[i].set("Volume " + ofToString(i + 1), 0.5, 0.0, 1.0));
			_volume_group[i].add(_volumeMin[i].set("Volume min " + ofToString(i + 1), 0.0, 0.0, 1.0));
			_volume_group[i].add(_volumeMax[i].set("Volume max " + ofToString(i + 1), 1.0, 0.0, 1.0));
			_volume_group[i].add(_volume_connect[i].set("V Connect to " + ofToString(i + 1), 0, 0, 6));
			_volume_group[i].setName("Volume");
			samplePanels[i].add(_volume_group[i]);

			_grainDirection_group[i].add(_grainDirection[i].set("grain forwards " + ofToString(i + 1), 0.0, -1.0, 1.0));
			_grainDirection_group[i].setName("Direction");
			samplePanels[i].add(_grainDirection_group[i]);

			if (!useAccumulatedPressure)
			{
                // in accumulated pressure mode (single granualr) the posX value is taken automatically from all sensors together so we only need this for the multi modes
				_posX_group[i].add(_posX[i].set("Play Position " + ofToString(i + 1), 0.5, 0.0, 1.0));
				_posX_group[i].add(_posXMin[i].set("Play Position min " + ofToString(i + 1), 0.0, 0.0, 1.0));
				_posX_group[i].add(_posXMax[i].set("Play Position max " + ofToString(i + 1), 1.0, 0.0, 1.0));
				_posX_group[i].add(_posX_connect[i].set("P Connect to " + ofToString(i + 1), 0, 0, 6));
				_posX_group[i].setName("Position");
				samplePanels[i].add(_posX_group[i]);
			}
		}
        //load from the XML
		samplePanels[i].loadFromFile(filePathPrefix + presetPath);
        //set the XML path manually so the native save and recal settings buttons work properly with our settings
		samplePanels[i].setFileName(filePathPrefix + presetPath);

        // apply all base settings to the grainular objects
			0.00f >> (*posX[i]) >> cloud[i]->in_position();
			_spread[i] >> cloud[i]->in_position_jitter();
			_in_length[i] >> cloud[i]->in_length();
			_in_density[i] >> cloud[i]->in_density();
			_in_distance_jitter[i] >> cloud[i]->in_distance_jitter();
			_in_pitch_jitter[i] >> cloud[i]->in_pitch_jitter();
			_grainDirection[i] >> cloud[i]->in_direction();
			_in_pitch[i] >> cloud[i]->in_pitch();

			if (firstRun) {
            // we onyl need to setup the audio engine when we start
				ampControl[i]->channels(2);
				//ampControl[i]->enableSmoothing(50.0f);
				ampControl[i]->setv(0.0f);

				cloud[i]->out_L() >> (*ampControl[i])[0] * dB(12.0f) >> engine.audio_out(0);
				cloud[i]->out_R() >> (*ampControl[i])[1] * dB(12.0f) >> engine.audio_out(1);
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
			waveformGraphics[i]->setWaveform(*sampleData[i], 0, ofColor(0, 100, 100, 255), uiWidth[i], uiHeigth[i]);
			samplePanels[i].setPosition(uiX[i], uiMaxY[i] + 10);

#endif
		

	}

	ofLogVerbose() << "finished patching\n";
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
	cout << "Load routine complete" << endl;
#else
    // waiting a bit on the raspberry pi as it was flipping out otherwsie
	ofSleepMillis(200);

#endif // HAS_ADC
	
}

void ofApp::populateVectors()
{
    //just filling stuff with blanks based on sizes read from the XML
	for (int vC = 0; vC < numberOfSlots; vC++)
	{

#ifndef HAS_ADC
		ofxSampleBufferPlotter* tmp_waveformGraphics = new ofxSampleBufferPlotter();
		waveformGraphics.push_back(tmp_waveformGraphics);
#endif

		int					tmp_grainVoices;
		grainVoices.push_back(tmp_grainVoices);

		pdsp::SampleBuffer* tmp_sampleData = new pdsp::SampleBuffer();
		sampleData.push_back(tmp_sampleData);

		pdsp::GrainCloud* tmp_cloud = new pdsp::GrainCloud();
		cloud.push_back(tmp_cloud);

		pdsp::ParameterAmp* tmp_ampControl = new pdsp::ParameterAmp();
		ampControl.push_back(tmp_ampControl);

		pdsp::PatchNode* tmp_posX = new pdsp::PatchNode();
		posX.push_back(tmp_posX);

		pdsp::PatchNode* tmp_jitY = new pdsp::PatchNode();
		jitY.push_back(tmp_jitY);


		bool				tmp_drawGrains;
		drawGrains.push_back(tmp_drawGrains);
		int					tmp_uiWidth;
		uiWidth.push_back(tmp_uiWidth);
		int					tmp_uiHeigth;
		uiHeigth.push_back(tmp_uiHeigth);
		int					tmp_uiX;
		uiX.push_back(tmp_uiX);
		int					tmp_uiY;
		uiY.push_back(tmp_uiY);
		int					tmp_uiMaxX;
		uiMaxX.push_back(tmp_uiMaxX);
		int					tmp_uiMaxY;
		uiMaxY.push_back(tmp_uiMaxY);
		int					tmp_controlX;
		controlX.push_back(tmp_controlX);
		int					tmp_controlY;
		controlY.push_back(tmp_controlY);

		ofxPanel				tmp_samplePanels;
		samplePanels.push_back(tmp_samplePanels);
		ofParameter<float>		tmp__in_length;
		_in_length.push_back(tmp__in_length);
		ofParameter<float>		tmp__in_lengthMin;
		_in_lengthMin.push_back(tmp__in_lengthMin);
		ofParameter<float>		tmp__in_lengthMax;
		_in_lengthMax.push_back(tmp__in_lengthMax);
		ofParameter<int>		tmp_in_length_connect;
		in_length_connect.push_back(tmp_in_length_connect);

		ofParameter<float>		tmp__in_density;
		_in_density.push_back(tmp__in_density);
		ofParameter<float>		tmp__in_densityMin;
		_in_densityMin.push_back(tmp__in_densityMin);
		ofParameter<float>		tmp__in_densityMax;
		_in_densityMax.push_back(tmp__in_densityMax);
		ofParameter<int>		tmp_in_density_connect;
		in_density_connect.push_back(tmp_in_density_connect);

		ofParameter<float>		tmp__in_distance_jitter;
		_in_distance_jitter.push_back(tmp__in_distance_jitter);
		ofParameter<float>		tmp__in_distance_jitterMin;
		_in_distance_jitterMin.push_back(tmp__in_distance_jitterMin);
		ofParameter<float>		tmp__in_distance_jitterMax;
		_in_distance_jitterMax.push_back(tmp__in_distance_jitterMax);
		ofParameter<int>		tmp_in_distJit_connect;
		in_distJit_connect.push_back(tmp_in_distJit_connect);

		ofParameter<float>		tmp__in_pitch_jitter;
		_in_pitch_jitter.push_back(tmp__in_pitch_jitter);
		ofParameter<float>		tmp__in_pitch_jitterMin;
		_in_pitch_jitterMin.push_back(tmp__in_pitch_jitterMin);
		ofParameter<float>		tmp__in_pitch_jitterMax;
		_in_pitch_jitterMax.push_back(tmp__in_pitch_jitterMax);
		ofParameter<int>		tmp_in_pitchJit_connect;
		in_pitchJit_connect.push_back(tmp_in_pitchJit_connect);

		ofParameter<float>		tmp__in_pitch;
		_in_pitch.push_back(tmp__in_pitch);
		ofParameter<float>		tmp__in_pitchMin;
		_in_pitchMin.push_back(tmp__in_pitchMin);
		ofParameter<float>		tmp__in_pitchMax;
		_in_pitchMax.push_back(tmp__in_pitchMax);
		ofParameter<int>		tmp_in_pitch_connect;
		in_pitch_connect.push_back(tmp_in_pitch_connect);

		ofParameter<float>		tmp__spread;
		_spread.push_back(tmp__spread);
		ofParameter<float>		tmp__spreadMin;
		_spreadMin.push_back(tmp__spreadMin);
		ofParameter<float>		tmp__spreadMax;
		_spreadMax.push_back(tmp__spreadMax);
		ofParameter<int>		tmp__spread_connect;
		_spread_connect.push_back(tmp__spread_connect);

		ofParameter<float>		tmp__posX;
		_posX.push_back(tmp__posX);
		ofParameter<float>		tmp__posXMin;
		_posXMin.push_back(tmp__posXMin);
		ofParameter<float>		tmp__posXMax;
		_posXMax.push_back(tmp__posXMax);
		ofParameter<int>		tmp__posX_connect;
		_posX_connect.push_back(tmp__posX_connect);

		ofParameter<float>		tmp__volume;
		_volume.push_back(tmp__volume);
		ofParameter<float>		tmp__volumeMin;
		_volumeMin.push_back(tmp__volumeMin);
		ofParameter<float>		tmp__volumeMax;
		_volumeMax.push_back(tmp__volumeMax);
		ofParameter<int>		tmp__volume_connect;
		_volume_connect.push_back(tmp__volume_connect);

		ofParameter<float>		tmp__grainDirection;
		_grainDirection.push_back(tmp__grainDirection);

		int					tmp_baseSensorValues;
		baseSensorValues.push_back(tmp_baseSensorValues);
		int tmp_zeroValues;
		zeroValues.push_back(tmp_zeroValues);

		string tempString;
		filePathsSet1.push_back(tempString);
		string tempString2;
		filePathsSet2.push_back(tempString2);
		string tempString3;
		fileNamesSet1.push_back(tempString3);
		string tempString4;
		fileNamesSet2.push_back(tempString4);
		
		ChannelEffects thisEffectSet;
		effectsPatching.push_back(thisEffectSet);

		ofParameterGroup	_in_length_group_temp;
		_in_length_group.push_back(_in_length_group_temp);
			
		ofParameterGroup		_in_density_group_temp;
		_in_density_group.push_back(_in_density_group_temp);

		ofParameterGroup	_in_distance_jitter_group_temp;
		_in_distance_jitter_group.push_back(_in_distance_jitter_group_temp);

		ofParameterGroup		_in_pitch_jitter_group_temp;
		_in_pitch_jitter_group.push_back(_in_pitch_jitter_group_temp);

		ofParameterGroup	_in_pitch_group_temp;
		_in_pitch_group.push_back(_in_pitch_group_temp);

		ofParameterGroup		_spread_group_temp;
		_spread_group.push_back(_spread_group_temp);

		ofParameterGroup		_posX_group_temp;
		_posX_group.push_back(_posX_group_temp);

		ofParameterGroup	_volume_group_temp;
		_volume_group.push_back(_volume_group_temp);

		ofParameterGroup	_grainDirection_group_temp;
		_grainDirection_group.push_back(_grainDirection_group_temp);
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
		cout << ofToString(unitID) + " zerValues for " + ofToString(i) + " = " + ofToString(zeroValues[i]) << endl;
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

void ofApp::checkForHits()
{
	// this method checks for gestural input, the performers squeeing the units hard and rapidly, they can use this to change presets kind of shitty way about it but it works
		if (!isCheckingHitPeaks && !isCheckingHitTroughs)
		{
			ofLogNotice() << "checking for any peaks" << endl;
			for (int u = 0; u < NUMBER_OF_SENSORS; u++)
			{
				if (normalisedA2DValues[u] > hitThreshHold)
				{

					hitPeakChecker += 1;
					oldValues[u] = normalisedA2DValues[u];
					hadHitPeak[u] = true;
				}
			}
			if (hitPeakChecker>1)
			{
				ofLogNotice() << "found peaks, checking for trough" << endl;
				isCheckingHitTroughs = true;
				timeSinceLastHitTrough = ofGetElapsedTimeMillis();
				hitPeakChecker = 0;

			}
			else
			{
				ofLogNotice() << "No peaks found, exiting this bit" << endl;

				hitPeakChecker = 0;
				for (int z = 0; z < NUMBER_OF_SENSORS; z++)
				{
					oldValues[z] = 0;
					hadHitPeak[z] = false;
				}

			}
		}

		if (ofGetElapsedTimeMillis() - timeSinceLastHitTrough < maxTroughDuration && isCheckingHitTroughs)
		{
			hitTroughChecker = 0;
			ofLogNotice() << "inside checking troughs" << endl;
			for (int u = 0; u < NUMBER_OF_SENSORS; u++)
			{
				hadHitTrough[u] = false;
				if (hadHitPeak[u])
				{

					if (normalisedA2DValues[u] < troughThreshold)
					{
						hitTroughChecker += 1;
						hadHitTrough[u] = true;


					}
					if (hitTroughChecker > 1)
					{
						ofLogNotice() << ofToString(hitTroughChecker) + " troughs found" << endl;
						completedFullHits += 1;
						if (completedFullHits > requiredHits - 1)
						{
							ofLogNotice() << "Total hits reached target " + ofToString(completedFullHits) << endl;

							timeSinceLastHitPeak = 0;
							timeSinceLastHitTrough = 0;
							hitTroughChecker = 0;
							completedFullHits = 0;
							for (int t = 0; t < NUMBER_OF_SENSORS; t++)
							{
								hadHitPeak[t] = false;
								hadHitTrough[t] = false;
							}
							isCheckingHitTroughs = false;
							isCheckingHitPeaks = false;
							onHitRoutine();
							goto finished;
						}
						hitTroughChecker = 0;
						timeSinceLastHitPeak = ofGetElapsedTimeMillis();
						isCheckingHitPeaks = true;
						isCheckingHitTroughs = false;
					}
				}
			}

		}
	finished:
		if (ofGetElapsedTimeMillis() - timeSinceLastHitTrough > maxTroughDuration && isCheckingHitTroughs)
		{
			if (hitTroughChecker > 1)
			{
				ofLogNotice() << ofToString(hitTroughChecker) + " troughs found" << endl;
				completedFullHits += 1;
				if (completedFullHits > requiredHits - 1)
				{
					ofLogNotice() << "Total hits reached target " + ofToString(completedFullHits) << endl;
					completedFullHits = 0;
					onHitRoutine();
				}
				hitTroughChecker = 0;
				timeSinceLastHitPeak = ofGetElapsedTimeMillis();
				isCheckingHitPeaks = true;
				isCheckingHitTroughs = false;
			}

			else {
				isCheckingHitTroughs = false;
				isCheckingHitPeaks = false;
				timeSinceLastHitPeak = 0;
				timeSinceLastHitTrough = 0;
				hitTroughChecker = 0;
				completedFullHits = 0;
				for (int t = 0; t < NUMBER_OF_SENSORS; t++)
				{
					hadHitPeak[t] = false;
					hadHitTrough[t] = false;
				}
				ofLogNotice() << "Hit count aborted no suitable trough" << endl;

			}
		}

		if (ofGetElapsedTimeMillis() - timeSinceLastHitPeak < maxPeakDuration && isCheckingHitPeaks)
		{
			ofLogNotice() << "inside checking peaks" << endl;
			hitPeakChecker = 0;
			for (int u = 0; u < NUMBER_OF_SENSORS; u++)
			{
				hadHitPeak[u] = false;
				if (hadHitTrough[u])
				{

					if (normalisedA2DValues[u] > hitThreshHold)
					{
						hitPeakChecker += 1;

						oldValues[u] = normalisedA2DValues[u];
						hadHitPeak[u] = true;
						if (hitPeakChecker > 1)
						{
							isCheckingHitTroughs = true;
							timeSinceLastHitTrough = false;
							hitPeakChecker = 0;
							ofLogNotice() << "Enough peaks found going to check troughs" << endl;
							timeSinceLastHitTrough = ofGetElapsedTimeMillis();

						}
					}
				}
			}
		}
		if (ofGetElapsedTimeMillis() - timeSinceLastHitPeak > maxPeakDuration && isCheckingHitPeaks)
		{
			if (hitPeakChecker > 1)
			{
				isCheckingHitTroughs = true;
				timeSinceLastHitTrough = false;
				hitPeakChecker = 0;
				ofLogNotice() << "Enough peaks found going to check troughs" << endl;
				timeSinceLastHitTrough = ofGetElapsedTimeMillis();

			}
			else {
				isCheckingHitPeaks = false;
				isCheckingHitTroughs = false;
				timeSinceLastHitPeak = 0;
				timeSinceLastHitTrough = 0;
				hitPeakChecker = 0;
				for (int t = 0; t < NUMBER_OF_SENSORS; t++)
				{
					hadHitPeak[t] = false;
					hadHitTrough[t] = false;
				}
				ofLogNotice() << "Hit count aborted no suitable peaks" << endl;

			}
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
				cout << "Triggered Narration play" << endl;
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
	case OP_MODE_SINGLE_GRAIN_MODE:
		if (key == 'p')
		{
			goToMode(OP_MODE_SWITCH_PRESETS);
		}
		if (key == ' ')
		{
			ofSendMessage(ofToString(BTN_MSG_M_OP_MODE_SIMULATION));
		}
		switch (key) {
		case '1':
			loadRoutine(0);
			break;
		}
		break;
	case OP_MODE_NARRATION_GUI:
		break;
	case OP_MODE_SIMULATION_MULTI:
		if (key == ' ')
		{
			ofSendMessage(ofToString(BTN_MSG_A_PAUSE_SIMULATION));
		}
		break;
	case OP_MODE_SIMULATION_SINGLE:
		if (key == ' ')
		{
			ofSendMessage(ofToString(BTN_MSG_A_PAUSE_SIMULATION));
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
		narrAmpControl.setv(0.0f);
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
		ampControl[i]->setv(0.0f);
		0.0 >> cloud[i]->in_position();
		_in_length[i] >> cloud[i]->in_length();
		_in_density[i] >> cloud[i]->in_density();
		_in_distance_jitter[i] >> cloud[i]->in_distance_jitter();
		_in_pitch_jitter[i] >> cloud[i]->in_pitch_jitter();
		_grainDirection[i] >> cloud[i]->in_direction();
		_in_pitch[i] >> cloud[i]->in_pitch();
		_spread[i] >> cloud[i]->in_position_jitter();
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
		if (presetIndex == 1) {
			for (int i = 0; i<numberOfSlots; i++) {
				samplePanels[i].saveToFile(filePathPrefix + unitID + "_preset_1.xml");
			}
		}
		else if (presetIndex == 2) {
			for (int i = 0; i<numberOfSlots; i++) {
				samplePanels[i].saveToFile(filePathPrefix + unitID + "_preset_2.xml");
			}
		}
		break;

	case BTN_MSG_A_SAVE_NARRATION:
		narrPanel.saveToFile(filePathPrefix + unitID + "_narr.xml");
		break;
	}

}

void ofApp::loadRoutine(int target) {
#ifndef HAS_ADC
    // in the laptop mode you can load other audio files to preview
	ampControl[target]->setv(0.0f);
	drawGrains[target] = false;


	//Open the Open File Dialog
	ofFileDialogResult openFileResult = ofSystemLoadDialog("select an audio sample");

	//Check if the user opened a file
	if (openFileResult.bSuccess) {

		string path = openFileResult.getPath();

		sampleData[target]->load(path);
		waveformGraphics[target]->setWaveform(*sampleData[target], 0, ofColor(0, 100, 100, 255), uiWidth[target], uiHeigth[target]);

		if (presetIndex == 1)
		{
			fileNamesSet1[target] = openFileResult.getName();
		}
		if (presetIndex==2)
		{
			fileNamesSet2[target] = openFileResult.getName();
		}

		ofLogVerbose("file loaded");

	}
	else {
		ofLogVerbose("User hit cancel");
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
	checkForHits();

}

void ofApp::buttonStateMachine() {
    //crappy state machine for getting more interaction out of a single button, we have click, double click, triple click and medium press 3 seconds and long press 6 seconds, works ok
	if (ofGetElapsedTimeMillis() > 5000)
	{
		if (clicks == 3 && ofGetElapsedTimeMillis() - click3Time > buttonPressTimeOut && waitingForClick)
		{
			clicks = 0;
			click1Time = 0;
			click2Time = 0;
			click3Time = 0;
			relayOut.setval_gpio("1");
			ofSleepMillis(400);
			relayOut.setval_gpio("0");

			if (oscDebug) {
				ofxOscMessage m;
				m.setAddress("/" + ofToString(unitID) + "/button");
				m.addIntArg(3);
				sender.sendMessage(m, false);
			}
			ofLogVerbose() << "triple click" << endl;

			if (oscDebug) {
				ofxOscMessage m;
				m.setAddress("/" + ofToString(unitID) + "/speaker");
				m.addIntArg(1);
				sender.sendMessage(m, false);
			}
			ofLogVerbose() << "Speaker Sync" << endl;

		}

		else if (clicks == 2 && ofGetElapsedTimeMillis() - click2Time > buttonPressTimeOut && waitingForClick)
		{
			clicks = 0;
			click1Time = 0;
			click2Time = 0;
			ofLogVerbose() << "double click" << endl;

			if (oscDebug) {
				ofxOscMessage m;
				m.setAddress("/" + ofToString(unitID) + "/button");
				m.addIntArg(2);
				sender.sendMessage(m, false);
			}

			switchPresets();
		}

		if (clicks == 1 && ofGetElapsedTimeMillis() - click1Time > buttonPressTimeOut && waitingForClick)
		{
			clicks = 0;
			click1Time = 0;
			ofLogVerbose() << "single click" << endl;
			if (oscDebug) {
				ofxOscMessage m;
				m.setAddress("/" + ofToString(unitID) + "/button");
				m.addIntArg(1);
				sender.sendMessage(m, false);
			}

		}

		if (ofToInt(state_button) == 0 && clicks == 0 && waitingForClick)
		{
			clicks = 1;
			click1Time = ofGetElapsedTimeMillis();
		}
		else if (ofToInt(state_button) == 0 && clicks == 1 && ofGetElapsedTimeMillis() - click1Time < buttonPressTimeOut && waitingForClick)
		{
			clicks = 2;
			click2Time = ofGetElapsedTimeMillis();
		}
		else if (ofToInt(state_button) == 0 && clicks == 2 && ofGetElapsedTimeMillis() - click2Time < buttonPressTimeOut && waitingForClick)
		{
			clicks = 3;
			click3Time = ofGetElapsedTimeMillis();
		}

		if (ofToInt(state_button) == 0)
		{
			waitingForClick = false;
		}
		if (ofToInt(state_button) == 1)
		{
			waitingForClick = true;
			if (clicks == 1)
			{
				clik1ReleaseTime = ofGetElapsedTimeMillis();
				if (clik1ReleaseTime - click1Time > 6000)
				{
					ofLogVerbose() << "6 second click" << endl;
					click1Time = 0;
					clicks = 0;
					clik1ReleaseTime = 0;
					if (oscDebug) {
						ofxOscMessage m;
						m.setAddress("/" + ofToString(unitID) + "/button");
						m.addIntArg(6000);
						sender.sendMessage(m, false);
					}
					if (shutdownPress) {
						OF_EXIT_APP(0);
					}
				}

				if (clik1ReleaseTime - click1Time > 3000)
				{
					ofLogVerbose() << "3 second click" << endl;
					click1Time = 0;
					clicks = 0;
					clik1ReleaseTime = 0;
					if (oscDebug) {
						ofxOscMessage m;
						m.setAddress("/" + ofToString(unitID) + "/button");
						m.addIntArg(3000);
						sender.sendMessage(m, false);
					}
				}
			}
		}
	}
}
#endif