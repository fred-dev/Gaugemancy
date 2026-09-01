#pragma once

// Raspberry Pi only: SPI ADC (pressure sensors), I2C accelerometer, GPIO LEDs/relay/button,
// the button click state machine, and the multi-sensor "hit gesture" detector.
// None of this exists on the OSX/Windows (on-screen simulation) build.
#if !defined(TARGET_OSX) && !defined(TARGET_WIN32)

#include "ofMain.h"
#include "ofxGPIO.h"
#include <stdio.h>
#include <stdlib.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <fcntl.h>

class ofApp;

class PiHardware {
	public:
		PiHardware(ofApp * app);

		void setup();
		void update();   // was ofApp::deviceOnlyUpdateRoutine()
		void shutdown();  // was the HAS_ADC block in ofApp::exit()

		void setLedsForPreset(int presetIndex);

		int a2dVal[6];

	private:
		ofApp * app;

		void setupLIS3DH();
		void updateLIS3DH();
		void setupADC();
		void setupButton();
		void initLedBlue();
		void initLedRed();
		void setupSpeakerControl();
		void syncSpeaker();

		void calibrateOnStart();
		void readADCValues();
		void normaliseADCValues();

		void buttonStateMachine();
		void checkForHits();
		void onHitRoutine();

		MCP a2d;
		I2c * bus;
		float accelScaleFactor, xg, yg, zg;

		int a2dChannel[6];
		unsigned char data[8][6];

		GPIO blueLed, redLed, relayOut, button;
		int state_button;

		bool buttonValue;
		int clicks;
		int click1Time, click2Time, click3Time, clik1ReleaseTime;
		bool waitingForClick, clickReleased;

		bool fireHitSwitch, isCountingHits, isCheckingHitPeaks, isCheckingHitTroughs;
		bool hadHitPeak[6], hadHitTrough[6];
		int timeBetweenHits, timeSinceLastHitPeak, timeSinceLastHitTrough, oldValues[6];
		int hitsCollected, hitPeakChecker, hitTroughsCollected, hitTroughChecker, completedFullHits;
};

#endif // !TARGET_OSX && !TARGET_WIN32
