// EffectManager.cpp
//
//  EffectManager.cpp
//  Gaugemancy
//
//  Created by Fred Rodrigues on 03/11/2024.
//

#include "EffectManager.h"

EffectManager::EffectManager() {
    // Initialize all effects
    effects.push_back(&bitCrushEffect);
    effects.push_back(&filterEffect);
    effects.push_back(&delayEffect);
    effects.push_back(&decimatorEffect);
    effects.push_back(&chorusEffect);
    effects.push_back(&reverbEffect);
    effects.push_back(&compressorEffect);
}

void EffectManager::setup() {
    for (auto& effect : effects) {
        effect->setup();
    }
}

void EffectManager::updateEffects(const ofParameter<float>& sensorValue) {
    for (auto& effect : effects) {
        effect->update(sensorValue);
    }
}

void EffectManager::applyPreset(const ofParameterGroup& preset) {
    for (auto& effect : effects) {
        // Assuming each effect has a method to apply parameters from the preset
        effect->getParamGroup() = preset;
    }
}
