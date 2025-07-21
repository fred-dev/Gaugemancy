// AudioManager.h
//
//  AudioManager.h
//  Gaugemancy
//
//  Created by Fred Rodrigues on 02/11/2024.
//

#pragma once
#include "ISensorStrategy.h"
#include "SettingsManager.h"
#include "EffectManager.h"

class AudioManager {
public:
    AudioManager(); // Added constructor declaration
    void setup(std::shared_ptr<ISensorStrategy> sensorStrategy, SettingsManager* settingsManager);
    void updateEffects();
    void applyPreset(const std::string& presetName, SettingsManager* settingsManager);
    EffectManager& getEffectManager() { return effectManager; }

private:
    std::shared_ptr<ISensorStrategy> sensorStrategy;
    SettingsManager* settingsManager;
    EffectManager effectManager; // Manages all effects
};
