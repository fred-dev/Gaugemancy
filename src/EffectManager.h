// EffectManager.h
//
//  EffectManager.h
//  Gaugemancy
//
//  Created by Fred Rodrigues on 03/11/2024.
//

#pragma once
#include "EffectBase.h"
#include "EFFBitCrushUnit.h"
#include "EFFFilterUnit.h"
#include "EFFDelayUnit.h"
#include "EFFDecimatorUnit.h"
#include "EFFChorusUnit.h"
#include "EFFReverbUnit.h"
#include "EFFCompressorUnit.h"

class EffectManager {
public:
    EffectManager();
    void setup();
    void updateEffects(const ofParameter<float>& sensorValue); // Update all effects with sensor data
    void applyPreset(const ofParameterGroup& preset);          // Apply preset settings to effects
    std::vector<EffectBase*>& getEffects() { return effects; }

private:
    EFFBitCrushUnit bitCrushEffect;
    EFFFilterUnit filterEffect;
    EFFDelayUnit delayEffect;
    EFFDecimatorUnit decimatorEffect;
    EFFChorusUnit chorusEffect;
    EFFReverbUnit reverbEffect;
    EFFCompressorUnit compressorEffect;

    std::vector<EffectBase*> effects; // Store effects as base pointers for easy management
};
