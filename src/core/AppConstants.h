#pragma once

/**
 * AppConstants - Centralized constants and enums
 * 
 * Contains all application-wide constants, enums, and configuration values
 * to avoid magic numbers and improve code readability.
 */

// Number of sensors, effects, and presets
constexpr int NUMBER_OF_SENSORS = 6;
constexpr int NUMBER_OF_EFFECTS = 6;
constexpr int NUMBER_OF_PRESETS = 4;

/**
 * OperationMode - Different modes/states of the application
 */
enum class OperationMode {
    Setup = 0,                    // Setup mode (unused, for interactive laptop setup)
    WaitForNarration = 1,         // Wait for narration input
    PlayNarration = 2,            // Play narration file
    SwitchPresets = 3,            // Switch between presets
    MultiGrain = 4,               // Granular mode with 6 audio files
    SingleGrain = 5,              // Granular mode with single grain
    NarrationGui = 6,             // GUI mode for narration parameters
    SimulationMulti = 7,          // Simulation mode with 6 files
    SimulationSingle = 8,         // Simulation mode with single grain
    NarrationGlitch = 9           // Glitch mode for narration
};

/**
 * ButtonActionMessage - Non-modal button actions
 */
enum class ButtonActionMessage {
    PauseNarration = 9,
    SkipNarration = 10,
    RestartNarration = 11,
    GlitchNarration = 12,
    ToggleSimulation = 13,
    SwitchPresets = 14,
    ReturnToNarration = 15,
    NarrationGui = 16,
    SaveGranular = 17,
    SaveNarration = 18,
    PauseSimulation = 19,
    CurvesOrSimulationDisplay = 20,
    EffectsOrGranular = 28
};

/**
 * ButtonModeMessage - Modal button actions that trigger mode changes
 */
enum class ButtonModeMessage {
    OpModePlayNarration = 21,
    OpModeSwitchPresets = 22,
    OpModeGrainMode = 23,
    OpModeNarrationGui = 24,
    OpModeSimulation = 25,
    OpModeNarrationGlitch = 26,
    Exit = 27
};

// UI Constants
constexpr int BUTTON_WIDTH = 250;
constexpr int BUTTON_HEIGHT = 45;

// Curve types
enum class CurveType {
    ExponentialEaseIn,
    QuarticEaseIn,
    ExponentialEaseOut,
    QuarticEaseOut,
    Linear
};

// Sensor channels
enum class SensorChannel {
    Channel0 = 0,
    Channel1 = 1,
    Channel2 = 2,
    Channel3 = 3,
    Channel4 = 4,
    Channel5 = 5
};
