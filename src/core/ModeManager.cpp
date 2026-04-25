#include "ModeManager.h"

ModeManager::ModeManager() 
    : currentMode(OperationMode::Setup)
    , previousMode(OperationMode::Setup)
    , transitioning(false) 
{
    // Initialize all callbacks to nullptr
    for (int i = 0; i < 10; ++i) {
        enterCallbacks[i] = nullptr;
        updateCallbacks[i] = nullptr;
        exitCallbacks[i] = nullptr;
    }
}

void ModeManager::changeMode(OperationMode newMode) {
    if (newMode == currentMode) {
        return;
    }
    
    previousMode = currentMode;
    transitioning = true;
    
    // Call exit callback for old mode
    if (exitCallbacks[static_cast<int>(currentMode)] != nullptr) {
        exitCallbacks[static_cast<int>(currentMode)]();
    }
    
    currentMode = newMode;
    
    // Call enter callback for new mode
    if (enterCallbacks[static_cast<int>(currentMode)] != nullptr) {
        enterCallbacks[static_cast<int>(currentMode)]();
    }
    
    transitioning = false;
}

OperationMode ModeManager::getCurrentMode() const {
    return currentMode;
}

void ModeManager::update() {
    if (updateCallbacks[static_cast<int>(currentMode)] != nullptr) {
        updateCallbacks[static_cast<int>(currentMode)]();
    }
}

bool ModeManager::isTransitioning() const {
    return transitioning;
}

OperationMode ModeManager::getPreviousMode() const {
    return previousMode;
}

void ModeManager::setEnterCallback(OperationMode mode, ModeCallback callback) {
    if (static_cast<int>(mode) >= 0 && static_cast<int>(mode) < 10) {
        enterCallbacks[static_cast<int>(mode)] = callback;
    }
}

void ModeManager::setUpdateCallback(OperationMode mode, ModeCallback callback) {
    if (static_cast<int>(mode) >= 0 && static_cast<int>(mode) < 10) {
        updateCallbacks[static_cast<int>(mode)] = callback;
    }
}

void ModeManager::setExitCallback(OperationMode mode, ModeCallback callback) {
    if (static_cast<int>(mode) >= 0 && static_cast<int>(mode) < 10) {
        exitCallbacks[static_cast<int>(mode)] = callback;
    }
}
