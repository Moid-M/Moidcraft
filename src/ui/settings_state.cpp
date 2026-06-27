#include "settings_state.hpp"
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <algorithm>

void SettingsState::init(UiRenderer* renderer, Input* input, GameSettings* settings) {
    m_renderer = renderer;
    m_input = input;
    m_settings = settings;

    auto& s0 = m_sliders[0];
    s0.label = "Render Distance"; s0.value = &m_settings->renderDistance;
    s0.minVal = 2.0f; s0.maxVal = 32.0f; s0.format = "%.0f chunks";

    auto& s1 = m_sliders[1];
    s1.label = "Brightness"; s1.value = &m_settings->brightness;
    s1.minVal = 0.5f; s1.maxVal = 1.5f; s1.format = "%.0f%%";

    auto& s2 = m_sliders[2];
    s2.label = "Mouse Sensitivity"; s2.value = &m_settings->mouseSensitivity;
    s2.minVal = 0.05f; s2.maxVal = 2.0f; s2.format = "%.2f";

    auto& s3 = m_sliders[3];
    s3.label = "FOV"; s3.value = &m_settings->fov;
    s3.minVal = 50.0f; s3.maxVal = 120.0f; s3.format = "%.0f";

    auto& s4 = m_sliders[4];
    s4.label = "FPS Cap"; s4.value = &m_settings->fpsCap;
    s4.minVal = 10.0f; s4.maxVal = 360.0f; s4.format = "%.0f";

    auto& t0 = m_toggles[0];
    t0.label = "Transparent Leaves"; t0.value = &m_settings->transparentLeaves;

    auto& t1 = m_toggles[1];
    t1.label = "VSync"; t1.value = &m_settings->vsync;
}

void SettingsState::layout() {
    float cx = m_fbW / 2.0f;
    float labelW = 180.0f;
    float trackW = 200.0f;
    float trackH = 6.0f;
    float rowH = 50.0f;
    float startY = 95.0f;

    for (int i = 0; i < m_sliderCount; i++) {
        m_sliders[i].labelX = cx - labelW - trackW / 2.0f;
        m_sliders[i].labelY = startY + i * rowH;
        m_sliders[i].trackX = cx - trackW / 2.0f;
        m_sliders[i].trackY = startY + i * rowH + 10.0f;
        m_sliders[i].trackW = trackW;
        m_sliders[i].trackH = trackH;
        m_sliders[i].displayX = cx + trackW / 2.0f + 10.0f;
        m_sliders[i].displayY = startY + i * rowH;
    }

    for (int i = 0; i < m_toggleCount; i++) {
        m_toggles[i].labelX = cx - labelW - 100.0f;
        m_toggles[i].labelY = startY + (m_sliderCount + i) * rowH;
        m_toggles[i].btnX = cx - 28.0f;
        m_toggles[i].btnY = startY + (m_sliderCount + i) * rowH + 2.0f;
        m_toggles[i].btnW = 56.0f;
        m_toggles[i].btnH = 24.0f;
    }
}

int SettingsState::hitTestSlider(float mx, float my) const {
    for (int i = 0; i < m_sliderCount; i++) {
        auto& s = m_sliders[i];
        if (mx >= s.trackX - 10 && mx <= s.trackX + s.trackW + 10 &&
            my >= s.trackY - 12 && my <= s.trackY + s.trackH + 12) {
            return i;
        }
    }
    return -1;
}

void SettingsState::updateSlider(int idx, float mx) {
    auto& s = m_sliders[idx];
    float t = (mx - s.trackX) / s.trackW;
    t = std::clamp(t, 0.0f, 1.0f);
    *s.value = s.minVal + t * (s.maxVal - s.minVal);
}

void SettingsState::update(int fbW, int fbH, int winW, int winH) {
    m_fbW = fbW;
    m_fbH = fbH;
    layout();

    if (!m_input || !m_settings) return;

    glm::vec2 mousePos = m_input->mousePos();
    float mx = mousePos.x * fbW / winW;
    float my = mousePos.y * fbH / winH;

    bool mouseDown = m_input->mouseButtonDown(GLFW_MOUSE_BUTTON_LEFT);
    bool mousePressed = mouseDown && !m_mouseWasDown;
    m_mouseWasDown = mouseDown;

    if (!mouseDown) {
        m_draggingSlider = -1;
    }

    if (m_draggingSlider >= 0) {
        updateSlider(m_draggingSlider, mx);
        return;
    }

    if (mousePressed) {
        int sl = hitTestSlider(mx, my);
        if (sl >= 0) {
            m_draggingSlider = sl;
            updateSlider(sl, mx);
            return;
        }

        for (int i = 0; i < m_toggleCount; i++) {
            auto& t = m_toggles[i];
            if (mx >= t.btnX && mx <= t.btnX + t.btnW &&
                my >= t.btnY && my <= t.btnY + t.btnH) {
                *t.value = !(*t.value);
                return;
            }
        }
    }

    m_hoveredButton = -1;
    float btnW = 320.0f, btnH = 44.0f;
    float btnX = (m_fbW - btnW) / 2.0f;
    float btnY = m_fbH - 80.0f;
    if (mx >= btnX && mx <= btnX + btnW && my >= btnY && my <= btnY + btnH) {
        m_hoveredButton = 0;
        if (mousePressed) {
            m_action = SettingsAction::Done;
        }
    }
}

void SettingsState::render(VkCommandBuffer cmd) {
    if (!m_renderer || !m_settings) return;

    m_renderer->drawRect(0, 0, m_fbW, m_fbH, {0.086f, 0.086f, 0.125f, 1.0f});

    float titleSize = 32.0f;
    int titleW = m_renderer->textWidth("Options", int(titleSize));
    float titleX = (m_fbW - titleW) / 2.0f;
    m_renderer->drawText("Options", titleX, 40.0f, titleSize, {1.0f, 1.0f, 1.0f, 1.0f});

    for (int i = 0; i < m_sliderCount; i++) {
        auto& s = m_sliders[i];

        m_renderer->drawText(s.label, s.labelX, s.labelY, 18.0f, {0.8f, 0.8f, 0.9f, 1.0f});

        m_renderer->drawRect(s.trackX, s.trackY, s.trackW, s.trackH, {0.2f, 0.2f, 0.25f, 1.0f});

        float t = (*s.value - s.minVal) / (s.maxVal - s.minVal);
        float filledW = t * s.trackW;
        if (filledW > 0) {
            m_renderer->drawRect(s.trackX, s.trackY, filledW, s.trackH, {0.6f, 0.6f, 0.7f, 1.0f});
        }

        float thumbX = s.trackX + filledW - 6.0f;
        float thumbY = s.trackY - 7.0f;
        glm::vec4 thumbColor = (m_draggingSlider == i) ? glm::vec4{1.0f, 1.0f, 0.8f, 1.0f} : glm::vec4{0.85f, 0.85f, 0.9f, 1.0f};
        m_renderer->drawRect(thumbX, thumbY, 12.0f, 20.0f, thumbColor);

        char valBuf[32];
        float val = *s.value;
        if (strstr(s.format, "%%")) {
            snprintf(valBuf, sizeof(valBuf), "%.0f%%", val * 100.0f);
        } else {
            snprintf(valBuf, sizeof(valBuf), s.format, val);
        }
        m_renderer->drawText(valBuf, s.displayX, s.displayY, 18.0f, {1.0f, 1.0f, 1.0f, 1.0f});
    }

    for (int i = 0; i < m_toggleCount; i++) {
        auto& t = m_toggles[i];
        m_renderer->drawText(t.label, t.labelX, t.labelY, 18.0f, {0.8f, 0.8f, 0.9f, 1.0f});

        bool on = *t.value;
        glm::vec4 bgColor = on ? glm::vec4{0.3f, 0.7f, 0.3f, 1.0f} : glm::vec4{0.3f, 0.3f, 0.3f, 1.0f};
        m_renderer->drawRect(t.btnX, t.btnY, t.btnW, t.btnH, bgColor);

        const char* label = on ? "ON" : "OFF";
        glm::vec4 textColor = on ? glm::vec4{1.0f, 1.0f, 1.0f, 1.0f} : glm::vec4{0.7f, 0.7f, 0.7f, 1.0f};
        int tw = m_renderer->textWidth(label, 14);
        float tx = t.btnX + (t.btnW - tw) / 2.0f;
        float ty = t.btnY + (t.btnH - 14) / 2.0f;
        m_renderer->drawTextFg(label, tx, ty, 14.0f, textColor);
    }

    float btnW = 320.0f, btnH = 44.0f;
    float btnX = (m_fbW - btnW) / 2.0f;
    float btnY = m_fbH - 80.0f;
    m_renderer->drawButton(btnX, btnY, btnW, btnH, m_hoveredButton == 0);
    float textSize = 20.0f;
    int textW = m_renderer->textWidth("Done", int(textSize));
    float textX = btnX + (btnW - textW) / 2.0f;
    float textY = btnY + (btnH - textSize) / 2.0f;
    glm::vec4 textColor = (m_hoveredButton == 0) ? glm::vec4{1.0f, 1.0f, 0.8f, 1.0f} : glm::vec4{1.0f, 1.0f, 1.0f, 1.0f};
    m_renderer->drawTextFg("Done", textX, textY, textSize, textColor);

    m_renderer->flushAll(cmd);
}
