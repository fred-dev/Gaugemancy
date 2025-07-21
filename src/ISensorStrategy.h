// ISensorStrategy.h
// Gaugemancy
//
// Created by Fred Rodrigues on 02/11/2024.

#pragma once
#include "ofMain.h"

class ISensorStrategy {
public:
    virtual ~ISensorStrategy() = default;
    virtual void updateSensorData() = 0;
    virtual ofParameter<float> getSensorValue(const std::string& name) const = 0;
};
