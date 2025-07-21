//
//  SimulationStrategy.cpp
//  Gaugemancy
//
//  Created by Fred Rodrigues on 02/11/2024.
//

#include "SimulationStrategy.h"

void SimulationStrategy::updateSensorData() {
    // Generate simulated sensor data
}

ofParameter<float> SimulationStrategy::getSensorValue(const std::string& name) const {
    // Return simulated sensor value
    return ofParameter<float>(0.5f); // Example value
}

void SimulationStrategy::setMockSensorValue(const std::string& name, float value) {
   // sensorValues[name].set(value);
}


