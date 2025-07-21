//
//  ISensorStrategy.cpp
//  Gaugemancy
//
//  Created by Fred Rodrigues on 02/11/2024.
//

#pragma once
#include "ISensorStrategy.h"

class SPIStrategy : public ISensorStrategy {
public:
    void updateSensorData() override;
    ofParameter<float> getSensorValue(const std::string& name) const override;

private:
    // Add SPI-specific members here
};
