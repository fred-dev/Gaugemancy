#pragma once

#include <vector>
#include "ofMain.h"

/**
 * SensorDataProcessor - Handles sensor input and processing
 * 
 * Manages reading ADC values, normalizing sensor data, applying curves,
 * and detecting hits/peaks/troughs from sensor input.
 */

struct SensorData {
    int rawValue;
    float normalizedValue;
    float processedValue;  // After curve application
    bool isHit;
    bool isPeak;
    bool isTrough;
};

class SensorDataProcessor {
public:
    SensorDataProcessor();
    
    // Initialization
    void setup();
    void setupADC();
    void calibrate(const std::vector<int>& baseValues);
    
    // Data acquisition
    void update();
    void readADC();
    
    // Data processing
    float normalizeValue(int rawValue, int minValue, int maxValue);
    float applyCurve(float value, int curveType);
    
    // Hit detection
    void checkForHits(float threshold, float troughThreshold);
    void resetHitDetection();
    
    // Accessors
    const std::vector<SensorData>& getSensorData() const;
    float getNormalizedValue(int channel) const;
    float getProcessedValue(int channel) const;
    int getRawValue(int channel) const;
    
    // Accumulated pressure
    float getAccumulatedPressure() const;
    void setUseAccumulatedPressure(bool use);
    
    // Hit state
    bool hasHit() const;
    int getCompletedHits() const;
    
    // Curve types (matches AppConstants::CurveType)
    enum CurveType {
        ExponentialEaseIn = 0,
        QuarticEaseIn = 1,
        ExponentialEaseOut = 2,
        QuarticEaseOut = 3,
        Linear = 4
    };
    
private:
    static constexpr int NumChannels = 6;
    std::vector<SensorData> sensors;
    
    // Calibration
    std::vector<int> baseValues;
    std::vector<int> zeroValues;
    int maxSensorValue;
    float normalizationMin;
    
    // Hit detection
    bool useHitGesture;
    bool fireHitSwitch;
    bool isCountingHits;
    bool isCheckingHitPeaks;
    bool isCheckingHitTroughs;
    
    std::vector<bool> hadHitPeak;
    std::vector<bool> hadHitTrough;
    std::vector<int> oldValues;
    
    int timeBetweenHits;
    int timeSinceLastHitPeak;
    int timeSinceLastHitTrough;
    int hitsCollected;
    int hitPeakChecker;
    int maxPeakDuration;
    int hitTroughsCollected;
    int hitTroughChecker;
    int maxTroughDuration;
    int completedFullHits;
    int requiredHits;
    float hitThreshold;
    float troughThreshold;
    
    // Accumulated pressure
    bool useAccumulatedPressure;
    float accumulatedPressure;
    float accumulatedPressureNormalized;
    float accumulationDenominator;
    
    // Platform-specific ADC handles (ifdef in cpp)
    void* adcHandle;  // Placeholder for platform-specific ADC handle
    
    // Curve processing helpers
    float exponentialEaseIn(float value);
    float quarticEaseIn(float value);
    float exponentialEaseOut(float value);
    float quarticEaseOut(float value);
};
