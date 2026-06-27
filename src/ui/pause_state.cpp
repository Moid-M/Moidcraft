#include "pause_state.hpp"
#include <GLFW/glfw3.h>

static constexpr float BTN_W = 320.0f;
static constexpr float BTN_H = 44.0f;

void PauseState::init(UiRenderer* renderer, Input* input) {
    m_renderer = renderer;
    m_input = input;
    m_buttons[0] = {"Continue",   0, 0, BTN_W, BTN_H, PauseAction::Continue};
    m_buttons[1] = {"Options",    0, 0, BTN_W, BTN_H, PauseAction::Options};
    m_buttons[2] = {"Exit World", 0, 0, BTN_W, BTN_H, PauseAction::ExitWorld};
    m_buttonCount = 3;
}

void PauseState::layoutButtons() {
    float cx = (m_fbW - BTN_W) / 2.0f;
    float startY = m_fbH / 2.0f - 40.0f;
    for (int i = 0; i < m_buttonCount; i++) {
        m_buttons[i].x = cx;
        m_buttons[i].y = startY + i * (BTN_H + 10);
        m_buttons[i].w = BTN_W;
        m_buttons[i].h = BTN_H;
    }
}

void PauseState::update(int fbW, int fbH, int winW, int winH) {
    m_fbW = fbW;
    m_fbH = fbH;
    layoutButtons();
    if (!m_input) return;

    glm::vec2 mousePos = m_input->mousePos();
    float mx = mousePos.x * fbW / winW;
    float my = mousePos.y * fbH / winH;

    bool mouseDown = m_input->mouseButtonDown(GLFW_MOUSE_BUTTON_LEFT);
    bool mousePressed = mouseDown && !m_mouseWasDown;
    m_mouseWasDown = mouseDown;

    m_hoveredButton = -1;
    for (int i = 0; i < m_buttonCount; i++) {
        auto& btn = m_buttons[i];
        if (mx >= btn.x && mx <= btn.x + btn.w &&
            my >= btn.y && my <= btn.y + btn.h) {
            m_hoveredButton = i;
            if (mousePressed) {
                m_action = btn.clickAction;
                return;
            }
        }
    }
}

void PauseState::render(VkCommandBuffer cmd) {
    if (!m_renderer) return;

    m_renderer->drawRect(0, 0, m_fbW, m_fbH, {0.0f, 0.0f, 0.0f, 0.5f});

    // Title
    float titleSize = 32.0f;
    int titleW = m_renderer->textWidth("Game Menu", int(titleSize));
    float titleX = (m_fbW - titleW) / 2.0f;
    float titleY = m_fbH / 2.0f - 120.0f;
    m_renderer->drawText("Game Menu", titleX, titleY, titleSize, {1.0f, 1.0f, 1.0f, 1.0f});

    // Buttons
    for (int i = 0; i < m_buttonCount; i++) {
        auto& btn = m_buttons[i];
        m_renderer->drawButton(btn.x, btn.y, btn.w, btn.h, i == m_hoveredButton);
        float textSize = 20.0f;
        int textW = m_renderer->textWidth(btn.label, int(textSize));
        float textX = btn.x + (btn.w - textW) / 2.0f;
        float textY = btn.y + (btn.h - textSize) / 2.0f;
        glm::vec4 textColor = (i == m_hoveredButton) ? glm::vec4(1.0f, 1.0f, 0.8f, 1.0f) : glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        m_renderer->drawTextFg(btn.label, textX, textY, textSize, textColor);
    }

    m_renderer->flushAll(cmd);
}
