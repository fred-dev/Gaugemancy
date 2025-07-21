// SPIStrategy.h
// Gaugemancy
//
// Created by Fred Rodrigues on 02/11/2024.

#pragma once
#include "ISensorStrategy.h"
#include <map>

class SPIStrategy : public ISensorStrategy {
public:
    SPIStrategy();
    ~SPIStrategy();

    void updateSensorData() override;
    ofParameter<float> getSensorValue(const std::string& name) const override;

private:
    std::map<std::string, ofParameter<float>> sensorValues;
    // Add SPI-specific members here, such as file descriptors or handles
};
