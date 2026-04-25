#include "GrainCloudManager.h"
#include "ofxXmlSettings.h"
GrainCloudManager::GrainCloudManager()
    : voiceId(0)
    , playing(false)
    , currentPosition(0.0f)
    , grainCloud(nullptr)
    , envelope(nullptr)
{
    currentParams = {
        0.5f,  // position
        0.1f,  // size
        1.0f,  // pitch
        0.5f,  // density
        0.0f,  // spread
        0.01f, // envelopeAttack
        0.1f   // envelopeDecay
    };
}

GrainCloudManager::~GrainCloudManager() {
    // Cleanup PDSP components if needed
}

void GrainCloudManager::setup(int voiceId) {
    this->voiceId = voiceId;
    ofLogNotice("GrainCloudManager") << "Setting up grain cloud " << voiceId;
    initPDSPComponents();
}

void GrainCloudManager::loadAudioFile(const std::string& filePath) {
    audioFilePath = filePath;
    ofLogNotice("GrainCloudManager") << "Loading audio file: " << filePath;
    
    // Load audio file into PDSP grain cloud
    // Implementation would use pdsp::loader to load the file
}

void GrainCloudManager::loadParametersFromXML(const std::string& xmlPath) {
    ofxXmlSettings xml;
    if (xml.load(xmlPath)) {
        ofLogNotice("GrainCloudManager") << "Loading parameters from: " << xmlPath;
        
        // Load parameters from XML
        currentParams.position = xml.getValue("POSITION", 0.5f);
        currentParams.size = xml.getValue("SIZE", 0.1f);
        currentParams.pitch = xml.getValue("PITCH", 1.0f);
        currentParams.density = xml.getValue("DENSITY", 0.5f);
        currentParams.spread = xml.getValue("SPREAD", 0.0f);
        currentParams.envelopeAttack = xml.getValue("ENVELOPE_ATTACK", 0.01f);
        currentParams.envelopeDecay = xml.getValue("ENVELOPE_DECAY", 0.1f);
        
        updateParameters(currentParams);
    } else {
        ofLogWarning("GrainCloudManager") << "Could not load parameters from: " << xmlPath;
    }
}

void GrainCloudManager::trigger() {
    playing = true;
    setGate(true);
}

void GrainCloudManager::release() {
    playing = false;
    setGate(false);
}

void GrainCloudManager::setGate(bool gate) {
    // Set gate on envelope/gate control
    // Implementation would interface with PDSP
}

void GrainCloudManager::setPosition(float pos) {
    currentParams.position = ofClamp(pos, 0.0f, 1.0f);
    currentPosition = currentParams.position;
    // Update PDSP parameter
}

void GrainCloudManager::setSize(float size) {
    currentParams.size = ofClamp(size, 0.001f, 1.0f);
    // Update PDSP parameter
}

void GrainCloudManager::setPitch(float pitch) {
    currentParams.pitch = pitch;
    // Update PDSP parameter
}

void GrainCloudManager::setDensity(float density) {
    currentParams.density = ofClamp(density, 0.0f, 1.0f);
    // Update PDSP parameter
}

void GrainCloudManager::setSpread(float spread) {
    currentParams.spread = ofClamp(spread, 0.0f, 1.0f);
    // Update PDSP parameter
}

void GrainCloudManager::setEnvelopeAttack(float attack) {
    currentParams.envelopeAttack = ofClamp(attack, 0.001f, 1.0f);
    // Update PDSP parameter
}

void GrainCloudManager::setEnvelopeDecay(float decay) {
    currentParams.envelopeDecay = ofClamp(decay, 0.001f, 1.0f);
    // Update PDSP parameter
}

void GrainCloudManager::updateParameters(const GrainParameters& params) {
    currentParams = params;
    currentPosition = params.position;
    
    // Apply all parameters to PDSP components
    setPosition(params.position);
    setSize(params.size);
    setPitch(params.pitch);
    setDensity(params.density);
    setSpread(params.spread);
    setEnvelopeAttack(params.envelopeAttack);
    setEnvelopeDecay(params.envelopeDecay);
}

pdsp::Patchable& GrainCloudManager::getOutput() {
    // Return stereo output
    // Implementation would return actual PDSP patchable
    static pdsp::Patchable dummy;
    return dummy;
}

pdsp::Patchable& GrainCloudManager::getLeftOutput() {
    // Return left channel output
    static pdsp::Patchable dummy;
    return dummy;
}

pdsp::Patchable& GrainCloudManager::getRightOutput() {
    // Return right channel output
    static pdsp::Patchable dummy;
    return dummy;
}

bool GrainCloudManager::isPlaying() const {
    return playing;
}

int GrainCloudManager::getVoiceId() const {
    return voiceId;
}

float GrainCloudManager::getCurrentPosition() const {
    return currentPosition;
}

void GrainCloudManager::saveParametersToXML(const std::string& xmlPath) {
    ofxXmlSettings xml;
    
    xml.addValue("POSITION", currentParams.position);
    xml.addValue("SIZE", currentParams.size);
    xml.addValue("PITCH", currentParams.pitch);
    xml.addValue("DENSITY", currentParams.density);
    xml.addValue("SPREAD", currentParams.spread);
    xml.addValue("ENVELOPE_ATTACK", currentParams.envelopeAttack);
    xml.addValue("ENVELOPE_DECAY", currentParams.envelopeDecay);
    
    if (xml.save(xmlPath)) {
        ofLogNotice("GrainCloudManager") << "Saved parameters to: " << xmlPath;
    } else {
        ofLogError("GrainCloudManager") << "Failed to save parameters to: " << xmlPath;
    }
}

void GrainCloudManager::initPDSPComponents() {
    // Initialize PDSP grain cloud and related components
    // This would create the actual pdsp::GrainCloud instance
    ofLogNotice("GrainCloudManager") << "Initializing PDSP components for voice " << voiceId;
}
