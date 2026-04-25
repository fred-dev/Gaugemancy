#pragma once

#include "AppConstants.h"
#include <vector>
#include <string>

/**
 * ModeManager - Handles application mode/state transitions
 * 
 * Manages the current operation mode and handles transitions between modes.
 * Provides callbacks for mode entry, update, and exit.
 */

class ModeManager {
public:
    ModeManager();
    
    // Mode management
    void changeMode(OperationMode newMode);
    OperationMode getCurrentMode() const;
    
    // Update loop - called every frame
    void update();
    
    // Mode callbacks (to be implemented by ofApp)
    using ModeCallback = void(*)();
    void setEnterCallback(OperationMode mode, ModeCallback callback);
    void setUpdateCallback(OperationMode mode, ModeCallback callback);
    void setExitCallback(OperationMode mode, ModeCallback callback);
    
    // Mode state
    bool isTransitioning() const;
    OperationMode getPreviousMode() const;
    
private:
    OperationMode currentMode;
    OperationMode previousMode;
    bool transitioning;
    
    // Callbacks for each mode
    ModeCallback enterCallbacks[10];
    ModeCallback updateCallbacks[10];
    ModeCallback exitCallbacks[10];
};
