//
//  SimulationStrategy.h
//  Gaugemancy
//
//  Created by Fred Rodrigues on 02/11/2024.
//

#pragma once
#include "ISensorStrategy.h"

class SimulationStrategy : public ISensorStrategy {
public:
    void updateSensorData() override;
    ofParameter<float> getSensorValue(const std::string& name) const override;
    void setMockSensorValue(const std::string& name, float value);

};

