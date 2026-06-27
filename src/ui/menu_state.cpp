#include "menu_state.hpp"
#include <algorithm>
#include <GLFW/glfw3.h>

static constexpr float BTN_W = 320.0f;
static constexpr float BTN_H = 44.0f;
static constexpr float TILE = 16.0f;

void MenuState::init(UiRenderer* renderer, Input* input) {
    m_renderer = renderer;
    m_input = input;

    addButton("Singleplayer",  240, MenuAction::Singleplayer);
    addButton("Multiplayer",   300, MenuAction::Multiplayer);
    addButton("Options",       360, MenuAction::Options);
    addButton("Quit",          420, MenuAction::Quit);
}

void MenuState::addButton(const char* label, float dy, MenuAction action) {
    float cx = (DESIGN_W - BTN_W) / 2.0f;
    m_buttons[m_buttonCount++] = {label, cx, dy, BTN_W, BTN_H, action};
}

void MenuState::update(int fbW, int fbH, int winW, int winH) {
    m_fbW = fbW;
    m_fbH = fbH;
    if (!m_input) return;

    glm::vec2 mousePos = m_input->mousePos();
    float mx = mousePos.x * fbW / winW;
    float my = mousePos.y * fbH / winH;

    float mxDes = mx / sx();
    float myDes = my / sy();

    bool mouseDown = m_input->mouseButtonDown(GLFW_MOUSE_BUTTON_LEFT);
    bool mousePressed = mouseDown && !m_mouseWasDown;
    m_mouseWasDown = mouseDown;

    m_hoveredButton = -1;
    for (int i = 0; i < m_buttonCount; i++) {
        auto& btn = m_buttons[i];
        if (mxDes >= btn.x && mxDes <= btn.x + btn.w &&
            myDes >= btn.y && myDes <= btn.y + btn.h) {
            m_hoveredButton = i;
            if (mousePressed) {
                m_action = btn.clickAction;
                return;
            }
        }
    }
}

void MenuState::render(VkCommandBuffer cmd) {
    if (!m_renderer) return;

    m_renderer->beginFrame(m_fbW, m_fbH);

    float sx = this->sx();
    float sy = this->sy();
    float ss = std::min(sx, sy);

    // Opaque dark background
    m_renderer->drawRect(0, 0, m_fbW, m_fbH, {0.086f, 0.086f, 0.125f, 1.0f});

    // Dark gradient overlay
    float gradH = m_fbH * 0.5f;
    m_renderer->drawRect(0, 0, m_fbW, gradH, {0.0f, 0.0f, 0.0f, 0.30f});

    // Title with shadow
    float titleSize = 64 * ss;
    int titleW = m_renderer->textWidth("MoidCraft", int(titleSize));
    float titleX = (m_fbW - titleW) / 2.0f;
    float titleY = 100 * sy;
    m_renderer->drawText("MoidCraft", titleX + 3 * ss, titleY + 3 * ss, titleSize, {0.0f, 0.0f, 0.0f, 0.5f});
    m_renderer->drawText("MoidCraft", titleX, titleY, titleSize, {1.0f, 1.0f, 1.0f, 1.0f});

    // Subtitle
    float subSize = 18 * ss;
    int subW = m_renderer->textWidth("A voxel sandbox game", int(subSize));
    m_renderer->drawText("A voxel sandbox game", (m_fbW - subW) / 2.0f, 180 * sy, subSize, {0.7f, 0.7f, 0.8f, 1.0f});

    // Buttons
    for (int i = 0; i < m_buttonCount; i++) {
        auto& btn = m_buttons[i];
        bool hovered = (i == m_hoveredButton);
        float bx = btn.x * sx;
        float by = btn.y * sy;
        float bw = btn.w * sx;
        float bh = btn.h * sy;
        m_renderer->drawButton(bx, by, bw, bh, hovered);
    }

    // Button labels + copyright (foreground)
    for (int i = 0; i < m_buttonCount; i++) {
        auto& btn = m_buttons[i];
        float bx = btn.x * sx;
        float by = btn.y * sy;
        float bw = btn.w * sx;
        float bh = btn.h * sy;

        float textSize = 20 * ss;
        int textW = m_renderer->textWidth(btn.label, int(textSize));
        float textX = bx + (bw - textW) / 2.0f;
        float textY = by + (bh - textSize) / 2.0f;
        bool hovered = (i == m_hoveredButton);
        glm::vec4 textColor = hovered ? glm::vec4(1.0f, 1.0f, 0.8f, 1.0f) : glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        m_renderer->drawTextFg(btn.label, textX, textY, textSize, textColor);
    }

    m_renderer->drawTextFg("MoidCraft 2026", 10 * sx, 700 * sy, 14 * ss, {0.4f, 0.4f, 0.5f, 1.0f});

    m_renderer->flushAll(cmd);
}

