#pragma once

#include <vector>
#include <memory>
#include "ofMain.h"
#include "ofxPDSP.h"

/**
 * GrainCloudManager - Manages granular synthesis voices
 * 
 * Handles creation, parameter control, and playback of granular synthesis
 * clouds. Each cloud can contain multiple grains with independent parameters.
 */

struct GrainParameters {
    float position;
    float size;
    float pitch;
    float density;
    float spread;
    float envelopeAttack;
    float envelopeDecay;
};

class GrainCloudManager {
public:
    GrainCloudManager();
    ~GrainCloudManager();
    
    // Initialization
    void setup(int voiceId);
    void loadAudioFile(const std::string& filePath);
    void loadParametersFromXML(const std::string& xmlPath);
    
    // Voice control
    void trigger();
    void release();
    void setGate(bool gate);
    
    // Parameter control
    void setPosition(float pos);
    void setSize(float size);
    void setPitch(float pitch);
    void setDensity(float density);
    void setSpread(float spread);
    void setEnvelopeAttack(float attack);
    void setEnvelopeDecay(float decay);
    
    // Dynamic parameter updates
    void updateParameters(const GrainParameters& params);
    
    // Audio output
    pdsp::Patchable& getOutput();
    pdsp::Patchable& getLeftOutput();
    pdsp::Patchable& getRightOutput();
    
    // State
    bool isPlaying() const;
    int getVoiceId() const;
    float getCurrentPosition() const;
    
    // Save/Load
    void saveParametersToXML(const std::string& xmlPath);
    
private:
    int voiceId;
    bool playing;
    float currentPosition;
    
    // PDSP grain cloud components
    void* grainCloud;  // Placeholder for actual pdsp::GrainCloud pointer
    void* envelope;    // Placeholder for envelope generator
    
    // Current parameters
    GrainParameters currentParams;
    
    // Audio file path
    std::string audioFilePath;
    
    // Initialize PDSP components
    void initPDSPComponents();
};
