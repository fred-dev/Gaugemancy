//
//  SettingsManager.cpp
//  Gaugemancy
//
//  Created by Fred Rodrigues on 02/11/2024.
//

#include "SettingsManager.h"

SettingsManager::SettingsManager() {}

SettingsManager::~SettingsManager() {}

void SettingsManager::loadSettings(const std::string& filepath) {
    if (settings.loadFile(filepath)) {
        // Load settings into ofParameterGroup
    }
}

ofParameter<int> SettingsManager::getEffectParameter(const std::string& effectName, const std::string& param) {
    // Example: retrieve parameters for an effect
    return parameters.getInt(effectName + ":" + param);
}

ofParameterGroup SettingsManager::getPresetParameters(const std::string& presetName) {
    ofParameterGroup presetGroup;
    // Load the preset parameters from settings into presetGroup
    // This will depend on how presets are stored in your settings file
    return presetGroup;
}
