#include "SensorDataProcessor.h"
#include <algorithm>

SensorDataProcessor::SensorDataProcessor()
    : maxSensorValue(1023)
    , normalizationMin(0.0f)
    , useHitGesture(false)
    , fireHitSwitch(false)
    , isCountingHits(false)
    , isCheckingHitPeaks(false)
    , isCheckingHitTroughs(false)
    , timeBetweenHits(100)
    , timeSinceLastHitPeak(0)
    , timeSinceLastHitTrough(0)
    , hitsCollected(0)
    , hitPeakChecker(0)
    , maxPeakDuration(500)
    , hitTroughsCollected(0)
    , hitTroughChecker(0)
    , maxTroughDuration(500)
    , completedFullHits(0)
    , requiredHits(3)
    , hitThreshold(0.8f)
    , troughThreshold(0.2f)
    , useAccumulatedPressure(false)
    , accumulatedPressure(0.0f)
    , accumulatedPressureNormalized(0.0f)
    , accumulationDenominator(6.0f)
    , adcHandle(nullptr)
{
    // Initialize sensor data
    sensors.resize(NumChannels);
    for (auto& sensor : sensors) {
        sensor.rawValue = 0;
        sensor.normalizedValue = 0.0f;
        sensor.processedValue = 0.0f;
        sensor.isHit = false;
        sensor.isPeak = false;
        sensor.isTrough = false;
    }
    
    // Initialize state vectors
    baseValues.resize(NumChannels, 0);
    zeroValues.resize(NumChannels, 0);
    oldValues.resize(NumChannels, 0);
    hadHitPeak.resize(NumChannels, false);
    hadHitTrough.resize(NumChannels, false);
}

void SensorDataProcessor::setup() {
    ofLogNotice("SensorDataProcessor") << "Setting up sensor processor";
    // Platform-specific setup would be called from ofApp
}

void SensorDataProcessor::setupADC() {
#ifdef HAS_ADC
    // Platform-specific ADC initialization
    ofLogNotice("SensorDataProcessor") << "Setting up ADC (Raspberry Pi)";
#else
    ofLogNotice("SensorDataProcessor") << "ADC not available (Desktop mode)";
#endif
}

void SensorDataProcessor::calibrate(const std::vector<int>& baseValues) {
    if (baseValues.size() == NumChannels) {
        this->baseValues = baseValues;
        ofLogNotice("SensorDataProcessor") << "Calibration complete";
    }
}

void SensorDataProcessor::update() {
    readADC();
    
    // Update hit detection timers
    if (isCheckingHitPeaks) {
        timeSinceLastHitPeak += 16; // Approximate frame time
    }
    if (isCheckingHitTroughs) {
        timeSinceLastHitTrough += 16;
    }
}

void SensorDataProcessor::readADC() {
#ifdef HAS_ADC
    // Platform-specific ADC reading implementation
    // This would interface with the MCP3008 or similar ADC chip
#else
    // Desktop mode - values would come from simulation or GUI
#endif
}

float SensorDataProcessor::normalizeValue(int rawValue, int minValue, int maxValue) {
    if (maxValue == minValue) {
        return 0.0f;
    }
    return ofClamp((float)(rawValue - minValue) / (float)(maxValue - minValue), 0.0f, 1.0f);
}

float SensorDataProcessor::applyCurve(float value, int curveType) {
    switch (curveType) {
        case ExponentialEaseIn:
            return exponentialEaseIn(value);
        case QuarticEaseIn:
            return quarticEaseIn(value);
        case ExponentialEaseOut:
            return exponentialEaseOut(value);
        case QuarticEaseOut:
            return quarticEaseOut(value);
        case Linear:
        default:
            return value;
    }
}

float SensorDataProcessor::exponentialEaseIn(float value) {
    if (value == 0.0f) return 0.0f;
    return pow(2.0f, 10.0f * (value - 1.0f));
}

float SensorDataProcessor::quarticEaseIn(float value) {
    return value * value * value * value;
}

float SensorDataProcessor::exponentialEaseOut(float value) {
    if (value == 0.0f) return 1.0f;
    return -pow(2.0f, -10.0f * value) + 1.0f;
}

float SensorDataProcessor::quarticEaseOut(float value) {
    float t = value - 1.0f;
    return -t * t * t * t + 1.0f;
}

void SensorDataProcessor::checkForHits(float threshold, float troughThreshold) {
    this->hitThreshold = threshold;
    this->troughThreshold = troughThreshold;
    
    for (int i = 0; i < NumChannels; ++i) {
        // Check for peak
        if (sensors[i].normalizedValue > hitThreshold && !hadHitPeak[i]) {
            hadHitPeak[i] = true;
            timeSinceLastHitPeak = 0;
            isCheckingHitPeaks = true;
        }
        
        // Check for trough after peak
        if (hadHitPeak[i] && sensors[i].normalizedValue < troughThreshold && !hadHitTrough[i]) {
            if (timeSinceLastHitPeak < maxPeakDuration) {
                hadHitTrough[i] = true;
                hitsCollected++;
                
                if (hitsCollected >= requiredHits) {
                    completedFullHits++;
                    fireHitSwitch = true;
                    hitsCollected = 0;
                }
            }
            
            // Reset for next cycle
            hadHitPeak[i] = false;
            hadHitTrough[i] = false;
        }
        
        // Timeout for peak
        if (hadHitPeak[i] && timeSinceLastHitPeak > maxPeakDuration) {
            hadHitPeak[i] = false;
        }
    }
}

void SensorDataProcessor::resetHitDetection() {
    fireHitSwitch = false;
    isCountingHits = false;
    isCheckingHitPeaks = false;
    isCheckingHitTroughs = false;
    hitsCollected = 0;
    completedFullHits = 0;
    
    for (int i = 0; i < NumChannels; ++i) {
        hadHitPeak[i] = false;
        hadHitTrough[i] = false;
    }
}

const std::vector<SensorData>& SensorDataProcessor::getSensorData() const {
    return sensors;
}

float SensorDataProcessor::getNormalizedValue(int channel) const {
    if (channel >= 0 && channel < NumChannels) {
        return sensors[channel].normalizedValue;
    }
    return 0.0f;
}

float SensorDataProcessor::getProcessedValue(int channel) const {
    if (channel >= 0 && channel < NumChannels) {
        return sensors[channel].processedValue;
    }
    return 0.0f;
}

int SensorDataProcessor::getRawValue(int channel) const {
    if (channel >= 0 && channel < NumChannels) {
        return sensors[channel].rawValue;
    }
    return 0;
}

float SensorDataProcessor::getAccumulatedPressure() const {
    return accumulatedPressureNormalized;
}

void SensorDataProcessor::setUseAccumulatedPressure(bool use) {
    useAccumulatedPressure = use;
}

bool SensorDataProcessor::hasHit() const {
    return fireHitSwitch;
}

int SensorDataProcessor::getCompletedHits() const {
    return completedFullHits;
}
