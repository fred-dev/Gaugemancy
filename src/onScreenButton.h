
#pragma once

#include "ofxMSAInteractiveObject.h"


#define		IDLE_COLOR		0xFFFFFF
#define		OVER_COLOR		0x00FF00
#define		DOWN_COLOR		0xFF0000

class onScreenButton : public ofxMSAInteractiveObject  {
public:
    
    string buttonName;
    int buttonMessage;
    ofTrueTypeFont labelFont;
    bool buttonActive;
    void setup(string label, int message) {
        ofLogVerbose()<<buttonName +  " button::setup() - hello!"<<endl;
        enableMouseEvents();
        enableKeyEvents();
        labelFont.load("verdana.ttf",15);
        buttonName=label;
        buttonMessage = message;
        buttonActive=false;
        
    }
    
    void exit() {
        
        ofLogVerbose()<< buttonName +  " button::exit() - goodbye!"<<endl;
    }
    
    
    void update() {
        //		x = ofGetWidth()/2 + cos(ofGetElapsedTimef() * 0.2) * ofGetWidth()/4;
        //		y = ofGetHeight()/2 + sin(ofGetElapsedTimef() * 0.2) * ofGetHeight()/4;
    }
    
    
    void draw() {
        ofPushStyle();
        if(isMousePressed()) ofSetHexColor(DOWN_COLOR);
        else if(isMouseOver()) ofSetHexColor(OVER_COLOR);
        else if (buttonActive) ofSetHexColor(DOWN_COLOR);
        else ofSetHexColor(IDLE_COLOR);
        
        
        ofNoFill();
        
        ofDrawRectangle(x, y, width, height);
        
        if (buttonActive) ofSetHexColor(DOWN_COLOR);
        else ofSetHexColor(IDLE_COLOR);
        labelFont.drawString(buttonName, x + ((width - labelFont.getStringBoundingBox(buttonName, 0, 0).getWidth())/2), y + ((height - labelFont.getStringBoundingBox(buttonName, 0, 0).getHeight())));

        ofPopStyle();
        
    }
    
    virtual void onRollOver(int x, int y) {
		ofLogVerbose() <<buttonName +  " button::onRollOver x " + ofToString( x) + " y " + ofToString( y) + buttonName <<endl;
    }
    
    virtual void onRollOut() {
        ofLogVerbose()<<buttonName +  " button::onRollOut x " + ofToString( x) + " y " + ofToString( y)<<endl;
    }
    
    virtual void onPress(int x, int y, int button) {
        ofLogVerbose()<<buttonName +  " button::onPress x " + ofToString( x) + " y " + ofToString( y)<<endl;
    }
    
    virtual void onRelease(int x, int y, int button) {
        ofLogVerbose()<<buttonName +  " button::onRelease x "+  ofToString( x) + " y " + ofToString( y)<<endl;
		ofSendMessage(ofToString(buttonMessage));
    }
    
    virtual void onReleaseOutside(int x, int y, int button) {
        ofLogVerbose()<<buttonName +  " button::onReleaseOutside x " + ofToString( x) + " y " + ofToString( y)<<endl;
    }
    
    
};
