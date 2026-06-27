#pragma once
#include "core/types.hpp"
#include "core/input.hpp"
#include "core/settings.hpp"
#include "ui/ui_renderer.hpp"
#include <glm/glm.hpp>

enum class SettingsAction { None, Done };

class SettingsState {
public:
    SettingsState() = default;
    void init(UiRenderer* renderer, Input* input, GameSettings* settings);
    void update(int fbW, int fbH, int winW, int winH);
    SettingsAction action() const { return m_action; }
    void resetAction() { m_action = SettingsAction::None; }
    void render(VkCommandBuffer cmd);

private:
    UiRenderer* m_renderer = nullptr;
    Input* m_input = nullptr;
    GameSettings* m_settings = nullptr;
    SettingsAction m_action = SettingsAction::None;

    int m_fbW = 1280, m_fbH = 720;
    int m_hoveredButton = -1;
    int m_draggingSlider = -1;
    bool m_mouseWasDown = false;

    struct SliderDef {
        const char* label;
        float* value;
        float minVal, maxVal;
        const char* format;
        float labelX, labelY;
        float trackX, trackY, trackW, trackH;
        float displayX, displayY;
    };

    struct ToggleDef {
        const char* label;
        bool* value;
        float labelX, labelY;
        float btnX, btnY, btnW, btnH;
    };

    SliderDef m_sliders[5];
    ToggleDef m_toggles[2];
    int m_sliderCount = 5;
    int m_toggleCount = 2;

    void layout();
    int hitTestSlider(float mx, float my) const;
    void updateSlider(int idx, float mx);
};
