// AudioManager.cpp
//
//  AudioManager.cpp
//  Gaugemancy
//
//  Created by Fred Rodrigues on 02/11/2024.
//

#include "AudioManager.h"

AudioManager::AudioManager() {
    // Initialize any necessary members here
}

void AudioManager::setup(std::shared_ptr<ISensorStrategy> sensorStrategy, SettingsManager* settingsManager) {
    this->sensorStrategy = sensorStrategy;
    this->settingsManager = settingsManager;
    effectManager.setup(); // Initialize effects
}

void AudioManager::updateEffects() {
    // Replace "sensorName" with actual sensor IDs as needed
    auto sensorValue = sensorStrategy->getSensorValue("sensorName");
    effectManager.updateEffects(sensorValue);
}

void AudioManager::applyPreset(const std::string& presetName, SettingsManager* settingsManager) {
    // Load effect parameters for the preset and update each effect
    ofParameterGroup presetParameters = settingsManager->getPresetParameters(presetName);
    effectManager.applyPreset(presetParameters);
}
