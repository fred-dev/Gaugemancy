//
//  SettingsManager.h
//  Gaugemancy
//
//  Created by Fred Rodrigues on 02/11/2024.
//

#pragma once
#include "ofMain.h"

#include "ofxXmlSettings.h"

class SettingsManager {
public:
    SettingsManager();
    ~SettingsManager();

    void loadSettings(const std::string& filepath);
    ofParameter<int> getEffectParameter(const std::string& effectName, const std::string& param);
    ofParameterGroup getPresetParameters(const std::string& presetName);


private:
    ofxXmlSettings settings;
    ofParameterGroup parameters;
};
