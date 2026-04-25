#include "PresetManager.h"
#include "ofMain.h"

PresetManager::PresetManager()
    : basePath("")
    , unitId("")
    , currentPresetIndex(0)
    , presetCount(4)
    , audioDeviceId(0)
    , engineBufferSize(512)
    , numberOfBuffers(4)
    , hasNarration(false)
    , narrationFilePath("")
{
}

void PresetManager::setup(const std::string& basePath, const std::string& unitId) {
    this->basePath = basePath;
    this->unitId = unitId;
    
    // Initialize presets
    presets.resize(presetCount);
    for (int i = 0; i < presetCount; ++i) {
        presets[i].id = i;
        presets[i].name = "Preset " + ofToString(i + 1);
        presets[i].hasNarration = false;
    }
}

int PresetManager::getCurrentPresetIndex() const {
    return currentPresetIndex;
}

void PresetManager::setCurrentPreset(int index) {
    if (index >= 0 && index < presetCount) {
        currentPresetIndex = index;
    }
}

int PresetManager::getPresetCount() const {
    return presetCount;
}

std::vector<std::string> PresetManager::getFilePathsForPreset(int presetIndex) const {
    if (presetIndex >= 0 && presetIndex < static_cast<int>(presets.size())) {
        return presets[presetIndex].audioFilePaths;
    }
    return std::vector<std::string>();
}

std::string PresetManager::getEffectsPatchFile(int presetIndex) const {
    if (presetIndex >= 0 && presetIndex < static_cast<int>(presets.size())) {
        return presets[presetIndex].effectsPatchFile;
    }
    return "";
}

std::string PresetManager::getBasePath() const {
    return basePath;
}

std::string PresetManager::getUnitId() const {
    return unitId;
}

bool PresetManager::loadPresetConfig(int presetIndex) {
    if (presetIndex < 0 || presetIndex >= presetCount) {
        return false;
    }
    
    std::string filename = basePath + unitId + "_preset_" + ofToString(presetIndex + 1) + ".xml";
    ofxXmlSettings xml;
    
    if (xml.loadFile(filename)) {
        parsePresetFromXML(presetIndex);
        return true;
    }
    
    ofLogWarning("PresetManager") << "Could not load preset file: " << filename;
    return false;
}

bool PresetManager::loadAppSettings(const std::string& filename) {
    if (appSettingsXML.loadFile(filename)) {
        audioDeviceId = appSettingsXML.getValue("AUDIO_DEVICE_ID", 0);
        engineBufferSize = appSettingsXML.getValue("ENGINE_BUFFER_SIZE", 512);
        numberOfBuffers = appSettingsXML.getValue("NUMBER_OF_BUFFERS", 4);
        return true;
    }
    return false;
}

bool PresetManager::loadUserSettings(const std::string& filename) {
    if (userSettingsXML.loadFile(filename)) {
        hasNarration = userSettingsXML.getValue("HAS_NARRATION", false);
        narrationFilePath = userSettingsXML.getValue("NARRATION_FILE", "");
        return true;
    }
    return false;
}

void PresetManager::switchToNextPreset() {
    currentPresetIndex = (currentPresetIndex + 1) % presetCount;
}

void PresetManager::switchToPreviousPreset() {
    currentPresetIndex = (currentPresetIndex - 1 + presetCount) % presetCount;
}

bool PresetManager::hasNarrationTrack() const {
    return hasNarration;
}

std::string PresetManager::getNarrationFilePath() const {
    return narrationFilePath;
}

int PresetManager::getAudioDeviceId() const {
    return audioDeviceId;
}

int PresetManager::getEngineBufferSize() const {
    return engineBufferSize;
}

int PresetManager::getNumberOfBuffers() const {
    return numberOfBuffers;
}

void PresetManager::parsePresetFromXML(int presetIndex) {
    // Implementation would parse XML and populate preset config
    // This is a placeholder for the actual XML parsing logic
    ofLogNotice("PresetManager") << "Parsing preset " << presetIndex;
}
