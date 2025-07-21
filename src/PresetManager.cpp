//
//  PresetManager.cpp
//  Gaugemancy
//
//  Created by Fred Rodrigues on 02/11/2024.
//

#include "PresetManager.h"

PresetManager::PresetManager() {}

PresetManager::~PresetManager() {}

void PresetManager::setup(AudioManager* audioManager, SettingsManager* settingsManager) {
    this->audioManager = audioManager;
    this->settingsManager = settingsManager;
}

void PresetManager::applyPreset(const std::string& presetName) {
    // Load preset parameters from SettingsManager and update AudioManager
    audioManager->applyPreset(presetName, settingsManager);
}
