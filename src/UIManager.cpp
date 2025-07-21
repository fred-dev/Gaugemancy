//
//  UIManager.cpp
//  Gaugemancy
//
//  Created by Fred Rodrigues on 02/11/2024.
//

#include "UIManager.h"

UIManager::UIManager(AudioManager* audioManager) : audioManager(audioManager) {}

void UIManager::setup() {
    gui.setup("Audio Effects");
    for (auto& effect : audioManager->getEffectManager().getEffects()) {
        gui.add(effect->getParamGroup());
    }
}

void UIManager::update() {
    // Update GUI elements based on real-time data
}

void UIManager::draw() {
    gui.draw();
}
