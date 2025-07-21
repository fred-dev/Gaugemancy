//
//  PresetManager.h
//  Gaugemancy
//
//  Created by Fred Rodrigues on 02/11/2024.
//

#pragma once
#include "AudioManager.h"
#include "SettingsManager.h"

class PresetManager {
public:
    PresetManager();
    ~PresetManager();

    void setup(AudioManager* audioManager, SettingsManager* settingsManager);
    void applyPreset(const std::string& presetName);

private:
    AudioManager* audioManager;
    SettingsManager* settingsManager;
};
