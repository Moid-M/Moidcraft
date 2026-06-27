#pragma once
#include "core/types.hpp"
#include "core/input.hpp"
#include "ui/ui_renderer.hpp"
#include <glm/glm.hpp>

enum class PauseAction {
    None, Continue, Options, ExitWorld
};

class PauseState {
public:
    PauseState() = default;
    void init(UiRenderer* renderer, Input* input);
    void update(int fbW, int fbH, int winW, int winH);
    PauseAction action() const { return m_action; }
    void resetAction() { m_action = PauseAction::None; }
    void render(VkCommandBuffer cmd);
private:
    UiRenderer* m_renderer = nullptr;
    Input* m_input = nullptr;
    PauseAction m_action = PauseAction::None;
    struct Button { const char* label; float x, y, w, h; PauseAction clickAction; };
    Button m_buttons[3];
    int m_buttonCount = 0;
    int m_hoveredButton = -1;
    bool m_mouseWasDown = false;
    int m_fbW = 1280, m_fbH = 720;
    void layoutButtons();
};
