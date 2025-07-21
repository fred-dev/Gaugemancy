//
//  EffectBase.h
//  Gaugemancy
//
//  Created by Fred Rodrigues on 03/11/2024.
//

#pragma once
#include "ofMain.h"

class EffectBase {
public:
    virtual ~EffectBase() = default;
    virtual void setup() = 0;
    virtual void update(const ofParameter<float>& sensorValue) = 0;
    virtual ofParameterGroup& getParamGroup() = 0;
};
