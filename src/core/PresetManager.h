#pragma once

#include <string>
#include <vector>
#include "ofxXmlSettings.h"

/**
 * PresetManager - Manages presets and file loading
 * 
 * Handles loading preset configurations from XML, managing file paths,
 * and switching between different preset configurations.
 */

struct PresetConfig {
    int id;
    std::string name;
    std::vector<std::string> audioFilePaths;
    std::string effectsPatchFile;
    bool hasNarration;
    std::string narrationFilePath;
};

class PresetManager {
public:
    PresetManager();
    
    // Initialization
    void setup(const std::string& basePath, const std::string& unitId);
    
    // Preset management
    int getCurrentPresetIndex() const;
    void setCurrentPreset(int index);
    int getPresetCount() const;
    
    // File paths
    std::vector<std::string> getFilePathsForPreset(int presetIndex) const;
    std::string getEffectsPatchFile(int presetIndex) const;
    std::string getBasePath() const;
    std::string getUnitId() const;
    
    // XML loading
    bool loadPresetConfig(int presetIndex);
    bool loadAppSettings(const std::string& filename);
    bool loadUserSettings(const std::string& filename);
    
    // Preset switching
    void switchToNextPreset();
    void switchToPreviousPreset();
    
    // Configuration accessors
    bool hasNarrationTrack() const;
    std::string getNarrationFilePath() const;
    int getAudioDeviceId() const;
    int getEngineBufferSize() const;
    int getNumberOfBuffers() const;
    
private:
    std::string basePath;
    std::string unitId;
    int currentPresetIndex;
    int presetCount;
    
    std::vector<PresetConfig> presets;
    
    // App settings
    int audioDeviceId;
    int engineBufferSize;
    int numberOfBuffers;
    bool hasNarration;
    std::string narrationFilePath;
    
    // XML helpers
    ofxXmlSettings appSettingsXML;
    ofxXmlSettings userSettingsXML;
    
    void parsePresetFromXML(int presetIndex);
};
