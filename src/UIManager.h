//
//  UIManager.h
//  Gaugemancy
//
//  Created by Fred Rodrigues on 02/11/2024.
//

#pragma once
#include "ofxGui.h"
#include "AudioManager.h"

class UIManager {
public:
    UIManager(AudioManager* audioManager);
    void setup();
    void update();
    void draw();

private:
    AudioManager* audioManager;
    ofxPanel gui;
    ofParameterGroup uiParams;
};
