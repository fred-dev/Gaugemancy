#pragma once

#include "ofxPDSP.h"
#include "ofxGui.h"
#include "ofxXmlSettings.h"
#include "ofxOsc.h"
#include "AudioPlayer.h"

#include "EFFBitCrushUnit.h"
#include "EFFDecimatorUnit.h"
#include "EFFDelayUnit.h"
#include "EFFFilterUnit.h"
#include "EFFChorusUnit.h"
#include "EFFReverbUnit.h"
#include "EFFCompressorUnit.h"


// stupid code to set the HAS_ADC define only if not on windows or mac
#ifndef TARGET_OSX
#ifndef TARGET_WIN32
#define HAS_ADC
#else
#endif
#endif

// we only need this for windows and mac
#ifndef HAS_ADC
#include "onScreenButton.h"
#endif // !HAS_ADC

// we only need these for raspberry pi
#ifdef HAS_ADC
#include "ofxGPIO.h"
#include <stdio.h>
#include <stdlib.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#endif

#define NUMBER_OF_SENSORS 6
#define NUMBER_OF_EFFECTS 6

// these are the different modes or scenes, these defines are used to make things easier to read, they are used to set the operationMode variable

#define OP_MODE_SETUP 0                     // so far unsued, I want to make the setup interactive on the laptop version
#define OP_MODE_WAIT_FOR_NARRATION 1        // if the unit has narration (option set from XML) it loads it and then waits for input
#define OP_MODE_PLAY_NARRATION 2            // mdoe that plays a narration file
#define OP_MODE_NARRATION_GUI 6             // mode for setting parameters for the narration granular sampler mode
#define OP_MODE_NARRATION_GLITCH 9          // narration granular sampler mode, it pauses the narration and switches to a granualar synth
#define OP_MODE_SWITCH_PRESETS 3            // switches from preset 1 to preset 2, the presets load different files into the granular system
#define OP_MODE_MULTI_GRAIN_MODE 4          // general granualr mode with 6 differetn audio files loaded at once
#define OP_MODE_SINGLE_GRAIN_MODE 5         // general granualr mode with a single granular
#define OP_MODE_SIMULATION_MULTI 7          // Simulation mode with 6 differetn audio files loaded at once
#define OP_MODE_SIMULATION_SINGLE 8         // Simulation mode with 6 differetn audio files loaded at once


//these are button action messages, they are send via events to the gotMessage() event listener method, they do not change modes but trigger non modal actions
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

//these are button mode messages, they are send via events to the gotMessage() event listener method, trigger mode changes when received
#define BTN_MSG_M_OP_MODE_PLAY_NARRATION 21
#define BTN_MSG_M_OP_MODE_SWITCH_PRESETS 22
#define BTN_MSG_M_OP_MODE_GRAIN_MODE 23
#define BTN_MSG_M_OP_MODE_NARRATION_GUI 24
#define BTN_MSG_M_OP_MODE_SIMULATION 25
#define BTN_MSG_M_OP_MODE_NARRATION_GLITCH 26
#define BTN_MSG_M_EXIT 27

//size of onscree buttons
#define BUTTON_WIDTH 250
#define BUTTON_HEIGHT 45

class ofApp : public ofBaseApp{

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
		void resetValuesAfterChanges();
		void mouseEntered(int x, int y);
		void mouseExited(int x, int y);
		void windowResized(int w, int h);
		void dragEvent(ofDragInfo dragInfo);
		void gotMessage(ofMessage msg);


		void initParameters();
		void settupInitMode();
		void updateInitMode();
		void drawInitMode();
		void exitInitMode();


		void setupWaitForNarrMode();
		void updateWaitForNarrMode();
#ifndef HAS_ADC
		void drawWaitForNarrMode();
		void createWaitForNarrModeButtons();
#define totalButtonsModeWaitForNarr 6
		onScreenButton waitForNarrButtons[totalButtonsModeWaitForNarr];
#endif // HAS_ADC
		void exitWaitForNarrMode();
		

		void setupPlayNarrMode();
		void updatePlayNarrMode();
#ifndef HAS_ADC
		void drawPlayNarrMode();
		void createPlayNarrationModeButtons();
#define totalButtonsModePlayNarration 6
		onScreenButton playNarrationButtons[totalButtonsModePlayNarration];
#endif // HAS_ADC
		void exitPlayNarrMode();
		

		void setupSwitchPresetsMode();
		void updateSwitchPresetsMode();
#ifndef HAS_ADC
		void drawSwitchPresetsMode();
		void createSwitchPresetsModeButtons();
#define totalButtonsModSwitchPresets 6
		onScreenButton switchPresetsButtons[totalButtonsModSwitchPresets];
#endif // HAS_ADC
		void exitSwitchPresetsMode();
		

		void setupMultiGrainMode();
		void updateMultiGrainMode();
		void exitMultiGrainMode();

#ifndef HAS_ADC
		void drawMultiGrainMode();
		void createMultiGrainModeButtons();
#define totalButtonsModeMultiGrain 6
		onScreenButton multiGrainModeButtons[totalButtonsModeMultiGrain];


		void drawMessages();
		void drawGrainClouds();
		void drawSimulationBars();
#endif // HAS_ADC
		

		void setupSingleGrainMode();
		void updateSingleGrainMode();
		void exitSingleGrainMode();
#ifndef HAS_ADC
		void drawSingleGrainMode();
		void createSingleGrainModeModeButtons();
#define totalButtonsModeSingleGrain 6
		onScreenButton SingleGrainModeButtons[totalButtonsModeSingleGrain];
		void setupNarrGuiMode();
		void updateNarrGuiMode();
		void drawNarrGuiMode();
		void exitNarrGuiMode();
		void createNarrGuiModeButtons();
#define totalButtonsModeNarrGui 6
		onScreenButton narrGuiModeButtons[totalButtonsModeNarrGui];	

		void generateSimulatedData();
		void mapSimulatedDataSingle();
		void mapSimulatedDataMulti();
		void setupSimulationMultiMode();
		void updateSimulationMultiMode();
		void drawSimulationMultiMode();
		void exitSimulationMultiMode();
		void createSimulationMultiModeButtons();
#define totalButtonsModeSimulationMulti 6
		onScreenButton simulationMultiButtons[totalButtonsModeSimulationMulti];

		void setupSimulationSingleMode();
		void updateSimulationSingleMode();
		void drawSimulationSingleMode();
		void exitSimulationSingleMode();
		void createSimulationSingleModeButtons();
#define totalButtonsModeSimulationAccu 6
		onScreenButton simulationSingleButtons[totalButtonsModeSimulationAccu];
#endif // HAS_ADC

		void setupNarrationGlitchMode();
		void updateNarrationGlitchMode();
		void exitNarrationGlitchMode();
#ifndef HAS_ADC
		void drawNarrationGlitchMode();
		void createNarrationGlitchModeButtons();
#define totalButtonsModeNarrationGlitch 6
		onScreenButton narrationGlitchButtons[totalButtonsModeNarrationGlitch];
#endif // HAS_ADC
		
		

		int operationMode;
		void exitAnyMode();
		void goToMode(int mode);


		//OP_MODE_SETUP
		void setupParamsFromXML();
		void setupFilePaths();
#ifdef HAS_ADC
		void setupADC();
		void setupButton();
		void initLedBlue();
		void initLedRed();
		void setupSpeakerControl();
		void syncSpeaker();

		bool buttonValue;
		int clicks;
		int click1Time, click2Time, click3Time, clik1ReleaseTime;
		bool waitingForClick, clickReleased;
		bool shutdownPress;
		bool doShutdown;
#endif // HAS_ADC


		void updateParametersFromValuesSingle();
		void updateParametersFromValuesMulti();

		void getAccumulatedPressure();

		void applyDynamicValuesToParameters(int &k, std::vector<ofParameter<int>> connectTo, int v, std::vector<ofParameter<float>> parameter, std::vector<ofParameter<float>> parameterMin, std::vector<ofParameter<float>> parameterMax, string paramName);

		void switchPresets();
		int presetSwitchTimer;
		bool firstPresetSwitch;
		int presetIndex;
		int previousOperationMode;

		string dataRoot;
		string filePathPrefix;
		string unitID;
		int logLevel;

		ofxOscMessage recyclingMessage;
		bool setLocalIp, setLocalToDHCP;
		string localIpFromXML;
		int localOSCPport, remoteOSCPort;
		string remoteOSCIp;
		bool oscDebug, oscLive;
		ofxOscSender sender;


		ofxXmlSettings fileSettingsXML;
		ofxXmlSettings appSettingsXML;
		ofxXmlSettings usernameXML;
		ofxXmlSettings effectsPatchXML;

		std::vector<string> filePathsSet1;
		std::vector<string> fileNamesSet1;

		std::vector<string> filePathsSet2;
		std::vector<string> fileNamesSet2;

		//OP_MODE_WAIT_FOR_NARRATION
		void setupNarration();
		AudioPlayer			narration;
		string				narrationFilePath;
		bool				hasNarration;
		float				narrationVolume;
		bool				shouldTriggerNarrationPlay;
		float				sensorRangeForNarrationGrain;
		bool				narrationIsPlaying, doNarrationGlitch;
		bool				hasGlitchSource;
		float				narrationGlitchPlayheadPos;
		void				setupNarrationGrain();
		bool				createNarrationSettings;
		float				narrationGlitchStrand;
		bool				narrationUsesSensor;
		bool				setupNarrationParams;
		float				narrationPlayheadPos;
		bool				narrGlithByMouse;
		int					narrrationGlitchSensor;
		float				narrationGlitchThreshold;

		int                         narrGrainVoices;
		pdsp::SampleBuffer          narrSampleData;
		pdsp::GrainCloud            narrCloud;


		pdsp::ParameterAmp      narrAmpControl;
		pdsp::PatchNode         narrPosX;
		pdsp::PatchNode         narrJitY;

		ofxSampleBufferPlotter  narrWaveformGraphics;

		pdsp::ADSR              narrAmpEnv;
		pdsp::Amp               narrVoiceAmpL;
		pdsp::Amp               vnarrViceAmpR;


		void controlOnNarr(int x, int y);


		bool    narrDrawGrains;

		int     narrUiWidth;
		int     narrUiHeigth;
		int     narrUiX;
		int     narrUiY;
		int     narrUiMaxX;
		int     narrUiMaxY;
		float     narrControlX;
		int     narrControlY;

		ofxPanel narrPanel;
		ofParameter<float> narr_in_length;
		ofParameter<float> narr_in_position_jitter;
		ofParameter<float> narr_in_density;
		ofParameter<float> narr_in_distance_jitter;
		ofParameter<float> narr_in_pitch_jitter;
		ofParameter<float> narr_in_pitch;
		ofParameter<float> narr_grainDirection;

		//OP_MODE_PLAY_NARRATION

		//OP_MODE_SWITCH_PRESETS
		
		//OP_MODE_MULIT_MODE

		ofxPDSPEngine           engine;
        int engineBufferSize;
        int numberOfBuffers;
        int audioDeviceId;

		void setupGraincloud(std::vector<string> paths, string presetPath);
		void populateVectors();
	
#ifndef HAS_ADC
		std::vector<ofxSampleBufferPlotter*>  waveformGraphics;
#endif
		std::vector<int>					grainVoices;
		std::vector<pdsp::SampleBuffer*>		sampleData;
		std::vector<pdsp::GrainCloud*>		cloud;
		std::vector<pdsp::ParameterAmp*> ampControl;


		std::vector<pdsp::PatchNode*> posX;

		//std::vector<pdsp::PatchNode>		posX;
		std::vector<pdsp::PatchNode*> jitY;

		//std::vector<pdsp::PatchNode>		jitY;


		std::vector<bool>					drawGrains;
		std::vector<int>					uiWidth;
		std::vector<int>					uiHeigth;
		std::vector<int>					uiX;
		std::vector<int>					uiY;
		std::vector<int>					uiMaxX;
		std::vector<int>					uiMaxY;
		std::vector<int>					controlX;
		std::vector<int>					controlY;

		std::vector<ofxPanel>				samplePanels;

		std::vector<ofParameterGroup>		_in_length_group;
		std::vector<ofParameter<float>>		_in_length;
		std::vector<ofParameter<float>>		_in_lengthMin;
		std::vector<ofParameter<float>>		_in_lengthMax;
		std::vector<ofParameter<int>>		in_length_connect;

		std::vector<ofParameterGroup>		_in_density_group;

		std::vector<ofParameter<float>>		_in_density;
		std::vector<ofParameter<float>>		_in_densityMin;
		std::vector<ofParameter<float>>		_in_densityMax;
		std::vector<ofParameter<int>>		in_density_connect;

		std::vector<ofParameterGroup>		_in_distance_jitter_group;
		std::vector<ofParameter<float>>		_in_distance_jitter;
		std::vector<ofParameter<float>>		_in_distance_jitterMin;
		std::vector<ofParameter<float>>		_in_distance_jitterMax;
		std::vector<ofParameter<int>>		in_distJit_connect;

		std::vector<ofParameterGroup>		_in_pitch_jitter_group;
		std::vector<ofParameter<float>>		_in_pitch_jitter;
		std::vector<ofParameter<float>>		_in_pitch_jitterMin;
		std::vector<ofParameter<float>>		_in_pitch_jitterMax;
		std::vector<ofParameter<int>>		in_pitchJit_connect;

		std::vector<ofParameterGroup>		_in_pitch_group;
		std::vector<ofParameter<float>>		_in_pitch;
		std::vector<ofParameter<float>>		_in_pitchMin;
		std::vector<ofParameter<float>>		_in_pitchMax;
		std::vector<ofParameter<int>>		in_pitch_connect;

		std::vector<ofParameterGroup>		_spread_group;
		std::vector<ofParameter<float>>		_spread;
		std::vector<ofParameter<float>>		_spreadMin;
		std::vector<ofParameter<float>>		_spreadMax;
		std::vector<ofParameter<int>>		_spread_connect;

		std::vector<ofParameterGroup>		_posX_group;
		std::vector<ofParameter<float>>		_posX;
		std::vector<ofParameter<float>>		_posXMin;
		std::vector<ofParameter<float>>		_posXMax;
		std::vector<ofParameter<int>>		_posX_connect;

		std::vector<ofParameterGroup>		_volume_group;
		std::vector<ofParameter<float>>		_volume;
		std::vector<ofParameter<float>>		_volumeMin;
		std::vector<ofParameter<float>>		_volumeMax;
		std::vector<ofParameter<int>>		_volume_connect;

		std::vector<ofParameterGroup>		_grainDirection_group;
		std::vector<ofParameter<float>>		_grainDirection;

		void calibrateOnStart();
		void readADCValues();
		void normaliseADCValues();
		std::vector<int>					baseSensorValues;
		std::vector<int> zeroValues;	

		bool fireHitSwitch, isCountingHits, isCheckingHitPeaks, isCheckingHitTroughs, hadHitPeak[6], hadHitTrough[6];
		void checkForHits();
		void onHitRoutine();
		int timeBetweenHits, timeSinceLastHitPeak, timeSinceLastHitTrough, oldValues[6], hitsCollected, hitPeakChecker, maxPeakDuration, hitTroughsCollected, hitTroughChecker, maxTroughDuration, completedFullHits, requiredHits;
		float hitThreshHold, troughThreshold;

		//OP_MODE_ACCUULATE_MODE
		bool useAccumulatedPressure;
		float accumulatedPressureNormalised;
		float accumulatedPressure;
        float accumulationDenominator;

		//OP_MODE_NARRATION_GUI

		//OP_MODE_SIMULATION_MULTI
#ifndef HAS_ADC
		ofxPanel mainGui;
		ofParameter<int> compressionSimSpeed;
		ofParameter<int> compressionSimExtent;
		ofParameter<int> simulatedInput;
		ofParameter<float> compressionSims[6];
		ofParameter<bool> sensorSimActive[6];
		ofParameter<bool> runSimulation;

		bool simulationRunning, simulationRising, simulationFalling;
		void runSimulationMethod(bool &run);
		void simulatedInputChanged(int & simulatedInput);
		void singleCompSimChanged(const void * guiSender, int & value);
		bool singleCompSimActive[6];

#endif

		//OP_MODE_SIMULATION_ACCU
		int maxSensorValue;
		float normalisedA2DValues[6];
		float normalisedA2DValuesMin;
		bool firstRun;
    
        float exponentialEaseIn(float value);
        float quarticEaseIn (float value);
    
        float exponentialEaseOut(float value);
        float quarticEaseOut (float value);
    
    ofParameter<int> curveSelector;

// effects
    
    std::vector<pdsp::Bitcruncher*> bitCrusherLs;
    std::vector<pdsp::Bitcruncher*> bitCrusherRs;
    
    std::vector<pdsp::Compressor*> compressors;
    
    std::vector<pdsp::Decimator*> decimatorLs;
    std::vector<pdsp::Decimator*> decimatorRs;
    
    std::vector<pdsp::Delay*> delayLs;
    std::vector<pdsp::Delay*> delayRs;
    std::vector<pdsp::Amp*>               delaySends;
    
    std::vector<pdsp::MultiLadder4*> multiLadderFilterLs;
    std::vector<pdsp::MultiLadder4*> multiLadderFilterRs;
    
    std::vector<pdsp::DimensionChorus*> choruss;
    
    std::vector<pdsp::BasiVerb*> reverbs;
    std::vector<pdsp::Amp*>               reverbSends;
    
    bool drawEffects;
    struct ChannelEffects {
        bool hasBitCrusher;
        bool hasDecimator;
        bool hasChorus;
        bool hasFilter;
        bool hasDelay;
        bool hasReverb;
    };
    
    std::vector<ofxPanel>				effectsPanels;
    
    std::vector<std::vector<ChannelEffects>> effectsPatching;
    
    void loadEffectPatchSettings();
    
    std::vector<std::vector<ofParameterGroup>> effectsParamtersForGui;
    
    std::vector<EFFBitCrushUnit> effBitCrushersParams;
    std::vector<EFFDecimatorUnit> effDecimatorsParams;
    std::vector<EFFDelayUnit> effDelaysParams;
    std::vector<EFFFilterUnit> effFiltersParams;
    std::vector<EFFChorusUnit> effChorussParams;
    std::vector<EFFReverbUnit> effReverbsParams;
    std::vector<EFFCompressorUnit> effCompressorsParams;

    void applyDynamicValuesToBitCrusherParameters(int k, int v);
    void applyDynamicValuesToDecimatorParameters(int k, int v);
    void applyDynamicValuesToDelayParameters(int k, int v);
    void applyDynamicValuesToFilterParameters(int k, int v);
    void applyDynamicValuesToChorusParameters(int k, int v);
    void applyDynamicValuesToReverbParameters(int k, int v);

    void updateEffectParametersFromValues();
    void transferEfffectParamtersToUnits();
    void updateEffects();
    
    void updateEffectsSimulation();
 

#ifndef HAS_ADC
		//Visualiser stuff

		void setupVisualiser();
		void setupGraphicEnv();
		ofTrueTypeFont messageFont;
		ofBoxPrimitive barsEmpty[6];
		ofBoxPrimitive barsFull[6];
		float sensorValues[6];
		ofEasyCam cam; // add mouse controls for camera movement
		ofParameter<float> camDistance;
		int barLength;
		int barWidth;
		ofFbo barsFbo;
        ofRectangle simulationArea;
        bool simulationNotCurve;
        ofFbo curvesFbo;
        ofPolyline curvesLine;
        std::string curvesNames[4];
        void generateCurvedLine();
        void setupCurvesDisplay();
        void drawCurvesDisplay(int x, int y, int width, int height);
        void drawCurvesDisplaySimulationSingle(int x, int y, int width, int height);
        void drawCurvesDisplaySimulationMulti(int x, int y, int width, int height);

    
        void curveTypeChanged(int &curve);
#endif



		int currentTarget;
		void controlOn(int x, int y);

        int buttonPressTimeOut;
    
#ifndef HAS_ADC
		void enableModeButtons(onScreenButton buttons[], int arraySize);
		void disableModeButtons(onScreenButton buttons[], int arraySize);
		void drawModeButtons(onScreenButton buttons[], int arraySize);
    

		void setupAllButtons();

		void simulationGFXUpdateRoutine();
#endif
		int numberOfSlots;

		int grainOperationModeTranslate;
		int simulationOperationModeTranslate;
		void loadRoutine(int target);
#ifdef HAS_ADC
		
		void deviceOnlyUpdateRoutine();
		void buttonStateMachine();

		int glitchSensor;
		//Data stuff
		MCP a2d;
		int a2dVal[6];;
		int a2dChannel[6];
		unsigned char data[8][6];

		GPIO blueLed, redLed, relayOut;
		GPIO button;
		string state_button;
#else
		int a2dVal[6];;
#endif
};
